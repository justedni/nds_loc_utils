#include "utils.h"

#include <fstream>
#include <iostream>

#include "p2.h"
#include "cakp.h"

namespace utils {

std::vector<uint8_t> readBinaryFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        std::cout << "Could not open file: " << filename << std::endl;
        return {};
    }

    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void saveBinaryFile(const std::vector<uint8_t>& data, const std::string& filename)
{
    if (data.size() > 0)
    {
        std::ofstream os(filename, std::ofstream::binary);
        os.write((char*)data.data(), data.size());
    }
}

ndsloc::EFileFormat getFileFormat(const void* data, std::size_t size)
{
    static constexpr char kDB[] = { 0x64, 0, 0, 0 };
    static constexpr char kDB_Short[] = { 0x08, 0, 0, 0 };
    static constexpr char kDB_Long[] = { 0x78, 0, 0, 0 };

    using namespace ndsloc;

    if (!data || size == 0)
        return EFileFormat::Empty;

    if (data && size >= 4)
    {
        uint8_t firstChar = reinterpret_cast<const uint8_t*>(data)[0];

        if (p2::isP2File((const uint8_t*)data))
        {
            return EFileFormat::P2;
        }
        else if (cakp::isCAKP((const uint8_t*)data))
        {
            return EFileFormat::CAKP;
        }
        else if (std::memcmp(data, kDB, sizeof(kDB)) == 0)
        {
            return EFileFormat::StringDB;
        }
        else if (std::memcmp(data, kDB_Short, sizeof(kDB_Short)) == 0)
        {
            return EFileFormat::StringDB_Short;
        }
        else if (std::memcmp(data, kDB_Long, sizeof(kDB_Long)) == 0
            || firstChar == 0x68)
        {
            return EFileFormat::StringDB_Long;
        }
        else
        {
            if (firstChar == 0xE6 || firstChar == 0xEC
                || firstChar == 0x2A || firstChar == 0x30)
            {
                return EFileFormat::TODO;
            }
        }
    }

    return EFileFormat::Unknown;
}

std::string getExtName(ndsloc::EFileFormat type)
{
    using namespace ndsloc;
    switch (type)
    {
    case EFileFormat::Empty: return "Empty";
    case EFileFormat::P2: return "P2";
    case EFileFormat::CAKP: return "CAKP";
    case EFileFormat::StringDB:
    case EFileFormat::StringDB_Short:
    case EFileFormat::StringDB_Long:
        return "StringDB";
    case EFileFormat::TODO:
        return "TODO";
    default:
        return "";
    }
}

void replace_in_string(std::string& str, const std::string& replace, const std::string& with)
{
    size_t index = 0;
    while (true)
    {
        index = str.find(replace, index);
        if (index == std::string::npos)
            break;

        str.replace(index, replace.size(), with);

        index += with.size();
    }
}

void replace_in_ustring(std::u16string& str, const std::u16string& replace, const std::u16string& with)
{
    size_t index = 0;
    while (true)
    {
        index = str.find(replace, index);
        if (index == std::u16string::npos)
            break;

        str.replace(index, replace.size(), with);

        index += with.size();
    }
}

uint16_t readUInt16(const uint8_t* dataPtr, uint32_t pos)
{
    return dataPtr[pos] + (dataPtr[pos + 1] << 8);
}

uint32_t readUInt24(const uint8_t* dataPtr, uint32_t pos)
{
    return dataPtr[pos] + (dataPtr[pos + 1] << 8) + (dataPtr[pos + 2] << 16);
}

uint32_t readUInt32(const uint8_t* dataPtr, uint32_t pos)
{
    return dataPtr[pos] + (dataPtr[pos + 1] << 8) + (dataPtr[pos + 2] << 16) + (dataPtr[pos + 3] << 24);
}

} // namespace Utils
