/*
    canonicalCameraModel -- the one string a camera body is identified by.

    WHY THIS TEST EXISTS. Two tables key on the model: the generated matrix table
    (cameramatrix.cpp, from libraw) and a DCP's UniqueCameraModel. Both want the model
    maker-prefixed and spaced, and EXIF supplies neither for Olympus, Fujifilm or
    Panasonic -- so those three makers used to miss BOTH lookups. Nothing said so: an
    unmatched matrix falls back to identity (a plausible-looking cast, not an error)
    and an unmatched profile shows an empty Profile dropdown.

    The cases below come in two kinds.

    PURE STRING cases pin the transformation itself against the real EXIF strings read off
    the sample files, and run everywhere.

    RESOLUTION cases are the ones that matter: they assert that a canonicalised model
    actually FINDS something -- a matrix in the built-in table, and a profile in whatever
    is installed on this machine. Those are end-to-end claims about real data rather than
    about a QString, which is the only way the original defect would have been caught.
    The profile half skips when no profiles are installed; the matrix half never skips,
    because the table is compiled in.
*/

#include <QtTest>
#include <QDir>
#include <QSignalSpy>

#include "ImageFormats/Raw/cameramodel.h"
#include "ImageFormats/Raw/cameramatrix.h"
#include "Develop/cameraprofilestore.h"

class TstCameraModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void prefixesMakersExifOmits_data();
    void prefixesMakersExifOmits();

    void keepsMakersExifAlreadyCarries_data();
    void keepsMakersExifAlreadyCarries();

    void spacesOutGluedNames_data();
    void spacesOutGluedNames();

    void leavesUnrelatedNamesAlone_data();
    void leavesUnrelatedNamesAlone();

    void handlesEmptyAndUnknownMakers();

    void olympusAliasesBothWays();

    void everySampleResolvesToAMatrix_data();
    void everySampleResolvesToAMatrix();

    void everySampleResolvesToAProfile_data();
    void everySampleResolvesToAProfile();

private:
    bool profilesInstalled = false;
};

/*
    The bodies in the sample set, as EXIF really spells them. Read off
    Pictures/_ThumbTest: Make/Model from tag 271/272, except the RAF matrix path, which
    takes the camera id from the RAF header instead (there is no Make there at all).
    The expected column is what BOTH tables spell the same body.
*/
static void addSamples()
{
    QTest::addColumn<QString>("make");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("expected");

    QTest::newRow("ORF E-M1")
        << "OLYMPUS IMAGING CORP.  " << "E-M1            " << "Olympus E-M1";
    QTest::newRow("ORF OM-1 Mark II")
        << "OM Digital Solutions   " << "OM-1MarkII      " << "Olympus OM-1 Mark II";
    QTest::newRow("RAF X-T2")      << "FUJIFILM" << "X-T2"      << "Fujifilm X-T2";
    QTest::newRow("RAF X-T50")     << "FUJIFILM" << "X-T50"     << "Fujifilm X-T50";
    QTest::newRow("RAF GFX 50S II")<< "FUJIFILM" << "GFX50S II" << "Fujifilm GFX 50S II";
    QTest::newRow("RW2 GX9")       << "Panasonic" << "DC-GX9"   << "Panasonic DC-GX9";
}

void TstCameraModel::initTestCase()
{
    for (const QString &r : CameraProfileStore::roots())
        if (QDir(r).exists()) profilesInstalled = true;
    if (!profilesInstalled) return;

    CameraProfileStore &store = CameraProfileStore::instance();
    QSignalSpy spy(&store, &CameraProfileStore::indexChanged);
    store.ensureIndex();
    if (!store.indexReady())
        QVERIFY2(spy.wait(60000), "the profile index did not finish building");
}

void TstCameraModel::prefixesMakersExifOmits_data() { addSamples(); }

void TstCameraModel::prefixesMakersExifOmits()
{
    QFETCH(QString, make);
    QFETCH(QString, model);
    QFETCH(QString, expected);
    QCOMPARE(canonicalCameraModel(make, model), expected);
}

