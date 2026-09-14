/*
    CameraProfileStore -- finding profiles, and deriving the synthesised "Camera Base".

    WHY THIS TEST EXISTS. A base entry is a claim about REAL FILES: that every "Camera *"
    profile of one camera generation shares a single colorimetric base, so one row can
    stand for all of them. That claim is what justifies replacing a per-profile "apply the
    look?" switch with one dropdown entry, and if it is wrong the entry silently renders
    like whichever profile it happened to be derived from.

    So the checks here are against whatever is installed, not against a fixture: every
    curve-bearing profile of a generation must map to the base that was derived for it, the
    base must carry none of the three creative pieces, and a camera shipping two
    generations must get two distinctly-named bases rather than one row that hides the
    other. It skips when no profiles are installed -- there is nothing to assert about a
    machine with no data.
*/

#include <QtTest>
#include <QDir>
#include <QSignalSpy>

#include "Develop/cameraprofilestore.h"

class TstCameraProfileStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void basesAreDerivedAndNamed();
    void everyCurveBearingProfileMapsToItsBase();
    void aBaseCarriesNoLook();
    void unknownNamesResolveToNothing();

private:
    /* A camera that actually has profiles installed, preferring one with two generations
       (the interesting case) -- found once in initTestCase. */
    QString model;
    QList<CameraProfileStore::Entry> entries;
};

void TstCameraProfileStore::initTestCase()
{
    bool anyRoot = false;
    for (const QString &r : CameraProfileStore::roots())
        if (QDir(r).exists()) anyRoot = true;
    if (!anyRoot) QSKIP("no camera profile folders on this machine");

    CameraProfileStore &store = CameraProfileStore::instance();
    QSignalSpy spy(&store, &CameraProfileStore::indexChanged);
    store.ensureIndex();
    if (!store.indexReady())
        QVERIFY2(spy.wait(30000), "the profile index did not finish building");

    /* Two cameras worth trying: a model known to ship both generations, then anything.
       The search is deliberately by CONTENT (does it yield two bases) rather than by a
       hardcoded camera, so the test still means something on a different machine. */
    const QStringList candidates = {
        "SONY ILCE-1M2", "SONY ILCE-7M4", "SONY ILCE-1", "NIKON Z 9", "Canon EOS R5"
    };
    for (const QString &m : candidates) {
        const QList<CameraProfileStore::Entry> e = store.forModel(m);
        if (e.isEmpty()) continue;
        model = m;
        entries = e;
        int bases = 0;
        for (const CameraProfileStore::Entry &x : e) if (x.isBase) ++bases;
        if (bases >= 2) break;          // the interesting case: stop here
    }
    if (model.isEmpty()) QSKIP("none of the sample cameras has profiles installed");
}

void TstCameraProfileStore::basesAreDerivedAndNamed()
{
    QVERIFY(!entries.isEmpty());

    QStringList baseNames;
    QSet<QString> allNames;
    for (const CameraProfileStore::Entry &e : entries) {
        QVERIFY2(!e.name.isEmpty(), "an entry with no name is unselectable");
        QVERIFY2(!allNames.contains(e.name),
                 qPrintable("duplicate entry name '" + e.name +
                            "' -- a profile is stored BY NAME, so one would be unreachable"));
        allNames.insert(e.name);
        if (e.isBase) baseNames << e.name;
    }

    qInfo() << model << "--" << entries.size() << "entries," << baseNames.size()
            << "base(s):" << baseNames;

    QVERIFY2(!baseNames.isEmpty(), "no Camera Base was derived for a camera that has profiles");
    for (const QString &n : baseNames)
        QVERIFY2(n.startsWith("Camera Base"),
                 qPrintable("unexpected base name: " + n));

    /* Two generations must be told apart, not collapsed. Adobe distinguishes them with a
       " v2" suffix on the profile names, which is what the base names inherit. */
    if (baseNames.size() >= 2) {
        QVERIFY2(baseNames.contains("Camera Base"), "the first generation lost its plain name");
        QVERIFY2(baseNames.filter(" v2").size() >= 1,
                 "a second generation was derived but not named apart");
    }

    /* The list is sorted, so a base sits where the user looks for it rather than after
       everything else. */
    for (int i = 1; i < entries.size(); ++i)
        QVERIFY(entries[i - 1].name.localeAwareCompare(entries[i].name) <= 0);
}

