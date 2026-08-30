#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "Metadata/xmp.h"
#include "Metadata/xmpapply.h"

/*
    Keywords out of an XMP sidecar.

    WHY THIS EXISTS. Winnow read dc:subject in exactly two places -- the two JPEG parsers
    -- so a keyworded NEF, CR2, CR3, ARW, DNG, TIFF or RW2 arrived with no keywords at
    all, and Metadata::parseSidecar (the ONLY route by which a raw file's keywords can
    reach the model, because Lightroom writes a .xmp beside a raw rather than into it)
    did not read them either. lr:hierarchicalSubject was not in the vocabulary at all.

    The fixture below is the shape Lightroom actually writes, taken from a real sidecar:
    both properties are an rdf:Bag of rdf:li, the hierarchical one carrying full paths
    with '|' separators, the flat one carrying the same keywords as leaf names.
*/

static const char *kSidecar =
    "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
    "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Test\">\n"
    " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
    "  <rdf:Description rdf:about=\"\"\n"
    "    xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"\n"
    "    xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
    "    xmlns:lr=\"http://ns.adobe.com/lightroom/1.0/\"\n"
    "   xmp:Rating=\"3\"\n"
    "   xmp:Label=\"Blue\">\n"
    "   <dc:subject>\n"
    "    <rdf:Bag>\n"
    "     <rdf:li>BIF</rdf:li>\n"
    "     <rdf:li>Cormorant</rdf:li>\n"
    "     <rdf:li>Neck Point</rdf:li>\n"
    "    </rdf:Bag>\n"
    "   </dc:subject>\n"
    "   <lr:hierarchicalSubject>\n"
    "    <rdf:Bag>\n"
    "     <rdf:li>Category|BIF</rdf:li>\n"
    "     <rdf:li>Fauna|Bird|Cormorant</rdf:li>\n"
    "     <rdf:li>Location|Canada|BC|Neck Point</rdf:li>\n"
    "    </rdf:Bag>\n"
    "   </lr:hierarchicalSubject>\n"
    "  </rdf:Description>\n"
    " </rdf:RDF>\n"
    "</x:xmpmeta>\n"
    "<?xpacket end=\"w\"?>\n";

/* The same document with no keyword properties at all -- the common case, and the one
   that must stay quiet rather than warn or invent an entry. */
static const char *kSidecarNoKeywords =
    "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
    "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Test\">\n"
    " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
    "  <rdf:Description rdf:about=\"\"\n"
    "    xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"\n"
    "   xmp:Rating=\"1\"/>\n"
    " </rdf:RDF>\n"
    "</x:xmpmeta>\n"
    "<?xpacket end=\"w\"?>\n";

class tst_xmpkeywords : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void flatKeywordsAreRead();
    void hierarchicalKeywordsAreRead();
    void absentKeywordsYieldEmptyLists();
    void applyXmpFillsBothKeywordLists();
    void applyXmpSnapshotsOriginals();

private:
    /* Write text to a .xmp in the temp dir and hand back an OPEN file, which is what
       both Xmp constructors take. */
    QFile *sidecar(const char *text, const QString &name);

    QTemporaryDir tmp;
    QList<QFile *> open;
};

void tst_xmpkeywords::initTestCase()
{
    QVERIFY(tmp.isValid());
}

QFile *tst_xmpkeywords::sidecar(const char *text, const QString &name)
{
    const QString path = tmp.filePath(name);
    {
        QFile w(path);
        if (!w.open(QIODevice::WriteOnly)) return nullptr;
        w.write(text);
        w.close();
    }
    QFile *f = new QFile(path);
    if (!f->open(QIODevice::ReadOnly)) { delete f; return nullptr; }
    open.append(f);
    return f;
}

void tst_xmpkeywords::flatKeywordsAreRead()
{
    QFile *f = sidecar(kSidecar, "flat.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);                 // the sidecar constructor: whole file is the packet
    QVERIFY(xmp.isValid);

    const QStringList kw = xmp.getItemList("subject");
    QCOMPARE(kw.size(), 3);
    QVERIFY(kw.contains("BIF"));
    QVERIFY(kw.contains("Cormorant"));
    QVERIFY(kw.contains("Neck Point"));   // a keyword with a space stays one entry
}

void tst_xmpkeywords::hierarchicalKeywordsAreRead()
{
    QFile *f = sidecar(kSidecar, "hier.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    const QStringList paths = xmp.getItemList("hierarchicalsubject");
    QCOMPARE(paths.size(), 3);
    QVERIFY(paths.contains("Fauna|Bird|Cormorant"));
    /* The ancestor names ("Location", "Canada", "BC") exist ONLY in this form -- the
       flat list has just the leaf. That is what makes searching for a parent keyword
       possible, so the separators must survive the read untouched. */
    QVERIFY(paths.contains("Location|Canada|BC|Neck Point"));
}

void tst_xmpkeywords::absentKeywordsYieldEmptyLists()
{
    QFile *f = sidecar(kSidecarNoKeywords, "bare.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    QVERIFY(xmp.getItemList("subject").isEmpty());
    QVERIFY(xmp.getItemList("hierarchicalsubject").isEmpty());
    // the document is otherwise fine and still readable
    QCOMPARE(xmp.getItem("Rating"), QString("1"));
}

void tst_xmpkeywords::applyXmpFillsBothKeywordLists()
{
    QFile *f = sidecar(kSidecar, "apply.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    ImageMetadata m;
    MetadataParameters p;
    applyXmp(xmp, m, p);

    /* The regression this whole change is about: before the shared helper, these two
       lines ran for JPEG and PNG only, so every raw format reached here empty. */
    QCOMPARE(m.keywords.size(), 3);
    QCOMPARE(m.keywordPaths.size(), 3);
    QVERIFY(m.keywords.contains("Cormorant"));
    QVERIFY(m.keywordPaths.contains("Category|BIF"));

    // the scalars the helper also carries, so the collapse did not drop any of them
    QCOMPARE(m.rating, QString("3"));
    QCOMPARE(m.label, QString("Blue"));
}

void tst_xmpkeywords::applyXmpSnapshotsOriginals()
{
/*
    Metadata::writeXMP decides what to write by comparing each field with its underscore
    shadow, so the helper must still take that snapshot. If it stopped, every field would
    look unedited forever and no rating or label change would ever be saved.
*/
    QFile *f = sidecar(kSidecar, "snapshot.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    ImageMetadata m;
    MetadataParameters p;
    applyXmp(xmp, m, p);

    QCOMPARE(m._rating, m.rating);
    QCOMPARE(m._label, m.label);
    QCOMPARE(m._title, m.title);
    QCOMPARE(m._creator, m.creator);
    QCOMPARE(m._copyright, m.copyright);
    QCOMPARE(m._email, m.email);
    QCOMPARE(m._url, m.url);
}

QTEST_MAIN(tst_xmpkeywords)
#include "tst_xmpkeywords.moc"
