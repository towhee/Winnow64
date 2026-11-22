#ifndef EMBEDDEDMASK_H
#define EMBEDDEDMASK_H

/*
    Masks the CAMERA embedded in the file -- today the semantic mattes an iPhone writes
    into a ProRAW DNG (DNG 1.6 "Semantic Mask" SubIFDs). They cost no inference: the phone
    already segmented the person, skin, hair, teeth, glasses or sky, and offering them as
    an "Embedded" submask (MaskTool::Embedded) puts that work to use.

    THE FORMAT. Each matte is a SubIFD of IFD0 with PhotometricInterpretation 52527
    (SemanticMask), 8-bit single-channel, compression 7 (a plain JPEG starting FFD8), one
    strip, at a quarter of the raw size (2016x1512 for 4032x3024), SENSOR-oriented (the
    same orientation as the raw, before EXIF rotation). Its tag SemanticName (0xCD2E)
    names it, e.g. urn:com:apple:photo:2018:aux:portraiteffectsmatte. A matte is present
    only when the phone found that content, so the list differs per image.

    The submask stores only the URN ({"name": urn}); the coverage is re-read from the file
    when needed (MW::ensureEmbeddedMask), rotated to the output orientation and registered
    in ObjectMask's store as a full-frame ObjectRef, so buildMaskBuffer, Feather and Edge
    treat it exactly like an object cutout.

    Header-only (all inline, no Q_OBJECT). list() is cached per path: the dock asks every
    time the mask tools menu opens.
*/

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include "ImageFormats/Raw/tiffwalk.h"
#include "Utilities/versionkey.h"

namespace EmbeddedMask {

constexpr quint16 kPhotometricSemanticMask = 52527;
constexpr quint16 kTagSemanticName = 0xCD2E;

/* Short label for a matte URN, for the menu and the submask row ("Person", "Sky").
   Unknown URNs show their last segment, so a new matte type is still usable. */
inline QString label(const QString &urn)
{
    const QString s = urn.section(':', -1).toLower();
    if (s.contains("portraiteffects")) return "Person";
    if (s.contains("skin"))            return "Skin";
    if (s.contains("hair"))            return "Hair";
    if (s.contains("teeth"))           return "Teeth";
    if (s.contains("glasses"))         return "Glasses";
    if (s.contains("sky"))             return "Sky";
    QString t = urn.section(':', -1);
    if (t.startsWith("semantic")) t = t.mid(8);
    if (t.endsWith("matte")) t.chop(5);
    if (t.isEmpty()) return "Matte";
    t[0] = t[0].toUpper();
    return t;
}

inline QString urnOf(const QString &paramsJson)
{
    return QJsonDocument::fromJson(paramsJson.toUtf8()).object().value("name").toString();
}

inline QString paramsFor(const QString &urn)
{
    QJsonObject o;
    o["name"] = urn;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/* One semantic-mask IFD found in the file. */
struct Matte {
    QString urn;
    QVector<quint32> stripOffsets, stripCounts;
};

/* Walk IFD0 (and its chain) and their SubIFDs for semantic masks. DNG only. key may be
   a version's row key (Utilities/versionkey.h): the mattes are the source file's. */
inline QVector<Matte> scan(const QString &key)
{
    QVector<Matte> out;
    const QString fPath = VersionKey::sourceOf(key);
    if (QFileInfo(fPath).suffix().compare("dng", Qt::CaseInsensitive) != 0) return out;
    QFile f(fPath);
    if (!f.open(QIODevice::ReadOnly)) return out;
    TiffWalk::Reader r;
    if (!r.init(&f)) return out;

    QList<quint32> queue;
    QSet<quint32> seen;
    queue << r.firstIfd();
    while (!queue.isEmpty() && seen.size() < 64) {
        const quint32 off = queue.takeFirst();
        if (off < 8 || seen.contains(off)) continue;
        seen.insert(off);
        TiffWalk::Ifd tags;
        QList<quint32> subs;
        quint32 next = 0;
        if (!r.readIfd(off, tags, subs, next)) continue;
        queue << subs;
        if (next) queue << next;
        if (!tags.contains(262) || !tags.contains(kTagSemanticName)) continue;
        if (r.scalar(tags.value(262)) != kPhotometricSemanticMask) continue;
        Matte m;
        QByteArray name = r.bytes(tags.value(kTagSemanticName));
        const int z = name.indexOf('\0');
        if (z >= 0) name = name.left(z);
        m.urn = QString::fromLatin1(name).trimmed();
        if (m.urn.isEmpty()) continue;
        if (tags.contains(273)) m.stripOffsets = r.u32s(tags.value(273));
        if (tags.contains(279)) m.stripCounts  = r.u32s(tags.value(279));
        if (m.stripOffsets.isEmpty() || m.stripOffsets.size() != m.stripCounts.size())
            continue;
        out.append(m);
    }
    return out;
}

/* The URNs of the mattes in this file, in file order (cached per path). */
inline QStringList list(const QString &fPath)
{
    static QMutex mx;
    static QHash<QString, QStringList> cache;
    {
        QMutexLocker lk(&mx);
        auto it = cache.constFind(fPath);
        if (it != cache.constEnd()) return it.value();
    }
    QStringList urns;
    for (const Matte &m : scan(fPath))
        if (!urns.contains(m.urn)) urns << m.urn;
    QMutexLocker lk(&mx);
    if (cache.size() > 256) cache.clear();
    cache.insert(fPath, urns);
    return urns;
}

/*
    Decode the named matte, SENSOR-oriented, 8-bit. Each strip is its own JPEG; several
    strips are stacked top to bottom. False if the file has no such matte or it does not
    decode.
*/
inline bool decode(const QString &fPath, const QString &urn, cv::Mat1b &out)
{
    out.release();
    for (const Matte &m : scan(fPath)) {
        if (m.urn != urn) continue;
        QFile f(VersionKey::sourceOf(fPath));
        if (!f.open(QIODevice::ReadOnly)) return false;
        std::vector<cv::Mat> strips;
        for (int i = 0; i < m.stripOffsets.size(); ++i) {
            const quint32 n = m.stripCounts.at(i);
            if (n == 0 || n > 64u * 1024u * 1024u || !f.seek(m.stripOffsets.at(i)))
                return false;
            const QByteArray b = f.read(n);
            if (b.size() != int(n)) return false;
            const cv::Mat buf(1, b.size(), CV_8U, const_cast<char *>(b.constData()));
            cv::Mat s = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
            if (s.empty()) return false;
            if (!strips.empty() && s.cols != strips.front().cols) return false;
            strips.push_back(s);
        }
        cv::Mat all;
        if (strips.size() == 1) all = strips.front();
        else cv::vconcat(strips, all);
        out = all;
        return !out.empty();
    }
    return false;
}

} // namespace EmbeddedMask

#endif // EMBEDDEDMASK_H