/*
    THE CLAIM THE WHOLE ENTRY RESTS ON: every curve-bearing profile of a generation, once
    its look is removed, IS the base derived for it. Checked by comparing the rendering
    inputs -- both matrix pairs and the HueSatMap -- of each real profile against the base
    the store hands back.
*/
void TstCameraProfileStore::everyCurveBearingProfileMapsToItsBase()
{
    CameraProfileStore &store = CameraProfileStore::instance();

    QList<std::shared_ptr<const Dcp::Profile>> bases;
    for (const CameraProfileStore::Entry &e : entries)
        if (e.isBase) {
            const auto p = store.profile(model, e.name);
            QVERIFY2(p != nullptr, qPrintable("base '" + e.name + "' did not resolve"));
            bases << p;
        }
    if (bases.isEmpty()) QSKIP("no bases derived for this camera");

    auto sameCharacterisation = [](const Dcp::Profile &a, const Dcp::Profile &b) {
        for (int c = 0; c < 2; ++c) {
            if (a.cal[c].haveColor != b.cal[c].haveColor) return false;
            if (a.cal[c].haveForward != b.cal[c].haveForward) return false;
            if (a.cal[c].illuminant != b.cal[c].illuminant) return false;
            if (a.cal[c].hueSatMap.v != b.cal[c].hueSatMap.v) return false;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    if (qAbs(a.cal[c].color.m[i][j] - b.cal[c].color.m[i][j]) > 1e-6f)
                        return false;
                    if (qAbs(a.cal[c].forward.m[i][j] - b.cal[c].forward.m[i][j]) > 1e-6f)
                        return false;
                }
        }
        return true;
    };

    int checked = 0;
    for (const CameraProfileStore::Entry &e : entries) {
        if (e.isBase) continue;
        const auto p = store.profile(model, e.name);
        if (!p || p->toneCurve.empty()) continue;       // only curve-bearing ones have a base
        ++checked;
        bool matched = false;
        for (const auto &b : bases) if (sameCharacterisation(*p, *b)) { matched = true; break; }
        QVERIFY2(matched,
                 qPrintable("'" + e.name + "' matches none of the derived bases -- the "
                            "one-base-per-generation assumption does not hold here"));
    }
    qInfo() << "checked" << checked << "curve-bearing profiles against" << bases.size()
            << "base(s)";
    QVERIFY(checked > 0);
}

void TstCameraProfileStore::aBaseCarriesNoLook()
{
    CameraProfileStore &store = CameraProfileStore::instance();
    int checked = 0;
    for (const CameraProfileStore::Entry &e : entries) {
        if (!e.isBase) continue;
        const auto p = store.profile(model, e.name);
        QVERIFY(p != nullptr);
        ++checked;
        /* All three creative pieces, not just the obvious one. The exposure offset is the
           easy one to leave behind, and it is the one that would move brightness. */
        QVERIFY2(p->lookTable.isEmpty(), "a base kept its look table");
        QVERIFY2(p->toneCurve.empty(), "a base kept its tone curve");
        QVERIFY2(qFuzzyIsNull(p->baselineExposureOffset),
                 "a base kept its exposure offset -- it would render brighter than the "
                 "characterisation it claims to be");
        /* ...and it is still a usable profile. */
        QVERIFY2(p->cal[0].haveColor, "a base lost its colour matrix");
        QCOMPARE(p->name, e.name);
    }
    QVERIFY(checked > 0);
}

void TstCameraProfileStore::unknownNamesResolveToNothing()
{
    CameraProfileStore &store = CameraProfileStore::instance();
    /* A name that no longer resolves must return null so the caller can render with the
       built-in matrix and SAY SO -- never a silent substitute. */
    QVERIFY(store.profile(model, "No Such Profile") == nullptr);
    QVERIFY(store.profile(model, QString()) == nullptr);
    QVERIFY(store.profile("No Such Camera At All", "Camera Base") == nullptr);
    QVERIFY(store.forModel("No Such Camera At All").isEmpty());
}

QTEST_MAIN(TstCameraProfileStore)
#include "tst_cameraprofilestore.moc"
