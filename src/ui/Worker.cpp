#include "Worker.h"

#include "QtNDSLocUtils.h"

#include "core/nds.h"
#include "core/utils.h"
#include "core/p2.h"
#include "core/cakp.h"
#include "core/strings.h"
#include "core/stringtable.h"
#include "core/lzss.h"

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

void Worker::extractP2Files(const QString& outFolder, const QStringList& files)
{
    for (const QString& file : files)
    {
        const std::string filePath = file.toStdString();

        auto* entry = m_fs->findEntryByName(filePath);
        if (entry)
        {
            auto* data = m_romData.data() + entry->start;

            if (entry->type == "p2")
            {
                auto p2file = ndsloc::P2File(data, entry->size);

                auto& subfiles = p2file.getFileTable();
                for (auto& subfile : subfiles)
                {
                    ndsloc::LZSSFile file(subfile.inputPtr, subfile.fileSize);
                    file.decompress();

                    auto outPath = QDir(outFolder).filePath(QString::fromStdString(subfile.getFilename()));
                    file.saveToDisk(outPath.toStdString());
                }
            }
            else
            {
                auto fileInfo = QFileInfo(QString::fromStdString(filePath));
                auto outPath = QDir(outFolder).filePath(fileInfo.fileName());
                
                auto suffix = fileInfo.suffix().toLower();
                if (suffix == "z")
                {
                    ndsloc::LZSSFile file(data, entry->size);
                    file.decompress();
                    file.saveToDisk(outPath.toStdString());
                }
                else
                {
                    std::ofstream os(outPath.toStdString(), std::ofstream::binary);
                    os.write((char*)data, entry->size);
                }
            }
        }
    }

    emit taskFinished(true);
}

void Worker::exportStrings(const QString& outFolder, const QStringList& files)
{
    using namespace ndsloc;

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

    std::ofstream os = strings::startCsvFile(outPath.toStdString());

    for (const QString& file : files)
    {
        const std::string filePath = file.toStdString();

        auto* entry = m_fs->findEntryByName(filePath);
        if (entry)
        {
            auto* data = m_romData.data() + entry->start;
            auto fileInfo = QFileInfo(QString::fromStdString(filePath));
            auto fileName = fileInfo.baseName();

            if (entry->type == "p2")
            {
                P2File file(data, entry->size);
                std::vector<String> out_strings;
                file.extractStrings(out_strings);
                strings::writeStringsToCsv(os, fileName.toStdString(), out_strings);
            }
            else if (entry->type == "z")
            {
                LZSSFile lzss(data, entry->size);
                lzss.decompress();

                const auto decompressed = lzss.getConvertedData();
                auto* data = decompressed.data();
                auto dataSize = static_cast<uint32_t>(decompressed.size());

                if (fileInfo.fileName().endsWith(".s.z")) // Compressed S file
                {
                    auto type = utils::getFileFormat(data, dataSize);
                    assert(type == EFileFormat::StringDB_Short);
                    auto u16strings = strings::exportDBStrings(data, dataSize, EFileFormat::StringDB_Short);
                    strings::writeStringsToCsv(os, fileName.toStdString(), u16strings);
                }
                else if (StringTableFile::looksValid(data, dataSize))
                {
                    std::vector<U16String> wide;
                    StringTableFile table(data, dataSize);
                    const bool ok = table.extractStrings(wide);

                    strings::writeStringsToCsv(os, fileName.toStdString(), wide);
                }
                else
                {
                    auto type = utils::getFileFormat(data, dataSize);
                    if (type == EFileFormat::StringDB || type == EFileFormat::StringDB_Long)
                    {
                        auto u16strings = strings::exportDBStrings(data, dataSize, type);
                        strings::writeStringsToCsv(os, fileName.toStdString(), u16strings);
                    }
                    else
                    {
                        assert(false);

                        //CAKPFile cakp(data, dataSize, 0, "");
                        ////cakp.setLanguage(m_language);
                        //std::vector<String> out_strings;
                        //const bool ok = cakp.extractStrings(out_strings);
                        //assert(ok);
                        //strings::writeStringsToCsv(os, fileName.toStdString(), out_strings);
                    }
                }
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
