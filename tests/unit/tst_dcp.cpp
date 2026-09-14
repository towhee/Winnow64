/*
    Dcp::parseFile / parseIfd -- reading a DNG camera profile.

    WHY THIS TEST EXISTS. A DCP is a bag of optional tags, and every way of getting one
    wrong is SILENT: a table read with the wrong dimensions shears the hue axis into
    colours that look deliberate, a ForwardMatrix swapped for a ColorMatrix renders a
    plausible-looking cast, and a missing tag left as an identity matrix is
    indistinguishable from a profile that legitimately omits it. None of that fails
    loudly enough to notice from a picture, so it is pinned here instead.

    The bulk of the test builds a synthetic .dcp byte by byte, so the expected values are
    written down rather than measured, and nobody's licensed profile has to be copied
    into the repo. The last test sweeps the REAL profiles on the machine if they happen
    to be installed, and skips if they are not.

    Pinned here:
      o every tag family round-trips: strings, illuminant codes, both matrix families,
        both lookup tables (with their shared dims tag), the tone curve, the scalars
      o a single-illuminant profile parses -- ProfileHueSatMapDims is SHARED between the
        two tables, so keying presence on the dims tag reports an absent second table as
        a corrupt one
      o malformed tags are REJECTED rather than partially accepted: a short matrix, a
        table whose count disagrees with its dims, a file with no ColorMatrix1
      o illuminantKelvin's standard values, which the interpolation is keyed on
*/

#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QDirIterator>
#include <cstring>

#include "ImageFormats/Dcp/dcp.h"

class TstDcp : public QObject
{
    Q_OBJECT

private slots:
    void fullProfileRoundTrips();
    void singleIlluminantProfileParses();
    void rejectsMalformed_data();
    void rejectsMalformed();
    void illuminantTemperatures();
    void realProfilesParse();

private:
    /* ---- a little TIFF writer (little endian, one IFD, values out of line) ---- */

    struct TagVal { quint16 tag; quint16 type; quint32 count; QByteArray val; };

    static QByteArray u16(quint16 v)
    {
        QByteArray a(2, '\0');
        uchar *p = reinterpret_cast<uchar *>(a.data());
        p[0] = uchar(v); p[1] = uchar(v >> 8);
        return a;
    }
    static QByteArray u32(quint32 v)
    {
        QByteArray a(4, '\0');
        uchar *p = reinterpret_cast<uchar *>(a.data());
        p[0] = uchar(v); p[1] = uchar(v >> 8); p[2] = uchar(v >> 16); p[3] = uchar(v >> 24);
        return a;
    }
    static QByteArray f32(float f)
    {
        quint32 bits; std::memcpy(&bits, &f, 4);
        return u32(bits);
    }
    /* SRATIONAL over a fixed denominator -- how DNG stores its matrices. */
    static QByteArray srat(double d)
    {
        const qint32 den = 1000000;
        const qint32 num = qint32(qRound(d * den));
        return u32(quint32(num)) + u32(quint32(den));
    }

    static TagVal tAscii(quint16 tag, const QByteArray &s)
    {
        const QByteArray z = s + '\0';
        return { tag, 2, quint32(z.size()), z };
    }
    static TagVal tShort(quint16 tag, quint16 v)   { return { tag, 3, 1, u16(v) }; }
    static TagVal tLong(quint16 tag, quint32 v)    { return { tag, 4, 1, u32(v) }; }
    static TagVal tLongs(quint16 tag, const QVector<quint32> &v)
    {
        QByteArray b; for (quint32 x : v) b += u32(x);
        return { tag, 4, quint32(v.size()), b };
    }
    static TagVal tSrats(quint16 tag, const QVector<double> &v)
    {
        QByteArray b; for (double x : v) b += srat(x);
        return { tag, 10, quint32(v.size()), b };
    }
    static TagVal tFloats(quint16 tag, const QVector<float> &v)
    {
        QByteArray b; for (float x : v) b += f32(x);
        return { tag, 11, quint32(v.size()), b };
    }

