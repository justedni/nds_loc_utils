#pragma once

#include <vector>
#include <string>

#include "types.h"

namespace ndsloc {
enum EFileFormat : uint8_t;

namespace strings {

std::ofstream startFile(const std::string& outPathNoExt, ExportFormat format);
void writeStrings(ExportFormat format, std::ofstream& os, uint32_t fileOffset, const std::string& filename, const std::vector<String>& strings);
void writeStrings(ExportFormat format, std::ofstream& os, uint32_t fileOffset, const std::string& filename, const std::vector<U16String>& strings);

} // namespace strings
} // namespace ndsloc
