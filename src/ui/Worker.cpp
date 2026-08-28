#include "Worker.h"

#include "QtNDSLocUtils.h"

#include "core/nds.h"
#include "core/utils.h"
#include "core/p2.h"
#include "core/strings.h"

#include <QFileInfo>
#include <QDir>

#include <fstream>

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

void Worker::loadRom(const QString& romPath)
{
    clearRom();

    const std::string path = romPath.toStdString();

    emit log("Reading file: " + path + "\n");

    auto romData = utils::readBinaryFile(path);
    if (romData.empty())
    {
        emit log("Failed to read file\n");
        emit romLoaded(false);
        return;
    }

    m_romData = std::move(romData);
    m_romPath = path;

    emit log("Parsing filesystem...\n");
    m_fs = std::make_unique<NDS::NDSFileSystem>(m_romData.data(), m_romData.size());
    m_fs->getRomFileSystem(true);

    const NdsFileEntryList entries = buildEntryList();

    emit log("Loaded " + std::to_string(m_romData.size()) + " bytes, " + std::to_string(entries.size()) + " files.\n");

    emit filesystemListed(entries);
    emit romLoaded(true);
}

NdsFileEntryList Worker::buildEntryList() const
{
    NdsFileEntryList list;
    if (!m_fs)
        return list;

    const auto& entries = m_fs->getAllEntries();
    list.reserve(static_cast<int>(entries.size()));

    for (const auto& entry : entries)
    {
        NdsFileEntry e;
        e.path = QString::fromStdString(entry.filename);
        e.type = QString::fromStdString(entry.type);
        e.size = static_cast<quint64>(entry.size);

        list.push_back(e);
    }

    return list;
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

void Worker::exportStrings(const QString& outFolder, const QStringList& files)
{
    if (!m_fs)
    {
        emit log("No ROM loaded.\n");
        emit taskFinished(false);
        return;
    }

    if (files.isEmpty())
    {
        emit log("No files selected.\n");
        emit taskFinished(false);
        return;
    }

    emit log("Exporting strings to: " + outFolder.toStdString() + "\n");

    auto outPath = QDir(outFolder).filePath("strings.csv");

    std::ofstream os = ndsloc::strings::startCsvFile(outPath.toStdString());

    for (const QString& file : files)
    {
        const std::string filePath = file.toStdString();

        auto* entry = m_fs->findEntryByName(filePath);
        if (entry)
        {
            if (entry->type == "P2")
            {
                auto* data = m_romData.data() + entry->start;
                auto p2file = ndsloc::P2Archive(data, entry->size);

                auto fileName = QFileInfo(QString::fromStdString(filePath)).baseName();
                p2file.exportAllCAKPStringsToCsv(os, fileName.toStdString());
            }
            else
            {
                // Not implemented
            }
        }
    }

    emit taskFinished(true);
}

void Worker::logCallback(const std::string& str)
{
    emit log(str);
}
