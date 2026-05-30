// Unit tests for Utilities path helpers.
//
// Pure string/path logic used throughout file handling and sidecar resolution.
// Inputs are absolute paths so results are deterministic and independent of the
// process working directory (getFolderPath/assocXmpPath resolve via QFileInfo).

#include <QtTest>
#include "utilities.h"

class TstPathUtils : public QObject
{
    Q_OBJECT
private slots:
    void fileName();
    void fileBase_beforeFirstDot();
    void suffix_lowercasedAfterLastDot();
    void folderPath_absolute();
    void assocXmp();
};

void TstPathUtils::fileName()
{
    QCOMPARE(Utilities::getFileName("/photos/2024/IMG_0001.NEF"), QString("IMG_0001.NEF"));
}

void TstPathUtils::fileBase_beforeFirstDot()
{
    QCOMPARE(Utilities::getFileBase("/photos/IMG_0001.NEF"), QString("IMG_0001"));
    // QFileInfo::baseName() is the part before the FIRST dot.
    QCOMPARE(Utilities::getFileBase("/photos/archive.tar.gz"), QString("archive"));
}

void TstPathUtils::suffix_lowercasedAfterLastDot()
{
    // getSuffix lowercases QFileInfo::suffix() (part after the LAST dot).
    QCOMPARE(Utilities::getSuffix("/photos/IMG_0001.NEF"), QString("nef"));
    QCOMPARE(Utilities::getSuffix("/photos/archive.tar.gz"), QString("gz"));
}

void TstPathUtils::folderPath_absolute()
{
    QCOMPARE(Utilities::getFolderPath("/photos/2024/IMG_0001.NEF"), QString("/photos/2024"));
}

void TstPathUtils::assocXmp()
{
    QCOMPARE(Utilities::assocXmpPath("/photos/2024/IMG_0001.NEF"),
             QString("/photos/2024/IMG_0001.xmp"));
}

QTEST_GUILESS_MAIN(TstPathUtils)
#include "tst_pathutils.moc"
