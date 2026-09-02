#ifndef INDEXMETADATA_H
#define INDEXMETADATA_H

#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QString>
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Cache/catalog.h"
#include "Main/global.h"

/*
    FILLING A ROW FROM THE LOCAL INDEX INSTEAD OF FROM THE FILE.

    A metadata read is ~20 ms for a raw and almost all of it is walking the file's own
    header; the catalog already holds everything that is displayed, sorted, filtered and
    searched. What it does NOT hold is the decode geometry, so a row filled this way is
    marked m.fromIndex and its scratch columns are left unset -- ImageDecoder reads the
    header itself at the point it actually decodes. The header walk moves from every row
    at load time to only the rows the user opens.

    ONE PLACE THAT KNOWS HOW A CATALOG ROW BECOMES AN ImageMetadata. There are two
    callers with quite different shapes -- Reader, one file at a time on a worker thread,
    and DataModel::addAllMetadata, every unread row at once on the GUI thread when a
    filter or sort needs the whole set -- and a second copy of this mapping would be a
    second place that spells a shutter speed. It lives here rather than in either of
    them for that reason.

    THE BULK FORM IS THE POINT for the second caller. fetchFresh takes a LIST, so
    thousands of rows are one database pass rather than one query per row, and the files
    it answers for are never opened at all.
*/
namespace IndexMetadata {

/*
    The freshness candidate for one file -- the path plus the stamps Catalog::fetchFresh
    compares. THE SIDECAR IS STAT'D, because the stamp includes it: a keyword edited in
    Lightroom rewrites the .xmp and never touches the raw, and without that stamp the
    index would keep serving the old keywords. One stat per image, against the ~20 ms
    header walk it avoids.
*/
inline CatalogRow candidate(const QFileInfo &fileInfo, Metadata *metadata)
{
    CatalogRow cand;
    cand.path = fileInfo.filePath();
    cand.srcSize = fileInfo.size();
    cand.srcMtime = fileInfo.lastModified().toSecsSinceEpoch();
    const QString sc = metadata->sidecarPath(cand.path);
    const QFileInfo si(sc);
    if (si.exists()) cand.sidecarMtime = si.lastModified().toSecsSinceEpoch();
    return cand;
}

/*
    Fill m from a fresh catalog row, leaving it exactly as a successful file read would --
    same struct, same fields, same spellings. Everything downstream (addToDatamodel, the
    datamodel row, the filters, the views) cannot tell which path produced it, which is
    the contract this whole mechanism rests on.
*/
inline void fill(ImageMetadata &m, const CatalogRow &r, const QFileInfo &fi,
                 int dmRow, int instance)
{
    m = ImageMetadata();                 // a clean struct, as a file read produces
    m.fPath = r.path;
    m.row = dmRow;
    m.instance = instance;
    m.fromIndex = true;

    m.type = r.ext;
    m.size = int(r.srcSize);
    m.createdDate = r.captured;
    m.modifiedDate = fi.lastModified();
    m.rating = r.rating ? QString::number(r.rating) : QString();
    m.label = r.label;
    m.pick = r.pick;
    m.title = r.title;
    m.creator = r.creator;
    m.copyright = r.copyright;
    m.make = r.make;
    m.model = r.model;
    m.lens = r.lens;
    m.ISONum = r.iso;
    m.apertureNum = r.aperture;
    m.exposureTimeNum = r.shutter;
    m.focalLengthNum = int(r.focalLength);
    m.width = r.width;
    m.height = r.height;
    m.gpsCoord = r.gpsCoord;
    m.orientation = r.orientation;
    m.exposureCompensation = r.exposureComp;
    m.focusX = float(r.focusX);
    m.focusY = float(r.focusY);
    m.email = r.email;
    m.url = r.url;
    m._rating = r._rating;
    m._label = r._label;
    m._creator = r._creator;
    m._title = r._title;
    m._copyright = r._copyright;
    m._email = r._email;
    m._url = r._url;
    m.developEdited = r.developed;
    m.devPreviewKey = r.devPreviewKey;
    /*  The FLAT vocabulary, which is what the Filters category and the search
        read, and the hierarchical paths as the file spelled them. Both are
        needed: the flat list is what is filtered on, and the paths are part of
        the row's searchable text, so a row without them searched differently
        from the same row read from its file (schema 7 exists for this). */
    /*  m.keywords is the LITERAL dc:subject list -- what may be written back
        to a file. The flat vocabulary is derived downstream by
        addMetadataForItem (flattenKeywords), so handing it the flat list here
        would put every ancestor into the user's own dc:subject on the next
        sidecar write. */
    m.keywords = r.keywordsLiteral;
    m.keywordPaths = r.keywordPaths;
    m.isSearch = false;

    /*  THE DISPLAY STRINGS, spelled exactly as the format parsers spell them.
        The model stores the NUMERIC aperture, shutter and focal length (the
        delegates format those), but ISO, aperture, exposureTime and focalLength
        as TEXT are what compose shootingInfo, which the info panel and the
        ShootingInfo column show. Left empty they would blank a column that has
        never been blank, so they are built here.

        This is a second place that knows how a shutter speed is spelled, which
        is a duplication worth being uneasy about -- the format parsers are the
        first. It is verified rather than asserted: the A/B fingerprint over
        every column of every row is identical with the index path on and off,
        which is what would catch a divergence. */
    m.ISO = r.iso ? QString::number(r.iso) : QString();
    if (r.aperture > 0) m.aperture = "f/" + QString::number(r.aperture, 'f', 1);
    if (r.focalLength > 0) m.focalLength = QString::number(r.focalLength, 'f', 0) + "mm";
    if (r.shutter > 0) {
        /*  Under a second is spelled as a reciprocal ("1/1250"), at or over a
            second as the number itself -- the convention every parser follows. */
        if (r.shutter < 1.0) m.exposureTime = "1/" + QString::number(qRound(1.0 / r.shutter));
        else                 m.exposureTime = QString::number(r.shutter);
        /*  " sec" is part of the spelling, not decoration: shootingInfo is
            composed from these strings and the info panel shows it verbatim.
            Leaving it off produced "1/80 at f/5.6" where every file-read row
            says "1/80 sec at f/5.6" -- caught by the A/B fingerprint, which is
            the whole reason a second place that spells a shutter speed is
            tolerable at all. */
        m.exposureTime += " sec";
    }

    /*  Composed exactly as Metadata::loadImageMetadata composes it, from the
        same fields, so the two paths cannot drift apart in spelling. */
    QString info = m.model;
    info += "  " + m.focalLength;
    info += "  " + m.exposureTime;
    info += (m.aperture == "") ? "" : " at " + m.aperture;
    info += (m.ISO == "") ? "" : ", ISO " + m.ISO;
    /*  TAKEN FROM THE CATALOG, NOT RECOMPOSED. The composition above builds the
        display strings the delegates and the info panel need, but shootingInfo
        itself is stored: Metadata::loadImageMetadata composes it only when the
        read SUCCEEDED, and Thumb::setImageDimensions marks a row MetaLoaded from
        the thumbnail path either way -- so a catalogued row may legitimately have
        an empty one, and nothing in the row says which case it was. Recomposing
        unconditionally was tried and moved the mismatch from a HEIC to an iPhone
        JPG. See the schema 7 note in cachedb.cpp. */
    m.shootingInfo = r.shootingInfo;

    m.metaStatus = G::MetaLoaded;
}

/*
    ONE FILE: ask the index and fill metadata->m, or return false and leave it alone.
*/
inline bool read(Metadata *metadata, const QFileInfo &fileInfo, int dmRow, int instance)
{
    const QString fPath = fileInfo.filePath();
    const QHash<QString, CatalogRow> got =
        Catalog::instance().fetchFresh({candidate(fileInfo, metadata)});
    const auto it = got.constFind(fPath);
    if (it == got.cend()) return false;
    fill(metadata->m, *it, fileInfo, dmRow, instance);
    return true;
}

}   // namespace IndexMetadata

#endif // INDEXMETADATA_H
