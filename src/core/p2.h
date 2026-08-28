#pragma once

#include <string>
#include <vector>

namespace ndsloc {

namespace strings {
    typedef std::vector<std::pair<int, std::string>> StringList;
}

struct P2SubFile
{
    uint16_t chunkId = 0;
    std::string fileName;
    uint32_t fileOffset = 0;
    uint16_t fileSize = 0;
    uint16_t maxSize = 0;
    uint8_t someFlag = 0;

    const uint8_t* inputPtr = nullptr;

    std::string getFilename() const
    {
        if (!fileName.empty())
            return fileName;
        else
            return std::to_string(chunkId) + ".Z";
    }
};

class P2Archive
{
public:
    P2Archive(const std::string& filePath);
    P2Archive(const uint8_t* inputPtr, int inputSize);

    std::vector<P2SubFile>& getFileTable() { return m_subfiles; }
    void updateEntry(int id, const uint8_t* data, uint32_t dataSize);
    void saveToDisk(const std::string& outPath);

    void exportAllCAKPStrings(const std::string& outPath);
    void importCAKPStringsFromIni(const std::string& iniFilePath);

    const std::vector<uint8_t>& getData() const { return m_inputBuffer; }

private:
    void readFileTable();
    void updateTableSizes();
    void replaceStrings(int chunkId, const std::vector<std::pair<int, std::string>>& strings);

    std::vector<uint8_t> m_inputBuffer;
    const uint8_t* m_inputPtr = nullptr;
    int m_inputSize = 0;

    std::vector<P2SubFile> m_subfiles;
    std::vector<strings::StringList> m_strings;
};

} // namespace ndsloc
