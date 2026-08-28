#include "Worker.h"

#include "QtNDSLocUtils.h"

#include "core/nds.h"
#include "core/utils.h"

Worker::Worker(QObject* parent)
    : QObject(parent)
{
}

Worker::~Worker()
{
}

void Worker::clearRom()
{
    m_fs.reset();
    m_romData.clear();
    m_romPath.clear();
}

void Worker::loadRom(const std::string& romPath)
{
    clearRom();

    emit log("Reading file: " + romPath + "\n");

    auto romData = utils::readBinaryFile(romPath);
    if (romData.empty())
    {
        emit log("Failed to read file\n");
        emit romLoaded(false);
        return;
    }

    m_romData = std::move(romData);
    m_romPath = romPath;

    emit log("Parsing filesystem...\n");
    m_fs = std::make_unique<NDS::NDSFileSystem>(m_romData.data(), m_romData.size());
    m_fs->getRomFileSystem();

    emit log("Loaded " + std::to_string(m_romData.size()) + " bytes.\n");
    emit romLoaded(true);
}

void Worker::printFilesystem()
{
    if (!m_fs)
    {
        emit log("No ROM loaded.\n");
        emit taskFinished(false);
        return;
    }

    std::string strings;
    for (const auto& entry : m_fs->getAllEntries())
    {
        strings += "  File: " + entry.filename + "\n";
    }

    emit log(strings);
    emit taskFinished(true);
}

void Worker::exportStrings(const std::string& outFolder)
{
    if (!m_fs)
    {
        emit log("No ROM loaded.\n");
        emit taskFinished(false);
        return;
    }

    emit log("Exporting strings to: " + outFolder + "\n");

    // TODO: write output string data

    emit taskFinished(true);
}

void Worker::logCallback(const std::string& str)
{
    emit log(str);
}
