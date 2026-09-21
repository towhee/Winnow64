#ifndef MODELDOWNLOADDLG_H
#define MODELDOWNLOADDLG_H

#include <QDialog>
#include <QVector>
#include "Utilities/modelstore.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;
class QCryptographicHash;

/*
    Asks to download one or more ONNX models, then downloads them -- one modal dialog with
    two states rather than a message box followed by a progress box:

        confirm   "Select Sky needs to download skyseg.onnx (168 MB)."   [Download][Not now]
        progress   a bar, "Downloading 1 of 2 - 42 MB of 128 MB"         [Cancel]

    accept() only on every model verified onto disk, so ModelStore::ensure can treat
    Accepted as "the bytes are really there".

    The transfer follows MW::downloadAndOpenUpdate (Main/mainwindow.cpp): its own
    QNetworkAccessManager, QSaveFile for an atomic commit, readyRead streamed straight to
    disk rather than buffered. Added here: the SHA-256 is fed incrementally from the same
    chunks, so verifying a 198 MB model costs no extra pass and no extra memory.
*/
class ModelDownloadDlg : public QDialog
{
    Q_OBJECT

public:
    ModelDownloadDlg(const QVector<ModelStore::Model> &models, QWidget *parent = nullptr);
    ~ModelDownloadDlg() override;

    /* Shown instead of the dialog when the volume cannot hold the download. */
    static void reportNoSpace(qint64 needed, qint64 available, QWidget *parent);

    static QString formatBytes(qint64 bytes);

private slots:
    void onDownloadClicked();
    void onCancelClicked();
    void onReadyRead();
    void onProgress(qint64 received, qint64 total);
    void onFinished();

private:
    void startNext();
    void failWith(const QString &message);

    QVector<ModelStore::Model> models;
    int     index = 0;              // which model is in flight
    qint64  doneBytes = 0;          // bytes of the models already committed
    qint64  totalBytes = 0;

    QLabel       *messageLabel = nullptr;
    QLabel       *detailLabel  = nullptr;
    QProgressBar *progressBar  = nullptr;
    QPushButton  *downloadBtn  = nullptr;
    QPushButton  *cancelBtn    = nullptr;

    QNetworkAccessManager *netManager = nullptr;
    QNetworkReply         *reply      = nullptr;
    QSaveFile             *file       = nullptr;
    QCryptographicHash    *hash       = nullptr;
    bool                   aborted    = false;
};

#endif // MODELDOWNLOADDLG_H
