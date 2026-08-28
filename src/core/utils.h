#pragma once

#include <vector>
#include <string>

namespace utils {

	std::vector<uint8_t> readBinaryFile(const std::string& filename);
	void saveBinaryFile(const std::vector<uint8_t>& data, const std::string& filename);

} // namespace Utils
