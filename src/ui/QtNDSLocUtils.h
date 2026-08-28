#pragma once

#include <QtWidgets/QMainWindow>
#include <QThread>
#include "ui_QtNDSLocUtils.h"

#include <string>

class QCheckBox;
class QComboBox;
class QLabel;

class Worker;

class QtNDSLocUtils : public QMainWindow
{
    Q_OBJECT

public:
    QtNDSLocUtils(QWidget* parent = nullptr);
    ~QtNDSLocUtils();

signals:
    void requestLoadRom(const std::string& romPath);
    void requestPrintFilesystem();
    void requestExportStrings(const std::string& outFolder);

private slots:
    void on_browseRom_clicked();
    void on_browseTargetFolder_clicked();
    void on_buttonPrintFilesystem_clicked();
    void on_buttonExportStrings_clicked();

private:
    void appendLog(const std::string& text);
    void onRomLoaded(bool success);
    void onTaskFinished(bool success);

    void beginTask();
    void updateUiState();

    Ui::QtNDSLocUtilsClass ui;

    QThread m_workerThread;
    Worker* m_worker = nullptr;

    bool m_bIsBusy = false;
    bool m_bRomLoaded = false;
};