    /* Assemble the tags into a .dcp. Entries must be in ascending tag order (TIFF requires
       it and a real reader may rely on it), so they are sorted here. */
    static QByteArray buildDcp(QList<TagVal> tags, quint16 magic = 0x4352)
    {
        std::sort(tags.begin(), tags.end(),
                  [](const TagVal &a, const TagVal &b) { return a.tag < b.tag; });

        const quint32 ifdOff = 8;
        const quint32 valOff = ifdOff + 2 + quint32(tags.size()) * 12 + 4;

        QByteArray entries, values;
        for (const TagVal &t : tags) {
            entries += u16(t.tag) + u16(t.type) + u32(t.count);
            if (t.val.size() <= 4) {
                entries += t.val.leftJustified(4, '\0');
            } else {
                entries += u32(valOff + quint32(values.size()));
                values += t.val;
                if (values.size() & 1) values += '\0';      // keep values word aligned
            }
        }
        return QByteArray("II") + u16(magic) + u32(ifdOff)
             + u16(quint16(tags.size())) + entries + u32(0) + values;
    }

    /* Write bytes to a file inside dir and parse them. */
    bool parseBytes(const QByteArray &bytes, Dcp::Profile &out, QString *err = nullptr)
    {
        const QString path = dir.filePath(QString("p%1.dcp").arg(++serial));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        f.write(bytes);
        f.close();
        return Dcp::parseFile(path, out, err);
    }

    /* The canonical well-formed profile the positive tests read. */
    QList<TagVal> goodTags() const;

    QTemporaryDir dir;
    int serial = 0;
};

/* Tag numbers, spelled out here rather than shared with dcp.cpp: a test that imported the
   production enum would agree with it even if both were wrong. */
enum {
    UniqueCameraModel = 50708, ColorMatrix1 = 50721, ColorMatrix2 = 50722,
    CameraCalibration1 = 50723, AnalogBalance = 50727, BaselineExposure = 50730,
    CalibrationIlluminant1 = 50778, CalibrationIlluminant2 = 50779,
    ProfileCalibrationSig = 50932, ProfileName = 50936, ProfileHueSatMapDims = 50937,
    ProfileHueSatMapData1 = 50938, ProfileHueSatMapData2 = 50939, ProfileToneCurve = 50940,
    ProfileEmbedPolicy = 50941, ProfileCopyright = 50942, ForwardMatrix1 = 50964,
    ForwardMatrix2 = 50965, ProfileLookTableDims = 50981, ProfileLookTableData = 50982,
    ProfileHueSatMapEncoding = 51107, BaselineExposureOffset = 51109,
    DefaultBlackRender = 51110
};

/* Real values, from a shipped Adobe Standard profile, so the numbers are the shape the
   parser will actually meet (small, signed, some negative). */
static const QVector<double> kColor1 = { 0.7952, -0.1689, -0.0575,
                                        -0.3746,  1.0825,  0.3378,
                                        -0.0405,  0.1362,  0.6120 };
static const QVector<double> kColor2 = { 0.7377, -0.0742, -0.0998,
                                        -0.4235,  1.1981,  0.2549,
                                        -0.0673,  0.1918,  0.5538 };
static const QVector<double> kFwd1   = { 0.5407,  0.2506,  0.1730,
                                         0.3306,  0.6136,  0.0558,
                                         0.1852,  0.0007,  0.6392 };
static const QVector<double> kFwd2   = { 0.5388,  0.1799,  0.2457,
                                         0.3091,  0.6107,  0.0802,
                                         0.1438,  0.0001,  0.6812 };

/* A 2 x 2 x 1 HueSatMap: small enough to write out, big enough to have a shape. */
static QVector<float> hsm(float bias)
{
    QVector<float> v;
    for (int i = 0; i < 4; ++i) {
        v << 10.0f * i + bias      // hue shift, degrees
          << 1.0f + 0.1f * i       // saturation scale
          << 1.0f - 0.05f * i;     // value scale
    }
    return v;
}

