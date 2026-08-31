#pragma once

#include <vector>
#include <string>

#include "types.h"

namespace ndsloc {
enum EFileFormat : uint8_t;

namespace strings {

std::vector<String> exportStringsFromBuffer(const uint8_t* rom, int totalSize, uint32_t sectionOffset, const std::string& sectionName, int addressStart, bool bRemoveForb = true);
std::vector<U16String> exportDBStrings(const uint8_t* dataPtr, int inputSize, EFileFormat type);
void writeString(std::ofstream& os, const String& pair);

std::ofstream startCsvFile(const std::string& out_path);
void writeStringsToCsv(std::ofstream& os, const std::string& filename, const std::vector<String>& strings);
void writeStringsToCsv(std::ofstream& os, const std::string& filename, const std::vector<U16String>& strings);
void writeCsvLine(std::ofstream& os, const std::string& filename, const std::string& subfilename, int offset, int subfileOffset, const std::string& text);
void writeCsvLine(std::ofstream& os, const std::string& filename, const std::string& subfilename, int offset, int subfileOffset, const std::u16string& text);

} // namespace strings
} // namespace ndsloc