void TstCameraModel::keepsMakersExifAlreadyCarries_data()
{
    QTest::addColumn<QString>("make");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("expected");

    /* Nikon and Canon put the maker in tag 272 themselves, and prefixing it again would
       break both lookups just as surely as omitting it did. */
    QTest::newRow("nikon")  << "NIKON CORPORATION" << "NIKON D850"   << "NIKON D850";
    QTest::newRow("canon")  << "Canon"             << "Canon EOS R5" << "Canon EOS R5";
    QTest::newRow("pentax") << "RICOH IMAGING COMPANY, LTD."
                            << "PENTAX K-3 Mark III" << "PENTAX K-3 Mark III";
    /* A parser that prefixed the model itself (Sony's did, before this helper) must be
       idempotent through it, or re-canonicalising a stored model doubles the maker. */
    QTest::newRow("already prefixed") << "SONY" << "Sony ILCE-9M2" << "Sony ILCE-9M2";
}

void TstCameraModel::keepsMakersExifAlreadyCarries()
{
    QFETCH(QString, make);
    QFETCH(QString, model);
    QFETCH(QString, expected);
    QCOMPARE(canonicalCameraModel(make, model), expected);
}

void TstCameraModel::spacesOutGluedNames_data()
{
    QTest::addColumn<QString>("make");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("expected");

    QTest::newRow("E-M1MarkIII")
        << "OLYMPUS CORPORATION" << "E-M1MarkIII" << "Olympus E-M1 Mark III";
    QTest::newRow("E-M5MarkII")
        << "OLYMPUS IMAGING CORP." << "E-M5MarkII" << "Olympus E-M5 Mark II";
    QTest::newRow("GFX100S") << "FUJIFILM" << "GFX100S" << "Fujifilm GFX 100S";
    /* IDEMPOTENT: a name that is already spaced must survive unchanged, or the second
       pass undoes the first. */
    QTest::newRow("already spaced mark")
        << "OLYMPUS CORPORATION" << "E-M1 Mark III" << "Olympus E-M1 Mark III";
    QTest::newRow("already spaced gfx")
        << "FUJIFILM" << "GFX 100 II" << "Fujifilm GFX 100 II";
}

void TstCameraModel::spacesOutGluedNames()
{
    QFETCH(QString, make);
    QFETCH(QString, model);
    QFETCH(QString, expected);
    QCOMPARE(canonicalCameraModel(make, model), expected);
}

void TstCameraModel::leavesUnrelatedNamesAlone_data()
{
    QTest::addColumn<QString>("make");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("expected");

    /* The GFX rule is Fujifilm-only AND anchored, because the obvious general version of
       it ("letters then digits get a space") destroys the X100 family. */
    QTest::newRow("X100V")  << "FUJIFILM" << "X100V"  << "Fujifilm X100V";
    QTest::newRow("X100VI") << "FUJIFILM" << "X100VI" << "Fujifilm X100VI";
    QTest::newRow("X-T100") << "FUJIFILM" << "X-T100" << "Fujifilm X-T100";
    /* Not Fujifilm, so the GFX rule must not fire even on a model starting that way. */
    QTest::newRow("non-fuji GFX") << "Acme" << "GFX100" << "Acme GFX100";
    /* "Mark" not followed by a version is a word, not a gluing. */
    QTest::newRow("E-M1X") << "OLYMPUS CORPORATION" << "E-M1X" << "Olympus E-M1X";
    QTest::newRow("PEN-F") << "OLYMPUS IMAGING CORP." << "PEN-F" << "Olympus PEN-F";
}

void TstCameraModel::leavesUnrelatedNamesAlone()
{
    QFETCH(QString, make);
    QFETCH(QString, model);
    QFETCH(QString, expected);
    QCOMPARE(canonicalCameraModel(make, model), expected);
}

void TstCameraModel::handlesEmptyAndUnknownMakers()
{
    /* No model, no answer -- never a bare maker word, which would prefix-match half the
       table and hand back some other camera's matrix. */
    QCOMPARE(canonicalCameraModel("FUJIFILM", QString()), QString());
    QCOMPARE(canonicalCameraModel("FUJIFILM", "   "), QString());
    /* No make: the model stands as it is. */
    QCOMPARE(canonicalCameraModel(QString(), "X-T2"), QString("X-T2"));
    /* An unlisted maker still gets Make + Model, the convention both tables follow. */
    QCOMPARE(canonicalCameraModel("Acme Optics", "Widget 5"), QString("Acme Optics Widget 5"));
}

