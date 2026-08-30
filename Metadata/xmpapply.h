#ifndef XMPAPPLY_H
#define XMPAPPLY_H

#include "Metadata/xmp.h"
#include "Metadata/imagemetadata.h"

/*
    Copy the fields Winnow reads out of an XMP packet into an ImageMetadata, then
    snapshot the originals so Metadata::writeXMP can tell later what the user changed.

    ONE DEFINITION, BECAUSE THERE USED TO BE EIGHT. Jpeg, Jpeg2, Nikon, Canon, Dng, Tiff
    and Panasonic each carried a byte-identical copy of this block, and copies drift:
    only the two JPEG readers ever collected keywords, so a keyworded NEF, CR2, DNG, TIFF
    or RW2 silently lost every one of them even though the parser had a valid Xmp object
    in its hand at the time.

    Png deliberately keeps its own version. It applies extra isEmpty() guards and mirrors
    the exif:/tiff: namespaces into the camera fields, because Lightroom and Photoshop
    drop the eXIf chunk when they write a PNG and put the camera data in the XMP instead.

    WHY A FREE FUNCTION IN ITS OWN HEADER, not a Metadata member. Metadata/metadata.h
    includes every parser header, so a parser that included it back would compile only as
    long as its own header happened to come first -- a cycle waiting to bite, and it would
    pull QtWidgets into seven more translation units. This depends on nothing but the two
    headers every parser already includes. Same reasoning as Cache/pathkey.h.

    THE CALLER KEEPS its own guard on whether there is a packet worth reading (the
    formats disagree: Panasonic also requires a non-zero offset and length) and its own
    p.xmpString report line (some call srcToString, Nikon calls docToQString).
*/
inline void applyXmp(Xmp &xmp, ImageMetadata &m, MetadataParameters &p)
{
    /* getItem() keys are case sensitive: "Rating", "Label" and "SerialNumber" are
       capitalised where "title", "creator" and "rights" are not, because each is keyed
       on the name registered in Xmp::initialize. */
    if (xmp.isValid) {
        p.xmpModifyDate = QDateTime::fromString(xmp.getItem("modifydate"), Qt::ISODate);
        m.rating = xmp.getItem("Rating");
        m.label = xmp.getItem("Label");
        m.title = xmp.getItem("title");
        m.cameraSN = xmp.getItem("SerialNumber");
        if (m.lens.isEmpty()) m.lens = xmp.getItem("Lens");
        m.lensSN = xmp.getItem("LensSerialNumber");
        if (m.creator.isEmpty()) m.creator = xmp.getItem("creator");
        m.copyright = xmp.getItem("rights");
        m.email = xmp.getItem("email");
        m.url = xmp.getItem("url");
        /* Keywords. dc:subject is the flat list every application writes;
           lr:hierarchicalSubject is Lightroom's parallel list of full keyword paths
           ie "Wildlife|Birds|Heron". A file can carry either, both or neither. */
        m.keywords = xmp.getItemList("subject");
        m.keywordPaths = xmp.getItemList("hierarchicalsubject");
    }

    // save original values so can determine if edited when writing changes
    m._rating = m.rating;
    m._label = m.label;
    m._title = m.title;
    m._creator = m.creator;
    m._copyright = m.copyright;
    m._email = m.email;
    m._url = m.url;
    m._orientation = m.orientation;
    m._rotationDegrees = m.rotationDegrees;
}

#endif // XMPAPPLY_H
