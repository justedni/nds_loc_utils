#pragma once

#include <QObject>

#include "FileEntry.h"

namespace NDS {
class NDSFileSystem;
}

class Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject* parent = nullptr);
    ~Worker() override;

public slots:
    void loadRom(const QString& romPath);
    void printFilesystem();
    void extractP2Files(const QString& outFolder, const QStringList& files);
    void exportStrings(const QString& outFolder, const QStringList& files, uint8_t format);

signals:
    void log(const std::string& str);
    void romLoaded(bool success);
    void filesystemListed(const NdsFileEntryList& entries);
    void taskFinished(bool success);

private:
    void logCallback(const std::string& str);

    void clearRom();
    NdsFileEntryList buildEntryList() const;

    std::unique_ptr<NDS::NDSFileSystem> m_fs;

    std::string m_romPath;
    std::vector<uint8_t> m_romData;
};
