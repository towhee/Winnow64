// Unit tests for Develop/History/historystore.* -- Develop history that survives a
// restart -- and DevelopHistory::seed's rule for trusting it.
//
// A saved history is only restored if its current step is the recipe the sidecar holds
// now; anything else means the image changed somewhere this index did not see, and the
// history would revert to states the user never had.

#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "Cache/cachedb.h"
#include "Develop/History/develophistory.h"
#include "Develop/History/historystore.h"

namespace {

EditStack stackWithExposure(float ev, int spots = 0)
{
    EditStack s;
    s.scopes.append(EditScope());
    s.scopes[0].name = "Global";
    s.scopes[0].params.exposure = ev;
    for (int i = 0; i < spots; ++i) {
        FillSpot f;
        f.paramsJson = QString("{\"kind\":\"spot\",\"pts\":[0.%1,0.5],\"size\":0.01}").arg(i);
        s.spots.append(f);
    }
    return s;
}

QVector<HistoryEntry> threeSteps()
{
    HistoryEntry a; a.action = "Original";     a.stack = stackWithExposure(0.0f);
    HistoryEntry b; b.action = "Exposure";     b.scope = "Global"; b.value = "+0.50";
                    b.mergeKey = "Global/exposure"; b.stack = stackWithExposure(0.5f);
    HistoryEntry c; c.action = "Spot heal";    c.stack = stackWithExposure(0.5f, 3);
                    c.syncId = 0xFEDCBA9876543210ULL;   // beyond a double's precision
    return {a, b, c};
}

/* The stamp of the recipe the three steps end at: what flushImage saves them with. */
const QString kStamp = HistoryEntry::recipeStamp(stackWithExposure(0.5f, 3));

std::function<bool(const QString &, QVector<HistoryEntry> &, int &, QString &)> loader()
{
    return [](const QString &path, QVector<HistoryEntry> &e, int &pos, QString &stamp) {
        return HistoryStore::load(path, e, pos, stamp);
    };
}

}   // namespace

class TstHistoryStore : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void roundTrip();
    void emptySaveRemoves();
    void moveCarriesVersions();
    void deleteTakesVersions();
    void seedRestoresWhenRecipeMatches();
    void seedIgnoresWhenRecipeMoved();
    void forgetRemovesSaved();
    void stampSurvivesSidecarRoundTrip();
    void unrecordedChangeStillRestores();

private:
    QTemporaryDir cacheTmp;
};

void TstHistoryStore::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(cacheTmp.isValid());
    CacheDb::instance().setPath(cacheTmp.filePath("index.db"));
    QVERIFY(CacheDb::instance().db().isOpen());
}

void TstHistoryStore::cleanupTestCase()
{
    CacheDb::instance().closeThisThread();
}

void TstHistoryStore::roundTrip()
{
    const QString p = "/Photos/A/IMG_1.NEF";
    HistoryStore::save(p, threeSteps(), 1, kStamp);
    QVector<HistoryEntry> e;
    int pos = -1;
    QString stamp;
    QVERIFY(HistoryStore::load(p, e, pos, stamp));
    QCOMPARE(pos, 1);
    QCOMPARE(e.size(), 3);
    QCOMPARE(e[1].action, QString("Exposure"));
    QCOMPARE(e[1].scope, QString("Global"));
    QCOMPARE(e[1].value, QString("+0.50"));
    QCOMPARE(e[1].mergeKey, QString("Global/exposure"));
    QCOMPARE(e[2].syncId, 0xFEDCBA9876543210ULL);
    QCOMPARE(e[2].stack.spots.size(), 3);
    QCOMPARE(stamp, kStamp);
    QCOMPARE(HistoryEntry::recipeKey(e[2].stack),
             HistoryEntry::recipeKey(stackWithExposure(0.5f, 3)));
    /* The key is the normalised path: case and separators do not matter. */
    QVERIFY(HistoryStore::load("/photos/a/img_1.nef", e, pos, stamp));
}

void TstHistoryStore::emptySaveRemoves()
{
    const QString p = "/Photos/A/IMG_2.NEF";
    HistoryStore::save(p, threeSteps(), 2, kStamp);
    HistoryStore::save(p, {}, 0, kStamp);
    QVector<HistoryEntry> e;
    int pos;
    QString stamp;
    QVERIFY(!HistoryStore::load(p, e, pos, stamp));
}

