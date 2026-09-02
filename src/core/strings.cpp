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

void writeIniString(std::ofstream& os, int offset, const std::string& text)
{
    os << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << offset << "=";
    os << text << "\n";
}

void writeIniString(std::ofstream& os, int offset, const std::u16string& text)
{
    writeIniString(os, offset, utf16ToUtf8(text));
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

std::ofstream startFile(const std::string& outPathNoExt, ExportFormat format)
{
    std::string fullPath = outPathNoExt;
    fullPath += (format == ExportFormat::Csv) ? ".csv" : ".ini";

    std::ofstream os(fullPath, std::ios::binary);

    if (format == ExportFormat::Csv)
    {
        os << "\xEF\xBB\xBF";
        os << "file,subfile,subfileoffset,offset,text\n";
    }

    return std::move(os);
}

void writeCsvLine(std::ofstream& os, const std::string& filename, const std::string& subfilename, int offset, int subfileOffset, const std::string& text)
{
    os << filename;
    os << ',' << subfilename;
    os << ",0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << subfileOffset;
    os << ",0x" << std::uppercase << std::setfill('0') << std::setw(4) << offset;
    os << ',' << csvEscape(text);
    os << '\n';
}

void writeCsvLine(std::ofstream& os, const std::string& filename, const std::string& subfilename, int offset, int subfileOffset, const std::u16string& text)
{
    writeCsvLine(os, filename, subfilename, offset, subfileOffset, utf16ToUtf8(text));
}

bool shouldIgnoreString(std::string str)
{
    if (str.empty()
        || str.find("DELETED") != std::string::npos)
        return true;

    return false;
}

const std::string& getSection(const String& entry) { return entry.section; }
uint32_t getSectionOffset(const String& entry) { return entry.sectionOffset; }

const std::string& getSection(const U16String&) { static const std::string empty; return empty; }
uint32_t getSectionOffset(const U16String&) { return 0; }

std::string getText(const String& entry) { return entry.text; }
std::string getText(const U16String& entry) { return utf16ToUtf8(entry.text); }

template <typename T>
void writeStringsImpl(ExportFormat format, std::ofstream& os, uint32_t fileOffset, const std::string& filename, const std::vector<T>& strings)
{
    for (const auto& entry : strings)
    {
        std::string text = getText(entry);
        if (shouldIgnoreString(text))
            continue;

        utils::replace_in_string(text, "\n", "\\n");
        utils::replace_in_string(text, "\r", "\\r");

        if (format == ExportFormat::Csv)
            strings::writeCsvLine(os, filename, getSection(entry), entry.offset, getSectionOffset(entry), text);
        else
            strings::writeIniString(os, fileOffset + entry.offset, text);
    }
}

void writeStrings(ExportFormat format, std::ofstream& os, uint32_t fileOffset, const std::string& filename, const std::vector<String>& strings)
{
    writeStringsImpl(format, os, fileOffset, filename, strings);
}
 
void writeStrings(ExportFormat format, std::ofstream& os, uint32_t fileOffset, const std::string& filename, const std::vector<U16String>& strings)
{
    writeStringsImpl(format, os, fileOffset, filename, strings);
}


} // namespace strings
} // namespace ndsloc
