#include <QtTest>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QStandardPaths>

#include "Datamodel/rowfields.h"
#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Main/global.h"

/*
    THE FIELD TABLE IS BINDING, NOT DOCUMENTATION.

    Datamodel/rowfields.h declares an image's fields once so that the model's
    columns, the catalog's image table and Catalog::categorySql cannot drift
    apart. A table nothing checks would drift exactly as the four hand-written
    lists it replaces already did -- which is why
    tst_catalog::categoryItemsMatchWhatTheDatamodelWrites had to be written.

    These cases fail the moment a field is added to one side and not the other,
    which is the whole reason the table exists.
*/
class tst_rowfields : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void everyPersistedFieldExistsInTheImageTable();
    void everyFieldHasAUniqueName();
    void residentAndScratchArePartitioned();
    void scratchIsNeverPersisted();
    void cleanupTestCase();

private:
    QTemporaryDir *tmp = nullptr;
    QSet<QString> imageTableColumns;
};

void tst_rowfields::initTestCase()
{
    tmp = new QTemporaryDir;
    QVERIFY(tmp->isValid());
    CacheDb::instance().setPath(tmp->filePath("index.db"));

    /*  Ask the real schema, not a copy of it pasted into the test -- a copy
        would be a fifth hand-maintained list, which is the problem. */
    QSqlDatabase db = CacheDb::instance().db();
    QVERIFY2(db.isOpen(), "index database did not open");
    QSqlQuery q(db);
    QVERIFY(q.exec("PRAGMA table_info(image)"));
    while (q.next()) imageTableColumns.insert(q.value(1).toString());
    QVERIFY2(!imageTableColumns.isEmpty(), "image table has no columns");
}

void tst_rowfields::cleanupTestCase()
{
    delete tmp;
    tmp = nullptr;
}

void tst_rowfields::everyPersistedFieldExistsInTheImageTable()
{
/*
    A field that names a SQL column must actually have one. This is the check
    that would have caught a field added to the model and forgotten in the
    catalog -- the drift that leaves an image searchable in a folder and
    invisible in the library.
*/
#define WF_CHECK(name, type, sql, sqltype, kind)                              \
    if (QString(sql).length() > 0) {                                          \
        QVERIFY2(imageTableColumns.contains(QString(sql)),                    \
                 qPrintable(QString("field %1 names SQL column '%2', which "  \
                                    "is not in the image table")              \
                                .arg(#name, sql)));                           \
    }
    WINNOW_ROW_FIELDS(WF_CHECK)
#undef WF_CHECK
}

void tst_rowfields::everyFieldHasAUniqueName()
{
    QSet<QString> names;
#define WF_NAME(name, type, sql, sqltype, kind) names.insert(#name);
    WINNOW_ROW_FIELDS(WF_NAME)
#undef WF_NAME
    QCOMPARE(names.size(), int(RowFields::fieldCount));
}

void tst_rowfields::residentAndScratchArePartitioned()
{
/*
    Every field is exactly one of Resident, Scratch or Derived. The split is
    what makes a 250,000-row catalog fit: resident fields are held for every
    row, scratch fields only for the rows a decoder or cache is working on.
*/
    int resident = 0, scratch = 0, derived = 0;
#define WF_KIND(name, type, sql, sqltype, kind)         \
    if (kind == WF_RESIDENT) ++resident;                \
    else if (kind == WF_SCRATCH) ++scratch;             \
    else if (kind == WF_DERIVED) ++derived;             \
    else QFAIL("field " #name " has an unknown kind");
    WINNOW_ROW_FIELDS(WF_KIND)
#undef WF_KIND
    QCOMPARE(resident + scratch + derived, int(RowFields::fieldCount));
    QVERIFY2(scratch > 0, "no scratch fields: the split has been lost");
    QVERIFY2(resident > scratch, "more scratch than resident looks wrong");
}

void tst_rowfields::scratchIsNeverPersisted()
{
/*
    Scratch is decode state -- segment offsets, ICC buffers. It is meaningless
    once the decode is done and must never reach the catalog, which is an index
    of what an image IS, not of how it was last read.
*/
#define WF_SCRATCH_CHECK(name, type, sql, sqltype, kind)                      \
    if (kind == WF_SCRATCH) {                                                 \
        QVERIFY2(QString(sql).isEmpty(),                                      \
                 qPrintable(QString("scratch field %1 names a SQL column")    \
                                .arg(#name)));                                \
    }
    WINNOW_ROW_FIELDS(WF_SCRATCH_CHECK)
#undef WF_SCRATCH_CHECK
}

QTEST_MAIN(tst_rowfields)
#include "tst_rowfields.moc"