void TstHistoryStore::moveCarriesVersions()
{
    const QString src = "/Photos/B/IMG_3.NEF", dst = "/Photos/C/IMG_3.NEF";
    HistoryStore::save(src, threeSteps(), 2, kStamp);
    HistoryStore::save(src + "/#v2", threeSteps(), 1, kStamp);
    HistoryStore::save(dst, threeSteps(), 0, kStamp);    // overwritten by the move
    HistoryStore::onMoved(src, dst);

    QVector<HistoryEntry> e;
    int pos;
    QString stamp;
    QVERIFY(!HistoryStore::load(src, e, pos, stamp));
    QVERIFY(!HistoryStore::load(src + "/#v2", e, pos, stamp));
    QVERIFY(HistoryStore::load(dst, e, pos, stamp));
    QCOMPARE(pos, 2);
    QVERIFY(HistoryStore::load(dst + "/#v2", e, pos, stamp));
    QCOMPARE(pos, 1);
}

void TstHistoryStore::deleteTakesVersions()
{
    const QString p = "/Photos/D/IMG_4.NEF";
    HistoryStore::save(p, threeSteps(), 2, kStamp);
    HistoryStore::save(p + "/#v1", threeSteps(), 2, kStamp);
    // A path prefix of p, not a version of it:
    HistoryStore::save("/Photos/D/IMG_40.NEF", threeSteps(), 2, kStamp);
    HistoryStore::onDeleted(p);
    QVector<HistoryEntry> e;
    int pos;
    QString stamp;
    QVERIFY(!HistoryStore::load(p, e, pos, stamp));
    QVERIFY(!HistoryStore::load(p + "/#v1", e, pos, stamp));
    QVERIFY(HistoryStore::load("/Photos/D/IMG_40.NEF", e, pos, stamp));
}

void TstHistoryStore::seedRestoresWhenRecipeMatches()
{
    const QString p = "/Photos/E/IMG_5.NEF";
    HistoryStore::save(p, threeSteps(), 2, kStamp);
    DevelopHistory h;
    h.loader = loader();
    h.seed(p, stackWithExposure(0.5f, 3));                // what the sidecar holds
    QCOMPARE(h.count(p), 3);
    QCOMPARE(h.pos(p), 2);
    QCOMPARE(h.at(p, 1)->action, QString("Exposure"));
}

void TstHistoryStore::seedIgnoresWhenRecipeMoved()
{
    const QString p = "/Photos/E/IMG_6.NEF";
    HistoryStore::save(p, threeSteps(), 2, kStamp);
    DevelopHistory h;
    h.loader = loader();
    h.seed(p, stackWithExposure(1.25f));                  // edited elsewhere since
    QCOMPARE(h.count(p), 1);
    QCOMPARE(h.at(p, 0)->action, QString("Saved settings"));
}

void TstHistoryStore::forgetRemovesSaved()
{
    const QString p = "/Photos/E/IMG_7.NEF";
    HistoryStore::save(p, threeSteps(), 2, kStamp);
    DevelopHistory h;
    h.remover = [](const QString &path) { HistoryStore::remove(path); };
    h.forget(p);
    QVector<HistoryEntry> e;
    int pos;
    QString stamp;
    QVERIFY(!HistoryStore::load(p, e, pos, stamp));
}

void TstHistoryStore::stampSurvivesSidecarRoundTrip()
{
    /* The stamp is taken from the in-memory recipe at flush, and compared next session
       against the recipe READ BACK from the sidecar. A lossy round trip would make every
       saved history look stale. */
    EditStack s = stackWithExposure(0.37f, 5);
    s.geometry.cropX = 0.1234567;     // a valid crop: sanitize resets one off the frame
    s.geometry.cropW = 0.7654321;
    s.scopes[0].params.temp = 5321.5f;
    const EditStack back = EditStack::fromBase64(s.toBase64());
    QCOMPARE(HistoryEntry::recipeStamp(back), HistoryEntry::recipeStamp(s));
    QCOMPARE(HistoryEntry::recipeStamp(EditStack()),
             HistoryEntry::recipeStamp(EditStack::fromBase64(QString())));
}

void TstHistoryStore::unrecordedChangeStillRestores()
{
    /* The recipe moved without a step being recorded (the sidecar now holds 0.9 EV while
       the newest step says 0.5): the history was saved WITH that recipe, so it is still
       the right history for this image. */
    const QString p = "/Photos/E/IMG_8.NEF";
    const EditStack now = stackWithExposure(0.9f, 3);
    HistoryStore::save(p, threeSteps(), 2, HistoryEntry::recipeStamp(now));
    DevelopHistory h;
    h.loader = loader();
    h.seed(p, now);
    QCOMPARE(h.count(p), 3);
}

QTEST_GUILESS_MAIN(TstHistoryStore)
#include "tst_historystore.moc"
