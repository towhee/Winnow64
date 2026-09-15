/*
    LutStore -- finding installed looks, and handing out the one that was chosen.

    WHY THIS TEST EXISTS. The parsers are covered by tst_lutparse; what is only here is
    the part that touches the filesystem, and it carries two properties nothing else can
    check:

      o THE KEY. A look is stored in a sidecar by its path under the Looks folder
        ("Fuji/Provia"), not by its filename and not by its .cube TITLE. Get the
        derivation wrong and every stored look stops resolving after an upgrade.
      o NO PATH TRAVERSAL. That key comes back out of a sidecar, which is user-editable
        text. lut() resolves it by LOOKUP against the index; a version that joined it onto
        a root would hand "../../../etc/passwd" to QFile. This is the test that says so.

    The singleton indexes once, so the fixture is written BEFORE anything touches the
    store. QStandardPaths test mode keeps it out of the real application folder.

    DELIBERATELY NOT COVERED: that scan() CREATES the Looks folder when it is missing.
    The store indexes once per process, so by the time a case could delete the folder the
    sweep has already run, and any test written here would end up calling mkpath itself
    and then asserting the folder exists -- a tautology that reads like coverage. The
    behaviour is one line in scan() and is verified by using the app.
*/

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <algorithm>

#include "Develop/lutstore.h"

class TstLutStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void keysAreThePathUnderTheRoot();
    void aLookResolvesAndParses();
    void junkFilesAreNotIndexed();
    void unknownKeysResolveToNothing();
    void keysCannotEscapeTheRoot();

private:
    QString looksDir;
};

/* A minimal valid .cube: the identity at size 2. */
static QByteArray identityCube(const char *title)
{
    QByteArray out;
    if (title) out += QByteArray("TITLE \"") + title + "\"\n";
    out += "LUT_3D_SIZE 2\n"
           "0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1\n";
    return out;
}

static bool writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(bytes) == bytes.size();
}

void TstLutStore::initTestCase()
{
    /* Test mode BEFORE the store is touched: AppDataLocation then points at a throwaway
       folder instead of the user's real Winnow data. */
    QStandardPaths::setTestModeEnabled(true);
    const QStringList roots = LutStore::roots();
    QVERIFY2(!roots.isEmpty(), "the store reported no roots at all");
    looksDir = roots.first();
    /* Start from empty so a previous run cannot leak into this one. */
    QDir(looksDir).removeRecursively();
    QVERIFY(QDir().mkpath(looksDir));

    QVERIFY(writeFile(looksDir + "/Fuji/Provia.cube", identityCube("PROVIA look")));
    QVERIFY(writeFile(looksDir + "/Kodak/Portra 400.CUBE", identityCube(nullptr)));
    QVERIFY(writeFile(looksDir + "/Plain.cube", identityCube(nullptr)));

    /* A valid identity Hald at level 2 (8x8 image, 4-cube). */
    {
        QImage img(8, 8, QImage::Format_RGB888);
        int k = 0;
        for (int y = 0; y < 8; ++y) {
            uchar *p = img.scanLine(y);
            for (int x = 0; x < 8; ++x, ++k) {
                p[x * 3 + 0] = uchar((k % 4) * 85);
                p[x * 3 + 1] = uchar(((k / 4) % 4) * 85);
                p[x * 3 + 2] = uchar((k / 16) * 85);
            }
        }
        QVERIFY(img.save(looksDir + "/Hald.png"));
    }

    /* Junk that must NOT be indexed. */
    QVERIFY(writeFile(looksDir + "/notalut.cube", "hello, I am not a LUT\n"));
    {
        QImage wrong(10, 20, QImage::Format_RGB888);      // not square
        wrong.fill(Qt::black);
        QVERIFY(wrong.save(looksDir + "/notsquare.png"));
    }

    LutStore::instance().ensureIndex();
    QTRY_VERIFY_WITH_TIMEOUT(LutStore::instance().indexReady(), 30000);
}

void TstLutStore::keysAreThePathUnderTheRoot()
{
    const QList<LutStore::Entry> e = LutStore::instance().entries();
    QStringList keys;
    for (const LutStore::Entry &x : e) keys << x.key;

    QVERIFY2(keys.contains("Fuji/Provia"),
             qPrintable("a subfolder did not become part of the key: " + keys.join(", ")));
    /* Extension dropped whatever its case: .CUBE keys the same as .cube. */
    QVERIFY(keys.contains("Kodak/Portra 400"));
    QVERIFY(keys.contains("Plain"));
    QVERIFY(keys.contains("Hald"));

    /* The label is the last segment -- what the combo shows. */
    for (const LutStore::Entry &x : e)
        if (x.key == "Fuji/Provia") {
            QCOMPARE(x.label, QString("Provia"));
            /* The TITLE is carried for the tooltip, and is NOT the key. */
            QCOMPARE(x.title, QString("PROVIA look"));
        }

    /* Sorted, so the combo does not shuffle between runs. */
    QStringList sorted = keys;
    std::sort(sorted.begin(), sorted.end(),
              [](const QString &a, const QString &b) {
                  return a.localeAwareCompare(b) < 0;
              });
    QCOMPARE(keys, sorted);
}

void TstLutStore::aLookResolvesAndParses()
{
    const auto t = LutStore::instance().lut("Fuji/Provia");
    QVERIFY2(t != nullptr, "an indexed look did not resolve");
    QVERIFY(t->isValid());
    QCOMPARE(t->size, 2);

    /* The same key twice returns the SAME table: parsed once, shared per render. */
    const auto again = LutStore::instance().lut("Fuji/Provia");
    QCOMPARE(again.get(), t.get());

    /* And the Hald resolves through the other reader. */
    const auto h = LutStore::instance().lut("Hald");
    QVERIFY(h != nullptr);
    QCOMPARE(h->size, 4);
}

void TstLutStore::junkFilesAreNotIndexed()
{
    const QList<LutStore::Entry> e = LutStore::instance().entries();
    for (const LutStore::Entry &x : e) {
        QVERIFY2(x.key != "notalut", "a file with no LUT keyword was offered as a look");
        QVERIFY2(x.key != "notsquare", "a non-square PNG was offered as a HaldCLUT");
    }
}

void TstLutStore::unknownKeysResolveToNothing()
{
    /* A look the user deleted, or a sidecar from another machine. nullptr means "render
       without a look and say so", never a silent substitute. */
    QVERIFY(LutStore::instance().lut("No Such Look") == nullptr);
    QVERIFY(LutStore::instance().lut(QString()) == nullptr);
    QVERIFY(LutStore::instance().lut("Fuji/Nope") == nullptr);
}

void TstLutStore::keysCannotEscapeTheRoot()
{
    /*
        THE SECURITY PROPERTY. The key is user-editable text from a sidecar. Because
        lut() only ever returns a path the scan itself found, none of these can name a
        file -- not even one that certainly exists.
    */
    const QStringList attacks = {
        "../../../../../../etc/passwd",
        "..",
        "../" + QFileInfo(looksDir).fileName() + "/Plain",
        "/etc/passwd",
        "Fuji/../../../etc/hosts",
    };
    for (const QString &a : attacks)
        QVERIFY2(LutStore::instance().lut(a) == nullptr,
                 qPrintable("a key escaped the Looks folder: " + a));
}

QTEST_MAIN(TstLutStore)
#include "tst_lutstore.moc"
