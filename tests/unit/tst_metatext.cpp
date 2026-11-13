#include <QtTest>

#include "Metadata/metatext.h"

/*
    The user-text guard (Metadata/metatext.h).

    Pins the rule that kept a binary "title" -- an iPhone DNG's own bytes from offset 0,
    ~10 million characters -- out of the datamodel, where sorting the Filters Titles list
    on it never finished: binary is discarded, text is kept, runaway text is cut.
*/
class tst_metatext : public QObject
{
    Q_OBJECT

private slots:
    void ordinaryTextIsKept();
    void binaryIsDiscarded();
    void longTextIsTruncated();
    void originalIsCleanedWithItsValue();
};

namespace {
struct Row {
    QString title, creator, copyright, email, url;
    QString _title, _creator, _copyright, _email, _url;
};
}

void tst_metatext::ordinaryTextIsKept()
{
    QString s = QString::fromUtf8("Héron at dawn\tline two\r\nline three");
    QVERIFY(!MetaText::clean(s));
    QCOMPARE(s, QString::fromUtf8("Héron at dawn\tline two\r\nline three"));
}

void tst_metatext::binaryIsDiscarded()
{
    // the bytes the DNG header decodes to: MM\0*...APPLEDNG, invalid UTF-8 replaced
    QString s = QString::fromUtf8(QByteArray("MM\0*\0\0\0\x12" "APPLEDNG\xfe\x01", 18));
    QVERIFY(MetaText::clean(s));
    QVERIFY(s.isEmpty());

    QString ctrl = "Title\x01";
    QVERIFY(MetaText::clean(ctrl));
    QVERIFY(ctrl.isEmpty());
}

void tst_metatext::longTextIsTruncated()
{
    QString s(MetaText::kMaxLen + 500, 'a');
    QVERIFY(MetaText::clean(s));
    QCOMPARE(s.size(), MetaText::kMaxLen);
}

void tst_metatext::originalIsCleanedWithItsValue()
{
    Row r;
    r.title = r._title = QString(MetaText::kMaxLen * 2, 'x');
    r.creator = "Rory";
    const QStringList changed = MetaText::cleanFields(r);
    QCOMPARE(changed, QStringList{"title"});
    QCOMPARE(r.title, r._title);           // unchanged field must not look edited
    QCOMPARE(r.creator, QString("Rory"));
}

QTEST_APPLESS_MAIN(tst_metatext)
#include "tst_metatext.moc"
