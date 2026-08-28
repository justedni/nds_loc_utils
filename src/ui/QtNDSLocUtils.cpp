#include "QtNDSLocUtils.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTextCursor>
#include <QTreeWidgetItem>

#include "Worker.h"

namespace
{
    constexpr int ColumnName = 0;
    constexpr int ColumnSize = 1;
    constexpr int ColumnType = 2;

    constexpr int PathRole   = Qt::UserRole + 1;
    constexpr int SizeRole   = Qt::UserRole + 2;
    constexpr int IsFileRole = Qt::UserRole + 3;

    const QStringList kDefaultSelection = {
        QStringLiteral("/mi/mi/10000"),
        //QStringLiteral("*.p2"),
    };

    QString humanSize(quint64 bytes)
    {
        if (bytes < 1024)
            return QString("%1 B").arg(bytes);
        if (bytes < 1024ull * 1024ull)
            return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    }
}

QtNDSLocUtils::QtNDSLocUtils(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    auto font = QFont(ui.textEditLog->font());
    font.setPointSize(8);
    ui.textEditLog->setFont(font);

    ui.splitterMain->setStretchFactor(0, 2);   // tree
    ui.splitterMain->setStretchFactor(1, 3);   // log

    ui.treeFiles->header()->setSectionResizeMode(ColumnName, QHeaderView::Interactive);
    ui.treeFiles->header()->setStretchLastSection(false);
    ui.treeFiles->setColumnWidth(ColumnName, 260);
    ui.treeFiles->setColumnWidth(ColumnSize, 80);

    qRegisterMetaType<NdsFileEntryList>("NdsFileEntryList");

    m_worker = new Worker();
    m_worker->moveToThread(&m_workerThread);

    connect(m_worker, &Worker::log, this, &QtNDSLocUtils::appendLog);
    connect(m_worker, &Worker::romLoaded, this, &QtNDSLocUtils::onRomLoaded);
    connect(m_worker, &Worker::filesystemListed,  this, &QtNDSLocUtils::onFilesystemListed);
    connect(m_worker, &Worker::taskFinished, this, &QtNDSLocUtils::onTaskFinished);

    connect(this, &QtNDSLocUtils::requestLoadRom, m_worker, &Worker::loadRom);
    connect(this, &QtNDSLocUtils::requestPrintFilesystem, m_worker, &Worker::printFilesystem);
    connect(this, &QtNDSLocUtils::requestExportStrings, m_worker, &Worker::exportStrings);

    connect(ui.treeFiles, &QTreeWidget::itemChanged, this, &QtNDSLocUtils::onTreeItemChanged);

    m_workerThread.start();

    ui.lineTargetFolder->setText(QDir::currentPath());

    updateUiState();
}

QtNDSLocUtils::~QtNDSLocUtils()
{
    m_workerThread.quit();
    m_workerThread.wait();

    delete m_worker;
    m_worker = nullptr;
}

void QtNDSLocUtils::updateUiState()
{
    const bool idle = !m_bIsBusy;

    ui.browseRom->setEnabled(idle);
    ui.browseTargetFolder->setEnabled(idle);

    ui.treeFiles->setEnabled(idle);
    ui.buttonSelectAll->setEnabled(idle && m_bRomLoaded);
    ui.buttonSelectNone->setEnabled(idle && m_bRomLoaded && m_selectedCount > 0);

    ui.buttonPrintFilesystem->setEnabled(idle && m_bRomLoaded);
    ui.buttonExportStrings->setEnabled(idle && m_bRomLoaded && m_selectedCount > 0);
}

void QtNDSLocUtils::beginTask()
{
    m_bIsBusy = true;
    updateUiState();
}

void QtNDSLocUtils::on_browseRom_clicked()
{
    QString romPath = QFileDialog::getOpenFileName(this, "Open NDS rom", "", "Nintendo DS Rom file (*.nds)");
    if (romPath.isEmpty())
        return;

    ui.lineRomPath->setText(romPath);

    m_bRomLoaded = false;
    m_selectedCount = 0;

    clearFileTree();

    beginTask();
    emit requestLoadRom(romPath);
}

void QtNDSLocUtils::on_browseTargetFolder_clicked()
{
    QString targetFolder = QFileDialog::getExistingDirectory(this, "Choose target directory");
    if (targetFolder.isEmpty())
        return;

    ui.lineTargetFolder->setText(targetFolder);
    updateUiState();
}

void QtNDSLocUtils::on_buttonPrintFilesystem_clicked()
{
    beginTask();
    emit requestPrintFilesystem();
}

void QtNDSLocUtils::on_buttonExportStrings_clicked()
{
    auto outPath = ui.lineTargetFolder->text();
    if (outPath.isEmpty())
        return;

    const QStringList files = selectedFiles();
    if (files.isEmpty())
        return;

    beginTask();
    emit requestExportStrings(outPath, files);
}

void QtNDSLocUtils::on_buttonSelectAll_clicked()
{
    setAllChecked(Qt::Checked);
}

void QtNDSLocUtils::on_buttonSelectNone_clicked()
{
    setAllChecked(Qt::Unchecked);
}

