#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "Metadata/versions.h"
#include "Metadata/xmp.h"
#include "Utilities/fileops.h"
#include "Utilities/versionkey.h"

/*
    VERSIONS (VIRTUAL COPIES) -- THE STORAGE CONTRACT. See Metadata/versions.h.

    Versions live in one sidecar attribute, winnow:Versions, beside the master's
    winnow:Develop. What must hold, because each failure loses the user's work
    silently:

      o A round trip keeps every field, including fields this build does not know.
      o Ids are never reused, even after a delete and a reload.
      o A read-modify-write leaves the rest of the sidecar -- the master's recipe,
        its rating, its keywords -- exactly as it was.
      o A corrupt or newer-schema set is never overwritten.
      o Another writer (standing in for an older Winnow) keeps winnow:Versions.
      o A version key is refused as a source path.
*/

static const char *kSidecar =
    "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
    "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Test\">\n"
    " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
    "  <rdf:Description rdf:about=\"\"\n"
    "    xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"\n"
    "    xmlns:winnow=\"http://winnow.ca/1.0/\"\n"
    "    xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
    "   xmp:Rating=\"3\"\n"
    "   winnow:Develop=\"MASTERBLOB\">\n"
    "   <dc:subject>\n"
    "    <rdf:Bag>\n"
    "     <rdf:li>heron</rdf:li>\n"
    "    </rdf:Bag>\n"
    "   </dc:subject>\n"
    "  </rdf:Description>\n"
    " </rdf:RDF>\n"
    "</x:xmpmeta>\n"
    "<?xpacket end=\"w\"?>\n";

class tst_versions : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void jsonRoundTripKeepsEveryField();
    void idsAreNeverReused();
    void nextIdIsRepairedFromTheIds();
    void corruptAndNewerAreReported();
    void updateKeepsTheRestOfTheSidecar();
    void updateCreatesASidecarWhenThereIsNone();
    void updateRefusesACorruptSet();
    void updateRefusesANewerSchema();
    void emptyingTheSetKeepsTheHighWaterMark();
    void anotherWriterKeepsTheVersions();
    void aVersionKeyIsRefused();
    void summariesKeyTheRecipe();

private:
    QTemporaryDir dir;
    QString image;              // an image with the fixture sidecar
    QString sidecar;
    QString readSidecar() const;
};

void tst_versions::init()
{
    QVERIFY(dir.isValid());
    static int n = 0;
    image = dir.filePath(QString("IMG_%1.NEF").arg(++n));
    sidecar = dir.filePath(QString("IMG_%1.xmp").arg(n));
    QFile img(image);
    QVERIFY(img.open(QIODevice::WriteOnly));
    img.write("not really a raw");
    img.close();
    QFile sc(sidecar);
    QVERIFY(sc.open(QIODevice::WriteOnly));
    sc.write(kSidecar);
    sc.close();
    QCOMPARE(FileOps::sidecarPath(image), sidecar);
}

QString tst_versions::readSidecar() const
{
    QFile f(sidecar);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    // the writer pretty-prints attributes as  name = "value"
    return QString::fromUtf8(f.readAll()).replace(" = \"", "=\"");
}

void tst_versions::jsonRoundTripKeepsEveryField()
{
    VersionSet set;
    ImageVersion a;
    a.name = "B&W \"moody\"";
    a.rating = 4;
    a.label = "Red";
    a.pick = "Picked";
    a.develop = "RECIPE-A";
    a.previewKey = "abc123";
    a.preview = QByteArray("JPEGBYTES").toBase64();
    a.extra.insert("futureField", 42);
    set.add(a);
    ImageVersion b;
    b.develop = "RECIPE-B";
    set.add(b);
    set.extra.insert("futureDoc", "kept");

    VersionSet::Status st;
    const VersionSet back = VersionSet::fromBase64(set.toBase64(), VersionSet::Detail::Full,
                                                   &st);
    QCOMPARE(st, VersionSet::Status::Ok);
    QCOMPARE(back.versions.size(), 2);
    QCOMPARE(back.nextId, 3);
    const ImageVersion *ra = back.find(1);
    QVERIFY(ra);
    QCOMPARE(ra->name, a.name);
    QCOMPARE(ra->rating, 4);
    QCOMPARE(ra->label, QString("Red"));
    QCOMPARE(ra->pick, QString("Picked"));
    QCOMPARE(ra->develop, QString("RECIPE-A"));
    QCOMPARE(ra->previewKey, QString("abc123"));
    QCOMPARE(QByteArray::fromBase64(ra->preview), QByteArray("JPEGBYTES"));
    QCOMPARE(ra->extra.value("futureField").toInt(), 42);
    QVERIFY(ra->created.isValid());
    QCOMPARE(back.extra.value("futureDoc").toString(), QString("kept"));
    QCOMPARE(back.find(2)->displayName(), QString("v2"));

    // Recipe detail drops only the preview bytes
    const VersionSet recipe = VersionSet::fromBase64(set.toBase64(),
                                                     VersionSet::Detail::Recipe);
    QCOMPARE(recipe.find(1)->develop, QString("RECIPE-A"));
    QVERIFY(recipe.find(1)->preview.isEmpty());
}

