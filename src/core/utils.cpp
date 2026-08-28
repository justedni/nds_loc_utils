#include "utils.h"

#include <fstream>
#include <iostream>

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

} // namespace Utils
