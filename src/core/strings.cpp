#include "strings.h"

#include "utils.h"

#include <algorithm>
#include <iomanip>
#include <fstream>

namespace ndsloc {
namespace strings {

void writeString(std::ofstream& os, std::pair<int, std::string>& pair)
{
    os << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << pair.first << "=";
    os << pair.second << "\n";
}

StringList exportStringsFromBuffer(const uint8_t* rom, int totalSize, int addressStart, bool bRemoveForb)
{
    StringList ret;

    static std::vector<std::string> completeStringsToIgnore = { "CAKP", "chara", "stmi", "shop", "score", "tex0", "tex1", "tex2", "beast", "bel0", "bel1", "bell", "info", "DELETED", "shift",
        "akubi", "point", "tgt1a", "tgt2a", "tgt3a", "tobig", "tosmall", "warp", "wall02", "wall03", "tou0", "tou1", "BOUT", "Light01", "Box12", "Plane01", "Plane02", "door", "BM01", "031A",
        "5A:0", "5C:0", "2B:0", "4B:0", "Co:3", "AnchorPos0", "AnchorPos1", "AnchorPos2", "AnchorPos3", "shutter1", "default.p2", "dummy3", "Bip01"
    };

    int firstAddr = 0;
    int lastAddr = 0;
    bool validCharFound = false;
    bool forbCharFound = false;
    for (int addr = 0; addr < totalSize; addr++) {
        bool usual = rom[addr] >= 0x41 && rom[addr] <= 0x7E;
        bool accents = rom[addr] == 0xC2 || rom[addr] == 0xC3 || (rom[addr] >= 0x80 && rom[addr] <= 0xBF);
        bool quotes = rom[addr] == 0xE2 || rom[addr] == 0x80 || rom[addr] == 0x9C || rom[addr] == 0x9D;
        bool unusual = (rom[addr] >= 0x20 && rom[addr] <= 0x40) || accents || quotes || rom[addr] == 0x0A;
        bool forb = rom[addr] == 0x93 || rom[addr] == 0x5F || rom[addr] == 0x2F;
        if (usual || unusual) {
            if (firstAddr == 0) {
                firstAddr = addr;
                lastAddr = addr;
            }
            else {
                lastAddr = addr;
            }
        }
        if (usual) {
            validCharFound = true;
        }
        if (forb && bRemoveForb) {
            forbCharFound = true;
        }
        if (!usual && !unusual) {
            if (firstAddr != 0) {
                if (!forbCharFound && validCharFound && lastAddr - firstAddr > 2) {
                    std::string subtitle;
                    for (int pAddr = firstAddr; pAddr <= lastAddr; pAddr++) {
                        if ((char)rom[pAddr] == 0x0A) {
                            subtitle.append("\\n");
                        }
                        else {
                            subtitle += (char)rom[pAddr];
                        }
                    }
                    if (subtitle.rfind("0:", 0) != 0
                        && subtitle.rfind("1A:", 0) != 0
                        && subtitle.rfind("5B:", 0) != 0
                        && subtitle.rfind("Co:0", 0) != 0
                        && subtitle.rfind("3F", 0) != 0
                        )
                    {
                        bool bIgnore = false;
                        for (auto& ign : completeStringsToIgnore)
                        {
                            if (subtitle == ign)
                            {
                                bIgnore = true;
                                break;
                            }
                        }

                        if (!bIgnore)
                        {
                            int actualAddress = addressStart + firstAddr;
                            ret.push_back({ actualAddress, subtitle });
                        }
                    }
                }

                firstAddr = 0;
                lastAddr = 0;
                validCharFound = false;
                forbCharFound = false;
            }
        }
    }

    return ret;
}

static std::string csvEscape(const std::string& in)
{
    if (in.find_first_of(",\"\r\n") == std::string::npos)
        return in;

    std::string out;
    out.reserve(in.size() + 2);
    out.push_back('"');
    for (char c : in) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::ofstream startCsvFile(const std::string& out_path)
{
    std::ofstream os(out_path);
    os << "file,subfile,offset,text\n";
    return std::move(os);
}

void writeCsvLine(std::ofstream& os, const std::string& filename, const std::string& subfilename, const std::pair<int, std::string>& pair)
{
    os << filename;
    os << ',' << subfilename;
    os << ",0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << pair.first;
    os << ',' << csvEscape(pair.second);
    os << '\n';
}

} // namespace strings
} // namespace ndsloc