void tst_versions::idsAreNeverReused()
{
    QVERIFY(Versions::update(image, [](VersionSet &s) {
        s.add(ImageVersion());
        s.add(ImageVersion());
    }));
    QVERIFY(Versions::update(image, [](VersionSet &s) {
        QVERIFY(s.remove(1));
        QVERIFY(s.remove(2));
    }));
    int newId = 0;
    QVERIFY(Versions::update(image, [&](VersionSet &s) {
        newId = s.add(ImageVersion()).id;
        // an edit that tries to wind nextId back is overruled
        s.nextId = 1;
    }));
    QCOMPARE(newId, 3);
    const VersionSet set = Versions::read(image);
    QCOMPARE(set.versions.size(), 1);
    QCOMPARE(set.versions.first().id, 3);
    QVERIFY(set.nextId >= 4);
}

void tst_versions::nextIdIsRepairedFromTheIds()
{
    // a hand-edited document whose nextId is behind its ids
    const QByteArray json =
        R"({"schema":1,"nextId":1,"versions":[{"id":5},{"id":5},{"id":0},{"id":2}]})";
    const VersionSet set = VersionSet::fromBase64(QString::fromLatin1(json.toBase64()));
    QCOMPARE(set.versions.size(), 2);           // duplicate and id 0 dropped
    QCOMPARE(set.nextId, 6);
}

void tst_versions::corruptAndNewerAreReported()
{
    VersionSet::Status st;
    VersionSet::fromBase64("", VersionSet::Detail::Full, &st);
    QCOMPARE(st, VersionSet::Status::Absent);
    VersionSet::fromBase64("!!!not base64!!!", VersionSet::Detail::Full, &st);
    QCOMPARE(st, VersionSet::Status::Corrupt);
    VersionSet::fromBase64(QString::fromLatin1(QByteArray("[1,2]").toBase64()),
                           VersionSet::Detail::Full, &st);
    QCOMPARE(st, VersionSet::Status::Corrupt);
    const QByteArray newer = R"({"schema":99,"nextId":2,"versions":[{"id":1}]})";
    const VersionSet set = VersionSet::fromBase64(QString::fromLatin1(newer.toBase64()),
                                                  VersionSet::Detail::Full, &st);
    QCOMPARE(st, VersionSet::Status::Newer);
    QCOMPARE(set.versions.size(), 1);           // still readable
}

void tst_versions::updateKeepsTheRestOfTheSidecar()
{
    QVERIFY(Versions::update(image, [](VersionSet &s) {
        ImageVersion v;
        v.name = "Crop";
        v.develop = "RECIPE";
        v.rating = 2;
        s.add(v);
    }));
    const QString text = readSidecar();
    QVERIFY(text.contains("winnow:Versions="));
    QVERIFY(text.contains("winnow:Develop=\"MASTERBLOB\""));
    QVERIFY(text.contains("xmp:Rating=\"3\""));
    QVERIFY(text.contains("heron"));

    const VersionSet set = Versions::read(image);
    QCOMPARE(set.versions.size(), 1);
    QCOMPARE(set.versions.first().name, QString("Crop"));
    QCOMPARE(Versions::readRecipe(image, 1), QString("RECIPE"));
    QVERIFY(Versions::readRecipe(image, 2).isEmpty());
    QCOMPARE(Versions::readVersion(image, 1).rating, 2);
    QCOMPARE(Versions::readVersion(image, 9).id, 0);
}

void tst_versions::updateCreatesASidecarWhenThereIsNone()
{
    QVERIFY(QFile::remove(sidecar));
    QVERIFY(Versions::update(image, [](VersionSet &s) { s.add(ImageVersion()); }));
    QVERIFY(QFile::exists(sidecar));
    QCOMPARE(Versions::read(image).versions.size(), 1);
}

