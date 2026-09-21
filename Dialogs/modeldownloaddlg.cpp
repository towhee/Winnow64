#include "Dialogs/modeldownloaddlg.h"
#include "Main/global.h"

#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

QString ModelDownloadDlg::formatBytes(qint64 bytes)
{
    if (bytes >= (1LL << 30)) return QString::number(double(bytes) / (1LL << 30), 'f', 1) + " GB";
    return QString::number(qRound(double(bytes) / (1LL << 20))) + " MB";
}

ModelDownloadDlg::ModelDownloadDlg(const QVector<ModelStore::Model> &models, QWidget *parent)
    : QDialog(parent), models(models)
{
    if (G::isLogger) G::log("ModelDownloadDlg::ModelDownloadDlg");

    for (ModelStore::Model m : models) totalBytes += ModelStore::info(m).bytes;

    /* One feature may need two models (Object Mask = SAM 2 encoder + decoder), so name the
       feature once and the files after it. */
    const QString feature = QString::fromLatin1(ModelStore::info(models.first()).feature);
    QStringList names;
    bool anyStale = false;
    for (ModelStore::Model m : models) {
        names << QString::fromLatin1(ModelStore::info(m).fileName);
        const ModelStore::State s = ModelStore::state(m);
        if (s == ModelStore::State::Stale || s == ModelStore::State::Corrupt) anyStale = true;
    }

    setWindowTitle(anyStale ? tr("Update AI Model") : tr("Download AI Model"));

    const QString what = anyStale
        ? tr("<b>%1</b> needs an updated model.").arg(feature)
        : tr("<b>%1</b> needs to download %2.")
              .arg(feature, names.size() == 1 ? names.first() : tr("%1 files").arg(names.size()));

    messageLabel = new QLabel(
        what + "<br><br>" +
        tr("Download size: <b>%1</b>  (%2)").arg(formatBytes(totalBytes), names.join(", ")) +
        "<br><br>" +
        tr("The model is stored in your Winnow application data folder and is only "
           "downloaded once. You can remove it later from Help &gt; Manage AI Models."),
        this);
    messageLabel->setWordWrap(true);
    messageLabel->setTextFormat(Qt::RichText);

    detailLabel = new QLabel(this);
    detailLabel->hide();

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->hide();

    auto *buttons = new QDialogButtonBox(this);
    downloadBtn = buttons->addButton(anyStale ? tr("Update") : tr("Download"),
                                     QDialogButtonBox::AcceptRole);
    cancelBtn   = buttons->addButton(tr("Not now"), QDialogButtonBox::RejectRole);
    connect(downloadBtn, &QPushButton::clicked, this, &ModelDownloadDlg::onDownloadClicked);
    connect(cancelBtn,   &QPushButton::clicked, this, &ModelDownloadDlg::onCancelClicked);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(messageLabel);
    layout->addWidget(detailLabel);
    layout->addWidget(progressBar);
    layout->addWidget(buttons);
    setMinimumWidth(460);

    setStyleSheet(G::css);
#ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
#endif
}

ModelDownloadDlg::~ModelDownloadDlg()
{
    /* reply/file/hash are torn down in onFinished; this only matters if the dialog is
       destroyed while a transfer is somehow still live. */
    delete hash;
    if (file) { file->cancelWriting(); delete file; }
}

void ModelDownloadDlg::onDownloadClicked()
{
    if (G::isLogger) G::log("ModelDownloadDlg::onDownloadClicked");
    messageLabel->setText(tr("<b>Downloading AI model</b>"));
    detailLabel->show();
    progressBar->show();
    downloadBtn->setEnabled(false);
    downloadBtn->hide();
    cancelBtn->setText(tr("Cancel"));
    netManager = new QNetworkAccessManager(this);
    startNext();
}

void ModelDownloadDlg::startNext()
{
    if (index >= models.size()) {         // every model committed and verified
        accept();
        return;
    }

    const ModelStore::ModelInfo &mi = ModelStore::info(models.at(index));
    const QString destPath = QDir(ModelStore::dir()).filePath(QString::fromLatin1(mi.fileName));

    /* QSaveFile writes a temp file beside the target and renames on commit(), so an
       aborted download can never leave a half-written model where a predictor would
       find it and try to parse it. */
    file = new QSaveFile(destPath);
    if (!file->open(QIODevice::WriteOnly)) {
        delete file;
        file = nullptr;
        failWith(tr("Could not write to<br>%1").arg(ModelStore::dir()));
        return;
    }

    hash = new QCryptographicHash(QCryptographicHash::Sha256);

    detailLabel->setText(models.size() == 1
        ? tr("%1 - 0 of %2").arg(QString::fromLatin1(mi.fileName), formatBytes(mi.bytes))
        : tr("Downloading %1 of %2: %3")
              .arg(index + 1).arg(models.size())
              .arg(QString::fromLatin1(mi.fileName)));

    reply = netManager->get(QNetworkRequest(QUrl(ModelStore::url(models.at(index)))));
    connect(reply, &QNetworkReply::readyRead,         this, &ModelDownloadDlg::onReadyRead);
    connect(reply, &QNetworkReply::downloadProgress,  this, &ModelDownloadDlg::onProgress);
    connect(reply, &QNetworkReply::finished,          this, &ModelDownloadDlg::onFinished);
}

