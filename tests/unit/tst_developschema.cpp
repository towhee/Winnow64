/*
    Develop settings schema version + migration chain (Develop/developschema.h).

    The chain is the ONLY thing standing between an old recipe and a renamed, rescaled or
    retuned setting, and while kVersion is 1 no real step exists to exercise it. So most
    cases install a test chain through DevelopSchema::testSteps() and drive the same code
    the sidecar (EditStack::fromJson / fromBase64) and preset (DevelopPresets::migrate)
    paths run.

    The test chain:
      v2  rescales "exposure" x2; a FULL recipe that lacks "contrast" gets the old
          default 10 written in (a changed default).
      v3  renames "oldKey" to "newKey" and touches the stack root ("geometry").
*/

#include <QtTest>
#include <QJsonDocument>

#include "Develop/developschema.h"
#include "Develop/developrevision.h"
#include "Develop/editstack.h"
#include "Develop/Presets/developpresets.h"

namespace {

void stepV2Params(QJsonObject &p, bool partial)
{
    if (p.contains("exposure")) p["exposure"] = p.value("exposure").toDouble() * 2.0;
    if (!partial && !p.contains("contrast")) p["contrast"] = 10.0;
}

void stepV3Params(QJsonObject &p, bool)
{
    if (p.contains("oldKey")) {
        p["newKey"] = p.value("oldKey");
        p.remove("oldKey");
    }
}

void stepV3Stack(QJsonObject &s)
{
    s["stepV3Ran"] = true;
}

const QVector<DevelopSchema::Step> kTestSteps = {
    {2, "2026-01-01", "Exposure doubled", stepV2Params, nullptr},
    {3, "2026-02-01", "oldKey renamed",   stepV3Params, stepV3Stack},
};

/* A sidecar-shaped stack: one scope, stamped `version` (absent when < 0). */
QJsonObject stackJson(int version, double exposure)
{
    QJsonObject params;
    params["exposure"] = exposure;
    QJsonObject scope;
    scope["name"] = "Global";
    scope["params"] = params;
    QJsonObject o;
    if (version >= 0) o["version"] = version;
    o["scopes"] = QJsonArray{scope};
    return o;
}

QJsonObject firstParams(const QJsonObject &stack)
{
    return stack.value("scopes").toArray().first().toObject().value("params").toObject();
}

struct ChainGuard {
    ChainGuard()  { DevelopSchema::testSteps() = &kTestSteps; }
    ~ChainGuard() { DevelopSchema::testSteps() = nullptr; }
};

} // namespace

class TstDevelopSchema : public QObject
{
    Q_OBJECT

private slots:
    void releasedChainIsConsistent();
    void currentStackIsUntouched();
    void absentStampReadsAsOne();
    void chainRunsInOrderFromEachStart();
    void changedDefaultFillsOnlyFullRecipes();
    void newerStackPassesThrough();
    void fromJsonMigratesAndRecords();
    void roundTripStampsCurrent();
    void presetMigratesLikeSidecar();
    void summariesSince();
    void revisionKeyPartIsEmptyUntilBumped();
};

void TstDevelopSchema::releasedChainIsConsistent()
{
    /* kVersion must be the last released step's `to`, steps must be consecutive from 2,
       and every step must say something to the user. */
    const QVector<DevelopSchema::Step> &s = DevelopSchema::releasedSteps();
    QCOMPARE(DevelopSchema::current(), DevelopSchema::kVersion);
    /* The two stores' version numbers are the shared one. */
    QCOMPARE(EditStack::kVersion, DevelopSchema::kVersion);
    QCOMPARE(DevelopPresets::kVersion, DevelopSchema::kVersion);
    if (s.isEmpty()) {
        QCOMPARE(DevelopSchema::kVersion, 1);
        return;
    }
    QCOMPARE(s.last().to, DevelopSchema::kVersion);
    for (int i = 0; i < s.size(); ++i) {
        QCOMPARE(s[i].to, i + 2);
        QVERIFY(s[i].summary && *s[i].summary);
        QVERIFY(s[i].date && *s[i].date);
        QVERIFY(s[i].params || s[i].stack);
    }
}

void TstDevelopSchema::currentStackIsUntouched()
{
    ChainGuard g;
    QJsonObject o = stackJson(3, 0.5);
    const QJsonObject before = o;
    QCOMPARE(DevelopSchema::migrateStack(o), 0);
    QCOMPARE(o, before);
}

void TstDevelopSchema::absentStampReadsAsOne()
{
    ChainGuard g;
    QJsonObject o = stackJson(-1, 0.5);
    QCOMPARE(DevelopSchema::migrateStack(o), 1);
    QCOMPARE(o.value("version").toInt(), 3);
    QCOMPARE(firstParams(o).value("exposure").toDouble(), 1.0);
}