void QtNDSLocUtils::appendLog(const std::string& text)
{
    ui.textEditLog->moveCursor(QTextCursor::End);
    ui.textEditLog->insertPlainText(QString::fromStdString(text));
    ui.textEditLog->moveCursor(QTextCursor::End);
}

void QtNDSLocUtils::onRomLoaded(bool success)
{
    m_bRomLoaded = success;
    m_bIsBusy = false;

    if (!success)
    {
        clearFileTree();
        m_selectedCount = 0;
    }

    updateUiState();

    if (success)
        appendLog("~~ rom loaded ~~\n");
    else
        appendLog("~~ failed to load rom ~~\n");
}

void QtNDSLocUtils::onFilesystemListed(const NdsFileEntryList& entries)
{
    populateFileTree(entries);
}

void QtNDSLocUtils::onTaskFinished(bool success)
{
    m_bIsBusy = false;
    updateUiState();

    if (success)
        appendLog("~~ finished! ~~\n");
}

void QtNDSLocUtils::clearFileTree()
{
    m_updatingTree = true;
    m_fileItems.clear();
    ui.treeFiles->clear();
    m_updatingTree = false;
}

void QtNDSLocUtils::populateFileTree(const NdsFileEntryList& entries)
{
    m_updatingTree = true;
    ui.treeFiles->setUpdatesEnabled(false);

    m_fileItems.clear();
    ui.treeFiles->clear();

    QHash<QString, QTreeWidgetItem*> folders;

    for (const NdsFileEntry& entry : entries)
    {
        const QStringList parts = entry.path.split('/', Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        QTreeWidgetItem* parent = nullptr;
        QString folderPath;

        for (int i = 0; i < parts.size() - 1; ++i)
        {
            folderPath += '/' + parts[i];

            auto it = folders.find(folderPath);
            if (it == folders.end())
            {
                auto* folder = parent ? new QTreeWidgetItem(parent)
                                      : new QTreeWidgetItem(ui.treeFiles);
                folder->setText(ColumnName, parts[i]);
                folder->setText(ColumnType, QStringLiteral("Folder"));
                folder->setFlags(folder->flags() | Qt::ItemIsUserCheckable);
                folder->setCheckState(ColumnName, Qt::Unchecked);
                folder->setData(ColumnName, PathRole, folderPath);
                folder->setData(ColumnName, IsFileRole, false);

                it = folders.insert(folderPath, folder);
            }

            parent = it.value();
        }

        const QString& fileName = parts.last();

        auto* file = parent ? new QTreeWidgetItem(parent)
                            : new QTreeWidgetItem(ui.treeFiles);
        file->setText(ColumnName, fileName);
        file->setText(ColumnSize, humanSize(entry.size));
        file->setTextAlignment(ColumnSize, Qt::AlignRight | Qt::AlignVCenter);
        file->setText(ColumnType, entry.type);
        file->setFlags(file->flags() | Qt::ItemIsUserCheckable);
        file->setCheckState(ColumnName, Qt::Unchecked);
        file->setData(ColumnName, PathRole, entry.path);
        file->setData(ColumnName, SizeRole, QVariant::fromValue(entry.size));
        file->setData(ColumnName, IsFileRole, true);

        m_fileItems.insert(entry.path, file);
    }

    ui.treeFiles->expandToDepth(0);
    ui.treeFiles->setUpdatesEnabled(true);
    m_updatingTree = false;

    m_selectedCount = 0;

    applyDefaultSelection();

    updateSelectionInfo();
    updateUiState();
}

void QtNDSLocUtils::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_updatingTree || column != ColumnName || item == nullptr)
        return;

    m_updatingTree = true;

    const Qt::CheckState state = item->checkState(ColumnName);
    if (item->childCount() > 0 && state != Qt::PartiallyChecked)
        setCheckStateRecursive(item, state);

    refreshParentCheckState(item->parent());

    m_updatingTree = false;

    updateSelectionInfo();
    updateUiState();
}

void QtNDSLocUtils::setCheckStateRecursive(QTreeWidgetItem* item, Qt::CheckState state)
{
    for (int i = 0; i < item->childCount(); ++i)
    {
        QTreeWidgetItem* child = item->child(i);
        child->setCheckState(ColumnName, state);
        setCheckStateRecursive(child, state);
    }
}

void QtNDSLocUtils::refreshParentCheckState(QTreeWidgetItem* item)
{
    while (item != nullptr)
    {
        int checked = 0;
        int partial = 0;
        const int count = item->childCount();

        for (int i = 0; i < count; ++i)
        {
            switch (item->child(i)->checkState(ColumnName))
            {
            case Qt::Checked: ++checked; break;
            case Qt::PartiallyChecked: ++partial; break;
            default: break;
            }
        }

        Qt::CheckState state = Qt::Unchecked;
        if (partial > 0 || (checked > 0 && checked < count))
            state = Qt::PartiallyChecked;
        else if (count > 0 && checked == count)
            state = Qt::Checked;

        item->setCheckState(ColumnName, state);
        item = item->parent();
    }
}

