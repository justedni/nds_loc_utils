#include "QtNDSLocUtils.h"

#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QStatusBar>
#include <QTextCursor>
#include <QTreeWidgetItem>

#include "Worker.h"

#include "core/strings.h"

namespace
{
    constexpr int ColumnName = 0;
    constexpr int ColumnSize = 1;
    constexpr int ColumnType = 2;

    constexpr int PathRole   = Qt::UserRole + 1;
    constexpr int SizeRole   = Qt::UserRole + 2;
    constexpr int IsFileRole = Qt::UserRole + 3;

    const QStringList kDefaultSelection = {
        QStringLiteral("/db/db_en.p2"),
        QStringLiteral("/ev/EV_AL.p2"),
        QStringLiteral("/ev/EV_AW.p2"),
        QStringLiteral("/ev/EV_BB.p2"),
        QStringLiteral("/ev/EV_DP.p2"),
        QStringLiteral("/ev/EV_HE.p2"),
        QStringLiteral("/ev/EV_NM.p2"),
        QStringLiteral("/ev/EV_PP.p2"),
        QStringLiteral("/ev/EV_S.p2"),
        QStringLiteral("/ev/EV_TT.p2"),
        QStringLiteral("/mi/mi/10000"),
        QStringLiteral("/UI/cm/str/rpt_en.z"),
        QStringLiteral("/UI/cm/str/cfg_en.s.z"),
        QStringLiteral("/UI/cm/str/enm_en.s.z"),
        QStringLiteral("/UI/cm/str/enm_en.z"),
        QStringLiteral("/UI/cm/str/panel_en.s.z"),
        QStringLiteral("/UI/cm/str/root_en.s.z"),
        QStringLiteral("/UI/cm/str/rpt_en.s.z"),
        QStringLiteral("/UI/cm/str/rpt_en.z"),
        QStringLiteral("/UI/cm/str/sav_en.s.z"),
        QStringLiteral("/UI/cm/str/select_en.s.z"),
        QStringLiteral("/UI/cm/str/status_en.s.z"),
        QStringLiteral("/UI/cm/str/ttl_en.s.z"),
        QStringLiteral("/UI/cm/str/world_id_en.s.z"),
        //QStringLiteral("*.p2"),
    };

    void setExpandedRecursive(QTreeWidgetItem* item, bool expanded)
    {
        item->setExpanded(expanded);
        for (int i = 0; i < item->childCount(); ++i)
            setExpandedRecursive(item->child(i), expanded);
    }

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
    connect(this, &QtNDSLocUtils::requestExtractP2Files, m_worker, &Worker::extractP2Files);
    connect(this, &QtNDSLocUtils::requestExtractZFiles, m_worker, &Worker::extractZFiles);
    connect(this, &QtNDSLocUtils::requestExtractRawFiles, m_worker, &Worker::extractRawFiles);
    connect(this, &QtNDSLocUtils::requestExportStrings, m_worker, &Worker::exportStrings);

    connect(ui.treeFiles, &QTreeWidget::itemChanged, this, &QtNDSLocUtils::onTreeItemChanged);

    ui.treeFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.treeFiles, &QTreeWidget::customContextMenuRequested, this, &QtNDSLocUtils::onTreeContextMenu);

    m_workerThread.start();

    setTargetFolder(QDir::currentPath());

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
    ui.buttonExportStringsToCsv->setEnabled(idle && m_bRomLoaded && m_selectedCount > 0);
    ui.buttonExportStringsToIni->setEnabled(idle && m_bRomLoaded && m_selectedCount > 0);
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

    loadRom(romPath);
}

void QtNDSLocUtils::loadRom(QString romPath)
{
    ui.lineRomPath->setText(romPath);

    m_bRomLoaded = false;
    m_selectedCount = 0;

    clearFileTree();

    beginTask();
    emit requestLoadRom(romPath);
}

