/*
    EditParams::lookLut -- the film look's place in the recipe.

    WHY THIS TEST EXISTS. The look is the first parameter the RENDER does not read.
    Develop::Apply never sees it; OutputTransform does. That breaks the assumption every
    other param quietly relies on, and it puts the whole weight of two separate
    guarantees on one function:

      o paramsToJson is the SIDECAR, so a look must round-trip to disk;
      o paramsToJson is ALSO the per-scope render-cache key (MW::developCompositeStack),
        so it is the only thing that makes changing a look re-render at all. Omit the
        field and the app shows a stale picture AND forgets the choice, with nothing
        failing anywhere.

    So the cache-key property is asserted as a property, not argued in a comment: two
    recipes differing only in their look must serialise differently.

    The other half is the EXCLUSIVITY invariant. The Profile row writes a camera profile
    or a look, never both, so "both set" is a state only a hand-edited sidecar can reach
    -- and honouring it would apply a stage-0 replacement AND an output-stage grade, a
    rendering no UI could show or undo.
*/

#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>

#include "Develop/editstack.h"

class TstLookParams : public QObject
{
    Q_OBJECT

private slots:
    void emptyLookIsOmittedFromTheSidecar();
    void roundTripsThroughJson();
    void isForwardTolerant();
    void changingTheLookChangesTheCacheKey();
    void identityAccountsForTheLook();
    void resetClearsTheLook();
    void sanitiseRejectsImplausibleKeys();
    void sanitiseEnforcesExclusivity();
};

void TstLookParams::emptyLookIsOmittedFromTheSidecar()
{
    /* An image with no look must write EXACTLY the sidecar it wrote before looks
       existed -- otherwise every stored recipe's cache key moves on upgrade and the whole
       library re-renders once for nothing. */
    const EditParams def;
    const QJsonObject o = EditStack::paramsToJson(def);
    QVERIFY2(!o.contains("lookLut"), "an unset look was written to the sidecar");
    QVERIFY2(!o.contains("cameraProfile"), "an unset profile was written");
}

void TstLookParams::roundTripsThroughJson()
{
    EditParams p;
    p.lookLut = "Fuji/Classic Chrome";
    const QJsonObject o = EditStack::paramsToJson(p);
    QVERIFY(o.contains("lookLut"));
    QCOMPARE(o.value("lookLut").toString(), QString("Fuji/Classic Chrome"));

    const EditParams back = EditStack::paramsFromJson(o);
    QCOMPARE(back.lookLut, p.lookLut);

    /* And through the text form the sidecar actually stores. */
    const QByteArray bytes = QJsonDocument(o).toJson(QJsonDocument::Compact);
    const QJsonObject reread = QJsonDocument::fromJson(bytes).object();
    QCOMPARE(EditStack::paramsFromJson(reread).lookLut, p.lookLut);
}

void TstLookParams::isForwardTolerant()
{
    /* A sidecar written by an older build has no lookLut key: it must read back as the
       default, not as an empty-string-that-was-deliberately-chosen or a warning. */
    QJsonObject old;
    old["exposure"] = 0.5;
    const EditParams p = EditStack::paramsFromJson(old);
    QVERIFY(p.lookLut.isEmpty());

    /* And a key this build does not know must be ignored rather than refused -- the same
       contract that lets a future field be added without a version bump. */
    QJsonObject future = EditStack::paramsToJson(EditParams());
    future["someFutureThing"] = "hello";
    const EditParams q = EditStack::paramsFromJson(future);
    QVERIFY(q.lookLut.isEmpty());
}

void TstLookParams::changingTheLookChangesTheCacheKey()
{
    /*
        THE PROPERTY THE WHOLE DESIGN RESTS ON. Nothing in Develop::Apply reads lookLut,
        so this serialisation is the ONLY signal that a look changed. If these two ever
        compare equal, switching look will show the previous render from cache.
    */
    EditParams a;
    EditParams b;
    b.lookLut = "Kodak/Portra 400";
    const QByteArray ja = QJsonDocument(EditStack::paramsToJson(a)).toJson();
    const QByteArray jb = QJsonDocument(EditStack::paramsToJson(b)).toJson();
    QVERIFY2(ja != jb, "a look change does not move the render cache key");

    EditParams c;
    c.lookLut = "Kodak/Portra 160";       // a different look, not merely a set one
    const QByteArray jc = QJsonDocument(EditStack::paramsToJson(c)).toJson();
    QVERIFY2(jb != jc, "two different looks share a cache key");
}

