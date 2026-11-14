#include <QtTest>

#include "Cache/decodeverdict.h"

/*
    Pins Cache/decodeverdict.h: what ImageCache does with a finished decode
    (Documentation 4.4.13). The order is the point. A decode started before a cache
    reload (the D key, Decode Raw, preview source, colour management) must never be
    cached, however good its pixels: it was accepted, and the right decode that landed
    next was then thrown away as "already cached" (C26).
*/
using namespace DecodeVerdict;

class tst_decodeverdict : public QObject
{
    Q_OBJECT
private slots:
    void goodDecodeIsAccepted();
    void staleInstanceWinsOverEverything();
    void keyGoneIsStale();
    void reloadSupersedesASuccess();
    void invalidateSupersedesASuccess();
    void statusClasses();
    void attemptAccounting();
};

void tst_decodeverdict::goodDecodeIsAccepted()
{
    QCOMPARE(judge(Result{}), Verdict::Accept);
}

void tst_decodeverdict::staleInstanceWinsOverEverything()
{
    Result r;
    r.instanceCurrent = false;
    r.generationCurrent = false;
    r.superseded = true;
    r.status = StatusClass::Terminal;
    QCOMPARE(judge(r), Verdict::Stale);     // never queued again for a dead queue
}

void tst_decodeverdict::keyGoneIsStale()
{
    Result r;
    r.keyInModel = false;
    QCOMPARE(judge(r), Verdict::Stale);
}

void tst_decodeverdict::reloadSupersedesASuccess()
{
    Result r;
    r.generationCurrent = false;            // D pressed while it decoded
    QCOMPARE(judge(r), Verdict::Superseded);
    r.status = StatusClass::Terminal;       // failing by old settings proves nothing
    QCOMPARE(judge(r), Verdict::Superseded);
}

void tst_decodeverdict::invalidateSupersedesASuccess()
{
    Result r;
    r.superseded = true;                    // the file or recipe changed meanwhile
    QCOMPARE(judge(r), Verdict::Superseded);
}

void tst_decodeverdict::statusClasses()
{
    Result r;
    r.status = StatusClass::NoGeometry;
    QCOMPARE(judge(r), Verdict::Defer);
    r.status = StatusClass::Transient;
    QCOMPARE(judge(r), Verdict::Retry);
    r.status = StatusClass::Terminal;
    QCOMPARE(judge(r), Verdict::Fail);
}

void tst_decodeverdict::attemptAccounting()
{
    // not attempts at the image: the attempts cap must not count them
    QVERIFY(givesAttemptBack(Verdict::Stale));
    QVERIFY(givesAttemptBack(Verdict::Superseded));
    QVERIFY(givesAttemptBack(Verdict::Defer));
    // real attempts
    QVERIFY(!givesAttemptBack(Verdict::Accept));
    QVERIFY(!givesAttemptBack(Verdict::Retry));
    QVERIFY(!givesAttemptBack(Verdict::Fail));
}

QTEST_GUILESS_MAIN(tst_decodeverdict)
#include "tst_decodeverdict.moc"
