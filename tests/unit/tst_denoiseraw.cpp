/*
    EditParams::denoiseRaw -- WHETHER the raw denoise (PMRID) runs, as part of the recipe.

    WHY THIS TEST EXISTS. The two amounts cannot express the question: denoiseLuma and
    denoiseChroma default NON-ZERO, so "an amount is set" is true for every raw ever
    opened. Before this field the answer lived in a global preference and in a session
    cache filled by the dock's "Denoise" checkbox, which meant a manual run reached
    neither the exporter nor the devPreview builder -- the loupe showed a denoised image
    while the stored preview stayed clean under a recipe hash asserting the two matched.

    Four properties are pinned here, and each one is a way that bug could come back:

      o UNSET is identity and is OMITTED from the JSON. Recipes are hashed to key the
        devPreview cache, so a field that serialises even when nothing was decided would
        re-key -- and so invalidate -- every preview ever built.
      o an EXPLICIT value survives the round trip, in both directions (on AND off): the
        whole point is that "denoise this one" and "leave this one alone" outlive the
        session that said so.
      o the unset fallback follows the preference, so nothing changes for the raws nobody
        has touched; and Auto run overrides an explicit off (the panel hides the
        per-image box while it is on, so an off winning would have no visible cause).
      o sanitize repairs a value no UI could produce, since sidecars are hand-editable.
*/

#include <QtTest>
#include <QJsonObject>

#include "Develop/editparams.h"
#include "Develop/editstack.h"

class TestDenoiseRaw : public QObject
{
    Q_OBJECT

private slots:
    void unsetIsIdentityAndUnserialised();
    void explicitValueSurvivesTheRoundTrip();
    void wantsIsAutoRunOrOptedIn();
    void sanitizeRepairsAnOutOfRangeValue();
    void unsetAmountsFollowTheRememberedValues();
    void rawRendererIsATriStateLikeDenoiseRaw();
};

void TestDenoiseRaw::unsetIsIdentityAndUnserialised()
{
    const EditParams def;
    QCOMPARE(def.denoiseRaw, -1);
    QVERIFY(def.isIdentity());

    /* Absent from the JSON, so the hash of an untouched recipe is what it always was. */
    const QJsonObject o = EditStack::paramsToJson(def);
    QVERIFY(!o.contains("denoiseRaw"));

    /* And an explicit value -- either way round -- is an EDIT: it has to reach the
       sidecar, which only happens for a non-identity stack. */
    EditParams on = def;
    on.denoiseRaw = 1;
    QVERIFY(!on.isIdentity());
    EditParams off = def;
    off.denoiseRaw = 0;
    QVERIFY(!off.isIdentity());
}

void TestDenoiseRaw::explicitValueSurvivesTheRoundTrip()
{
    for (int want : {0, 1}) {
        EditParams p;
        p.denoiseRaw = want;
        const QJsonObject o = EditStack::paramsToJson(p);
        QVERIFY(o.contains("denoiseRaw"));

        const EditParams back = EditStack::paramsFromJson(o);
        QCOMPARE(back.denoiseRaw, want);
    }

    /* A recipe written before the field existed reads as unset, not as off -- otherwise
       every image edited under an older build would silently stop being denoised. */
    QJsonObject legacy = EditStack::paramsToJson(EditParams());
    legacy.remove("denoiseRaw");            // belt and braces: it is not there anyway
    const EditParams old = EditStack::paramsFromJson(legacy);
    QCOMPARE(old.denoiseRaw, -1);
}

void TestDenoiseRaw::wantsIsAutoRunOrOptedIn()
{
    EditParams p;                            // unset: the preference decides
    QCOMPARE(p.wantsDenoiseRaw(true), true);
    QCOMPARE(p.wantsDenoiseRaw(false), false);

    p.denoiseRaw = 1;                        // opted in, even with Auto run off
    QCOMPARE(p.wantsDenoiseRaw(false), true);
    QCOMPARE(p.wantsDenoiseRaw(true), true);

    /* Off is overridden by Auto run: the Raw panel hides the per-image box while Auto
       run is on, so an "off" winning would render clean with no visible reason. */
    p.denoiseRaw = 0;
    QCOMPARE(p.wantsDenoiseRaw(true), true);
    QCOMPARE(p.wantsDenoiseRaw(false), false);
}