QList<TstDcp::TagVal> TstDcp::goodTags() const
{
    return {
        tAscii(UniqueCameraModel, "Canon EOS 77D"),
        tAscii(ProfileName, "Adobe Standard"),
        tAscii(ProfileCopyright, "Copyright 2017 Adobe Systems, Inc."),
        tAscii(ProfileCalibrationSig, "com.adobe"),
        tShort(CalibrationIlluminant1, 17),         // Standard light A
        tShort(CalibrationIlluminant2, 21),         // D65
        tSrats(ColorMatrix1, kColor1),
        tSrats(ColorMatrix2, kColor2),
        tSrats(ForwardMatrix1, kFwd1),
        tSrats(ForwardMatrix2, kFwd2),
        tSrats(AnalogBalance, { 1.0, 1.0, 1.0 }),
        tSrats(BaselineExposure, { 0.25 }),
        tLongs(ProfileHueSatMapDims, { 2, 2, 1 }),
        tFloats(ProfileHueSatMapData1, hsm(0.0f)),
        tFloats(ProfileHueSatMapData2, hsm(1.0f)),
        tLong(ProfileHueSatMapEncoding, 0),
        tLongs(ProfileLookTableDims, { 2, 1, 1 }),
        tFloats(ProfileLookTableData, { 0, 1.2f, 1, 5, 1.3f, 1 }),
        tFloats(ProfileToneCurve, { 0, 0, 0.5f, 0.6f, 1, 1 }),
        tSrats(BaselineExposureOffset, { -0.5 }),
        tLong(ProfileEmbedPolicy, 0),
        tLong(DefaultBlackRender, 1)
    };
}

void TstDcp::fullProfileRoundTrips()
{
    QVERIFY(dir.isValid());

    Dcp::Profile p;
    QString err;
    QVERIFY2(parseBytes(buildDcp(goodTags()), p, &err), qPrintable(err));
    QVERIFY(p.valid);

    QCOMPARE(p.uniqueCameraModel, QString("Canon EOS 77D"));
    QCOMPARE(p.name, QString("Adobe Standard"));
    QCOMPARE(p.calibrationSignature, QString("com.adobe"));
    QVERIFY(p.copyright.startsWith("Copyright 2017"));
    QCOMPARE(p.embedPolicy, 0);
    QCOMPARE(p.defaultBlackRender, true);
    QVERIFY(!p.hasThirdIlluminant);

    QCOMPARE(p.cal[0].illuminant, 17);
    QCOMPARE(p.cal[1].illuminant, 21);
    QVERIFY(p.dual());

    /* THE MATRIX FAMILIES MUST NOT BE SWAPPED. ColorMatrix and ForwardMatrix are both
       9 signed values of similar magnitude, so a swap costs nothing at parse time and
       everything at render time. Each is checked against its own numbers. */
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            QVERIFY(qAbs(p.cal[0].color.m[i][j]   - float(kColor1[i * 3 + j])) < 1e-5f);
            QVERIFY(qAbs(p.cal[1].color.m[i][j]   - float(kColor2[i * 3 + j])) < 1e-5f);
            QVERIFY(qAbs(p.cal[0].forward.m[i][j] - float(kFwd1[i * 3 + j]))   < 1e-5f);
            QVERIFY(qAbs(p.cal[1].forward.m[i][j] - float(kFwd2[i * 3 + j]))   < 1e-5f);
        }
    }
    QVERIFY(p.cal[0].haveColor && p.cal[0].haveForward);
    QVERIFY(p.cal[1].haveColor && p.cal[1].haveForward);
    /* Absent, and reported as absent rather than as an identity the caller cannot tell
       from a real one -- CameraCalibration is the per-individual-camera tweak and is
       almost always missing. */
    QVERIFY(!p.cal[0].haveCalibration);

    QCOMPARE(p.cal[0].hueSatMap.hueDivs, 2);
    QCOMPARE(p.cal[0].hueSatMap.satDivs, 2);
    QCOMPARE(p.cal[0].hueSatMap.valDivs, 1);
    QCOMPARE(p.cal[0].hueSatMap.entries(), 4);
    QCOMPARE(int(p.cal[0].hueSatMap.v.size()), 12);
    QCOMPARE(p.cal[0].hueSatMap.encoding, 0);
    /* The two tables share one dims tag, so they must come back the same shape -- that is
       what makes interpolating between them entry by entry legal. */
    QCOMPARE(p.cal[1].hueSatMap.hueDivs, p.cal[0].hueSatMap.hueDivs);
    QCOMPARE(int(p.cal[1].hueSatMap.v.size()), int(p.cal[0].hueSatMap.v.size()));
    /* ...and they must be the DIFFERENT tables, not the same one read twice. */
    QVERIFY(qAbs(p.cal[0].hueSatMap.v[0] - 0.0f) < 1e-5f);
    QVERIFY(qAbs(p.cal[1].hueSatMap.v[0] - 1.0f) < 1e-5f);

    QCOMPARE(p.lookTable.hueDivs, 2);
    QCOMPARE(p.lookTable.satDivs, 1);
    QCOMPARE(int(p.lookTable.v.size()), 6);
    QVERIFY(qAbs(p.lookTable.v[1] - 1.2f) < 1e-5f);

    QCOMPARE(int(p.toneCurve.size()), 6);
    QVERIFY(qAbs(p.toneCurve[3] - 0.6f) < 1e-5f);

    QVERIFY(qAbs(p.baselineExposure - 0.25f) < 1e-5f);
    QVERIFY(qAbs(p.baselineExposureOffset + 0.5f) < 1e-5f);
    QVERIFY(qAbs(p.analogBalance[1] - 1.0f) < 1e-5f);
}

