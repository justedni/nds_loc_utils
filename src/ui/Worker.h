#pragma once

#include <QObject>

namespace NDS {
class NDSFileSystem;
}

class Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject* parent = nullptr);
    ~Worker() override;

    void logCallback(const std::string& str);

public slots:
    void loadRom(const std::string& romPath);
    void printFilesystem();
    void exportStrings(const std::string& outFolder);

signals:
    void log(const std::string& str);
    void romLoaded(bool success);
    void taskFinished(bool success);

private:
    void clearRom();

    std::unique_ptr<NDS::NDSFileSystem> m_fs;

    std::string m_romPath;
    std::vector<uint8_t> m_romData;
};
