#pragma once

#include <vector>
#include <string>

#include "types.h"

namespace ndsloc {
enum EFileFormat : uint8_t;

namespace strings {

std::vector<String> exportStringsFromBuffer(const uint8_t* rom, int totalSize, uint32_t sectionOffset, const std::string& sectionName, int addressStart, bool bRemoveForb = true);
std::vector<U16String> exportDBStrings(const uint8_t* dataPtr, int inputSize, EFileFormat type);

std::ofstream startFile(const std::string& out_path, ExportFormat format);
void writeStrings(ExportFormat format, std::ofstream& os, uint32_t fileOffset, const std::string& filename, const std::vector<String>& strings);
void writeStrings(ExportFormat format, std::ofstream& os, uint32_t fileOffset, const std::string& filename, const std::vector<U16String>& strings);

} // namespace strings
} // namespace ndsloc
