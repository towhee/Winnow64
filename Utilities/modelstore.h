#ifndef MODELSTORE_H
#define MODELSTORE_H

#include <QObject>
#include <QString>
#include <QVector>

class QWidget;

/*
    ON-DEMAND ONNX MODELS

    Winnow ships two small models inside the app (focus_point_model.onnx, pmrid.onnx) and
    downloads the other seven on first use, into

        <QStandardPaths::AppDataLocation>/Models
        mac: ~/Library/Application Support/Winnow/Models
        win: %APPDATA%/Winnow/Models

    Together those seven are ~771 MB, and most users never touch the features that need
    them, so bundling them made every DMG and installer that much larger for nothing.

    ModelStore is the single seam for "where is model X?". Before it, nine call sites each
    inlined QDir(QCoreApplication::applicationDirPath()).filePath("<x>.onnx").

    THE APPEND-ONLY SERVER. The download URL carries a version, the file on disk does not:

        https://winnow.ca/winnow/models/<stem>-<version>.onnx  ->  <Models>/<stem>.onnx

    A published <stem>-<n>.onnx is NEVER overwritten or deleted. Every Winnow already
    installed fetches exactly the bytes whose SHA-256 it was compiled with, forever.
    Re-exporting a model means uploading <stem>-<n+1>.onnx ALONGSIDE the old one and
    bumping version in the catalog -- tools/gen_model_catalog.sh does this and fails the
    release if bytes changed while the version did not.

    STALENESS. Expected size + SHA-256 are compiled in (the catalog table in the .cpp), not
    fetched: the model and the code that feeds it are versioned together. Re-hashing 198 MB
    on every click would be far too slow, so <Models>/models.json records what was verified
    at download time and state() resolves in O(1) against it -- a user who upgrades Winnow
    to a build wanting new weights gets Stale, and is prompted to update rather than
    silently running the superseded model. That matters concretely: a mismatched SAM 2
    decoder does not merely produce worse output, it fails OpenCV DNN's parser.

    THREADING. path() / isAvailable() / state() are safe from any thread (render workers
    call them). ensure(), which may show a dialog, is GUI-thread only.
*/
namespace ModelStore {

enum class Model {
    FocusPoint,     // focus_point_model.onnx -- bundled
    Pmrid,          // pmrid.onnx             -- bundled
    U2Net,          // u2net.onnx
    SkySeg,         // skyseg.onnx
    Midas,          // midas.onnx
    Migan,          // migan.onnx
    Lama,           // lama.onnx
    Sam2Encoder,    // sam2_encoder.onnx
    Sam2Decoder     // sam2_decoder.onnx
};

struct ModelInfo {
    Model       id;
    const char *fileName;   // name ON DISK, never versioned ("u2net.onnx")
    const char *feature;    // shown in the prompt ("Select Subject")
    int         version;    // picks the URL; bumped on re-export
    qint64      bytes;      // expected size
    const char *sha256;     // expected hash, lowercase hex
    bool        bundled;    // ships inside the app -- never downloaded
};

enum class State {
    Bundled,    // inside the app; cannot drift from the code that loads it
    Ready,      // downloaded and verified against this build's catalog
    Missing,    // not downloaded
    Stale,      // downloaded, but this build wants different weights
    Corrupt     // present, but the bytes do not hash to anything we expect
};

const QVector<ModelInfo> &catalog();
const ModelInfo          &info(Model);

/* <AppDataLocation>/Models, mkpath'd on demand. */
QString dir();

/* Absolute path to load from, or "" when the model is absent, stale or corrupt.
   applicationDirPath() is checked FIRST, so a dev tree with models still beside the
   binary keeps working unchanged and the two bundled models resolve. Never downloads. */
QString path(Model);

bool  isAvailable(Model);
bool  isAvailable(const QVector<Model> &models);
State state(Model);

/* Human text for the Manage dialog and for "why is this feature unavailable" messages. */
QString stateText(State);

/* GUI THREAD ONLY. Everything in `need` Ready/Bundled -> returns true immediately.
   Otherwise shows ModelDownloadDlg naming the feature and the total MB, and on confirm
   downloads + verifies them. Returns true once they are all on disk, so the caller just
   carries on inline -- the dialog is MODAL, so by the time this returns the bytes really
   are there. Returns false if the user declined, cancelled, or the download failed, and
   the caller bails exactly as it does today for an absent model.
   A silent false under G::isTest / stress / QStandardPaths test mode. */
bool ensure(const QVector<Model> &need, QWidget *parent);

/* Files in dir() that no catalog entry claims -- models a newer Winnow dropped. Listed in
   the Manage dialog for the user to delete; never removed automatically, because a
   downgrade would then have to re-fetch them. */
QVector<QString> orphans();
qint64           orphanBytes();

bool   remove(Model);        // delete a downloaded model
qint64 bytesOnDisk();        // total of everything in dir()

/* The append-only download URL for a model: <base>/<stem>-<version>.onnx */
QString url(Model);

/* Used by ModelDownloadDlg. recordDownloaded() writes the sidecar entry for a model it has
   just verified onto disk, so the next state() sees it without a restart; invalidate()
   drops a cached State when the file changed underneath us. */
void recordDownloaded(Model m, const QString &sha256);
void invalidate(Model m);

/* Discard any .part left by an interrupted download. Called once at startup. */
void sweepPartials();

}   // namespace ModelStore

#endif // MODELSTORE_H
