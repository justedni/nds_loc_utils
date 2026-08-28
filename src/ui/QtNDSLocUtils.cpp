#include "QtNDSLocUtils.h"

#include <QFileDialog>
#include <QTextCursor>

#include "Worker.h"

QtNDSLocUtils::QtNDSLocUtils(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    auto font = QFont(ui.textEditLog->font());
    font.setPointSize(8);
    ui.textEditLog->setFont(font);

    m_worker = new Worker();
    m_worker->moveToThread(&m_workerThread);

    connect(m_worker, &Worker::log, this, &QtNDSLocUtils::appendLog);
    connect(m_worker, &Worker::romLoaded, this, &QtNDSLocUtils::onRomLoaded);
    connect(m_worker, &Worker::taskFinished, this, &QtNDSLocUtils::onTaskFinished);

    connect(this, &QtNDSLocUtils::requestLoadRom, m_worker, &Worker::loadRom);
    connect(this, &QtNDSLocUtils::requestPrintFilesystem, m_worker, &Worker::printFilesystem);
    connect(this, &QtNDSLocUtils::requestExportStrings, m_worker, &Worker::exportStrings);

    m_workerThread.start();

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
    ui.lineRomPath->setEnabled(idle);
    ui.lineTargetFolder->setEnabled(idle);

    ui.buttonPrintFilesystem->setEnabled(idle && m_bRomLoaded);
    ui.buttonExportStrings->setEnabled(idle && m_bRomLoaded);
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
    beginTask();

    emit requestLoadRom(romPath.toStdString());
}

void QtNDSLocUtils::on_browseTargetFolder_clicked()
{
    QString targetFolder = QFileDialog::getExistingDirectory(this, "Choose target directory");
    if (targetFolder.isEmpty())
        return;

    ui.lineTargetFolder->setText(targetFolder);
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

    beginTask();
    emit requestExportStrings(outPath.toStdString());
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
    updateUiState();

    if (success)
        appendLog("~~ rom loaded ~~\n");
    else
        appendLog("~~ failed to load rom ~~\n");
}

void QtNDSLocUtils::onTaskFinished(bool success)
{
    m_bIsBusy = false;
    updateUiState();

    if (success)
        appendLog("~~ finished! ~~\n");
}
