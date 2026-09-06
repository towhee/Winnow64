#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "Metadata/xmp.h"
#include "Metadata/keywordpaths.h"

/*
    THE KEYWORD WRITE-BACK CONTRACT.

    Winnow reads keywords from two properties and writes them to the same two, so what
    matters is not the shape of either list on its own but that reading back what was
    written reproduces exactly what the user assigned. That round trip is the contract,
    and it is easy to break in ways nothing else notices: a leaf written into dc:subject
    that no path claims becomes a spurious ROOT keyword on the next read, and a path
    dropped from lr:hierarchicalSubject silently demotes its keyword to a bare name.

    WHAT IS WRITTEN, as MW::applyKeywordsToSelection composes it:

      dc:subject             the LEAF of every assigned path, de-duplicated
      lr:hierarchicalSubject every assigned path of depth 2 or more, whole

    A depth-1 path is written to dc:subject only. It carries no hierarchy, it is fewer
    bytes, and it is what an application that has never heard of lr: writes.

    These cases compose the two lists the way the app does and put them through the real
    Xmp writer and the real reader, so a change to either side has to keep the round trip
    intact rather than merely keep its own half self-consistent.
*/

static const char *kBareSidecar =
    "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
    "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Test\">\n"
    " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
    "  <rdf:Description rdf:about=\"\"\n"
    "    xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"\n"
    "   xmp:Rating=\"2\"/>\n"
    " </rdf:RDF>\n"
    "</x:xmpmeta>\n"
    "<?xpacket end=\"w\"?>\n";

class tst_keywordwrite : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void assignedPathsRoundTrip();
    void aSecondWriteIsByteIdentical();
    void clearingRemovesBothProperties();
    void twoKeywordsSharingALeafRoundTrip();
    void anOrphanLeafIsNotSwallowedByAPath();

private:
    /*  Compose the two properties exactly as MW::applyKeywordsToSelection does. Kept
        here rather than imported so this file states the contract independently: if the
        app's composition drifts from it, these cases fail rather than follow it. */
    struct Emitted { QStringList subject, hierarchical; };
    static Emitted emit_(QStringList paths)
    {
        paths.sort(Qt::CaseInsensitive);
        Emitted e;
        QSet<QString> seen;
        for (const QString &p : paths) {
            const QString leaf = keywordLeafOf(p);
            const QString fold = keywordFold(leaf);
            if (!fold.isEmpty() && !seen.contains(fold)) {
                seen.insert(fold);
                e.subject << leaf;
            }
            if (p.contains('|')) e.hierarchical << p;
        }
        return e;
    }

    QFile *sidecar(const char *text, const QString &name);
    Xmp *writeAndReload(const Emitted &e, const QString &name);

    QTemporaryDir tmp;
    QList<QFile *> open;
    QList<Xmp *> made;
};

void tst_keywordwrite::initTestCase()
{
    QVERIFY(tmp.isValid());
}

QFile *tst_keywordwrite::sidecar(const char *text, const QString &name)
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

Xmp *tst_keywordwrite::writeAndReload(const Emitted &e, const QString &name)
{
    QFile *f = sidecar(kBareSidecar, name + "-in.xmp");
    if (!f) return nullptr;
    Xmp xmp(*f, 0);
    if (!xmp.isValid) return nullptr;
    if (!xmp.setItemList("subject", e.subject)) return nullptr;
    if (!xmp.setItemList("hierarchicalsubject", e.hierarchical)) return nullptr;

    const QString outPath = tmp.filePath(name + "-out.xmp");
    {
        QFile w(outPath);
        if (!w.open(QIODevice::WriteOnly)) return nullptr;
        w.write(xmp.docToByteArray());
        w.close();
    }
    QFile *r = new QFile(outPath);
    if (!r->open(QIODevice::ReadOnly)) { delete r; return nullptr; }
    open.append(r);
    Xmp *back = new Xmp(*r, 0);
    made.append(back);
    return back;
}

void tst_keywordwrite::assignedPathsRoundTrip()
{
/*
    THE CONTRACT. A mixed set -- deep paths and a bare root -- must come back exactly,
    with no additions and no losses, through the real writer and the real reader.
*/
    const QStringList assigned = {
        "Fauna|Bird|Heron",
        "Location|Canada|BC|Neck Point",
        "Sooke"                                 // depth 1: dc:subject only
    };

    Xmp *back = writeAndReload(emit_(assigned), "roundtrip");
    QVERIFY(back);
    QVERIFY(back->isValid);

    QStringList got = keywordEffectivePaths(back->getItemList("subject"),
                                            back->getItemList("hierarchicalsubject"));
    got.sort(Qt::CaseInsensitive);
    QStringList want = assigned;
    want.sort(Qt::CaseInsensitive);
    QCOMPARE(got, want);

    /*  And the depth-1 keyword is in dc:subject WITHOUT a one-element hierarchical
        entry beside it -- writing "Sooke" as a path would be a hierarchy the user never
        created. */
    QVERIFY(back->getItemList("subject").contains("Sooke"));
    QVERIFY(!back->getItemList("hierarchicalsubject").contains("Sooke"));
}

