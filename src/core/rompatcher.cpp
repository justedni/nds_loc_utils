#include "rompatcher.h"

#include "strings.h"
#include "stringtable.h"
#include "nds.h"
#include "utils.h"
#include "p2.h"
#include "cakp.h"
#include "lzss.h"

#include <algorithm>
#include <cstring>
#include <assert.h>

namespace ndsloc {
namespace patcher {

bool compareLines(const std::vector<strings::CsvLine> refLines, std::vector<strings::CsvLine>& modLines)
{
    bool bNeedUpdate = false;

    for (int i = 0; i < modLines.size(); i++)
    {
        auto& modLine = modLines[i];

        auto found = std::find_if(refLines.begin(), refLines.end(), [&](const auto& e) { return e.offset == modLine.offset; });
        if (found == refLines.end())
        {
            assert(false);
            continue;
        }

        const strings::CsvLine* foundRefLine = &(*found);
        if (foundRefLine->text != modLine.text)
        {
            modLine.bNeedUpdate = true;
            bNeedUpdate = true;
        }
    }

    return bNeedUpdate;
}

void compareCsvs(const std::vector<strings::CsvNdsFile>& refFiles, std::vector<strings::CsvNdsFile>& modFiles)
{
    for (auto& modFile : modFiles)
    {
        auto foundRefFile = std::find_if(refFiles.begin(), refFiles.end(), [&](const auto& e) { return e.filename == modFile.filename; });
        if (foundRefFile != refFiles.end())
        {
            bool bNeedUpdate = compareLines(foundRefFile->lines, modFile.lines);
            if (bNeedUpdate)
                modFile.bNeedUpdate = true;

            const auto& refSubfiles = foundRefFile->subfiles;
            for (auto& modSubfile : modFile.subfiles)
            {
                auto foundRefSubFile = std::find_if(refSubfiles.begin(), refSubfiles.end(), [&](const auto& e) { return e.filename == modSubfile.filename; });
                if (foundRefSubFile != refSubfiles.end())
                {
                    bool bDirty = compareLines(foundRefSubFile->lines, modSubfile.lines);
                    if (bDirty)
                    {
                        modSubfile.bNeedUpdate = true;
                        modFile.bNeedUpdate = true;
                    }
                }
            }
        }
    }
}


void patchP2File(P2File& file, NDS::NDSFileSystem& fs, NDS::NDSEntry* entry, const strings::CsvNdsFile& patchData)
{
    auto& subfiles = file.readAndGetFileTable();

    for (auto& patchFile : patchData.subfiles)
    {
        if (!patchFile.bNeedUpdate)
            continue;

        auto found = std::find_if(subfiles.begin(), subfiles.end(), [&](auto& e) { return e.getFilename() == patchFile.filename; });
        assert(found != subfiles.end());
        if (found != subfiles.end())
        {
            auto& subfile = *found;
            if (subfile.isCompressed())
            {
                uint32_t newSize = patcher::patchCompressedSection(subfile.inputPtr, subfile.fileSize, subfile.maxSize, patchFile.lines, false);
                uint16_t subfileIndex = std::distance(subfiles.begin(), found);
                file.updateEntrySizeInTable(subfileIndex, newSize);
            }
            else
            {
                assert(false);
            }
        }
    }
}

void createPatch(const std::string& romPath, const std::vector<strings::CsvNdsFile>& modFiles)
{
    auto ndsRom = utils::readBinaryFile(romPath);

    createPatch(ndsRom.data(), ndsRom.size(), modFiles);

    utils::saveBinaryFile(ndsRom, romPath + "_mod");
}

void createPatch(uint8_t* rom, uint32_t romSize, const std::vector<strings::CsvNdsFile>& modFiles)
{
    auto ndsfs = NDS::NDSFileSystem(rom, romSize);
    ndsfs.getRomFileSystem(true);

    for (auto& modFile : modFiles)
    {
        if (!modFile.bNeedUpdate)
            continue;

        auto* entry = ndsfs.findEntryByName(modFile.filename);
        assert(entry);
        if (!entry)
            continue;

        auto* data = rom + entry->start;

        if (entry->type == "p2")
        {
            auto p2file = ndsloc::P2File(data, entry->size);
            patchP2File(p2file, ndsfs, entry, modFile);
        }
        else if (entry->type == "z")
        {
            uint32_t newSize = patcher::patchCompressedSection(data, entry->size, entry->maxSize, modFile.lines, true);
            ndsfs.patchEntrySize(*entry, newSize);
        }
    }
}

enum class TextEncoding
{
    Utf8,
    Utf16LE
};

struct PatchTarget
{
    uint8_t* buffer = nullptr;
    uint32_t bufferSize = 0;  // whole decompressed subfile, for bounds checking
    uint32_t offset = 0;      // where the string starts, from buffer
    uint32_t capacity = 0;    // bytes the slot holds (includes terminator)
    TextEncoding encoding = TextEncoding::Utf8;
};

enum class PatchStatus
{
    Ok,
    BadTarget,
    BadEncoding,
    BadEscape,
    BadSourceText,
    TooLong
};


struct PatchResult
{
    PatchStatus status = PatchStatus::BadTarget;
    uint32_t required = 0;
    uint32_t written = 0;

