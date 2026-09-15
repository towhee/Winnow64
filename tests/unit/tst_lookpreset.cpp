/*
    DevelopPresets::assignParam -- the preset key namespace.

    WHY THIS TEST EXISTS. The documentation calls assignParam "the easiest thing in
    Develop to get wrong", and until now it had no test at all. There are TWO key
    namespaces: DevelopProperties::applyKeyToParams takes dock slider keys and div-scales
    them (0..100 -> 0..1), while this one takes EditStack JSON field names raw. A field
    wired into the wrong one is silently off by a factor of a hundred.

    The specific hazard for a STRING field is sharper. assignParam computes
    `const float f = v.toFloat()` up front and the body is one long if/else-if chain: a
    string key that is not given its own branch does not fail, it falls through to a
    numeric one or to nothing at all. cameraProfile and lookLut both have to be read with
    toString() ABOVE that tail, and nothing but a test says so.

    The chain has no final `else`, deliberately -- an unknown key must be ignored so a
    preset written by a newer build still applies what it can. That tolerance is also
    what makes a typo invisible, so it is pinned here too.
*/

#include <QtTest>
#include <QVariant>

#include "Develop/Presets/developpresets.h"

class TstLookPreset : public QObject
{
    Q_OBJECT

private slots:
    void lookIsReadAsAString();
    void cameraProfileIsReadAsAString();
    void viewTransformIsReadAsAnInt();
    void aNumericFieldIsStillRaw();
    void unknownKeysAreIgnored();
};

void TstLookPreset::lookIsReadAsAString()
{
    EditParams p;
    DevelopPresets::assignParam("lookLut", QVariant("Fuji/Classic Chrome"), p);
    QCOMPARE(p.lookLut, QString("Fuji/Classic Chrome"));

    /* The failure this guards: falling through to `v.toFloat()` gives 0 and leaves the
       string empty, so the preset applies no look and says nothing. */
    QVERIFY2(!p.lookLut.isEmpty(),
             "lookLut fell through to the numeric tail of assignParam");

    /* A key with a slash in it must survive intact -- the key IS a path under the Looks
       folder, which is the whole reason looks are not keyed by bare name. */
    EditParams q;
    DevelopPresets::assignParam("lookLut", QVariant("Kodak/Portra/400 warm"), q);
    QCOMPARE(q.lookLut, QString("Kodak/Portra/400 warm"));
}

void TstLookPreset::cameraProfileIsReadAsAString()
{
    /* Same hazard, same branch, and it had no test either. */
    EditParams p;
    DevelopPresets::assignParam("cameraProfile", QVariant("Camera Vivid"), p);
    QCOMPARE(p.cameraProfile, QString("Camera Vivid"));
}

void TstLookPreset::viewTransformIsReadAsAnInt()
{
    /* The third whole-image field: an int, read with toInt() not the float tail. */
    EditParams p;
    DevelopPresets::assignParam("viewTransform", QVariant(2), p);
    QCOMPARE(p.viewTransform, 2);
}

void TstLookPreset::aNumericFieldIsStillRaw()
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

void TstLookPreset::unknownKeysAreIgnored()
{
    /* No final else, deliberately: a preset from a newer build must apply what it can
       rather than refuse. The cost is that a typo is silent, which is why the real keys
       above are asserted one by one. */
    EditParams p;
    const EditParams def;
    DevelopPresets::assignParam("lookLutt", QVariant("Fuji/Provia"), p);
    DevelopPresets::assignParam("somethingFromTheFuture", QVariant(3), p);
    QCOMPARE(p.lookLut, def.lookLut);
    QVERIFY(p.isIdentity());
}

QTEST_MAIN(TstLookPreset)
#include "tst_lookpreset.moc"
