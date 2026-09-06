#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace NDS {
struct NDSEntry;
class NDSFileSystem;
}

namespace ndsloc {

namespace strings {
struct CsvNdsFile;
struct CsvLine;
}

namespace patcher {

void compareCsvs(const std::vector<strings::CsvNdsFile>& refFiles, std::vector<strings::CsvNdsFile>& modFiles);
void createPatch(const std::string& romPath, const std::vector<strings::CsvNdsFile>& modFiles);
void createPatch(uint8_t* rom, uint32_t romSize, const std::vector<strings::CsvNdsFile>& modFiles);

uint32_t patchCompressedSection(uint8_t* inputPtr, uint32_t inputSize, uint32_t maxSize, const std::vector<strings::CsvLine>& lines, bool bPadFF);

} // namespace patcher
} // namespace ndsloc
