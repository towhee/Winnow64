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
    void setItemListWritesAnRdfBag();
    void setItemListRoundTripsSpecialCharacters();
    void setItemListOnEmptyRemovesTheProperty();
    void setItemRefusesAListProperty();
    void setItemListReplacesAnAttributeFormSubject();
    void hierarchicalSubjectWritesUnderTheLrNamespace();

private:
    /* Write text to a .xmp in the temp dir and hand back an OPEN file, which is what
       both Xmp constructors take. */
    QFile *sidecar(const char *text, const QString &name);

    /* Serialise xmp, re-read it as a fresh document, and hand back the reloaded one.
       A write is only proven by a READ THROUGH THE PARSER: the shape of the printed
       text is not the contract, what getItemList makes of it is. */
    Xmp *reload(Xmp &xmp, const QString &name);

    QTemporaryDir tmp;
    QList<QFile *> open;
    QList<Xmp *> reloaded;
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

Xmp *tst_xmpkeywords::reload(Xmp &xmp, const QString &name)
{
    const QString path = tmp.filePath(name);
    {
        QFile w(path);
        if (!w.open(QIODevice::WriteOnly)) return nullptr;
        w.write(xmp.docToByteArray());
        w.close();
    }
    QFile *f = new QFile(path);
    if (!f->open(QIODevice::ReadOnly)) { delete f; return nullptr; }
    open.append(f);
    Xmp *out = new Xmp(*f, 0);
    reloaded.append(out);
    return out;
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

void tst_xmpkeywords::setItemListWritesAnRdfBag()
{
    QFile *f = sidecar(kSidecarNoKeywords, "bagwrite.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    const QStringList kw = {"Heron", "Neck Point", "BIF"};
    QVERIFY(xmp.setItemList("subject", kw));

    Xmp *back = reload(xmp, "bagwrite2.xmp");
    QVERIFY(back);
    QVERIFY(back->isValid);
    /* ORDER IS PRESERVED. dc:subject is an unordered Bag by XMP's rules, but the
       write-back contract sorts before writing so a no-op rewrite is byte-identical,
       and that is only worth anything if the order survives the round trip. */
    QCOMPARE(back->getItemList("subject"), kw);

    // the scalar that was already there is undisturbed
    QCOMPARE(back->getItem("Rating"), QString("1"));
}

void tst_xmpkeywords::setItemListRoundTripsSpecialCharacters()
{
/*
    Nothing in Xmp escapes anything, and it does not need to: docToQString prints
    through rapidxml::print, whose copy_and_expand_chars turns & < > " ' into entities,
    and parse<0> translates them back. This case exists because that is a property of
    the PRINTER, not of the writer -- one parse_no_entity_translation flag, or a revival
    of the commented-out walk() serialiser, would silently start corrupting sidecars.
    "F&B" is a real keyword in a real user vocabulary, not a contrived one.
*/
    QFile *f = sidecar(kSidecarNoKeywords, "escape.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    const QStringList kw = {
        "F&B",                  // ampersand: the one that actually occurs
        "<angle>",
        "quote\"d",
        "apostrophe's",
        QString::fromUtf8("na\xC3\xAFve"),          // naive, with a diaeresis
        QString::fromUtf8("\xE3\x82\xB5\xE3\x82\xAE")   // sagi, Japanese for heron
    };
    QVERIFY(xmp.setItemList("subject", kw));

    Xmp *back = reload(xmp, "escape2.xmp");
    QVERIFY(back);
    QVERIFY(back->isValid);
    QCOMPARE(back->getItemList("subject"), kw);
}

void tst_xmpkeywords::setItemListOnEmptyRemovesTheProperty()
{
/*
    An empty list means the property is ABSENT, not present-and-empty.
    Metadata::parseSidecar reads keywords deliberately unguarded on isEmpty so that
    removing every keyword in another application syncs to Winnow; writing an empty Bag
    instead of removing the property would leave a file that reads back as "no keywords"
    only by accident, and would differ from what every other application writes.
*/
    QFile *f = sidecar(kSidecar, "clear.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);
    QCOMPARE(xmp.getItemList("subject").size(), 3);

    QVERIFY(xmp.setItemList("subject", QStringList()));

    const QString text = xmp.docToQString();
    QVERIFY2(!text.contains("dc:subject"), "the property must be gone, not emptied");

    Xmp *back = reload(xmp, "clear2.xmp");
    QVERIFY(back);
    QVERIFY(back->isValid);
    QVERIFY(back->getItemList("subject").isEmpty());
    // the OTHER keyword property is untouched -- clearing one must not clear both
    QCOMPARE(back->getItemList("hierarchicalsubject").size(), 3);
}

void tst_xmpkeywords::setItemRefusesAListProperty()
{
/*
    THE REGRESSION THAT MATTERS. setItem removes the existing element before deciding
    what to write, and it has no List branch, so calling it on dc:subject used to delete
    every keyword and put nothing back -- silently, with the file still valid. The guard
    has to refuse BEFORE the removal, which is what the second half of this case checks.
*/
    QFile *f = sidecar(kSidecar, "refuse.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    QVERIFY2(!xmp.setItem("subject", "Heron"), "setItem must refuse a list property");
    QVERIFY2(!xmp.setItem("hierarchicalsubject", "A|B"), "and the other one");

    QCOMPARE(xmp.getItemList("subject").size(), 3);
    QCOMPARE(xmp.getItemList("hierarchicalsubject").size(), 3);

    Xmp *back = reload(xmp, "refuse2.xmp");
    QVERIFY(back);
    QCOMPARE(back->getItemList("subject").size(), 3);
}

void tst_xmpkeywords::setItemListReplacesAnAttributeFormSubject()
{
/*
    An application that writes a single keyword may put it on rdf:Description as an
    attribute rather than as a Bag. Both forms are the same property, so a write must
    remove whichever one it finds -- otherwise the file ends up carrying dc:subject
    twice and which one wins is down to the reader.
*/
    static const char *kAttrSubject =
        "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
        "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Test\">\n"
        " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
        "  <rdf:Description rdf:about=\"\"\n"
        "    xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
        "   dc:subject=\"Old\"/>\n"
        " </rdf:RDF>\n"
        "</x:xmpmeta>\n"
        "<?xpacket end=\"w\"?>\n";

    QFile *f = sidecar(kAttrSubject, "attr.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);

    QVERIFY(xmp.setItemList("subject", QStringList() << "New"));

    const QString text = xmp.docToQString();
    QVERIFY2(!text.contains("\"Old\""), "the attribute form must be removed, not left");

    Xmp *back = reload(xmp, "attr2.xmp");
    QVERIFY(back);
    QVERIFY(back->isValid);
    QCOMPARE(back->getItemList("subject"), QStringList() << "New");
}

void tst_xmpkeywords::hierarchicalSubjectWritesUnderTheLrNamespace()
{
/*
    lr: is the one namespace no fixture necessarily declares, because a file with no
    hierarchy has no reason to. includeSchemaNamespace has to add it, or the document
    prints an undeclared prefix and the next parse rejects it.
*/
    QFile *f = sidecar(kSidecarNoKeywords, "lr.xmp");
    QVERIFY(f);
    Xmp xmp(*f, 0);
    QVERIFY(xmp.isValid);
    QVERIFY2(!xmp.docToQString().contains("xmlns:lr"), "fixture must not predeclare lr");

    const QStringList paths = {"Fauna|Bird|Heron", "Location|Canada|BC|Neck Point"};
    QVERIFY(xmp.setItemList("hierarchicalsubject", paths));

    QVERIFY(xmp.docToQString().contains("xmlns:lr"));

    Xmp *back = reload(xmp, "lr2.xmp");
    QVERIFY(back);
    QVERIFY2(back->isValid, "an undeclared lr: prefix would fail the reparse here");
    QCOMPARE(back->getItemList("hierarchicalsubject"), paths);
    // the separators are the whole point of the property and must survive untouched
    QVERIFY(back->getItemList("hierarchicalsubject").at(0).contains('|'));
}

QTEST_MAIN(tst_xmpkeywords)
#include "tst_xmpkeywords.moc"
