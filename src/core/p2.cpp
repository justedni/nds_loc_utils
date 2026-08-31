#include "p2.h"

#include <filesystem>
#include <assert.h>
#include <fstream>

#include "utils.h"
#include "lzss.h"
#include "strings.h"
#include "cakp.h"
#include "stringtable.h"

namespace ndsloc {

namespace p2 {

bool isP2File(const uint8_t* buffer)
{
    return (std::memcmp(buffer, "P2", 2) == 0);
}

} // namespace p2

P2File::P2File(const std::string& filePath)
{
    m_inputBuffer = utils::readBinaryFile(filePath);
    m_inputPtr = m_inputBuffer.data();
    m_inputSize = m_inputBuffer.size();
}

P2File::P2File(const uint8_t* inputPtr, uint32_t inputSize)
    : m_inputPtr(inputPtr)
    , m_inputSize(inputSize)
    , m_language(cakp::LANG_EN)
{
}


bool P2File::readFileTable()
{
    m_subfiles.clear();

    const uint8_t* dataPtr = m_inputPtr;
    const uint32_t inputSize = m_inputSize;

    if (!p2::isP2File(dataPtr) || inputSize < p2::kSectorTableAt)
        return false; // Not a P2 file

    const uint8_t fileCount = dataPtr[2];
    const uint8_t fileType = dataPtr[3];
    const uint32_t firstFileOffset = utils::readUInt24(dataPtr, 0x0C);

    if (fileCount == 0)
        return false;

    if (fileType != p2::kTypePlain && fileType != p2::kTypeNamed)
        return false; // Unknown P2 type

    uint32_t currentPos = p2::kSectorTableAt;
    if (currentPos + fileCount * 2 > inputSize)
        return false;

    m_subfiles.resize(fileCount);

    for (int i = 0; i < fileCount; i++)
    {
        uint16_t val = utils::readUInt16(dataPtr, currentPos);
        m_subfiles[i].fileOffset = firstFileOffset + val * p2::kSectorSize;
        m_subfiles[i].chunkId = static_cast<uint16_t>(i);
        currentPos += 2;
    }

    if (fileType == p2::kTypeNamed) // P2 file with bigger header containing filesizes and filenames
    {
        for (int i = 0; i < fileCount; i++)
        {
            auto& fileDesc = m_subfiles[i];

            uint32_t val = utils::readUInt24(dataPtr, currentPos);
            fileDesc.fileSize = val;
            if (i + 1 < fileCount)
            {
                auto& nextFileDesc = m_subfiles[i + 1];
                fileDesc.maxSize = nextFileDesc.fileOffset - fileDesc.fileOffset;
            }
            else
            {
                fileDesc.maxSize = inputSize - fileDesc.fileOffset;
            }
            fileDesc.someFlag = dataPtr[currentPos + 3]; // Don't know what this value is for
            currentPos += 4;
        }

        for (int i = 0; i < fileCount; i++)
        {
            auto& fileDesc = m_subfiles[i];
            const char* name = static_cast<const char*>((char*)dataPtr + currentPos);
            fileDesc.fileName = std::string(name, std::min(strlen(name), size_t(8)));
            currentPos += 8;
        }
    }
    else if (fileType == 0x00) // needs to deduce the chunk sizes
    {
        for (int i = 0; i < fileCount; i++)
        {
            auto& fileDesc = m_subfiles[i];

            if (i + 1 < fileCount)
            {
                auto& nextFileDesc = m_subfiles[i + 1];
                fileDesc.fileSize = nextFileDesc.fileOffset - fileDesc.fileOffset;
            }
            else
            {
                fileDesc.fileSize = inputSize - fileDesc.fileOffset;
            }

            fileDesc.maxSize = fileDesc.fileSize;
            fileDesc.someFlag = 0x80;
        }
    }
    else
    {
        assert(false); // Unknown P2 type
    }

    for (int i = 0; i < fileCount; i++)
    {
        auto& fileDesc = m_subfiles[i];
        fileDesc.inputPtr = dataPtr + fileDesc.fileOffset;
    }
}

void P2File::saveToDisk(const std::string& outPath)
{
    utils::saveBinaryFile(m_inputBuffer, outPath);
}

void appendStrings(const std::vector<String>& strings, std::vector<String>& out)
{
    for (auto& str : strings)
    {
        if (str.text == "\x01")
            continue;

        out.push_back(std::move(str));
    }
}

void appendWideStrings(const P2SubFile& subfile, const std::vector<U16String>& strings, std::vector<String>& out)
{
    for (auto& str : strings)
    {
        out.emplace_back(str.offset, utf16ToUtf8(str.text), subfile.fileOffset, subfile.getFilename());
    }
}

bool P2File::extractPayload(const P2SubFile& subfile, const uint8_t* payload, uint32_t payloadSize, uint32_t payloadOffset, uint32_t depth, std::vector<String>& out)
{
    assert(subfile.fileOffset == payloadOffset);

    if (payload == nullptr || payloadSize < 8)
        return true; // stub entries

    std::vector<String> strings;

    if (cakp::isCAKP(payload))
    {
        CAKPFile cakp(payload, payloadSize, payloadOffset, subfile.getFilename());
        cakp.setLanguage(m_language);
        const bool ok = cakp.extractStrings(strings);

        appendStrings(strings, out);
        return ok;
    }

    if (p2::isP2File(payload))
    {
        if (depth >= m_maxDepth)
            return true;

        P2File nested(payload, payloadSize);
        nested.setLanguage(m_language);
        //nested.setMaxDepth(m_maxDepth);

        std::vector<String> nestedOut;
        const bool ok = nested.extractStrings(nestedOut);
        appendStrings(nestedOut, out);
        return ok;
    }

    if (StringTableFile::looksValid(payload, payloadSize))
    {
        std::vector<U16String> wide;
        StringTableFile table(payload, payloadSize);
        const bool ok = table.extractStrings(wide);
        appendWideStrings(subfile, wide, out);
        return ok;
    }

    CAKPFile section(payload, payloadSize, payloadOffset, subfile.getFilename());
    section.setLanguage(m_language);
    section.extractSectionStrings(strings);
    appendStrings(strings, out);
    return true;
}

uint32_t getExpectedDecompressedSize(const uint8_t* dataPtr, uint32_t dataSize)
{
    if (dataSize < 4)
        return 0;

    const uint32_t packed = utils::readUInt24(dataPtr, 1);
    if (packed != 0)
        return packed;

    return dataSize >= 8 ? utils::readUInt32(dataPtr, 4) : 0;
}

bool P2File::extractSubFile(const P2SubFile& subfile, uint32_t depth, std::vector<String>& out)
{
    if (subfile.isCompressed())
    {
        ndsloc::LZSSFile lzss(subfile.inputPtr, subfile.fileSize);
        lzss.decompress();

        const auto decompressed = lzss.getConvertedData();
        auto* data = decompressed.data();
        auto dataSize = static_cast<uint32_t>(decompressed.size());

        const uint32_t expected = getExpectedDecompressedSize(subfile.inputPtr, subfile.maxSize);
        assert(expected == dataSize);

        auto type = utils::getFileFormat(data, dataSize);

        return extractPayload(subfile, data, dataSize, subfile.fileOffset, depth, out);
    }
    else
    {
        return extractPayload(subfile, subfile.inputPtr, subfile.fileSize, subfile.fileOffset, depth, out);
    }
}

bool P2File::extractStrings(std::vector<String>& out)
{
    out.clear();

    if (!readFileTable())
        return false;

    bool ok = true;
    for (const P2SubFile& subfile : m_subfiles)
    {
        if (!extractSubFile(subfile, 0, out))
            ok = false;
    }

    return ok;
}

} // namespace ndsloc
