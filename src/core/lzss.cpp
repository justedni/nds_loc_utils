#include "lzss.h"

#include <assert.h>

#include "utils.h"

namespace ndsloc {

LZSSFile::LZSSFile(const std::string& inputPath)
{
    m_inputBuffer = utils::readBinaryFile(inputPath);
    m_inputPtr = m_inputBuffer.data();
    m_inputSize = m_inputBuffer.size();
}

LZSSFile::LZSSFile(const uint8_t* inputPtr, int inputSize)
    : m_inputPtr(inputPtr)
    , m_inputSize(inputSize)
{
}

void LZSSFile::decompress()
{
    assert(m_inputSize > 0 && m_inputPtr != nullptr);

    if (m_inputPtr[0] == 0x10) // LZSS
    {
        decompressLzss();
    }
    else if (m_inputPtr[0] == 0x11) // ONZ
    {
        decompressOnz();
    }
    else
    {
        // Not implemented
    }
}


void LZSSFile::decompressLzss()
{
    int currentPosition = 4;
    int distance = 1;

    const uint8_t* compressedData = m_inputPtr;
    const int inputSize = m_inputSize;

    int outputSize = compressedData[1] + (compressedData[2] << 8) + (compressedData[3] << 16);
    m_outputBuffer.resize(outputSize);
    uint8_t* outPtrStart = m_outputBuffer.data();
    uint8_t* outPtr = outPtrStart;

    int processedBytes = 0;
    while (processedBytes <= outputSize && currentPosition < inputSize)
    {
        int forward = 1;
        for (int i = 0; i < 8; i++)
        {
            if (currentPosition + forward >= inputSize)
            {
                break;
            }

            if (processedBytes >= outputSize)
            {
                break;
            }

            if (((compressedData[currentPosition] >> (7 - i)) & 1) == 1)
            {
                int amountToCopy = 3 + ((compressedData[currentPosition + forward] >> 4) & 0xF);
                int copyFrom = distance + ((compressedData[currentPosition + forward] & 0xF) << 8)
                    + compressedData[currentPosition + forward + 1];
                int copyPosition = processedBytes - copyFrom;
                for (int u = 0; u < amountToCopy; u++)
                {
                    if ((copyPosition + (u % copyFrom)) < processedBytes)
                    {
                        *outPtr = outPtrStart[copyPosition + (u % copyFrom)];
                        outPtr++;
                        processedBytes++;
                    }
                    else
                    {
                        return;
                    }
                }

                forward += 2;
            }
            else
            {
                *outPtr = compressedData[currentPosition + forward];
                outPtr++;

                processedBytes++;
                forward++;
            }
        }

        currentPosition += forward;
    }

    while (processedBytes < outputSize)
    {
        *outPtr = 0;
        outPtr++;

        processedBytes++;
    }
}

void LZSSFile::decompressOnz()
{
    int currentPosition = 4;

    const uint8_t* compressedData = m_inputPtr;
    const int inputSize = m_inputSize;

    int outputSize = compressedData[1] + (compressedData[2] << 8) + (compressedData[3] << 16);
    m_outputBuffer.resize(outputSize);
    uint8_t* outPtrStart = m_outputBuffer.data();
    uint8_t* outPtr = outPtrStart;

    int processedBytes = 0;
    while (processedBytes <= outputSize && currentPosition < inputSize)
    {
        int forward = 1;
        for (int i = 0; i < 8; i++)
        {
            if (currentPosition + forward >= inputSize)
            {
                break;
            }

            if (processedBytes >= outputSize)
            {
                break;
            }

            if (((compressedData[currentPosition] >> (7 - i)) & 1) == 1)
            {
                int amountToCopy;
                int copyFrom;
                uint8_t byte1 = compressedData[currentPosition + forward];
                uint8_t byte2 = compressedData[currentPosition + forward + 1];

                if ((byte1 & 0xF0) == 0x10)
                {
                    uint8_t byte3 = compressedData[currentPosition + forward + 2];
                    uint8_t byte4 = compressedData[currentPosition + forward + 3];

                    amountToCopy = 0x111 + ((byte1 & 0xF) << 12) + (byte2 << 4) + (byte3 >> 4);
                    copyFrom = 1 + ((byte3 & 0xF) << 8) + byte4;
                    forward += 4;
                }
                else if ((byte1 & 0xF0) == 0x00)
                {
                    uint8_t byte3 = compressedData[currentPosition + forward + 2];

                    amountToCopy = 0x11 + ((byte1 & 0xF) << 4) + (byte2 >> 4);
                    copyFrom = 1 + ((byte2 & 0xF) << 8) + byte3;
                    forward += 3;
                }
                else
                {
                    amountToCopy = 0x1 + (byte1 >> 4);
                    copyFrom = 1 + ((byte1 & 0xF) << 8) + byte2;
                    forward += 2;
                }

                int copyPosition = processedBytes - copyFrom;
                for (int u = 0; u < amountToCopy; u++)
                {
                    if ((copyPosition + (u % copyFrom)) < processedBytes)
                    {
                        *outPtr = outPtrStart[copyPosition + (u % copyFrom)];
                        outPtr++;
                        processedBytes++;
                    }
                    else
                    {
                        return;
                    }
                }
            }
            else
            {
                *outPtr = compressedData[currentPosition + forward];
                outPtr++;

                processedBytes++;
                forward++;
            }
        }

        currentPosition += forward;
    }

    while (processedBytes < outputSize)
    {
        *outPtr = 0;
        outPtr++;

        processedBytes++;
    }
}

void LZSSFile::saveToDisk(const std::string& outPath)
{
    utils::saveBinaryFile(m_outputBuffer, outPath);
}

void LZSSFile::compress(ECompressType type)
{
    switch (type)
    {
    case ECompressType::LZSS:
        compressLzss();
        break;
    case ECompressType::ONZ:
        compressOnz();
        break;
    default:
        assert(false);
    }
}

void LZSSFile::compressLzss()
{
    assert(m_inputSize > 0 && m_inputPtr != nullptr);

    const int LzssBufferSize = 18;
    const uint8_t LzssCompressionType = 0x10;

    compress_impl(LzssBufferSize, LzssCompressionType);
}

void LZSSFile::compressOnz()
{
    assert(m_inputSize > 0 && m_inputPtr != nullptr);

    const int OnzBufferSize = 16;
    const uint8_t OnzCompressionType = 0x11;

    compress_impl(OnzBufferSize, OnzCompressionType);
}

std::pair<int, int> LZSSFile::search(const std::vector<uint8_t>& slidingWindow, const std::deque<uint8_t>& readAheadBuffer, int distance)
{
    int slidingWindowSize = slidingWindow.size();
    int readAheadBufferSize = readAheadBuffer.size();

    if (readAheadBufferSize == 0)
    {
        return std::make_pair<int, int>(0, -1);
    }

    std::vector<int> offsets;

    for (int i = 0; i < slidingWindowSize - distance; i++)
    {
        if (slidingWindow[i] == readAheadBuffer[0])
        {
            offsets.push_back(i);
        }
    }

    if (offsets.size() == 0)
    {
        return std::make_pair<int, int>(0, 0);
    }

    for (int i = 1; i < readAheadBufferSize; i++)
    {
        for (auto it = offsets.begin(); it != offsets.end(); )
        {
            auto offset = *it;
            if ((slidingWindow[offset + (i % (slidingWindowSize - offset))] != readAheadBuffer[i])
                && offsets.size() > 1)
            {
                it = offsets.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (offsets.size() < 2)
        {
            i = readAheadBufferSize;
        }
    }

    int size = 1;
    bool keepGoing = true;
    while ((readAheadBufferSize > size) && keepGoing)
    {
        if (slidingWindow[offsets[0] + (size % (slidingWindowSize - offsets[0]))] == readAheadBuffer[size])
        {
            size++;
        }
        else
        {
            keepGoing = false;
        }
    }

    return std::make_pair<int, int>(slidingWindowSize - offsets[0], (int)size);
}

void LZSSFile::compress_impl(int readAheadBufferSize, uint8_t compressionType)
{
    const int BlockSize = 8;
    const int SlidingWindowSize = 4096;

    int distance = 1;

    const uint8_t* uncompressedData = m_inputPtr;
    const int inputSize = m_inputSize;

    std::deque<uint8_t> readAheadBuffer;

    std::vector<uint8_t> slidingWindow;

    m_outputBuffer.clear();

    int position = 0;

    // Header
    m_outputBuffer.push_back(compressionType);
    m_outputBuffer.push_back(inputSize & 0xff);
    m_outputBuffer.push_back((inputSize >> 8) & 0xff);
    m_outputBuffer.push_back((inputSize >> 16) & 0xff);

    while (position < readAheadBufferSize)
    {
        readAheadBuffer.push_back(uncompressedData[position]);
        position++;
    }

    bool isCompressed[BlockSize];

    while (readAheadBuffer.size() > 0)
    {
        std::vector<uint8_t> data;

        for (int i = BlockSize - 1; i >= 0; i--)
        {
            std::pair<int, int> dataSeed = search(slidingWindow, readAheadBuffer, distance);

            if (dataSeed.second > 2)
            {
                isCompressed[i] = true;
                uint8_t byte0 = (uint8_t)(((dataSeed.second - (readAheadBufferSize - 0xF)) & 0xF) << 4);
                byte0 += (uint8_t)(((dataSeed.first - distance) >> 8) & 0xF);
                uint8_t byte1 = (uint8_t)((dataSeed.first - distance) & 0xFF);
                data.push_back(byte0);
                data.push_back(byte1);
            }
            else if (dataSeed.second >= 0)
            {
                dataSeed.second = 1;
                isCompressed[i] = false;
                data.push_back(readAheadBuffer.front());
            }
            else
            {
                isCompressed[i] = false;
            }

            for (int u = 0; u < dataSeed.second; u++)
            {
                if (slidingWindow.size() >= SlidingWindowSize)
                {
                    slidingWindow.erase(slidingWindow.begin());
                }

                slidingWindow.push_back(readAheadBuffer.front());
                readAheadBuffer.erase(readAheadBuffer.begin());
            }

            while ((readAheadBuffer.size() < readAheadBufferSize) && (position < inputSize))
            {
                readAheadBuffer.push_back(uncompressedData[position]);
                position++;
            }
        }

        uint8_t blockData = 0;
        for (int i = 0; i < BlockSize; i++)
        {
            if (isCompressed[i])
            {
                blockData += (uint8_t)(1 << i);
            }
        }

        m_outputBuffer.push_back(blockData);
        for (auto& byte : data)
        {
            m_outputBuffer.push_back(byte);
        }
    }
}

} // namespace ndsloc
