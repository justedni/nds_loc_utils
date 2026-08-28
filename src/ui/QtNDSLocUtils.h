#pragma once

#include <QtWidgets/QMainWindow>
#include <QStringList>
#include <QThread>

#include "ui_QtNDSLocUtils.h"

#include "FileEntry.h"

#include <string>

class QTreeWidgetItem;
class Worker;

class QtNDSLocUtils : public QMainWindow
{
    Q_OBJECT

public:
    QtNDSLocUtils(QWidget* parent = nullptr);
    ~QtNDSLocUtils();

signals:
    void requestLoadRom(const QString& romPath);
    void requestPrintFilesystem();
    void requestExportStrings(const QString& outFolder, const QStringList& files);

private slots:
    void on_browseRom_clicked();
    void on_browseTargetFolder_clicked();
    void on_buttonPrintFilesystem_clicked();
    void on_buttonExportStrings_clicked();
    void on_buttonSelectAll_clicked();
    void on_buttonSelectNone_clicked();

private:
    void appendLog(const std::string& text);
    void onRomLoaded(bool success);
    void onFilesystemListed(const NdsFileEntryList& entries);
    void onTaskFinished(bool success);

    // File tree
    void clearFileTree();
    void populateFileTree(const NdsFileEntryList& entries);
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void setCheckStateRecursive(QTreeWidgetItem* item, Qt::CheckState state);
    void refreshParentCheckState(QTreeWidgetItem* item);
    void recomputeFolderStates(QTreeWidgetItem* item);
    void setAllChecked(Qt::CheckState state);
    void collectCheckedFiles(const QTreeWidgetItem* item, QStringList& out) const;
    QStringList selectedFiles() const;
    void updateSelectionInfo();

    void applyDefaultSelection();
    QList<QTreeWidgetItem*> matchPattern(const QString& pattern) const;
    void expandAncestors(QTreeWidgetItem* item);

    // UI state
    void beginTask();
    void updateUiState();

    Ui::QtNDSLocUtilsClass ui;

    QThread m_workerThread;
    Worker* m_worker = nullptr;

    bool m_bIsBusy = false;
    bool m_bRomLoaded = false;
    bool m_updatingTree = false;
    int m_selectedCount = 0;

    QHash<QString, QTreeWidgetItem*> m_fileItems;
};
