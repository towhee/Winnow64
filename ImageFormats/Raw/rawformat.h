#ifndef RAWFORMAT_H
#define RAWFORMAT_H

#include <memory>
#include <QImage>
#include <QFile>
#include <QString>
#include <QAtomicInt>
#include "Metadata/imagemetadata.h"
#include "ImageFormats/Raw/rawimage.h"
#include "Develop/editparams.h"

/*
    Base class for full-sensor RAW decoding. The shared, camera-agnostic pipeline lives
    here in Decode(); the single piece that varies per vendor/format — turning the
    file's packed/compressed bitstream into a normalised CFA mosaic — is the pure-virtual
    UnpackCfa() hook that subclasses (CanonRaw, NikonRaw, ...) override.

    Demosaic and colour conversion are deliberately NOT virtual: they operate on the
    normalised RawImage and are identical across cameras.

    Threading: construct one RawFormat per decode (see ImageDecoder). Each decoder thread
    owns its own instance and keeps all per-decode scratch in that instance — never share
    a single instance across threads. This is intentionally separate from the Metadata-
    owned parser singletons (Canon, Nikon, ...), even though a vendor's UnpackCfa() will
    live in the same .cpp as its parser.
*/
class RawFormat
{
public:
    virtual ~RawFormat() = default;

    /* Factory: build the RawFormat for a (lower-case, dot-less) file extension, or
       nullptr if the format has no sensor decoder yet. */
    static std::unique_ptr<RawFormat> Create(const QString &ext);

    /* Full sensor decode: file -> demosaiced, colour-managed display QImage. When edit is
       non-null and non-identity, develop adjustments are applied in the linear working space
       (before the output transform). abort (when non-null) is polled between stages and inside
       the demosaic loop so a long decode can bail promptly on shutdown / navigation; an aborted
       decode returns false with lastError() == "Aborted". */
    bool Decode(QFile &file, const ImageMetadata &m, QImage &out,
                const EditParams *edit = nullptr,
                const QAtomicInt *abort = nullptr);

    QString lastError() const { return errMsg; }

protected:
    /* THE override point. Read the vendor bitstream described by m and produce a
       normalised CFA mosaic in raw (width/height/pattern/levels/matrix). */
    virtual bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) = 0;

    /* Shared pre-demosaic step: per-2x2-position black subtraction + clip. */
    static void SubtractBlack(RawImage &raw);

    QString errMsg;
};

#endif // RAWFORMAT_H