void TstLookParams::identityAccountsForTheLook()
{
    EditParams p;
    QVERIFY(p.isIdentity());
    p.lookLut = "Fuji/Provia";
    QVERIFY2(!p.isIdentity(),
             "an image with a look reported itself untouched -- it would render through "
             "the identity early-out and never see the look");
}

void TstLookParams::resetClearsTheLook()
{
    /* The look sits in Basic, so Reset Basic must clear it; miss this and the section's
       Reset button and its Preview eye both silently skip the row. */
    EditParams p;
    p.lookLut = "Fuji/Provia";
    p.cameraProfile = "Adobe Standard";
    EditParams::resetGroup(p, EditParams::Group::Basic);
    QVERIFY(p.lookLut.isEmpty());
    QVERIFY(p.cameraProfile.isEmpty());
    QVERIFY(p.isIdentity());
}

void TstLookParams::sanitiseRejectsImplausibleKeys()
{
    const EditParams def;

    /* Too long: a bound on hand-edited input, not a format limit. */
    {
        EditParams p;
        p.lookLut = QString("x").repeated(EditStack::kMaxLookNameLen + 1);
        QStringList issues;
        EditStack::sanitizeParams(p, "test", &issues);
        QCOMPARE(p.lookLut, def.lookLut);
        QVERIFY2(!issues.isEmpty(), "a repair was made without saying so");
    }
    /* An embedded NUL, which truncates the string in half the places it is used. */
    {
        EditParams p;
        p.lookLut = QString("Fuji/Provia") + QChar(u'\0') + "evil";
        QStringList issues;
        EditStack::sanitizeParams(p, "test", &issues);
        QCOMPARE(p.lookLut, def.lookLut);
    }
    /* A control character: this string reaches a combo, a hash key and a tooltip. */
    {
        EditParams p;
        p.lookLut = QString("Fuji/Pro") + QChar(u'\n') + "via";
        QStringList issues;
        EditStack::sanitizeParams(p, "test", &issues);
        QCOMPARE(p.lookLut, def.lookLut);
    }
    /* A perfectly ordinary key must survive untouched -- including its slash, which is
       the whole point of keying on a path. */
    {
        EditParams p;
        p.lookLut = "Fuji/Classic Chrome";
        QStringList issues;
        EditStack::sanitizeParams(p, "test", &issues);
        QCOMPARE(p.lookLut, QString("Fuji/Classic Chrome"));
        QVERIFY(issues.isEmpty());
    }
}

void TstLookParams::sanitiseEnforcesExclusivity()
{
    EditParams p;
    p.cameraProfile = "Camera Vivid";
    p.lookLut = "Fuji/Provia";
    QStringList issues;
    EditStack::sanitizeParams(p, "test", &issues);
    QVERIFY2(p.cameraProfile.isEmpty(),
             "a sidecar naming both a profile and a look kept both -- the render would "
             "apply a stage-0 replacement AND an output grade");
    QCOMPARE(p.lookLut, QString("Fuji/Provia"));
    QVERIFY2(!issues.isEmpty(), "the repair was silent");

    /* Either one alone is untouched. */
    EditParams a;
    a.cameraProfile = "Camera Vivid";
    EditStack::sanitizeParams(a, "test", nullptr);
    QCOMPARE(a.cameraProfile, QString("Camera Vivid"));

    EditParams b;
    b.lookLut = "Fuji/Provia";
    EditStack::sanitizeParams(b, "test", nullptr);
    QCOMPARE(b.lookLut, QString("Fuji/Provia"));
}

QTEST_MAIN(TstLookParams)
#include "tst_lookparams.moc"
