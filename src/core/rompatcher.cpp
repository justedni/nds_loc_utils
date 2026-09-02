#include "rompatcher.h"

#include "strings.h"
#include "nds.h"
#include "utils.h"
#include "p2.h"
#include "cakp.h"

#include <assert.h>

namespace ndsloc {
namespace patcher {

bool compareLines(const std::vector<strings::CsvLine> refLines, std::vector<strings::CsvLine>& modLines)
{
    bool bNeedUpdate = false;

    for (int i = 0; i < modLines.size(); i++)
    {
        auto& modLine = modLines[i];

        auto found = std::find_if(refLines.begin(), refLines.end(), [&](const auto& e) { return e.offset == modLine.offset; });
        if (found == refLines.end())
        {
            assert(false);
            continue;
        }

        const strings::CsvLine* foundRefLine = &(*found);
        if (foundRefLine->text != modLine.text)
        {
            modLine.bNeedUpdate = true;
            bNeedUpdate = true;
        }
    }

    return bNeedUpdate;
}

void compareCsvs(const std::vector<strings::CsvNdsFile>& refFiles, std::vector<strings::CsvNdsFile>& modFiles)
{
    for (auto& modFile : modFiles)
    {
        auto foundRefFile = std::find_if(refFiles.begin(), refFiles.end(), [&](const auto& e) { return e.filename == modFile.filename; });
        if (foundRefFile != refFiles.end())
        {
            bool bNeedUpdate = compareLines(foundRefFile->lines, modFile.lines);
            if (bNeedUpdate)
                modFile.bNeedUpdate = true;

            const auto& refSubfiles = foundRefFile->subfiles;
            for (auto& modSubfile : modFile.subfiles)
            {
                auto foundRefSubFile = std::find_if(refSubfiles.begin(), refSubfiles.end(), [&](const auto& e) { return e.filename == modSubfile.filename; });
                if (foundRefSubFile != refSubfiles.end())
                {
                    bool bDirty = compareLines(foundRefSubFile->lines, modSubfile.lines);
                    if (bDirty)
                    {
                        modSubfile.bNeedUpdate = true;
                        modFile.bNeedUpdate = true;
                    }
                }
            }
        }
    }
}

void createPatch(const std::string& romPath, const std::vector<strings::CsvNdsFile>& modFiles)
{
    auto ndsRom = utils::readBinaryFile(romPath);

    auto ndsfs = NDS::NDSFileSystem(ndsRom.data(), ndsRom.size());
    ndsfs.getRomFileSystem(true);

    for (auto& modFile : modFiles)
    {
        if (!modFile.bNeedUpdate)
            continue;

        auto* entry = ndsfs.findEntryByName(modFile.filename);
        assert(entry);
        if (!entry)
            continue;

        auto* data = ndsRom.data() + entry->start;

        if (entry->type == "p2")
        {
            auto p2file = ndsloc::P2File(data, entry->size);
            p2file.applyChanges(modFile);
        }
        else if (entry->type == "z")
        {
            assert(false); // TODO
        }
    }

    utils::saveBinaryFile(ndsRom, romPath + "_mod");
}

void patchLine(uint8_t* data, unsigned int addr, const char* targetLine)
{
    int addrGap = 0;
    bool ended = false;

    for (int i = 0; i < 1023; i++)
    {
        if (data[addr + i] == 0x00) {
            break;
        }

        if (targetLine[i + addrGap] == '\\' && targetLine[i + addrGap + 1] == 'n') {
            data[addr + i] = 0x0A;
            addrGap++;
            continue;
        }

        if (targetLine[i + addrGap] == 0 || targetLine[i + addrGap] == '\0') {
            ended = true;
        }

        if (ended)
            data[addr + i] = 0x20;
        else
            data[addr + i] = targetLine[i + addrGap];
    }
}

} // namespace patcher
} // namespace ndsloc
