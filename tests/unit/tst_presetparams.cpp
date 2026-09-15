/*
    DevelopPresets::assignParam -- the preset key namespace.

    WHY THIS TEST EXISTS. The documentation calls assignParam "the easiest thing in
    Develop to get wrong", and until this was written it had no test at all. There are TWO
    key namespaces: DevelopProperties::applyKeyToParams takes dock slider keys and
    div-scales them (0..100 -> 0..1), while this one takes EditStack JSON field names raw.
    A field wired into the wrong one is silently off by a factor of a hundred.

    The hazard for a STRING field is sharper. assignParam computes
    `const float f = v.toFloat()` up front and the body is one long if/else-if chain: a
    string key that is not given its own branch does not fail, it falls through to a
    numeric one or to nothing at all. cameraProfile has to be read with toString() ABOVE
    that tail, and nothing but a test says so.

    The chain has no final `else`, deliberately -- an unknown key must be ignored so a
    preset written by a newer build still applies what it can. That tolerance is also
    what makes a typo invisible, so it is pinned here too.
*/

#include <QtTest>
#include <QVariant>

#include "Develop/Presets/developpresets.h"

class TstPresetParams : public QObject
{
    Q_OBJECT

private slots:
    void cameraProfileIsReadAsAString();
    void viewTransformIsReadAsAnInt();
    void aNumericFieldIsStillRaw();
    void unknownKeysAreIgnored();
};

void TstPresetParams::cameraProfileIsReadAsAString()
{
    EditParams p;
    DevelopPresets::assignParam("cameraProfile", QVariant("Camera Vivid"), p);
    QCOMPARE(p.cameraProfile, QString("Camera Vivid"));

    /* The failure this guards: falling through to `v.toFloat()` gives 0 and leaves the
       string empty, so the preset applies no profile and says nothing. */
    QVERIFY2(!p.cameraProfile.isEmpty(),
             "cameraProfile fell through to the numeric tail of assignParam");

    /* A name with a space in it survives intact. */
    EditParams q;
    DevelopPresets::assignParam("cameraProfile", QVariant("Camera Monochrome D"), q);
    QCOMPARE(q.cameraProfile, QString("Camera Monochrome D"));
}

void TstPresetParams::viewTransformIsReadAsAnInt()
{
    /* The other whole-image field: an int, read with toInt() not the float tail. */
    EditParams p;
    DevelopPresets::assignParam("viewTransform", QVariant(2), p);
    QCOMPARE(p.viewTransform, 2);
}

void TstPresetParams::aNumericFieldIsStillRaw()
{
    /*
        THE OTHER NAMESPACE. A preset stores the EditParams value unscaled, so 0.5 here
        means exposure 0.5 EV -- not 0.005 as it would if this key were being read with
        the dock's divisor. Pinned so a future field cannot be wired into the wrong one
        without something failing.
    */
    EditParams p;
    DevelopPresets::assignParam("exposure", QVariant(0.5), p);
    QCOMPARE(p.exposure, 0.5f);
}

void TstPresetParams::unknownKeysAreIgnored()
{
    /* No final else, deliberately: a preset from a newer build must apply what it can
       rather than refuse. The cost is that a typo is silent, which is why the real keys
       above are asserted one by one. */
    EditParams p;
    const EditParams def;
    DevelopPresets::assignParam("cameraProfileX", QVariant("Camera Vivid"), p);
    DevelopPresets::assignParam("somethingFromTheFuture", QVariant(3), p);
    QCOMPARE(p.cameraProfile, def.cameraProfile);
    QVERIFY(p.isIdentity());
}

QTEST_MAIN(TstPresetParams)
#include "tst_presetparams.moc"
