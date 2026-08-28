#include "p2.h"

#include <filesystem>
#include <assert.h>
#include <fstream>

#include "utils.h"
#include "lzss.h"
#include "strings.h"

namespace ndsloc {

P2Archive::P2Archive(const std::string& filePath)
{
    m_inputBuffer = utils::readBinaryFile(filePath);
    m_inputPtr = m_inputBuffer.data();
    m_inputSize = m_inputBuffer.size();

    auto path = std::filesystem::path(filePath);
    m_filename = (path.parent_path().stem().string() + std::string("/") + path.stem().string());

    readFileTable();
}

void P2Archive::readFileTable()
{
    m_subfiles.clear();

    const uint8_t* dataPtr = m_inputPtr;
    int inputSize = m_inputSize;

    if (dataPtr[0] != 0x50 && dataPtr[1] != 0x32)
    {
        assert(false); // Not a P2 file
        return;
    }

    uint8_t fileCount = dataPtr[2];
    uint8_t fileType = dataPtr[3];
    uint32_t firstFileOffset = dataPtr[0xc] + (dataPtr[0xd] << 8) + (dataPtr[0xe] << 16);

    m_subfiles.resize(fileCount);
    uint32_t currentPos = 0x10;

    for (int i = 0; i < fileCount; i++)
    {
        uint16_t val = dataPtr[currentPos] + (dataPtr[currentPos + 1] << 8);
        m_subfiles[i].fileOffset = firstFileOffset + val * 0x200;
        m_subfiles[i].chunkId = i;
        currentPos += 2;
    }

    if (fileType == 0x80) // P2 file with bigger header containing filesizes and filenames
    {
        for (int i = 0; i < fileCount; i++)
        {
            auto& fileDesc = m_subfiles[i];

            uint32_t val = dataPtr[currentPos] + (dataPtr[currentPos + 1] << 8) + (dataPtr[currentPos + 2] << 16);
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

void P2Archive::updateEntry(int id, const uint8_t* data, uint32_t dataSize)
{
    assert(id >= 0 && id < m_subfiles.size());
    if (id < 0 || id >= m_subfiles.size())
        return;

    auto& fileDesc = m_subfiles[id];
    assert(dataSize < fileDesc.maxSize);
    if (dataSize < fileDesc.maxSize)
    {
        memcpy_s((void*)fileDesc.inputPtr, dataSize, data, dataSize);
        int remaining = fileDesc.fileSize - dataSize;
        if (remaining > 0)
        {
            memset((void*)(fileDesc.inputPtr + dataSize), 0, remaining);
        }

        fileDesc.fileSize = dataSize;
    }

    updateTableSizes();
}

void P2Archive::updateTableSizes()
{
    const uint8_t fileCount = m_subfiles.size();
    uint8_t fileType = m_inputBuffer[3];

    uint32_t currentPos = 0x10;
    currentPos += (2 * fileCount);

    if (fileType == 0x80) // P2 file with bigger header containing filesizes and filenames
    {
        for (int i = 0; i < fileCount; i++)
        {
            auto& fileDesc = m_subfiles[i];

            int newSize = fileDesc.fileSize;
            m_inputBuffer[currentPos] = static_cast<uint8_t>(newSize & 0xFF);
            m_inputBuffer[currentPos + 1] = static_cast<uint8_t>((newSize >> 8) & 0xFF);
            m_inputBuffer[currentPos + 2] = static_cast<uint8_t>((newSize >> 16) & 0xFF);
            currentPos += 4;
        }
    }
    else if (fileType == 0x00) // needs to deduce the chunk sizes
    {
        // The filesize is not saved in the table
    }
    else
    {
        assert(false); // Unknown P2 type
    }
}

void P2Archive::saveToDisk(const std::string& outPath)
{
    utils::saveBinaryFile(m_inputBuffer, outPath);
}

void P2Archive::exportAllCAKPStrings(const std::string& outPath)
{
    std::ofstream os(outPath);

    m_strings.clear();
    m_strings.resize(m_subfiles.size());

    for (int i = 0; i < m_subfiles.size(); i++)
    {
        auto& subfile = m_subfiles[i];

        ndsloc::LZSSFile lzss(subfile.inputPtr, subfile.fileSize);
        lzss.decompress();

        const auto buffer = lzss.getConvertedData();
        m_strings[i] = strings::exportStringsFromBuffer(buffer.data(), buffer.size(), 0);
        
        auto& strings = m_strings[i];
        if (!strings.empty())
        {
            os << "[";
            os << std::to_string(subfile.chunkId);
            os << ":";
            os << subfile.fileName;
            os << "]\n";

            for (auto& elem : strings)
            {
                strings::writeString(os, elem);
            }

            os << "\n";
        }
    }
}

void stripEol(std::string& line)
{
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
    {
        line.pop_back();
    }
}

bool parseSectionHeader(const std::string& line, int& outChunkId, std::string& outFileName)
{
    if (line.size() < 3 || line.front() != '[' || line.back() != ']')
        return false;

    const std::string content = line.substr(1, line.size() - 2);
    const auto separator = content.find(':');
    if (separator == std::string::npos)
        return false;

    outChunkId = std::stoi(content.substr(0, separator));
    outFileName = content.substr(separator + 1);
    return !outFileName.empty();
}

void P2Archive::replaceStrings(int chunkId, const std::vector<std::pair<int, std::string>>& strings)
{
    assert(chunkId >= 0 && chunkId < m_subfiles.size());
    auto& subfile = m_subfiles[chunkId];

    // Decompress first
    ndsloc::LZSSFile previous(subfile.inputPtr, subfile.fileSize);
    previous.decompress();

    // Then replace the string
    auto convData = previous.getConvertedData();

    for (auto& targetStr : strings)
    {
        // TODO: currently we don't check that the new size doesn't overflow!!
        auto* targetPtr = convData.data() + targetStr.first;
        auto& newStr = targetStr.second;
        memcpy_s((void*)targetPtr, newStr.size(), newStr.data(), newStr.size());
    }

    // And recompress
    ndsloc::LZSSFile newer(convData.data(), convData.size());
    newer.compress();

    auto newerData = newer.getConvertedData();
    updateEntry(chunkId, newerData.data(), newerData.size());
}

void P2Archive::importCAKPStringsFromIni(const std::string& iniFilePath)
{
    std::ifstream is(iniFilePath);

    int currentChunkId = -1;
    std::string currentSectionName;
    std::vector<std::pair<int, std::string>> changedStrings;

    int lineNumber = 0;

    char entryname[32];
    char entryval[1024];

    int chunkToRegenerate = -1;

    std::string line;
    while (std::getline(is, line))
    {
        lineNumber++;
        stripEol(line);

        if (line.empty())
            continue;

        if (line[0] == '[')
        {
            if (chunkToRegenerate != -1)
            {
                replaceStrings(chunkToRegenerate, changedStrings);
            }

            chunkToRegenerate = -1;
            changedStrings.clear();

            int blockId = -1;
            std::string blockName;
            if (parseSectionHeader(line, blockId, blockName))
            {
                currentChunkId = blockId;
                currentSectionName = blockName;
            }
            else
            {
                assert(false);
            }

            continue;
        }
        else
        {

            int ret = sscanf(line.c_str(), "%31[A-Za-z_0-9]=%[^\t\r\n]", entryname, entryval);
            entryname[31] = '\0';
            if (ret < 2)
                continue;

            std::string entrynameStr = std::string(entryname);
            if (entrynameStr.compare(0, 2, "0x") == 0)
            {
                unsigned int addr = std::stoul(entrynameStr.substr(2), nullptr, 16);

                assert(currentChunkId >= 0 && currentChunkId < m_strings.size());
                auto& stringList = m_strings[currentChunkId];

                auto found = std::find_if(stringList.begin(), stringList.end(), [&addr](auto& p) { return p.first == addr; });
                if (found != stringList.end() && found->second != entryval)
                {
                    // String has changed: needs to regenerate
                    chunkToRegenerate = currentChunkId;
                    changedStrings.emplace_back(addr, entryval);
                }
            }
        }
    }

    if (chunkToRegenerate != -1)
    {
        replaceStrings(chunkToRegenerate, changedStrings);
    }
}

} // namespace ndsloc
