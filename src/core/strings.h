#pragma once

#include <vector>
#include <string>

namespace ndsloc {
namespace strings {

typedef std::vector<std::pair<int, std::string>> StringList;

StringList exportStringsFromBuffer(const uint8_t* rom, int totalSize, int addressStart, bool bRemoveForb = true);
void writeString(std::ofstream& os, std::pair<int, std::string>& pair);

} // namespace strings
} // namespace ndsloc
