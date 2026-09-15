#ifndef LUTPARSE_H
#define LUTPARSE_H

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>
#include <algorithm>
#include <cmath>

#include "Develop/lut3d.h"

/*
    Readers for the two open LUT formats, filling a Lut3d::Table.

    NO FILESYSTEM. These take bytes and an already-decoded QImage, so they unit-test
    against fixtures built in the test source rather than against files on disk, and so
    the size guard can run on the image HEADER before anything is decoded.
    Develop/lutstore owns opening files and deciding what to try.

    THEY REJECT RATHER THAN REPAIR. A LUT that is short, long, non-finite or absurdly
    large is refused with a reason, never padded or truncated to fit: these are untrusted
    user downloads, and a table quietly filled with zeros is a black picture nobody can
    explain. Every failure path sets *err.

    WHY BOTH FORMATS SHARE ONE INDEXER. A .cube's data lines and a HaldCLUT's raster
    order are the same sequence -- red fastest, then green, then blue -- so both fill
    Lut3d::Table::v directly. That is asserted rather than assumed: tst_lutparse builds
    one analytic look, emits it as BOTH formats, and requires the two to agree. An index
    transposition in either reader alone cannot pass that.
*/
namespace LutParse {

/* The .cube spec allows 65536 for a 1-D table; that is 768 KB, which is fine. The 3-D
   cap lives in Lut3d::kMaxSize, where the memory actually matters. */
constexpr int kMax1DSize = 65536;

namespace detail {

inline bool fail(QString *err, const QString &why)
{
    if (err) *err = why;
    return false;
}

/* One line, comment stripped and trimmed. '#' begins a comment anywhere on the line. */
inline QByteArray clean(const QByteArray &line)
{
    const int hash = line.indexOf('#');
    return (hash < 0 ? line : line.left(hash)).trimmed();
}

inline bool toFloat(const QByteArray &tok, float &out)
{
    bool ok = false;
    /* QByteArray::toFloat is C-locale by definition -- QLocale would read "0,5" as 0.5
       in a European locale and silently change every LUT on those machines. */
    const float v = tok.toFloat(&ok);
    if (!ok || !std::isfinite(v)) return false;
    out = v;
    return true;
}

} // namespace detail

/*
    IRIDAS / Adobe .cube -- a plain-text 3-D (or 1-D) table.

        # comment
        TITLE "Some Look"
        LUT_3D_SIZE 33
        DOMAIN_MIN 0.0 0.0 0.0
        DOMAIN_MAX 1.0 1.0 1.0
        0.0 0.0 0.0
        ... 33^3 triplets, RED VARYING FASTEST ...

    Keywords may appear before or between data lines, so the parse recognises by keyword
    rather than by line number. Values may exceed 1 or go negative (an HDR-authored look);
    they are stored as they are and clamped only at quantisation.
*/
inline bool ParseCube(const QByteArray &bytes, Lut3d::Table &out, QString *err = nullptr)
{
    out = Lut3d::Table();
    if (err) err->clear();

    int size3D = 0;
    int size1D = 0;
    std::vector<float> data;
    data.reserve(1 << 16);

    const QList<QByteArray> lines = bytes.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray line = detail::clean(raw);
        if (line.isEmpty()) continue;

        const QList<QByteArray> tok = line.simplified().split(' ');
        const QByteArray key = tok.at(0).toUpper();

        if (key == "TITLE") {
            /* Everything after the keyword, quotes stripped. Display only. */
            QByteArray t = line.mid(tok.at(0).size()).trimmed();
            if (t.size() >= 2 && t.startsWith('"') && t.endsWith('"'))
                t = t.mid(1, t.size() - 2);
            out.title = t.toStdString();
            continue;
        }
        if (key == "LUT_3D_SIZE" || key == "LUT_1D_SIZE") {
            if (size3D || size1D)
                return detail::fail(err, "more than one LUT size declared");
            if (tok.size() < 2) return detail::fail(err, "LUT size has no value");
            bool ok = false;
            const int n = tok.at(1).toInt(&ok);
            if (!ok) return detail::fail(err, "LUT size is not a number");
            if (key == "LUT_3D_SIZE") {
                if (n < 2 || n > Lut3d::kMaxSize)
                    return detail::fail(err, QString("LUT_3D_SIZE %1 out of range (2..%2)")
                                                 .arg(n).arg(Lut3d::kMaxSize));
                size3D = n;
            } else {
                if (n < 2 || n > kMax1DSize)
                    return detail::fail(err, QString("LUT_1D_SIZE %1 out of range (2..%2)")
                                                 .arg(n).arg(kMax1DSize));
                size1D = n;
            }
            continue;
        }
        if (key == "DOMAIN_MIN" || key == "DOMAIN_MAX") {
            if (tok.size() < 4) return detail::fail(err, "DOMAIN_ needs three values");
            float *dst = (key == "DOMAIN_MIN") ? out.domainMin : out.domainMax;
            for (int c = 0; c < 3; ++c)
                if (!detail::toFloat(tok.at(1 + c), dst[c]))
                    return detail::fail(err, "DOMAIN_ value is not a finite number");
            continue;
        }

        /* Anything else must be a data triplet. An unknown keyword lands here and fails
           as "not a number", which is the honest message. */
        if (tok.size() < 3)
            return detail::fail(err, QString("unrecognised line: %1")
                                         .arg(QString::fromUtf8(line.left(40))));
        for (int c = 0; c < 3; ++c) {
            float f = 0.0f;
            if (!detail::toFloat(tok.at(c), f))
                return detail::fail(err, QString("not a finite number: %1")
                                             .arg(QString::fromUtf8(tok.at(c))));
            data.push_back(f);
        }
    }