    bool ok() const { return status == PatchStatus::Ok; }
};

enum class PadMode
{
    Zero,
    Spaces,
    FF,
};


uint32_t encodedSizeUtf8(uint32_t cp)
{
    if (cp < 0x80)    return 1;
    if (cp < 0x800)   return 2;
    if (cp < 0x10000) return 3;
    return 4;
}

uint32_t encodedSizeUtf16(uint32_t cp)
{
    return cp < 0x10000 ? 2u : 4u;
}

bool hexDigit(char c, uint32_t& value)
{
    if (c >= '0' && c <= '9') { value = static_cast<uint32_t>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { value = static_cast<uint32_t>(c - 'a') + 10; return true; }
    if (c >= 'A' && c <= 'F') { value = static_cast<uint32_t>(c - 'A') + 10; return true; }
    return false;
}

bool readHex(const char* text, size_t& at, uint32_t digits, uint32_t& value)
{
    value = 0;

    for (uint32_t i = 0; i < digits; ++i)
    {
        uint32_t digit = 0;
        if (text[at] == '\0' || !hexDigit(text[at], digit))
            return false;

        value = (value << 4) | digit;
        ++at;
    }

    return true;
}

bool decodeUtf8(const char* text, size_t& at, uint32_t& cp)
{
    const uint8_t lead = static_cast<uint8_t>(text[at]);
    uint32_t extra = 0;

    if (lead < 0x80) { cp = lead;         extra = 0; }
    else if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F;  extra = 1; }
    else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F;  extra = 2; }
    else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07;  extra = 3; }
    else return false;

    ++at;

    for (uint32_t i = 0; i < extra; ++i)
    {
        const uint8_t next = static_cast<uint8_t>(text[at]);
        if ((next & 0xC0) != 0x80)
            return false;

        cp = (cp << 6) | (next & 0x3F);
        ++at;
    }

    return cp <= 0x10FFFF;
}

PatchStatus decodeSource(const char* text, std::vector<uint32_t>& out)
{
    out.clear();

    if (text == nullptr)
        return PatchStatus::BadSourceText;

    size_t at = 0;
    while (text[at] != '\0')
    {
        if (text[at] != '\\')
        {
            uint32_t cp = 0;
            if (!decodeUtf8(text, at, cp))
                return PatchStatus::BadSourceText;

            out.push_back(cp);
            continue;
        }

        ++at;

        const char escape = text[at];
        uint32_t value = 0;

        switch (escape)
        {
        case 'n':  out.push_back(0x0A); ++at; break;
        case 't':  out.push_back(0x09); ++at; break;
        case '\\': out.push_back('\\'); ++at; break;

        case 'x':
            ++at;
            if (!readHex(text, at, 2, value))
                return PatchStatus::BadEscape;
            out.push_back(value);
            break;

        case 'u':
            ++at;
            if (!readHex(text, at, 4, value))
                return PatchStatus::BadEscape;
            out.push_back(value);
            break;

        default:
            return PatchStatus::BadEscape;
        }
    }

    return PatchStatus::Ok;
}


PatchStatus checkTarget(const PatchTarget& target)
{
    if (target.buffer == nullptr || target.capacity == 0)
        return PatchStatus::BadTarget;

    if (target.offset > target.bufferSize || target.capacity > target.bufferSize - target.offset)
        return PatchStatus::BadTarget;

    if (target.encoding == TextEncoding::Utf16LE && (target.capacity & 1) != 0)
        return PatchStatus::BadEncoding;

    return PatchStatus::Ok;
}

PatchResult prepare(const PatchTarget& target, const char* text, std::vector<uint32_t>& codePoints)
{
    PatchResult result;

    result.status = checkTarget(target);
    if (result.status != PatchStatus::Ok)
        return result;

    result.status = decodeSource(text, codePoints);
    if (result.status != PatchStatus::Ok)
        return result;

    const bool wide = target.encoding == TextEncoding::Utf16LE;
    const uint32_t terminator = wide ? 2u : 1u;

    uint32_t needed = terminator;
    for (uint32_t cp : codePoints)
        needed += wide ? encodedSizeUtf16(cp) : encodedSizeUtf8(cp);

    result.required = needed;

    // Todo: change this if we prefer to write a partial line instead
    // of not writing at all
    if (needed > target.capacity)
        result.status = PatchStatus::TooLong;

    return result;
}


void encodeUtf8(uint32_t cp, uint8_t* out, uint32_t& at)
{
    if (cp < 0x80)
    {
        out[at++] = static_cast<uint8_t>(cp);
    }
    else if (cp < 0x800)
    {
        out[at++] = static_cast<uint8_t>(0xC0 | (cp >> 6));
        out[at++] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        out[at++] = static_cast<uint8_t>(0xE0 | (cp >> 12));
        out[at++] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
        out[at++] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    }
    else
    {
        out[at++] = static_cast<uint8_t>(0xF0 | (cp >> 18));
        out[at++] = static_cast<uint8_t>(0x80 | ((cp >> 12) & 0x3F));
        out[at++] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
        out[at++] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    }
}