void tst_versions::updateRefusesACorruptSet()
{
    {
        QFile f(sidecar);
        QVERIFY(f.open(QIODevice::ReadWrite));
        Xmp xmp(f, 0);
        QVERIFY(xmp.setItem("versions", "!!!garbage!!!"));
        QVERIFY(xmp.writeSidecar(f));
    }
    bool called = false;
    QVERIFY(!Versions::update(image, [&](VersionSet &) { called = true; }));
    QVERIFY(!called);
    QVERIFY(readSidecar().contains("!!!garbage!!!"));
}

void tst_versions::updateRefusesANewerSchema()
{
    const QByteArray newer = R"({"schema":99,"nextId":2,"versions":[{"id":1,"x":1}]})";
    const QByteArray b64 = newer.toBase64();
    {
        QFile f(sidecar);
        QVERIFY(f.open(QIODevice::ReadWrite));
        Xmp xmp(f, 0);
        QVERIFY(xmp.setItem("versions", b64));
        QVERIFY(xmp.writeSidecar(f));
    }
    QVERIFY(!Versions::update(image, [](VersionSet &s) { s.add(ImageVersion()); }));
    QVERIFY(readSidecar().contains(QString::fromLatin1(b64)));
}

void tst_versions::emptyingTheSetKeepsTheHighWaterMark()
{
    // an edit that issues no id writes nothing
    QVERIFY(Versions::update(image, [](VersionSet &) {}));
    QVERIFY(!readSidecar().contains("winnow:Versions="));

    /* Deleting the last version keeps the attribute as {nextId, versions:[]}, so the
       next version is v2, not a second v1 that would inherit v1's cached state. */
    QVERIFY(Versions::update(image, [](VersionSet &s) { s.add(ImageVersion()); }));
    QVERIFY(Versions::update(image, [](VersionSet &s) { s.remove(1); }));
    QVERIFY(readSidecar().contains("winnow:Versions="));
    QVERIFY(readSidecar().contains("winnow:Develop=\"MASTERBLOB\""));
    const VersionSet empty = Versions::read(image);
    QVERIFY(empty.isEmpty());
    QCOMPARE(empty.nextId, 2);
    int id = 0;
    QVERIFY(Versions::update(image, [&](VersionSet &s) { id = s.add(ImageVersion()).id; }));
    QCOMPARE(id, 2);
}

void tst_versions::anotherWriterKeepsTheVersions()
{
    /* Stands in for an older Winnow (or writeXMP) changing the master's rating: it
       re-serialises the parsed document, so an attribute it never touches survives. */
    QVERIFY(Versions::update(image, [](VersionSet &s) {
        ImageVersion v;
        v.develop = "RECIPE";
        s.add(v);
    }));
    const QString before = Versions::readRecipe(image, 1);
    {
        QFile f(sidecar);
        QVERIFY(f.open(QIODevice::ReadWrite));
        Xmp xmp(f, 0);
        QVERIFY(xmp.setItem("rating", "5"));
        QVERIFY(xmp.writeSidecar(f));
    }
    QVERIFY(readSidecar().contains("xmp:Rating=\"5\""));
    QCOMPARE(Versions::readRecipe(image, 1), before);
    QCOMPARE(before, QString("RECIPE"));
}

void tst_versions::aVersionKeyIsRefused()
{
    const QString key = VersionKey::make(image, 1);
    QVERIFY(!Versions::update(key, [](VersionSet &s) { s.add(ImageVersion()); }));
    QVERIFY(Versions::read(key).isEmpty());
    QVERIFY(!readSidecar().contains("winnow:Versions="));
}

void tst_versions::summariesKeyTheRecipe()
{
    VersionSet set;
    ImageVersion a;
    a.develop = "R";
    a.name = "A";
    a.rating = 5;
    set.add(a);
    set.add(ImageVersion());                    // identity: not developed, no key
    const auto sums = Versions::summaries(set, [](const QString &r) { return "K" + r; });
    QCOMPARE(sums.size(), 2);
    QCOMPARE(sums.at(0).id, 1);
    QVERIFY(sums.at(0).developed);
    QCOMPARE(sums.at(0).devPreviewKey, QString("KR"));
    QCOMPARE(sums.at(0).rating, 5);
    QVERIFY(!sums.at(1).developed);
    QVERIFY(sums.at(1).devPreviewKey.isEmpty());
}

QTEST_GUILESS_MAIN(tst_versions)
#include "tst_versions.moc"
