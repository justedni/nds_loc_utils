#pragma once

#include <stdint.h>
#include <vector>
#include <string>

namespace ndsloc {

struct U16String;

class StringTableFile
{
public:
    StringTableFile(const uint8_t* inputPtr, uint32_t inputSize);

    static bool looksValid(const uint8_t* inputPtr, uint32_t inputSize);

    bool extractStrings(std::vector<U16String>& out) const;

private:
    uint32_t count() const;
    uint32_t dataAt() const;

    const uint8_t* m_buffer = nullptr;
    uint32_t m_bufferSize = 0;
};

std::string utf16ToUtf8(const std::u16string& in);

} // namespace ndsloc
