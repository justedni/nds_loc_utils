#pragma once

#include <string>
#include <vector>
#include <deque>

namespace ndsloc {

class LZSSFile
{
public:
    LZSSFile(const std::string& inputPath);
    LZSSFile(const uint8_t* inputPtr, int inputSize);

    void decompress();
    enum ECompressType: uint8_t{ LZSS = 0, ONZ };
    void compress(ECompressType type = LZSS);

    const std::vector<uint8_t>& getConvertedData() const { return m_outputBuffer; }
    void saveToDisk(const std::string& outPath);

private:
    void decompressLzss();
    void decompressOnz();

    void compressLzss();
    void compressOnz();

    void compress_impl(int readAheadBufferSize, uint8_t compressionType);
    static std::pair<int, int> search(const std::vector<uint8_t>& slidingWindow, const std::deque<uint8_t>& readAheadBuffer, int distance);

    std::vector<uint8_t> m_inputBuffer;
    const uint8_t* m_inputPtr = nullptr;
    int m_inputSize = 0;

    std::vector<uint8_t> m_outputBuffer;
};

} // namespace ndsloc
