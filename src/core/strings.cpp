#include "strings.h"

#include "utils.h"

#include <algorithm>
#include <string>
#include <iomanip>
#include <fstream>
#include <assert.h>

#include "stringtable.h"

namespace ndsloc {
namespace strings {

std::vector<U16String> exportDBStrings(const uint8_t* dataPtr, int inputSize, EFileFormat type)
{
    std::vector<U16String> ret;

    auto processString = [&](int64_t pos, int size)
    {
        std::u16string text;
        text.reserve(size);

        for (uint32_t at = 0; at + 1 < size; at += 2)
        {
            const char16_t unit = static_cast<char16_t>(utils::readUInt16(dataPtr, pos + at));

            if (unit == 0)
                break; // NULL terminator

            text.push_back(unit);
        }

        ret.emplace_back(pos, std::move(text));
    };

    switch (type)
    {
    case EFileFormat::StringDB:
    {
        const uint8_t fileCount = dataPtr[0];
        const uint32_t stringsStartOffset = 4 + fileCount * 4;
        uint32_t startPos = stringsStartOffset;

        struct dbString
        {
            uint32_t startPos = 0;
            uint32_t endPos = 0;
            std::string str;
        };

        std::vector<dbString> testStrings;
        testStrings.resize(fileCount);

        uint32_t currentPos = 0x4;
        for (int i = 0; i < fileCount; i++)
        {
            uint32_t endPos = utils::readUInt32(dataPtr, currentPos); currentPos += 4;
            testStrings[i].endPos = stringsStartOffset + endPos;
            testStrings[i].startPos = startPos;

            auto strSize = stringsStartOffset + endPos - startPos;
            startPos += strSize;
        }

        for (auto& s : testStrings)
        {
            int strSize = s.endPos - s.startPos;
            processString(s.startPos, strSize);
        }

        break;
    }
    case EFileFormat::StringDB_Long:
    {
        struct dbString
        {
            uint32_t val1;
            uint32_t val2;
            uint32_t start;
            uint32_t endTitle;
            uint32_t endContent;

            std::string str;
            uint32_t titleSize;
            uint32_t contentSize;
        };

        const uint8_t fileCount = dataPtr[0];
        const uint32_t stringsStartOffset = 4 + fileCount * 20; // Big header has 20 bytes per entry
        uint32_t startPos = stringsStartOffset;

        std::vector<dbString> testStrings;
        testStrings.resize(fileCount);

        uint32_t currentPos = 0x4;
        for (int i = 0; i < fileCount; i++)
        {
            testStrings[i].val1 = utils::readUInt32(dataPtr, currentPos); currentPos += 4;
            testStrings[i].val2 = utils::readUInt32(dataPtr, currentPos); currentPos += 4;
            testStrings[i].start = utils::readUInt32(dataPtr, currentPos); currentPos += 4;
            testStrings[i].endTitle = utils::readUInt32(dataPtr, currentPos); currentPos += 4;
            testStrings[i].endContent = utils::readUInt32(dataPtr, currentPos); currentPos += 4;

            testStrings[i].titleSize = testStrings[i].endTitle - testStrings[i].start;
            testStrings[i].contentSize = testStrings[i].endContent - testStrings[i].endTitle;
        }

        for (auto& s : testStrings)
        {
            auto titleStart = (s.start * 2) + stringsStartOffset;
            processString(titleStart, s.titleSize * 2);

            auto contentStart = (s.endTitle) * 2 + stringsStartOffset;
            processString(contentStart, s.contentSize * 2);
        }

        break;
    }
    case EFileFormat::StringDB_Short:
    {
        uint32_t pos = 0;
        const uint16_t stringCount = dataPtr[pos] + (dataPtr[pos + 1] << 8);
        pos += 4;
        const uint32_t unknown = dataPtr[pos] + (dataPtr[pos + 1] << 8);
        pos += 4;

        while (pos < inputSize)
        {
            uint32_t strSize = dataPtr[pos] + (dataPtr[pos + 1] << 8) + (dataPtr[pos + 2] << 16);
            strSize -= 4;
            pos += 4;

            processString(pos, strSize);

            pos += strSize;
        }
        break;
    }
    default:
        assert(false);
    }

    return ret;
}


void writeString(std::ofstream& os, const String& pair)
{
    os << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << pair.offset << "=";
    os << pair.text << "\n";
}

std::vector<String> exportStringsFromBuffer(const uint8_t* rom, int totalSize, uint32_t sectionOffset, const std::string& sectionName, int addressStart, bool bRemoveForb)
{
    std::vector<String> ret;

    static std::vector<std::string> completeStringsToIgnore = { "CAKP", "chara", "stmi", "shop", "score", "tex0", "tex1", "tex2", "beast", "bel0", "bel1", "bell", "info", "DELETED", "shift",
        "akubi", "point", "tgt1a", "tgt2a", "tgt3a", "tobig", "tosmall", "warp", "wall02", "wall03", "tou0", "tou1", "BOUT", "Light01", "Box12", "Plane01", "Plane02", "door", "BM01", "031A",
        "5A:0", "5C:0", "2B:0", "4B:0", "Co:3", "AnchorPos0", "AnchorPos1", "AnchorPos2", "AnchorPos3", "shutter1", "default.p2", "dummy3", "Bip01"
    };

    int firstAddr = 0;
    int lastAddr = 0;
    bool validCharFound = false;
    bool forbCharFound = false;
    for (int addr = 0; addr < totalSize; addr++) {
        bool usual = rom[addr] >= 0x41 && rom[addr] <= 0x7E;
        bool accents = rom[addr] == 0xC2 || rom[addr] == 0xC3 || (rom[addr] >= 0x80 && rom[addr] <= 0xBF);
        bool quotes = rom[addr] == 0xE2 || rom[addr] == 0x80 || rom[addr] == 0x9C || rom[addr] == 0x9D;
        bool unusual = (rom[addr] >= 0x20 && rom[addr] <= 0x40) || accents || quotes || rom[addr] == 0x0A;
        bool forb = rom[addr] == 0x93 || rom[addr] == 0x5F || rom[addr] == 0x2F;
        if (usual || unusual) {
            if (firstAddr == 0) {
                firstAddr = addr;
                lastAddr = addr;
            }
            else {
                lastAddr = addr;
            }
        }
        if (usual) {
            validCharFound = true;
        }
        if (forb && bRemoveForb) {
            forbCharFound = true;
        }
        if (!usual && !unusual) {
            if (firstAddr != 0) {
                if (!forbCharFound && validCharFound && lastAddr - firstAddr > 2) {
                    std::string subtitle;
                    for (int pAddr = firstAddr; pAddr <= lastAddr; pAddr++) {
                        if ((char)rom[pAddr] == 0x0A) {
                            subtitle.append("\\n");
                        }
                        else {
                            subtitle += (char)rom[pAddr];
                        }
                    }
                    if (subtitle.rfind("0:", 0) != 0
                        && subtitle.rfind("1A:", 0) != 0
                        && subtitle.rfind("5B:", 0) != 0
                        && subtitle.rfind("Co:0", 0) != 0
                        && subtitle.rfind("3F", 0) != 0
                        )
                    {
                        bool bIgnore = false;
                        for (auto& ign : completeStringsToIgnore)
                        {
                            if (subtitle == ign)
                            {
                                bIgnore = true;
                                break;
                            }
                        }

                        if (!bIgnore)
                        {
                            int actualAddress = addressStart + firstAddr;
                            ret.emplace_back(actualAddress, std::move(subtitle), sectionOffset, std::string(sectionName));
                        }
                    }
                }

                firstAddr = 0;
                lastAddr = 0;
                validCharFound = false;
                forbCharFound = false;
            }
        }
    }

    return ret;
}

static std::string csvEscape(const std::string& in)
{
    if (in.find_first_of(",\"\r\n") == std::string::npos)
        return in;

    std::string out;
    out.reserve(in.size() + 2);
    out.push_back('"');
    for (char c : in) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::ofstream startCsvFile(const std::string& out_path)
{
    std::ofstream os(out_path, std::ios::binary);
    os << "\xEF\xBB\xBF";
    os << "file,subfile,subfileoffset,offset,text\n";
    return std::move(os);
}

void writeStringsToCsv(std::ofstream& os, const std::string& filename, const std::vector<String>& strings)
{
    for (auto& pair : strings)
    {
        std::string text = pair.text;
        utils::replace_in_string(text, "\n", "\\n");
        utils::replace_in_string(text, "\r", "\\r");
        strings::writeCsvLine(os, filename, pair.section, pair.offset, pair.sectionOffset, text);
    }
}

void writeStringsToCsv(std::ofstream& os, const std::string& filename, const std::vector<U16String>& strings)
{
    for (auto& pair : strings)
    {
        std::u16string text = pair.text;
        utils::replace_in_ustring(text, u"\n", u"\\n");
        utils::replace_in_ustring(text, u"\r", u"\\r");
        strings::writeCsvLine(os, filename, "", pair.offset, 0, text);
    }
}

void writeCsvLine_common(std::ofstream& os, const std::string& filename, const std::string& subfilename, int offset, int subfileOffset)
{
    os << filename;
    os << ',' << subfilename;
    os << ",0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << subfileOffset;
    os << ",0x" << offset;
}

void writeCsvLine(std::ofstream& os, const std::string& filename, const std::string& subfilename, int offset, int subfileOffset, const std::string& text)
{
    writeCsvLine_common(os, filename, subfilename, offset, subfileOffset);
    os << ',' << csvEscape(text);
    os << '\n';
}

void writeCsvLine(std::ofstream& os, const std::string& filename, const std::string& subfilename, int offset, int subfileOffset, const std::u16string& text)
{
    writeCsvLine_common(os, filename, subfilename, offset, subfileOffset);
    auto conv_str = utf16ToUtf8(text);
    utils::replace_in_string(conv_str, "\n", "\\n");
    utils::replace_in_string(conv_str, "\r", "\\r");

    os << ',' << csvEscape(conv_str);
    os << '\n';
}

} // namespace strings
} // namespace ndsloc
