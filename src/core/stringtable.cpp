#include "stringtable.h"

#include "utils.h"
#include "types.h"

namespace ndsloc {

StringTableFile::StringTableFile(const uint8_t* inputPtr, uint32_t inputSize)
    : m_buffer(inputPtr)
    , m_bufferSize(inputSize)
{
}

uint32_t StringTableFile::count() const
{
    return utils::readUInt32(m_buffer, 0);
}

uint32_t StringTableFile::dataAt() const
{
    return 4 + count() * 4;
}

bool StringTableFile::looksValid(const uint8_t* inputPtr, uint32_t inputSize)
{
    if (!inputPtr || inputSize < 8)
        return false;

    const uint32_t n = utils::readUInt32(inputPtr, 0);

    if (n == 0 || (n & 1) != 0)
        return false;

    if (n > (inputSize - 4) / 4)
        return false;

    const uint32_t dataAt = 4 + n * 4;
    if (dataAt >= inputSize)
        return false;

    uint32_t previous = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        const uint32_t end = utils::readUInt32(inputPtr, 4 + i * 4);

        if (end < previous || (end & 1) != 0)
            return false;

        if (end > inputSize - dataAt)
            return false;

        previous = end;
    }

    return dataAt + previous == inputSize;
}

bool StringTableFile::extractStrings(std::vector<U16String>& out) const
{
    out.clear();

    if (!looksValid(m_buffer, m_bufferSize))
        return false;

    const uint32_t n = count();
    const uint32_t base = dataAt();

    out.reserve(n);

    uint32_t start = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        const uint32_t end = utils::readUInt32(m_buffer, 4 + i * 4);

        std::u16string text;
        text.reserve((end - start) / 2);

        for (uint32_t at = start; at + 1 < end; at += 2)
        {
            const char16_t unit = static_cast<char16_t>(utils::readUInt16(m_buffer, base + at));

            if (unit == 0)
                break; // NULL terminator

            text.push_back(unit);
        }

        out.emplace_back(base + start, std::move(text));
        start = end;
    }

    return true;
}

std::string utf16ToUtf8(const std::u16string& in)
{
    std::string out;
    out.reserve(in.size());

    for (size_t i = 0; i < in.size(); ++i)
    {
        uint32_t cp = in[i];

        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < in.size())
        {
            const uint32_t low = in[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF)
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }

        if (cp < 0x80)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    return out;
}

} // namespace ndsloc