void QtNDSLocUtils::recomputeFolderStates(QTreeWidgetItem* item)
{
    const int count = item->childCount();

    int checked = 0;
    int partial = 0;

    for (int i = 0; i < count; ++i)
    {
        QTreeWidgetItem* child = item->child(i);

        if (!child->data(ColumnName, IsFileRole).toBool())
            recomputeFolderStates(child);

        switch (child->checkState(ColumnName))
        {
        case Qt::Checked:          ++checked; break;
        case Qt::PartiallyChecked: ++partial; break;
        default:                             break;
        }
    }

    if (item == ui.treeFiles->invisibleRootItem())
        return;

    Qt::CheckState state = Qt::Unchecked;
    if (partial > 0 || (checked > 0 && checked < count))
        state = Qt::PartiallyChecked;
    else if (count > 0 && checked == count)
        state = Qt::Checked;

    item->setCheckState(ColumnName, state);
}

void QtNDSLocUtils::setAllChecked(Qt::CheckState state)
{
    m_updatingTree = true;
    ui.treeFiles->setUpdatesEnabled(false);

    setCheckStateRecursive(ui.treeFiles->invisibleRootItem(), state);

    ui.treeFiles->setUpdatesEnabled(true);
    m_updatingTree = false;

    updateSelectionInfo();
    updateUiState();
}

void QtNDSLocUtils::collectCheckedFiles(const QTreeWidgetItem* item, QStringList& out) const
{
    for (int i = 0; i < item->childCount(); ++i)
    {
        const QTreeWidgetItem* child = item->child(i);

        if (child->data(ColumnName, IsFileRole).toBool())
        {
            if (child->checkState(ColumnName) == Qt::Checked)
                out << child->data(ColumnName, PathRole).toString();
        }
        else
        {
            collectCheckedFiles(child, out);
        }
    }
}

QStringList QtNDSLocUtils::selectedFiles() const
{
    QStringList out;
    collectCheckedFiles(ui.treeFiles->invisibleRootItem(), out);
    return out;
}

void QtNDSLocUtils::updateSelectionInfo()
{
    m_selectedCount = selectedFiles().size();

    ui.statusBar->showMessage(m_selectedCount == 0
        ? QStringLiteral("No file selected")
        : QStringLiteral("%1 file(s) selected").arg(m_selectedCount));
}

QList<QTreeWidgetItem*> QtNDSLocUtils::matchPattern(const QString& pattern) const
{
    QList<QTreeWidgetItem*> matches;

    const bool isWildcard = pattern.contains('*') || pattern.contains('?');
    const bool isFullPath = pattern.startsWith('/');

    if (!isWildcard && isFullPath)
    {
        const auto it = m_fileItems.find(pattern);
        if (it != m_fileItems.end())
            matches << it.value();
        return matches;
    }

    QRegularExpression regex;
    if (isWildcard)
    {
        regex = QRegularExpression(
            QRegularExpression::anchoredPattern(
                QRegularExpression::wildcardToRegularExpression(pattern)),
            QRegularExpression::CaseInsensitiveOption);
    }

    for (auto it = m_fileItems.constBegin(); it != m_fileItems.constEnd(); ++it)
    {
        const QString& fullPath = it.key();
        const QString fileName = fullPath.section('/', -1);

        const bool hit = isWildcard
            ? (regex.match(fullPath).hasMatch() || regex.match(fileName).hasMatch())
            : (isFullPath ? fullPath.compare(pattern, Qt::CaseInsensitive) == 0
                          : fileName.compare(pattern, Qt::CaseInsensitive) == 0);

        if (hit)
            matches << it.value();
    }

    return matches;
}

void QtNDSLocUtils::expandAncestors(QTreeWidgetItem* item)
{
    for (QTreeWidgetItem* parent = item->parent(); parent != nullptr; parent = parent->parent())
        parent->setExpanded(true);
}

void QtNDSLocUtils::applyDefaultSelection()
{
    if (kDefaultSelection.isEmpty() || m_fileItems.isEmpty())
        return;

    m_updatingTree = true;
    ui.treeFiles->setUpdatesEnabled(false);

    QTreeWidgetItem* firstMatch = nullptr;
    int ticked = 0;

    for (const QString& pattern : kDefaultSelection)
    {
        const QList<QTreeWidgetItem*> matches = matchPattern(pattern);

        if (matches.isEmpty())
        {
            appendLog("Default selection: no match for \"" + pattern.toStdString() + "\"\n");
            continue;
        }

        for (QTreeWidgetItem* item : matches)
        {
            if (item->checkState(ColumnName) == Qt::Checked)
                continue;

            item->setCheckState(ColumnName, Qt::Checked);
            expandAncestors(item);
            ++ticked;

            if (firstMatch == nullptr)
                firstMatch = item;
        }
    }

    recomputeFolderStates(ui.treeFiles->invisibleRootItem());

    ui.treeFiles->setUpdatesEnabled(true);
    m_updatingTree = false;

    if (ticked > 0 && firstMatch != nullptr)
    {
        ui.treeFiles->scrollToItem(firstMatch, QAbstractItemView::PositionAtCenter);
    }
}