void TstCameraModel::olympusAliasesBothWays()
{
    /* The canonical spelling follows libraw ("Olympus OM-1"); Adobe files the same body
       under "OM Digital Solutions OM-1". Each must offer the other, or the profile lookup
       finds nothing for every OM-branded body. */
    QCOMPARE(cameraModelAliases("Olympus OM-1 Mark II"),
             QStringList() << "OM Digital Solutions OM-1 Mark II");
    QCOMPARE(cameraModelAliases("OM Digital Solutions OM-1"),
             QStringList() << "Olympus OM-1");
    /* A maker the two agree on has no aliases -- an extra candidate is a chance to match
       the wrong camera, not a free retry. */
    QVERIFY(cameraModelAliases("Fujifilm X-T2").isEmpty());
    QVERIFY(cameraModelAliases("Nikon D850").isEmpty());
    QVERIFY(cameraModelAliases(QString()).isEmpty());
}

void TstCameraModel::everySampleResolvesToAMatrix_data() { addSamples(); }

void TstCameraModel::everySampleResolvesToAMatrix()
{
    QFETCH(QString, make);
    QFETCH(QString, model);

    const QString raw = model.simplified();
    const QString canonical = canonicalCameraModel(make, model);

    float m[3][3];
    QVERIFY2(xyzToCamForModel(canonical, m),
             qPrintable(QString("no matrix for the canonical model '%1' -- the raw would "
                                "render through an identity matrix")
                            .arg(canonical)));

    /* And the canonicalisation is what did it: the bare EXIF model finds nothing. This is
       the regression the helper exists for, so it is asserted rather than assumed. */
    float bare[3][3];
    QVERIFY2(!xyzToCamForModel(raw, bare),
             qPrintable(QString("'%1' matched the table unprefixed -- this case no longer "
                                "guards anything").arg(raw)));
}

void TstCameraModel::everySampleResolvesToAProfile_data() { addSamples(); }

void TstCameraModel::everySampleResolvesToAProfile()
{
    if (!profilesInstalled) QSKIP("no camera profile folders on this machine");

    QFETCH(QString, make);
    QFETCH(QString, model);
    const QString canonical = canonicalCameraModel(make, model);

    CameraProfileStore &store = CameraProfileStore::instance();
    const QList<CameraProfileStore::Entry> entries = store.forModel(canonical);
    if (entries.isEmpty())
        QSKIP(qPrintable(QString("no profiles installed for %1").arg(canonical)));

    /* A real profile, not only a synthesised base -- a base is derived FROM real ones, so
       bases alone would mean the lookup found the wrong camera's files. */
    int real = 0;
    for (const CameraProfileStore::Entry &e : entries) if (!e.isBase) ++real;
    QVERIFY2(real > 0, qPrintable(QString("only derived bases for %1").arg(canonical)));

    /*
        HOW MANY is reported, not asserted, and the difference matters.

        A body with a full Adobe install gets ~6-13 rows: "Adobe Standard" from the
        CameraProfiles/Adobe Standard tree, the "Camera *" profiles from the Camera tree
        beside it, and the "Camera Base" derived from those. FUJIFILM GETS EXACTLY ONE,
        and that is NOT a canonicalisation failure -- the model resolves, there is simply
        almost nothing for it to resolve TO in the tree this store sweeps. Fuji's film
        simulations are not .dcp files at all: they live in Adobe's OTHER profile root
        (CameraRaw/Settings/Adobe/Profiles) as .xmp Look presets, which
        CameraProfileStore does not read. See "ADOBE LOOK PROFILES (.xmp)" in
        notes/Documentation.txt. No "Camera Base" is derived for Fuji either, because
        Adobe Standard carries a LookTable but no tone curve, which is what deriveBases
        selects on.

        Asserting a count here would only pin one machine's Adobe install; printing it
        keeps the asymmetry visible, so a thin Fuji list is read as the missing root it
        is rather than as this bug coming back.
    */
    qInfo("%s: %d entries (%d real, %d derived)", qPrintable(canonical),
          int(entries.size()), real, int(entries.size()) - real);

    /* And the entries actually open. */
    for (const CameraProfileStore::Entry &e : entries) {
        const auto p = store.profile(canonical, e.name);
        QVERIFY2(p != nullptr,
                 qPrintable(QString("profile '%1' for %2 did not load")
                                .arg(e.name, canonical)));
        QVERIFY(p->cal[0].haveColor);
    }

    /* The bare EXIF model must NOT resolve -- same guard as the matrix case. */
    QVERIFY2(store.forModel(model.simplified()).isEmpty(),
             qPrintable(QString("'%1' found profiles unprefixed -- this case no longer "
                                "guards anything").arg(model.simplified())));
}

QTEST_MAIN(TstCameraModel)
#include "tst_cameramodel.moc"