void tst_keywordwrite::aSecondWriteIsByteIdentical()
{
/*
    IDEMPOTENCE, which is what stops a rating edit from slowly rewriting a keyword block.
    Reading a written file and writing it again unchanged must produce the same bytes --
    otherwise Metadata::writeXMP's change detection would see a difference on every pass
    and every unrelated edit would touch the user's keywords.
*/
    const QStringList assigned = {
        "Fauna|Bird|Heron", "Fauna|Bird|Eagle", "Location|Canada|BC", "Sooke"
    };

    Xmp *first = writeAndReload(emit_(assigned), "idem1");
    QVERIFY(first);
    const QStringList reread = keywordEffectivePaths(
        first->getItemList("subject"), first->getItemList("hierarchicalsubject"));

    Xmp *second = writeAndReload(emit_(reread), "idem2");
    QVERIFY(second);

    QCOMPARE(second->getItemList("subject"), first->getItemList("subject"));
    QCOMPARE(second->getItemList("hierarchicalsubject"),
             first->getItemList("hierarchicalsubject"));
}

void tst_keywordwrite::clearingRemovesBothProperties()
{
/*
    Taking the last keyword off an image must REMOVE the properties, not leave two empty
    Bags. Metadata::parseSidecar reads keywords unguarded on isEmpty precisely so that a
    removal elsewhere syncs; a present-but-empty property would read as "no keywords" only
    by accident, and differs from what every other application writes.
*/
    Xmp *back = writeAndReload(emit_(QStringList()), "cleared");
    QVERIFY(back);
    QVERIFY(back->isValid);
    QVERIFY(back->getItemList("subject").isEmpty());
    QVERIFY(back->getItemList("hierarchicalsubject").isEmpty());

    const QString text = back->docToQString();
    QVERIFY2(!text.contains("dc:subject"), "an empty list must remove the property");
    QVERIFY2(!text.contains("lr:hierarchicalSubject"), "and the other one");
}

void tst_keywordwrite::twoKeywordsSharingALeafRoundTrip()
{
/*
    The case path identity exists for, through the file. Both Vancouvers must survive,
    and dc:subject must carry the shared leaf ONCE -- writing it twice is a duplicate tag
    anywhere else the file is opened, and de-duplicating it must not cost either path.
*/
    const QStringList assigned = {
        "Location|Canada|BC|Vancouver",
        "Location|USA|WA|Vancouver"
    };

    const Emitted e = emit_(assigned);
    QCOMPARE(e.subject, QStringList{"Vancouver"});      // once, not twice
    QCOMPARE(e.hierarchical.size(), 2);

    Xmp *back = writeAndReload(e, "twovans");
    QVERIFY(back);

    QStringList got = keywordEffectivePaths(back->getItemList("subject"),
                                            back->getItemList("hierarchicalsubject"));
    got.sort(Qt::CaseInsensitive);
    QStringList want = assigned;
    want.sort(Qt::CaseInsensitive);
    QCOMPARE(got, want);
    QVERIFY2(!got.contains("Vancouver"),
             "the shared leaf must be consumed, not left as a third root keyword");
}

void tst_keywordwrite::anOrphanLeafIsNotSwallowedByAPath()
{
/*
    THE COLLISION THAT WOULD BE SILENT. An image carries the ROOT keyword "Heron" and,
    separately, the path "Fauna|Bird|Heron" -- the same leaf, deliberately two keywords.

    Leaf consumption is what stops the Lightroom double becoming two entries, and it
    keys on the leaf name, so this is exactly the shape it could over-apply to: consume
    the root into the path and the user silently loses a keyword they assigned.

    IT IS ACCEPTED THAT THEY MERGE, and the case pins that rather than pretending
    otherwise. dc:subject cannot express the difference -- it holds one "Heron" either
    way -- so no writer could round-trip both, and a reader has to choose. Choosing the
    PATH is right: it is the richer statement, and it is the one Lightroom itself would
    have produced. What must not happen is the third outcome, where a spurious root
    appears BESIDE the path on a file that only ever had one keyword.
*/
    const QStringList assigned = {"Heron", "Fauna|Bird|Heron"};

    Xmp *back = writeAndReload(emit_(assigned), "orphan");
    QVERIFY(back);

    const QStringList got = keywordEffectivePaths(
        back->getItemList("subject"), back->getItemList("hierarchicalsubject"));

    QCOMPARE(got, QStringList{"Fauna|Bird|Heron"});
    QVERIFY2(!got.contains("Heron"), "no spurious root beside the path");
}

QTEST_APPLESS_MAIN(tst_keywordwrite)
#include "tst_keywordwrite.moc"