void encodeUtf16(uint32_t cp, uint8_t* out, uint32_t& at)
{
    if (cp < 0x10000)
    {
        out[at++] = static_cast<uint8_t>(cp & 0xFF);
        out[at++] = static_cast<uint8_t>(cp >> 8);
        return;
    }

    const uint32_t adjusted = cp - 0x10000;
    const uint16_t high = static_cast<uint16_t>(0xD800 + (adjusted >> 10));
    const uint16_t low = static_cast<uint16_t>(0xDC00 + (adjusted & 0x3FF));

    out[at++] = static_cast<uint8_t>(high & 0xFF);
    out[at++] = static_cast<uint8_t>(high >> 8);
    out[at++] = static_cast<uint8_t>(low & 0xFF);
    out[at++] = static_cast<uint8_t>(low >> 8);
}

PatchResult patchLine(const PatchTarget& target, const char* text, PadMode padMode)
{
    std::vector<uint32_t> codePoints;
    PatchResult result = prepare(target, text, codePoints);

    if (!result.ok())
        return result;

    const bool wide = target.encoding == TextEncoding::Utf16LE;
    uint8_t* const slot = target.buffer + target.offset;

    uint32_t at = 0;
    for (uint32_t cp : codePoints)
    {
        if (wide)
            encodeUtf16(cp, slot, at);
        else
            encodeUtf8(cp, slot, at);
    }

    slot[at++] = 0;
    if (wide)
        slot[at++] = 0;

    result.written = at;

    const uint8_t pad = (padMode == PadMode::Spaces) ? 0x20 : 0x00;

    if (padMode == PadMode::Spaces && wide)
    {
        for (uint32_t i = at; i + 1 < target.capacity; i += 2)
        {
            slot[i] = 0x20;
            slot[i + 1] = 0x00;
        }
    }
    else
    {
        for (uint32_t i = at; i < target.capacity; ++i)
            slot[i] = pad;
    }

    return result;
}

void copyBinaryBuffer(const uint8_t* srcData, uint32_t srcSize, uint8_t* targetData, uint32_t targetSize, uint32_t maxSize, PadMode padMode)
{
    if (targetSize <= maxSize)
    {
        std::memcpy((void*)targetData, srcData, srcSize);
        int remaining = targetSize - srcSize;
        if (remaining > 0)
        {
            uint8_t pad = 0x00;
            if (padMode == PadMode::Spaces) { pad = 0x20; }
            else if(padMode == PadMode::FF) { pad = 0xFF; }
            memset((void*)(targetData + srcSize), pad, remaining);
        }
    }
    else
    {
        assert(false);
    }
}

uint32_t patchCompressedSection(uint8_t* inputPtr, uint32_t inputSize, uint32_t maxSize, const std::vector<strings::CsvLine>& lines, bool bPadFF)
{
    ndsloc::LZSSFile previous(inputPtr, inputSize);
    previous.decompress();

    auto decompressed = previous.getConvertedData();
    auto* data = decompressed.data();
    auto dataSize = static_cast<uint32_t>(decompressed.size());

    std::vector<String> strings;

    if (cakp::isCAKP(data))
    {
        CAKPFile cakp(data, dataSize, "");
        cakp.extractStrings(strings);
    }
    else if (StringTableFile::looksValid(data, dataSize))
    {
        std::vector<U16String> wide;
        StringTableFile table(data, dataSize);
        const bool ok = table.extractStrings(wide);
        strings::appendWideStrings("", wide, strings);
    }

    auto algo = previous.getCompressionMethod();

    for (auto& line : lines)
    {
        if (line.bNeedUpdate)
        {
            auto foundString = std::find_if(strings.begin(), strings.end(), [&](auto& e) { return e.offset == line.offset; });
            assert(foundString != strings.end());
            if (foundString != strings.end())
            {
                auto ref = *foundString;
 
                PatchTarget target;
                target.buffer = data;
                target.bufferSize = dataSize;
                target.offset = line.offset;
                target.encoding = ref.bIsWide ? TextEncoding::Utf16LE : TextEncoding::Utf8;
                target.capacity = ref.text.size() + 1;
                if (ref.bIsWide) { target.capacity *= 2; }
  
                patchLine(target, line.text.c_str(), PadMode::Zero);
            }
        }
    }

    ndsloc::LZSSFile newer(decompressed.data(), decompressed.size());
    newer.compress(algo);

    auto newerData = newer.getConvertedData();

    auto padMode = bPadFF ? PadMode::FF : PadMode::Zero;
    copyBinaryBuffer(newerData.data(), newerData.size(), inputPtr, inputSize, maxSize, padMode);

    return newerData.size();
}

} // namespace patcher
} // namespace ndsloc
