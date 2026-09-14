#include "ImageFormats/Dcp/dcp.h"

#include <cmath>

namespace Dcp {

namespace {

/* DNG tag numbers for the profile tags. Verified against the DNG specification; the
   encoding tags in particular are 51107/51108 and NOT the 51089/51090 they are often
   misremembered as. */
enum Tag {
    UniqueCameraModel        = 50708,
    ColorMatrix1             = 50721,
    ColorMatrix2             = 50722,
    CameraCalibration1       = 50723,
    CameraCalibration2       = 50724,
    AnalogBalance            = 50727,
    BaselineExposure         = 50730,
    CalibrationIlluminant1   = 50778,
    CalibrationIlluminant2   = 50779,
    ProfileCalibrationSig    = 50932,
    ProfileName              = 50936,
    ProfileHueSatMapDims     = 50937,
    ProfileHueSatMapData1    = 50938,
    ProfileHueSatMapData2    = 50939,
    ProfileToneCurve         = 50940,
    ProfileEmbedPolicy       = 50941,
    ProfileCopyright         = 50942,
    ForwardMatrix1           = 50964,
    ForwardMatrix2           = 50965,
    ProfileLookTableDims     = 50981,
    ProfileLookTableData     = 50982,
    ProfileHueSatMapEncoding = 51107,
    ProfileLookTableEncoding = 51108,
    BaselineExposureOffset   = 51109,
    DefaultBlackRender       = 51110,
    /* DNG 1.6 tri-illuminant. Detected, not read -- see the header. */
    CalibrationIlluminant3   = 52529,
    ColorMatrix3             = 52531
};

/* A tag can be missing (legal), present and well formed, or present and wrong. The third
   case must not be reported as the first: a ColorMatrix with 8 values is a corrupt file,
   not a profile without a ColorMatrix, and silently leaving an identity matrix behind
   would render as a strong colour cast with nothing to point at. */
enum class Got { Absent, Ok, Bad };

/* Tables large enough to be an allocation attack rather than a profile. Adobe's largest
   ship at 90 x 30 x 16 (43200 entries); two orders of magnitude of headroom is plenty. */
constexpr int kMaxTableEntries = 2000000;
constexpr int kMaxToneCurve    = 8192;      // values, i.e. 4096 points

Got matrix(TiffWalk::Reader &r, const TiffWalk::Ifd &tags, quint16 tag, Matrix3 &out)
{
    if (!tags.contains(tag)) return Got::Absent;
    TiffWalk::Entry e = tags.value(tag);
    const QVector<double> v = r.reals(e);
    if (v.size() != 9) return Got::Bad;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out.m[i][j] = float(v[i * 3 + j]);
    return Got::Ok;
}

/* One value as a double, for the scalar rational / float tags (scalar() is integer-only). */
Got real1(TiffWalk::Reader &r, const TiffWalk::Ifd &tags, quint16 tag, float &out)
{
    if (!tags.contains(tag)) return Got::Absent;
    TiffWalk::Entry e = tags.value(tag);
    const QVector<double> v = r.reals(e);
    if (v.isEmpty()) return Got::Bad;
    out = float(v[0]);
    return Got::Ok;
}

/*
    A HueSatMap or a LookTable: three LONG dimensions in one tag, the data in another, an
    optional encoding in a third. The dims and the data are cross-checked against each
    other -- count must be exactly hue * sat * val * 3 -- because a table read with the
    wrong dimensions does not fail, it shears the hue axis and produces colours that look
    deliberate.
*/
Got table(TiffWalk::Reader &r, const TiffWalk::Ifd &tags,
          quint16 dimsTag, quint16 dataTag, quint16 encTag, Table3D &out)
{
    /* The DATA tag is what decides presence, not the dims: a single-illuminant profile
       carries ProfileHueSatMapDims and ProfileHueSatMapData1 with no Data2, so keying on
       the shared dims tag would report the absent second table as corrupt. */
    if (!tags.contains(dataTag)) return Got::Absent;
    if (!tags.contains(dimsTag)) return Got::Bad;

    TiffWalk::Entry de = tags.value(dimsTag);
    const QVector<quint32> dims = r.u32s(de);
    if (dims.size() != 3) return Got::Bad;
    const quint32 h = dims[0], s = dims[1], val = dims[2];
    if (h < 1 || s < 1 || val < 1) return Got::Bad;
    if (quint64(h) * s * val > quint64(kMaxTableEntries)) return Got::Bad;

    TiffWalk::Entry te = tags.value(dataTag);
    const QVector<double> v = r.reals(te);
    if (quint64(v.size()) != quint64(h) * s * val * 3) return Got::Bad;

    out.hueDivs = int(h);
    out.satDivs = int(s);
    out.valDivs = int(val);
    out.v.resize(size_t(v.size()));
    for (int i = 0; i < v.size(); ++i) out.v[size_t(i)] = float(v[i]);

    out.encoding = 0;
    if (tags.contains(encTag)) {
        TiffWalk::Entry ee = tags.value(encTag);
        out.encoding = int(r.scalar(ee));
    }
    return Got::Ok;
}

QString ascii(TiffWalk::Reader &r, const TiffWalk::Ifd &tags, quint16 tag)
{
    if (!tags.contains(tag)) return QString();
    TiffWalk::Entry e = tags.value(tag);
    return r.ascii(e).trimmed();
}

bool fail(QString *err, const QString &msg)
{
    if (err) *err = msg;
    return false;
}

} // namespace

bool Profile::dual() const
{
    return cal[0].haveColor && cal[1].haveColor &&
           cal[0].illuminant != cal[1].illuminant &&
           illuminantKelvin(cal[0].illuminant) > 0.0f &&
           illuminantKelvin(cal[1].illuminant) > 0.0f;
}

float illuminantKelvin(int code)
{
    /* EXIF LightSource codes. The ones that name a CIE illuminant or a standard daylight
       phase have a definite temperature; the rest (Unknown, Other, Flash-without-a-phase)
       return 0, which the interpolation reads as "cannot key on this". */
    switch (code) {
    case 1:  return 5500.0f;    // Daylight
    case 2:  return 4230.0f;    // Fluorescent (unspecified -- the CIE F-series average)
    case 3:  return 2856.0f;    // Tungsten (incandescent) == Standard A
    case 4:  return 5500.0f;    // Flash
    case 9:  return 5500.0f;    // Fine weather
    case 10: return 6504.0f;    // Cloudy
    case 11: return 7504.0f;    // Shade
    case 12: return 6430.0f;    // Daylight fluorescent   D  5700-7100 K
    case 13: return 5000.0f;    // Day white fluorescent  N  4600-5400 K
    case 14: return 4230.0f;    // Cool white fluorescent W  3900-4500 K
    case 15: return 3450.0f;    // White fluorescent      WW 3200-3700 K
    case 17: return 2856.0f;    // Standard light A
    case 18: return 4874.0f;    // Standard light B
    case 19: return 6774.0f;    // Standard light C
    case 20: return 5503.0f;    // D55
    case 21: return 6504.0f;    // D65
    case 22: return 7504.0f;    // D75
    case 23: return 5003.0f;    // D50
    case 24: return 3200.0f;    // ISO studio tungsten
    default: return 0.0f;
    }
}

bool parseIfd(TiffWalk::Reader &r, const TiffWalk::Ifd &tags, Profile &out, QString *err)
{
    out = Profile();

    out.uniqueCameraModel    = ascii(r, tags, UniqueCameraModel);
    out.name                 = ascii(r, tags, ProfileName);
    out.copyright            = ascii(r, tags, ProfileCopyright);
    out.calibrationSignature = ascii(r, tags, ProfileCalibrationSig);

    if (tags.contains(ProfileEmbedPolicy)) {
        TiffWalk::Entry e = tags.value(ProfileEmbedPolicy);
        out.embedPolicy = int(r.scalar(e));
    }
    if (tags.contains(DefaultBlackRender)) {
        TiffWalk::Entry e = tags.value(DefaultBlackRender);
        out.defaultBlackRender = r.scalar(e) != 0;
    }
    if (tags.contains(CalibrationIlluminant1)) {
        TiffWalk::Entry e = tags.value(CalibrationIlluminant1);
        out.cal[0].illuminant = int(r.scalar(e));
    }
    if (tags.contains(CalibrationIlluminant2)) {
        TiffWalk::Entry e = tags.value(CalibrationIlluminant2);
        out.cal[1].illuminant = int(r.scalar(e));
    }
    out.hasThirdIlluminant = tags.contains(CalibrationIlluminant3) || tags.contains(ColorMatrix3);

    struct Pair { quint16 color, forward, calib, hsm; };
    const Pair pair[2] = {
        { ColorMatrix1, ForwardMatrix1, CameraCalibration1, ProfileHueSatMapData1 },
        { ColorMatrix2, ForwardMatrix2, CameraCalibration2, ProfileHueSatMapData2 }
    };

    for (int i = 0; i < 2; ++i) {
        Calibration &c = out.cal[i];
        switch (matrix(r, tags, pair[i].color, c.color)) {
        case Got::Ok:     c.haveColor = true; break;
        case Got::Absent: break;
        case Got::Bad:    return fail(err, QString("DCP: ColorMatrix%1 is not 9 values.").arg(i + 1));
        }
        switch (matrix(r, tags, pair[i].forward, c.forward)) {
        case Got::Ok:     c.haveForward = true; break;
        case Got::Absent: break;
        case Got::Bad:    return fail(err, QString("DCP: ForwardMatrix%1 is not 9 values.").arg(i + 1));
        }
        switch (matrix(r, tags, pair[i].calib, c.calibration)) {
        case Got::Ok:     c.haveCalibration = true; break;
        case Got::Absent: break;
        case Got::Bad:    return fail(err, QString("DCP: CameraCalibration%1 is not 9 values.").arg(i + 1));
        }
        /* Both HueSatMaps share ONE dims tag -- the two tables are necessarily the same
           shape, which is what makes interpolating between them entry by entry legal. */
        if (table(r, tags, ProfileHueSatMapDims, pair[i].hsm,
                  ProfileHueSatMapEncoding, c.hueSatMap) == Got::Bad)
            return fail(err, QString("DCP: ProfileHueSatMapData%1 does not match its dims.").arg(i + 1));
    }

    if (!out.cal[0].haveColor)
        return fail(err, "DCP: no ColorMatrix1 -- not a camera profile.");

    if (table(r, tags, ProfileLookTableDims, ProfileLookTableData,
              ProfileLookTableEncoding, out.lookTable) == Got::Bad)
        return fail(err, "DCP: ProfileLookTableData does not match its dims.");

    if (tags.contains(AnalogBalance)) {
        TiffWalk::Entry e = tags.value(AnalogBalance);
        const QVector<double> v = r.reals(e);
        if (v.size() != 3) return fail(err, "DCP: AnalogBalance is not 3 values.");
        for (int i = 0; i < 3; ++i) out.analogBalance[i] = float(v[i]);
    }

    if (real1(r, tags, BaselineExposure, out.baselineExposure) == Got::Bad)
        return fail(err, "DCP: BaselineExposure is unreadable.");
    if (real1(r, tags, BaselineExposureOffset, out.baselineExposureOffset) == Got::Bad)
        return fail(err, "DCP: BaselineExposureOffset is unreadable.");

    if (tags.contains(ProfileToneCurve)) {
        TiffWalk::Entry e = tags.value(ProfileToneCurve);
        const QVector<double> v = r.reals(e);
        if (v.size() < 4 || v.size() % 2 != 0 || v.size() > kMaxToneCurve)
            return fail(err, "DCP: ProfileToneCurve is not an even run of x,y pairs.");
        out.toneCurve.resize(size_t(v.size()));
        for (int i = 0; i < v.size(); ++i) out.toneCurve[size_t(i)] = float(v[i]);
    }

    out.valid = true;
    return true;
}

namespace {

/* Shared by peek() and parseFile(): open, validate the header, read the one IFD. */
bool openIfd(QFile &f, const QString &path, TiffWalk::Reader &r, TiffWalk::Ifd &tags,
             QString *err)
{
    if (!f.open(QIODevice::ReadOnly)) return fail(err, "DCP: cannot open " + path);

    /* A .dcp is a TIFF whose magic word is 0x4352 rather than 42. TiffWalk::Reader accepts
       any magic (the raw formats all use their own), so the check is here: it is the only
       thing that distinguishes a profile from any other little TIFF, and reading one as
       the other produces an empty profile rather than an error. 42 is accepted too, so the
       same entry point reads a DNG's IFD0. */
    const QByteArray hdr = f.read(4);
    if (hdr.size() < 4) return fail(err, "DCP: file is too short.");
    const uchar *h = reinterpret_cast<const uchar *>(hdr.constData());
    const bool be = (h[0] == 'M' && h[1] == 'M');
    if (!be && !(h[0] == 'I' && h[1] == 'I')) return fail(err, "DCP: not a TIFF-structured file.");
    const quint16 magic = be ? quint16((h[2] << 8) | h[3]) : quint16((h[3] << 8) | h[2]);
    if (magic != 0x4352 && magic != 42) return fail(err, "DCP: wrong magic (not a camera profile).");

    if (!r.init(&f)) return fail(err, "DCP: unreadable TIFF header.");

    QList<quint32> subs;
    quint32 next = 0;
    if (!r.readIfd(r.firstIfd(), tags, subs, next)) return fail(err, "DCP: unreadable IFD.");
    return true;
}

} // namespace

bool peek(const QString &path, QString &uniqueCameraModel, QString &name)
{
    uniqueCameraModel.clear();
    name.clear();

    QFile f(path);
    TiffWalk::Reader r;
    TiffWalk::Ifd tags;
    if (!openIfd(f, path, r, tags, nullptr)) return false;

    uniqueCameraModel = ascii(r, tags, UniqueCameraModel);
    name = ascii(r, tags, ProfileName);
    /* A profile with no ColorMatrix1 is not a camera profile, and the menu must not offer
       it -- checked here rather than left to the full parse, which the menu never runs. */
    return !uniqueCameraModel.isEmpty() && tags.contains(ColorMatrix1);
}

bool parseFile(const QString &path, Profile &out, QString *err)
{
    out = Profile();

    QFile f(path);
    TiffWalk::Reader r;
    TiffWalk::Ifd tags;
    if (!openIfd(f, path, r, tags, err)) return false;

    return parseIfd(r, tags, out, err);
}

} // namespace Dcp