void QtNDSLocUtils::setTargetFolder(QString targetPath)
{
    ui.lineTargetFolder->setText(targetPath);
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

void QtNDSLocUtils::on_buttonExportStringsToCsv_clicked()
{
    auto outPath = ui.lineTargetFolder->text();
    if (outPath.isEmpty())
        return;

    const QStringList files = selectedFiles();
    if (files.isEmpty())
        return;

    beginTask();
    emit requestExportStrings(outPath, files, ndsloc::ExportFormat::Csv);
}

void QtNDSLocUtils::on_buttonExportStringsToIni_clicked()
{
    auto outPath = ui.lineTargetFolder->text();
    if (outPath.isEmpty())
        return;

    const QStringList files = selectedFiles();
    if (files.isEmpty())
        return;

    beginTask();
    emit requestExportStrings(outPath, files, ndsloc::ExportFormat::Ini);
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

void QtNDSLocUtils::setItemsChecked(const QList<QTreeWidgetItem*>& items, Qt::CheckState state)
{
    if (items.isEmpty())
        return;

    m_updatingTree = true;
    ui.treeFiles->setUpdatesEnabled(false);

    for (QTreeWidgetItem* item : items)
    {
        item->setCheckState(ColumnName, state);
        setCheckStateRecursive(item, state);
    }

    recomputeFolderStates(ui.treeFiles->invisibleRootItem());

    ui.treeFiles->setUpdatesEnabled(true);
    m_updatingTree = false;

    updateSelectionInfo();
    updateUiState();
}

void QtNDSLocUtils::collectFilePaths(const QTreeWidgetItem* item, QStringList& out) const
{
    for (int i = 0; i < item->childCount(); ++i)
    {
        const QTreeWidgetItem* child = item->child(i);

        if (child->data(ColumnName, IsFileRole).toBool())
            out << child->data(ColumnName, PathRole).toString();
        else
            collectFilePaths(child, out);
    }
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


void QtNDSLocUtils::onTreeContextMenu(const QPoint& pos)
{
    if (m_bIsBusy)
        return;

    QTreeWidgetItem* clicked = ui.treeFiles->itemAt(pos);

    QMenu menu(this);

    if (clicked == nullptr)
    {
        QAction* expandAll = menu.addAction(tr("Expand all"));
        connect(expandAll, &QAction::triggered, ui.treeFiles, &QTreeWidget::expandAll);

        QAction* collapseAll = menu.addAction(tr("Collapse all"));
        connect(collapseAll, &QAction::triggered, ui.treeFiles, &QTreeWidget::collapseAll);

        menu.exec(ui.treeFiles->viewport()->mapToGlobal(pos));
        return;
    }

    if (!ui.treeFiles->selectedItems().contains(clicked))
        ui.treeFiles->setCurrentItem(clicked);

    const TreeContext ctx = buildTreeContext(clicked);

    if (ctx.hasFolders)
        addFolderActions(menu, ctx);

    if (ctx.hasFiles)
        addFileActions(menu, ctx);

    addCommonActions(menu, ctx);

    if (!menu.isEmpty())
    {
        menu.exec(ui.treeFiles->viewport()->mapToGlobal(pos));
    }
}

QtNDSLocUtils::TreeContext QtNDSLocUtils::buildTreeContext(QTreeWidgetItem* clicked) const
{
    TreeContext ctx;
    ctx.clicked = clicked;

    QList<QTreeWidgetItem*> selection = ui.treeFiles->selectedItems();
    if (selection.isEmpty())
        selection << clicked;

    const QSet<QTreeWidgetItem*> selectionSet(selection.begin(), selection.end());

    for (QTreeWidgetItem* item : selection)
    {
        bool coveredByAncestor = false;
        for (QTreeWidgetItem* p = item->parent(); p != nullptr; p = p->parent())
        {
            if (selectionSet.contains(p))
            {
                coveredByAncestor = true;
                break;
            }
        }

        if (!coveredByAncestor)
            ctx.items << item;
    }


    QStringList types;

    for (QTreeWidgetItem* item : ctx.items)
    {
        if (item->data(ColumnName, IsFileRole).toBool())
        {
            ctx.hasFiles = true;
            ctx.filePaths << item->data(ColumnName, PathRole).toString();

            types.append(item->text(ColumnType));
        }
        else
        {
            ctx.hasFolders = true;
            collectFilePaths(item, ctx.filePaths);
        }
    }

    QString commonType = "";
    bool uniform = !ctx.filePaths.isEmpty();
    for (auto& type : types)
    {
        if (commonType.isEmpty())
            commonType = type;
        else if (commonType != type)
        {
            uniform = false;
            break;
        }
    }

    if (uniform)
        ctx.commonType = commonType;

    return ctx;
}

void QtNDSLocUtils::addFolderActions(QMenu& menu, const TreeContext& ctx)
{
    QAction* expand = menu.addAction(tr("Expand"));
    connect(expand, &QAction::triggered, this, [ctx]() {
        for (QTreeWidgetItem* item : ctx.items)
            setExpandedRecursive(item, true);
    });

    QAction* collapse = menu.addAction(tr("Collapse"));
    connect(collapse, &QAction::triggered, this, [ctx]() {
        for (QTreeWidgetItem* item : ctx.items)
            setExpandedRecursive(item, false);
    });

    menu.addSeparator();
}

void QtNDSLocUtils::addFileActions(QMenu& menu, const TreeContext& ctx)
{
    int fileCount = ctx.filePaths.size();
    auto getActionName = [&fileCount](const char* name, const char* type) -> QString
    {
        return (fileCount > 1)
            ? tr("%1 %2 %3 files...").arg(name, QString::number(fileCount), type)
            : tr("%1 %2 file...").arg(name, type);
    };

    if (ctx.commonType == QLatin1String("p2"))
    {
        QAction* extract = menu.addAction(getActionName("Extract and decompress", "P2"));
        connect(extract, &QAction::triggered, this, [this, ctx]() { actionExtractFile(ctx, EExtractType::P2); });
    }
    else if (ctx.commonType == QLatin1String("z"))
    {
        QAction* decompress = menu.addAction(getActionName("Decompress", "Z"));
        connect(decompress, &QAction::triggered, this, [this, ctx]() { actionExtractFile(ctx, EExtractType::Z); });
    }

    QAction* extractRaw = menu.addAction(getActionName("Extract", "raw"));
    connect(extractRaw, &QAction::triggered, this, [this, ctx]() { actionExtractFile(ctx, EExtractType::Raw); });

    menu.addSeparator();
}

void QtNDSLocUtils::addCommonActions(QMenu& menu, const TreeContext& ctx)
{
    QAction* tick = menu.addAction(tr("Tick for export"));
    tick->setEnabled(!ctx.items.isEmpty());
    connect(tick, &QAction::triggered, this, [this, ctx]() {
        setItemsChecked(ctx.items, Qt::Checked);
    });

    QAction* untick = menu.addAction(tr("Untick"));
    untick->setEnabled(!ctx.items.isEmpty());
    connect(untick, &QAction::triggered, this, [this, ctx]() {
        setItemsChecked(ctx.items, Qt::Unchecked);
    });

    menu.addSeparator();

    QAction* copyPath = menu.addAction(ctx.items.size() > 1
        ? tr("Copy %1 paths").arg(ctx.items.size())
        : tr("Copy path"));
    connect(copyPath, &QAction::triggered, this, [ctx]() {
        QStringList paths;
        for (QTreeWidgetItem* item : ctx.items)
            paths << item->data(ColumnName, PathRole).toString();

        QGuiApplication::clipboard()->setText(paths.join('\n'));
    });
}

void QtNDSLocUtils::actionExtractFile(const TreeContext& ctx, EExtractType type)
{
    if (ctx.filePaths.isEmpty())
        return;

    QString startDir = m_lastExtractFolder;
    if (startDir.isEmpty())
        startDir = ui.lineTargetFolder->text();
    if (startDir.isEmpty())
        startDir = QFileInfo(ui.lineRomPath->text()).absolutePath();

    const QString outFolder = QFileDialog::getExistingDirectory(
        this,
        tr("Extract %1 file(s) to...").arg(ctx.filePaths.size()),
        startDir);

    if (outFolder.isEmpty())
        return;

    if (!QFileInfo(outFolder).isWritable())
    {
        QMessageBox::warning(this, tr("Extract"), tr("This folder is not writable:\n%1").arg(outFolder));
        return;
    }

    m_lastExtractFolder = outFolder;

    appendLog("Extracting " + std::to_string(ctx.filePaths.size()) + " file(s) to: " + outFolder.toStdString() + "\n");

    switch (type)
    {
    case EExtractType::Raw:
        emit requestExtractRawFiles(outFolder, ctx.filePaths);
        break;
    case EExtractType::P2:
        emit requestExtractP2Files(outFolder, ctx.filePaths);
        break;
    case EExtractType::Z:
        emit requestExtractZFiles(outFolder, ctx.filePaths);
        break;
    default:
        assert(false);
    }
}
