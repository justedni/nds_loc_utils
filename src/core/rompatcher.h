#pragma once

#include <vector>
#include <string>

namespace ndsloc {

namespace strings {
struct CsvNdsFile;
}

namespace patcher {

static void compareCsvs(const std::vector<strings::CsvNdsFile>& refFiles, std::vector<strings::CsvNdsFile>& modFiles);
static void createPatch(const std::string& romPath, const std::vector<strings::CsvNdsFile>& modFiles);

static void patchLine(uint8_t* data, unsigned int addr, const char* targetLine);
}

} // namespace ndsloc