void TstDcp::singleIlluminantProfileParses()
{
    QVERIFY(dir.isValid());

    QList<TagVal> tags;
    for (const TagVal &t : goodTags()) {
        if (t.tag == ColorMatrix2 || t.tag == ForwardMatrix2 ||
            t.tag == ProfileHueSatMapData2 || t.tag == CalibrationIlluminant2) continue;
        tags << t;
    }

    Dcp::Profile p;
    QString err;
    QVERIFY2(parseBytes(buildDcp(tags), p, &err), qPrintable(err));
    QVERIFY(p.valid);

    QVERIFY(p.cal[0].haveColor);
    QVERIFY(!p.cal[1].haveColor);
    /* The regression this guards: ProfileHueSatMapDims is still present (it belongs to
       table 1), so a presence test keyed on the dims tag calls the absent second table
       corrupt and fails the whole profile. */
    QVERIFY(!p.cal[0].hueSatMap.isEmpty());
    QVERIFY(p.cal[1].hueSatMap.isEmpty());
    QVERIFY(!p.dual());
}

void TstDcp::rejectsMalformed_data()
{
    QTest::addColumn<QString>("kind");
    QTest::newRow("wrong magic")       << "magic";
    QTest::newRow("short ColorMatrix") << "shortMatrix";
    QTest::newRow("table count != dims") << "badTable";
    QTest::newRow("no ColorMatrix1")   << "noColor";
    QTest::newRow("odd tone curve")    << "oddCurve";
}

void TstDcp::rejectsMalformed()
{
    QVERIFY(dir.isValid());
    QFETCH(QString, kind);

    QList<TagVal> tags;
    quint16 magic = 0x4352;
    for (const TagVal &t : goodTags()) {
        if (kind == "shortMatrix" && t.tag == ColorMatrix1) {
            tags << tSrats(ColorMatrix1, kColor1.mid(0, 8));
            continue;
        }
        if (kind == "badTable" && t.tag == ProfileHueSatMapDims) {
            tags << tLongs(ProfileHueSatMapDims, { 3, 2, 1 });   // data is still 2 x 2 x 1
            continue;
        }
        if (kind == "noColor" && t.tag == ColorMatrix1) continue;
        if (kind == "oddCurve" && t.tag == ProfileToneCurve) {
            tags << tFloats(ProfileToneCurve, { 0, 0, 0.5f });
            continue;
        }
        tags << t;
    }
    if (kind == "magic") magic = 42 + 1;

    Dcp::Profile p;
    QString err;
    QVERIFY(!parseBytes(buildDcp(tags, magic), p, &err));
    QVERIFY(!p.valid);
    QVERIFY2(!err.isEmpty(), "a rejection must say why");
}

