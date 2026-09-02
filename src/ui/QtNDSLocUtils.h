#pragma once

#include <QtWidgets/QMainWindow>
#include <QStringList>
#include <QThread>

#include "ui_QtNDSLocUtils.h"

#include "FileEntry.h"
#include "ExportResult.h"

#include <string>

class QTreeWidgetItem;
class Worker;

class QtNDSLocUtils : public QMainWindow
{
    Q_OBJECT

public:
    QtNDSLocUtils(QWidget* parent = nullptr);
    ~QtNDSLocUtils();

    void loadRom(QString romPath);
    void setTargetFolder(QString targetPath);

signals:
    void requestLoadRom(const QString& romPath);
    void requestExtractP2Files(const QString& outFolder, const QStringList& files);
    void requestExtractZFiles(const QString& outFolder, const QStringList& files);
    void requestExtractRawFiles(const QString& outFolder, const QStringList& files);
    void requestExportStrings(const QString& outFolder, const QStringList& files, uint8_t format, uint8_t language);

private slots:
    void on_browseRom_clicked();
    void on_browseTargetFolder_clicked();
    void on_buttonExportStringsToCsv_clicked();
    void on_buttonExportStringsToIni_clicked();
    void on_buttonSelectAll_clicked();
    void on_buttonSelectNone_clicked();

private:
    struct TreeContext
    {
        QTreeWidgetItem* clicked = nullptr;
        QList<QTreeWidgetItem*> items;
        QStringList filePaths;

        bool hasFiles = false;
        bool hasFolders = false;
        QString commonType;
    };

    void appendLog(const std::string& text);
    void appendLogLine(const QString& text);
    void onRomLoaded(bool success);
    void onFilesystemListed(const NdsFileEntryList& entries);
    void onTaskFinished(bool success);
    void onExportFinished(const NdsExportResult& result);

    void clearFileTree();
    void populateFileTree(const NdsFileEntryList& entries);
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void setCheckStateRecursive(QTreeWidgetItem* item, Qt::CheckState state);
    void refreshParentCheckState(QTreeWidgetItem* item);
    void recomputeFolderStates(QTreeWidgetItem* item);
    void setAllChecked(Qt::CheckState state);
    void setItemsChecked(const QList<QTreeWidgetItem*>& items, Qt::CheckState state);
    void collectCheckedFiles(const QTreeWidgetItem* item, QStringList& out) const;
    void collectFilePaths(const QTreeWidgetItem* item, QStringList& out) const;
    QStringList selectedFiles() const;
    void updateSelectionInfo();

    void applyDefaultSelection();
    QList<QTreeWidgetItem*> matchPattern(const QString& pattern) const;
    void expandAncestors(QTreeWidgetItem* item);

    void onTreeContextMenu(const QPoint& pos);
    TreeContext buildTreeContext(QTreeWidgetItem* clicked) const;
    void addFolderActions(QMenu& menu, const TreeContext& ctx);
    void addFileActions(QMenu& menu, const TreeContext& ctx);
    void addCommonActions(QMenu& menu, const TreeContext& ctx);

    enum EExtractType : uint8_t { Raw = 0, P2, Z };
    void actionExtractFile(const TreeContext& ctx, EExtractType type);

    int8_t selectedLanguage() const;

    void beginTask();
    void updateUiState();

    Ui::QtNDSLocUtilsClass ui;

    QThread m_workerThread;
    Worker* m_worker = nullptr;

    bool m_bIsBusy = false;
    bool m_bRomLoaded = false;
    bool m_updatingTree = false;
    int m_selectedCount = 0;

    QString m_lastExtractFolder;
    QHash<QString, QTreeWidgetItem*> m_fileItems;
};
