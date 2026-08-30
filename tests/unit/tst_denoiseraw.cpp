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
        has touched.
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
    void wantsFollowsThePreferenceOnlyWhenUnset();
    void sanitizeRepairsAnOutOfRangeValue();
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

void TestDenoiseRaw::wantsFollowsThePreferenceOnlyWhenUnset()
{
    EditParams p;                            // unset: the preference decides
    QCOMPARE(p.wantsDenoiseRaw(true), true);
    QCOMPARE(p.wantsDenoiseRaw(false), false);

    p.denoiseRaw = 1;                        // pinned on, even with Auto run off
    QCOMPARE(p.wantsDenoiseRaw(false), true);
    QCOMPARE(p.wantsDenoiseRaw(true), true);

    p.denoiseRaw = 0;                        // pinned off, even with Auto run on
    QCOMPARE(p.wantsDenoiseRaw(true), false);
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

QTEST_APPLESS_MAIN(TestDenoiseRaw)
#include "tst_denoiseraw.moc"