void TstDcp::illuminantTemperatures()
{
    /* The interpolation is keyed on these, so they are part of the format, not a detail. */
    QCOMPARE(Dcp::illuminantKelvin(17), 2856.0f);   // Standard light A -- the warm end
    QCOMPARE(Dcp::illuminantKelvin(21), 6504.0f);   // D65 -- the cool end of nearly every DCP
    QCOMPARE(Dcp::illuminantKelvin(23), 5003.0f);   // D50
    /* Unknown / Other name no temperature, and must say so rather than guess one: a
       fabricated temperature would silently mis-weight the interpolation. */
    QVERIFY(Dcp::illuminantKelvin(0) == 0.0f);
    QVERIFY(Dcp::illuminantKelvin(255) == 0.0f);
}

void TstDcp::realProfilesParse()
{
    /* Opportunistic: sweep whatever real profiles are installed. They are licensed to
       their authors and are NOT copied into the repo, so this skips when absent -- the
       synthetic tests above are the ones that must always run. */
#ifdef Q_OS_MAC
    const QString root = "/Library/Application Support/Adobe/CameraRaw/CameraProfiles";
#else
    const QString root = "C:/ProgramData/Adobe/CameraRaw/CameraProfiles";
#endif
    QDir d(root);
    if (!d.exists()) QSKIP("no camera profiles installed on this machine");

    /* STRIDE-SAMPLE the whole tree rather than taking the first N in iteration order.
       The two top-level folders hold different KINDS of profile -- "Adobe Standard"
       carries a HueSatMap and no LookTable, "Camera" (Camera Standard, Camera Vivid, ...)
       carries a LookTable and no HueSatMap -- and iteration order returns one folder
       entirely before the other, so an unstrided cap exercises only half the parser and
       reports the other half's tables as universally absent. */
    QStringList paths;
    QDirIterator it(root, QStringList() << "*.dcp", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) paths << it.next();
    if (paths.isEmpty()) QSKIP("no camera profiles installed on this machine");
    paths.sort();

    const int wanted = 300;
    const int stride = qMax(1, paths.size() / wanted);

    int parsed = 0, failed = 0, dual = 0, withLook = 0, withHsm = 0, withForward = 0;
    QString firstErr, firstErrFile;
    for (int i = 0; i < paths.size(); i += stride) {
        const QString path = paths[i];
        Dcp::Profile p;
        QString err;
        if (!Dcp::parseFile(path, p, &err)) {
            ++failed;
            if (firstErr.isEmpty()) { firstErr = err; firstErrFile = path; }
            continue;
        }
        ++parsed;
        QVERIFY2(!p.uniqueCameraModel.isEmpty(),
                 qPrintable("no UniqueCameraModel in " + path));
        QVERIFY2(p.cal[0].haveColor, qPrintable("no ColorMatrix1 in " + path));
        if (p.dual()) ++dual;
        if (p.cal[0].haveForward) ++withForward;
        if (!p.lookTable.isEmpty()) ++withLook;
        if (!p.cal[0].hueSatMap.isEmpty()) ++withHsm;
    }

    qInfo() << "parsed" << parsed << "of" << paths.size() << "profiles;" << dual
            << "dual-illuminant," << withForward << "with a ForwardMatrix,"
            << withHsm << "with a HueSatMap," << withLook << "with a LookTable";
    QVERIFY2(failed == 0,
             qPrintable(QString("%1 profile(s) failed, first: %2 (%3)")
                            .arg(failed).arg(firstErrFile, firstErr)));
    QVERIFY(parsed > 0);
    /* Both table kinds must actually have been read. Zero of either means the tag is
       being missed, not that no profile has one. */
    QVERIFY(withHsm > 0);
    QVERIFY(withLook > 0);
    QVERIFY(withForward > 0);
    /* Adobe's profiles are two-illuminant by construction; if none are, the illuminant
       tags are being misread. */
    QVERIFY(dual > parsed / 2);
}

QTEST_MAIN(TstDcp)
#include "tst_dcp.moc"
