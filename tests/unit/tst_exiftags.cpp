// Unit tests for the EXIF/TIFF tag-id -> name table (Metadata/exif.cpp).
//
// The hash maps numeric IFD tag ids to human-readable names used throughout
// metadata display and reporting. A mangled entry silently mislabels metadata,
// so these pin a sample of well-known, stable tags and guard against the table
// being gutted.

#include <QtTest>
#include "Metadata/exif.h"

class TstExifTags : public QObject
{
    Q_OBJECT
private slots:
    void knownTags();
    void unknownTagIsEmpty();
    void tableNotTrivial();
};

void TstExifTags::knownTags()
{
    Exif exif;
    QCOMPARE(exif.hash.value(256), QString("ImageWidth"));
    QCOMPARE(exif.hash.value(257), QString("ImageHeight"));
    QCOMPARE(exif.hash.value(258), QString("BitsPerSample"));
    QCOMPARE(exif.hash.value(259), QString("Compression"));
    QCOMPARE(exif.hash.value(262), QString("PhotometricInterpretation"));
    QCOMPARE(exif.hash.value(271), QString("Make"));
    QCOMPARE(exif.hash.value(272), QString("Model"));
    QCOMPARE(exif.hash.value(274), QString("Orientation"));
    QCOMPARE(exif.hash.value(23),  QString("ISO"));
}

void TstExifTags::unknownTagIsEmpty()
{
    Exif exif;
    QVERIFY(!exif.hash.contains(0xDEADBEEF));
    QVERIFY(exif.hash.value(0xDEADBEEF).isEmpty());
}

void TstExifTags::tableNotTrivial()
{
    Exif exif;
    QVERIFY2(exif.hash.size() > 50, "EXIF tag table is unexpectedly small");
}

QTEST_GUILESS_MAIN(TstExifTags)
#include "tst_exiftags.moc"
