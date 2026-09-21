#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDateTime>
#include <QRegularExpression>

#include "Utilities/modelstore.h"

using ModelStore::Model;
using ModelStore::State;

/*
    ModelStore decides whether a downloaded .onnx may be handed to a predictor. The
    property that matters is the NEGATIVE one: a file that is not exactly the bytes this
    build was compiled against must never be loaded. A superseded SAM 2 decoder does not
    merely produce worse masks, it fails OpenCV DNN's parser -- and silently running the
    wrong weights would be worse still.

    The catalog's hashes belong to real 130-210 MB models, so a test cannot fabricate a
    Ready file. It can pin everything around that: Missing, Stale (intact but not ours),
    Corrupt, orphan detection, removal, and that ensure() never reaches the network from a
    test run. QStandardPaths test mode redirects AppDataLocation, so this writes to a
    throwaway location and never touches the user's real Models folder.
*/
class tst_modelstore : public QObject
{
    Q_OBJECT

private:
    /* Write `content` as a model file. When `recordSha` is true, tell ModelStore it was
       verified with that content's hash -- through the real recordDownloaded(), not by
       writing models.json behind its back (the sidecar is read once per process, so a
       hand-written file would be invisible to the in-memory copy). */
    void placeFile(const QString &fileName, const QByteArray &content)
    {
        const QString path = QDir(ModelStore::dir()).filePath(fileName);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
        f.close();
    }

    static QString sha(const QByteArray &b)
    {
        return QString::fromLatin1(
            QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex());
    }

    void clearModelsDir()
    {
        QDir d(ModelStore::dir());
        for (const QFileInfo &fi : d.entryInfoList(QDir::Files)) QFile::remove(fi.absoluteFilePath());
        for (const ModelStore::ModelInfo &mi : ModelStore::catalog())
            ModelStore::invalidate(mi.id);
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY2(QStandardPaths::isTestModeEnabled(),
                 "test mode must redirect AppDataLocation away from the real Models folder");
        clearModelsDir();
    }

    void cleanup() { clearModelsDir(); }

    /* Every enum value appears exactly once, and the generated fields are well formed.
       A hand-edit that drops a row or mangles a hash fails here rather than at runtime. */
    void catalogIsWellFormed()
    {
        const QVector<ModelStore::ModelInfo> &cat = ModelStore::catalog();
        QCOMPARE(cat.size(), 9);

        QSet<int> seen;
        for (const ModelStore::ModelInfo &mi : cat) {
            QVERIFY2(!seen.contains(int(mi.id)), mi.fileName);
            seen.insert(int(mi.id));

            QVERIFY2(mi.bytes > 0, mi.fileName);
            QVERIFY2(mi.version >= 1, mi.fileName);
            const QString h = QString::fromLatin1(mi.sha256);
            QCOMPARE(h.size(), 64);
            QVERIFY2(!h.contains(QRegularExpression("[^0-9a-f]")), mi.fileName);
            QVERIFY2(QString::fromLatin1(mi.fileName).endsWith(".onnx"), mi.fileName);
            QVERIFY2(qstrlen(mi.feature) > 0, mi.fileName);
        }
    }

    /* Only the two small models ship inside the app; everything else is on demand. */
    void bundledSetIsExactlyTwo()
    {
        int bundled = 0;
        for (const ModelStore::ModelInfo &mi : ModelStore::catalog())
            if (mi.bundled) ++bundled;
        QCOMPARE(bundled, 2);
        QVERIFY(ModelStore::info(Model::FocusPoint).bundled);
        QVERIFY(ModelStore::info(Model::Pmrid).bundled);
        QVERIFY(!ModelStore::info(Model::U2Net).bundled);
    }

    /* The URL must carry the version. An unversioned URL would let a re-export overwrite
       the bytes older builds depend on -- the one mistake the whole scheme guards against. */
    void urlCarriesVersion()
    {
        for (const ModelStore::ModelInfo &mi : ModelStore::catalog()) {
            const QString u = ModelStore::url(mi.id);
            QVERIFY2(u.startsWith("https://"), qPrintable(u));
            QString stem = QString::fromLatin1(mi.fileName);
            stem.chop(5);
            const QString expect = stem + "-" + QString::number(mi.version) + ".onnx";
            QVERIFY2(u.endsWith(expect), qPrintable(u + " != ..." + expect));
        }
    }

    void missingModelHasNoPath()
    {
        QCOMPARE(ModelStore::state(Model::U2Net), State::Missing);
        QVERIFY(ModelStore::path(Model::U2Net).isEmpty());
        QVERIFY(!ModelStore::isAvailable(Model::U2Net));
        QVERIFY(!ModelStore::isAvailable(QVector<Model>() << Model::Sam2Encoder << Model::Sam2Decoder));
    }

    /* THE CENTRAL SAFETY PROPERTY. A file that is intact but is not the export this build
       wants (the shape of "user upgraded Winnow, old model still on disk") is Stale, and
       path() refuses it. */
    void supersededModelIsStaleAndRefused()
    {
        const QByteArray old = "an older u2net export";
        placeFile("u2net.onnx", old);
        ModelStore::recordDownloaded(Model::U2Net, sha(old));   // verified, back when

        QCOMPARE(ModelStore::state(Model::U2Net), State::Stale);
        QVERIFY2(ModelStore::path(Model::U2Net).isEmpty(),
                 "a superseded model must never be handed to a predictor");
        QVERIFY(!ModelStore::isAvailable(Model::U2Net));
    }