void ModelDownloadDlg::onReadyRead()
{
    if (!reply || !file) return;
    /* Stream to disk AND into the hash from the same chunk: the model is never held in
       memory, and verification costs no second pass over 198 MB. */
    const QByteArray chunk = reply->readAll();
    file->write(chunk);
    if (hash) hash->addData(chunk);
}

void ModelDownloadDlg::onProgress(qint64 received, qint64 total)
{
    const ModelStore::ModelInfo &mi = ModelStore::info(models.at(index));
    const qint64 expected = total > 0 ? total : mi.bytes;
    if (totalBytes > 0)
        progressBar->setValue(int((doneBytes + received) * 100 / totalBytes));

    const QString where = models.size() == 1
        ? QString::fromLatin1(mi.fileName)
        : tr("%1 of %2: %3").arg(index + 1).arg(models.size())
                            .arg(QString::fromLatin1(mi.fileName));
    detailLabel->setText(tr("%1 - %2 of %3")
                         .arg(where, formatBytes(received), formatBytes(expected)));
}

void ModelDownloadDlg::onFinished()
{
    if (!reply) return;
    QNetworkReply *r = reply;
    reply = nullptr;
    r->deleteLater();

    if (aborted) {                       // user pressed Cancel
        if (file) { file->cancelWriting(); delete file; file = nullptr; }
        delete hash; hash = nullptr;
        reject();
        return;
    }

    if (r->error() != QNetworkReply::NoError) {
        if (file) { file->cancelWriting(); delete file; file = nullptr; }
        delete hash; hash = nullptr;
        failWith(tr("The download failed:<br>%1").arg(r->errorString()));
        return;
    }

    const QByteArray tail = r->readAll();      // anything readyRead did not consume
    file->write(tail);
    if (hash) hash->addData(tail);

    const ModelStore::Model      m  = models.at(index);
    const ModelStore::ModelInfo &mi = ModelStore::info(m);
    const QString actual = QString::fromLatin1(hash->result().toHex());
    delete hash; hash = nullptr;

    /* Verify BEFORE commit(), so a bad download never becomes a file a predictor can
       find. The size check is redundant against the hash but names the likely cause
       (a truncated transfer) in the log. */
    if (actual != QString::fromLatin1(mi.sha256)) {
        file->cancelWriting();
        delete file;
        file = nullptr;
        qWarning("ModelStore: %s failed verification (expected %s, got %s)",
                 mi.fileName, mi.sha256, actual.toUtf8().constData());
        failWith(tr("The downloaded file was damaged and has been discarded.<br><br>"
                    "Please try again."));
        return;
    }

    const bool committed = file->commit();
    delete file;
    file = nullptr;
    if (!committed) {
        failWith(tr("Could not finish saving %1.").arg(QString::fromLatin1(mi.fileName)));
        return;
    }

    ModelStore::recordDownloaded(m, actual);
    doneBytes += mi.bytes;
    ++index;
    startNext();
}

void ModelDownloadDlg::onCancelClicked()
{
    if (reply) {            // mid-transfer: abort and let onFinished tidy up
        aborted = true;
        reply->abort();
        return;
    }
    reject();               // still on the confirm page: "Not now"
}

void ModelDownloadDlg::failWith(const QString &message)
{
    QMessageBox box(QMessageBox::Warning, tr("Download AI Model"), message,
                    QMessageBox::Ok, this);
    box.setTextFormat(Qt::RichText);
    box.setStyleSheet(G::css);
    box.exec();
    reject();
}

void ModelDownloadDlg::reportNoSpace(qint64 needed, qint64 available, QWidget *parent)
{
    QMessageBox box(QMessageBox::Warning, tr("Download AI Model"),
                    tr("This model needs %1 but only %2 is free on the volume holding<br>%3")
                        .arg(formatBytes(needed), formatBytes(available), ModelStore::dir()),
                    QMessageBox::Ok, parent);
    box.setTextFormat(Qt::RichText);
    box.setStyleSheet(G::css);
    box.exec();
}