void TstDevelopSchema::chainRunsInOrderFromEachStart()
{
    ChainGuard g;
    /* From 1: both steps. */
    QJsonObject a = stackJson(1, 0.25);
    QJsonArray scopes = a.value("scopes").toArray();
    QJsonObject sc = scopes[0].toObject();
    QJsonObject p = sc.value("params").toObject();
    p["oldKey"] = 7;
    sc["params"] = p;
    scopes[0] = sc;
    a["scopes"] = scopes;
    QCOMPARE(DevelopSchema::migrateStack(a), 1);
    QCOMPARE(firstParams(a).value("exposure").toDouble(), 0.5);
    QCOMPARE(firstParams(a).value("newKey").toInt(), 7);
    QVERIFY(!firstParams(a).contains("oldKey"));
    QVERIFY(a.value("stepV3Ran").toBool());

    /* From 2: only v3, so exposure is NOT doubled again. */
    QJsonObject b = stackJson(2, 0.25);
    QCOMPARE(DevelopSchema::migrateStack(b), 2);
    QCOMPARE(firstParams(b).value("exposure").toDouble(), 0.25);
    QVERIFY(b.value("stepV3Ran").toBool());
}

void TstDevelopSchema::changedDefaultFillsOnlyFullRecipes()
{
    ChainGuard g;
    QJsonObject full;
    DevelopSchema::migrateParams(full, 1, false);
    QCOMPARE(full.value("contrast").toDouble(), 10.0);

    /* A preset is partial: an absent key means "don't touch", so nothing is inserted. */
    QJsonObject partial;
    DevelopSchema::migrateParams(partial, 1, true);
    QVERIFY(!partial.contains("contrast"));
}

void TstDevelopSchema::newerStackPassesThrough()
{
    ChainGuard g;
    QJsonObject o = stackJson(9, 0.5);
    const QJsonObject before = o;
    QCOMPARE(DevelopSchema::migrateStack(o), 0);
    QCOMPARE(o, before);

    /* ...and fromBase64 still reports it as written by a newer Winnow. */
    const QString b64 = QString::fromLatin1(
        QJsonDocument(stackJson(9, 0.5)).toJson(QJsonDocument::Compact).toBase64());
    QStringList issues;
    const EditStack s = EditStack::fromBase64(b64, nullptr, &issues);
    QCOMPARE(s.migratedFrom, 0);
    QVERIFY(!issues.isEmpty());
    QVERIFY(issues.first().startsWith("Written by a newer"));
}

void TstDevelopSchema::fromJsonMigratesAndRecords()
{
    ChainGuard g;
    const EditStack s = EditStack::fromJson(stackJson(1, 0.25));
    QCOMPARE(s.migratedFrom, 1);
    QCOMPARE(s.version, 3);
    QVERIFY(!s.scopes.isEmpty());
    QCOMPARE(s.scopes.first().params.exposure, 0.5f);
    /* The changed default reached the parsed params too. */
    QCOMPARE(s.scopes.first().params.contrast, 10.0f);
}

void TstDevelopSchema::roundTripStampsCurrent()
{
    ChainGuard g;
    const EditStack s = EditStack::fromJson(stackJson(1, 0.25));
    const QJsonObject out = s.toJson();
    QCOMPARE(out.value("version").toInt(), 3);
    const EditStack again = EditStack::fromJson(out);
    QCOMPARE(again.migratedFrom, 0);
    QCOMPARE(again.scopes.first().params.exposure, 0.5f);
}

void TstDevelopSchema::presetMigratesLikeSidecar()
{
    ChainGuard g;
    DevelopPreset p;
    p.version = 1;
    /* QSettings hands ini values back as strings; an untouched one must stay one. */
    p.params.insert("exposure", 0.25);
    p.params.insert("highlights", QString("-30"));
    DevelopPresets::migrate(p);
    QCOMPARE(p.version, 3);
    QCOMPARE(p.params.value("exposure").toDouble(), 0.5);
    QVERIFY(!p.params.contains("contrast"));                 // partial: not inserted
    QCOMPARE(p.params.value("highlights").typeId(), QMetaType::QString);

    /* Same values as the sidecar path. */
    EditParams viaPreset;
    for (auto it = p.params.constBegin(); it != p.params.constEnd(); ++it)
        DevelopPresets::assignParam(it.key(), it.value(), viaPreset);
    const EditStack viaSidecar = EditStack::fromJson(stackJson(1, 0.25));
    QCOMPARE(viaPreset.exposure, viaSidecar.scopes.first().params.exposure);

    /* A preset from a newer build is left alone. */
    DevelopPreset newer;
    newer.version = 9;
    newer.params.insert("exposure", 0.25);
    DevelopPresets::migrate(newer);
    QCOMPARE(newer.version, 9);
    QCOMPARE(newer.params.value("exposure").toDouble(), 0.25);
}

void TstDevelopSchema::summariesSince()
{
    ChainGuard g;
    QCOMPARE(DevelopSchema::summariesSince(1).size(), 2);
    QCOMPARE(DevelopSchema::summariesSince(2).size(), 1);
    QVERIFY(DevelopSchema::summariesSince(2).first().contains("oldKey renamed"));
    QVERIFY(DevelopSchema::summariesSince(3).isEmpty());
}

void TstDevelopSchema::revisionKeyPartIsEmptyUntilBumped()
{
    /* 0 = never bumped must leave every existing devPreview key unchanged. */
    if (DevelopRevision::kEdited == 0)
        QVERIFY(DevelopRevision::keyPart("abc123").isEmpty());
    else
        QCOMPARE(DevelopRevision::keyPart("abc123"),
                 QString("|dev=%1").arg(DevelopRevision::kEdited));
    if (DevelopRevision::kIdentity == 0)
        QVERIFY(DevelopRevision::keyPart("Rabc123").isEmpty());
}

QTEST_GUILESS_MAIN(TstDevelopSchema)
#include "tst_developschema.moc"