    /* The file CHANGED after it was verified: it hashes to neither the catalog nor to what
       the sidecar recorded for it. Refused, and reported differently from Stale so the user
       is told to re-download rather than offered an 'update'. */
    void damagedModelIsCorruptAndRefused()
    {
        placeFile("skyseg.onnx", "the verified content");
        ModelStore::recordDownloaded(Model::SkySeg, sha("the verified content"));

        placeFile("skyseg.onnx", "something else entirely");    // damaged in place
        ModelStore::invalidate(Model::SkySeg);

        QCOMPARE(ModelStore::state(Model::SkySeg), State::Corrupt);
        QVERIFY(ModelStore::path(Model::SkySeg).isEmpty());
    }

    /* Bit rot: bytes go bad WITHOUT changing size or mtime, so the sidecar's record still
       looks current and is trusted rather than re-hashed -- that is the whole point of the
       sidecar, and re-reading 200 MB on every click is not an option. The file is still
       refused; only the label differs (Stale, so the user is offered a re-download). This
       pins that the fast path can never hand a predictor something it should not have. */
    void silentlyRottedModelIsStillRefused()
    {
        const QByteArray body = "0123456789";
        placeFile("midas.onnx", body);
        ModelStore::recordDownloaded(Model::Midas, sha(body));

        const QString path = QDir(ModelStore::dir()).filePath("midas.onnx");
        const QDateTime when = QFileInfo(path).lastModified();
        placeFile("midas.onnx", "9876543210");               // same length
        {   // restore the mtime too, so the record still looks current
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.setFileTime(when, QFileDevice::FileModificationTime));
        }
        ModelStore::invalidate(Model::Midas);

        QVERIFY(ModelStore::state(Model::Midas) != State::Ready);
        QVERIFY2(ModelStore::path(Model::Midas).isEmpty(),
                 "a file that does not match this build must never be loaded");
    }

    /* A file with no sidecar record at all is hashed once, and since it cannot match the
       catalog it is refused -- not trusted just because it has the right name. */
    void unknownFileIsRefused()
    {
        placeFile("sam2_encoder.onnx", "no provenance at all");
        ModelStore::invalidate(Model::Sam2Encoder);

        QVERIFY(ModelStore::state(Model::Sam2Encoder) != State::Ready);
        QVERIFY(ModelStore::path(Model::Sam2Encoder).isEmpty());
    }

    /* Hashing a file with no record must WRITE that record, so the next state() is the
       cheap path rather than re-reading 200 MB on every click. */
    void hashingWritesTheSidecar()
    {
        const QByteArray body = "records itself";
        placeFile("lama.onnx", body);
        ModelStore::invalidate(Model::Lama);
        ModelStore::state(Model::Lama);                     // forces the hash

        QFile f(QDir(ModelStore::dir()).filePath("models.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        QVERIFY(root.contains("lama.onnx"));
        QCOMPARE(root.value("lama.onnx").toObject().value("sha256").toString(), sha(body));
    }

    /* A file no catalog entry claims -- what a newer Winnow leaves behind when it stops
       using a model. Listed for the user, never auto-deleted. */
    void orphansAreDetected()
    {
        placeFile("retired_model.onnx", "left over from an older Winnow");
        placeFile("u2net.onnx", "a real catalog entry");

        const QVector<QString> found = ModelStore::orphans();
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first(), QString("retired_model.onnx"));
        QCOMPARE(ModelStore::orphanBytes(),
                 qint64(QByteArray("left over from an older Winnow").size()));
    }

    void removeDeletesFileAndSidecarEntry()
    {
        const QByteArray body = "to be removed";
        placeFile("migan.onnx", body);
        ModelStore::recordDownloaded(Model::Migan, sha(body));
        QVERIFY(QFile::exists(QDir(ModelStore::dir()).filePath("migan.onnx")));

        QVERIFY(ModelStore::remove(Model::Migan));
        QVERIFY(!QFile::exists(QDir(ModelStore::dir()).filePath("migan.onnx")));
        QCOMPARE(ModelStore::state(Model::Migan), State::Missing);

        QFile f(QDir(ModelStore::dir()).filePath("models.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!QJsonDocument::fromJson(f.readAll()).object().contains("migan.onnx"));
    }

    /* A bundled model lives inside the app -- not ours to delete. */
    void bundledModelCannotBeRemoved()
    {
        QVERIFY(!ModelStore::remove(Model::Pmrid));
    }

    /* ensure() must never open a dialog or reach the network from a test run. */
    void ensureIsSilentUnderTestMode()
    {
        QVERIFY(!ModelStore::ensure(QVector<Model>() << Model::U2Net, nullptr));
    }

    void sweepPartialsRemovesOnlyPartFiles()
    {
        placeFile("u2net.onnx.part", "interrupted download");
        placeFile("u2net.onnx", "a finished file");
        ModelStore::sweepPartials();

        QVERIFY(!QFile::exists(QDir(ModelStore::dir()).filePath("u2net.onnx.part")));
        QVERIFY(QFile::exists(QDir(ModelStore::dir()).filePath("u2net.onnx")));
    }
};

QTEST_MAIN(tst_modelstore)
#include "tst_modelstore.moc"