void TestDenoiseRaw::sanitizeRepairsAnOutOfRangeValue()
{
    EditStack s;
    s.scopes.append(EditScope());
    s.scopes[0].params.denoiseRaw = 7;       // no control can produce this; a sidecar can
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.denoiseRaw, -1);

    /* The legal values are left alone -- a repair pass that "fixes" valid data would
       quietly drop the user's decision. */
    for (int want : {-1, 0, 1}) {
        EditStack t;
        t.scopes.append(EditScope());
        t.scopes[0].params.denoiseRaw = want;
        EditStack::sanitize(t);
        QCOMPARE(t.scopes[0].params.denoiseRaw, want);
    }
}

/*
    The two amounts are tri-state too (2026-10-13): -1 = unset, follow the REMEMBERED
    amounts (G::denoiseLumaDefault / ChromaDefault -- passed in here, as autoRun is), so
    a slider change carries to every image that never set its own. Unset must be
    identity and absent from the JSON (the devPreview key), an explicit value must
    survive the round trip, and sanitize must keep -1 legal while repairing other
    negatives.
*/
/*
    The RAW RENDERER (EditParams::rawEngine + appleRawVersion, 2026-10-14) is the same
    kind of field: per image, so a version can render with Apple Raw 9 while its master
    uses Winnow, and UNSET follows the default. The same four properties hold -- unset is
    identity and absent from the JSON (no preview re-keyed), an explicit choice survives
    the round trip, the version is written only under Apple, and sanitize repairs what no
    control can produce. (RawRender::resolve, which reads the globals, is exercised in
    the app; this pins the recipe.)
*/
void TestDenoiseRaw::rawRendererIsATriStateLikeDenoiseRaw()
{
    const EditParams def;
    QCOMPARE(def.rawEngine, -1);
    QVERIFY(def.isIdentity());
    QVERIFY(!EditStack::paramsToJson(def).contains("rawEngine"));
    QVERIFY(!EditStack::paramsToJson(def).contains("appleRawVersion"));

    EditParams winnow;
    winnow.rawEngine = 0;
    winnow.appleRawVersion = 9;                 // meaningless under Winnow: not written
    QVERIFY(!winnow.isIdentity());              // a renderer choice alone is an edit
    QJsonObject o = EditStack::paramsToJson(winnow);
    QCOMPARE(o.value("rawEngine").toInt(), 0);
    QVERIFY(!o.contains("appleRawVersion"));

    EditParams apple9;
    apple9.rawEngine = 1;
    apple9.appleRawVersion = 9;
    o = EditStack::paramsToJson(apple9);
    const EditParams back = EditStack::paramsFromJson(o);
    QCOMPARE(back.rawEngine, 1);
    QCOMPARE(back.appleRawVersion, 9);

    EditParams appleNewest;
    appleNewest.rawEngine = 1;                  // version 0 = newest, the default
    QVERIFY(!EditStack::paramsToJson(appleNewest).contains("appleRawVersion"));

    EditStack s;
    s.scopes.append(EditScope());
    s.scopes[0].params.rawEngine = 5;
    s.scopes[0].params.appleRawVersion = -3;
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.rawEngine, -1);
    QCOMPARE(s.scopes[0].params.appleRawVersion, 0);
}

void TestDenoiseRaw::unsetAmountsFollowTheRememberedValues()
{
    EditParams p;
    QCOMPARE(p.denoiseLuma, -1.0f);
    QCOMPARE(p.denoiseChroma, -1.0f);
    QVERIFY(p.isIdentity());
    QCOMPARE(p.denoiseLumaOr(0.4f), 0.4f);
    QCOMPARE(p.denoiseChromaOr(0.9f), 0.9f);

    const QJsonObject unset = EditStack::paramsToJson(p);
    QVERIFY(!unset.contains("denoiseLuma"));
    QVERIFY(!unset.contains("denoiseChroma"));
    QCOMPARE(EditStack::paramsFromJson(unset).denoiseLuma, -1.0f);

    p.denoiseLuma = 0.3f;                    // the image set its own: remembered ignored
    QCOMPARE(p.denoiseLumaOr(0.4f), 0.3f);
    QVERIFY(!p.isIdentity());
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.denoiseLuma, 0.3f);
    QCOMPARE(back.denoiseChroma, -1.0f);

    EditStack s;
    s.scopes.append(EditScope());
    s.scopes[0].params.denoiseLuma = -5.0f;  // damaged sidecar -> unset
    s.scopes[0].params.denoiseChroma = 3.0f; // out of range -> clamped
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.denoiseLuma, -1.0f);
    QCOMPARE(s.scopes[0].params.denoiseChroma, 1.0f);
}

QTEST_APPLESS_MAIN(TestDenoiseRaw)
#include "tst_denoiseraw.moc"