    if (!size3D && !size1D) return detail::fail(err, "no LUT_3D_SIZE or LUT_1D_SIZE");
    for (int c = 0; c < 3; ++c)
        if (!(out.domainMax[c] > out.domainMin[c]))
            return detail::fail(err, "DOMAIN_MAX is not above DOMAIN_MIN");

    const size_t want = size3D ? size_t(size3D) * size3D * size3D * 3
                               : size_t(size1D) * 3;
    if (data.size() != want)
        return detail::fail(err, QString("expected %1 values, found %2")
                                     .arg(want).arg(data.size()));

    if (size3D) { out.size = size3D;   out.v = std::move(data); }
    else        { out.size1D = size1D; out.v1D = std::move(data); }
    return true;
}

/*
    HaldCLUT -- RawTherapee's film-simulation format, a LUT drawn as an image.

    A Hald of LEVEL L is an L^3 x L^3 pixel square holding a cube of size S = L^2, its
    entries in plain raster order with red varying fastest. So level 8 is 512x512 for a
    64-cube, and level 12 is 1728x1728 for a 144-cube.

    THE LEVEL IS DERIVED FROM THE IMAGE, NEVER THE FILENAME: packs rename these freely,
    and a wrong level silently shears the whole table. The cube root must come out exact.

    8-bit sRGB is what RawTherapee ships; 16-bit is accepted because QImage gives it for
    free and the extra precision is real. Alpha is ignored.
*/
inline bool FromHald(const QImage &img, Lut3d::Table &out, QString *err = nullptr)
{
    out = Lut3d::Table();
    if (err) err->clear();

    if (img.isNull()) return detail::fail(err, "image did not decode");
    const int w = img.width();
    if (w != img.height())
        return detail::fail(err, QString("not square (%1x%2)").arg(w).arg(img.height()));

    /* w must be L^3 exactly. Round the cube root and verify rather than trusting it. */
    const int level = int(std::lround(std::cbrt(double(w))));
    if (level < 2 || level * level * level != w)
        return detail::fail(err, QString("width %1 is not a cube of an integer level")
                                     .arg(w));
    const int size = level * level;
    if (size > Lut3d::kMaxSize)
        return detail::fail(err, QString("level %1 gives a %2-cube, over the %3 limit")
                                     .arg(level).arg(size).arg(Lut3d::kMaxSize));

    const size_t entries = size_t(size) * size * size;
    out.size = size;
    out.v.resize(entries * 3);

    const bool deep = img.depth() > 32;
    const QImage src = deep ? img.convertToFormat(QImage::Format_RGBX64)
                            : img.convertToFormat(QImage::Format_RGB888);
    if (src.isNull()) return detail::fail(err, "could not convert the image");

    size_t k = 0;
    for (int y = 0; y < w; ++y) {
        if (deep) {
            const quint16 *p = reinterpret_cast<const quint16 *>(src.constScanLine(y));
            for (int x = 0; x < w; ++x, ++k) {
                out.v[k * 3 + 0] = float(p[x * 4 + 0]) / 65535.0f;
                out.v[k * 3 + 1] = float(p[x * 4 + 1]) / 65535.0f;
                out.v[k * 3 + 2] = float(p[x * 4 + 2]) / 65535.0f;
            }
        } else {
            const uchar *p = src.constScanLine(y);
            for (int x = 0; x < w; ++x, ++k) {
                out.v[k * 3 + 0] = float(p[x * 3 + 0]) / 255.0f;
                out.v[k * 3 + 1] = float(p[x * 3 + 1]) / 255.0f;
                out.v[k * 3 + 2] = float(p[x * 3 + 2]) / 255.0f;
            }
        }
    }
    if (k != entries)
        return detail::fail(err, "pixel count did not fill the table");
    return true;
}

/* The largest image width worth decoding, for a caller holding only a header size.
   Level L gives an L^3-wide image and an L^2 cube, so the cap follows kMaxSize. */
inline int MaxHaldWidth()
{
    const int level = int(std::floor(std::sqrt(double(Lut3d::kMaxSize))));
    return level * level * level;
}

} // namespace LutParse

#endif // LUTPARSE_H
