#ifndef COLORSPACES_H
#define COLORSPACES_H

#include <QByteArray>
#include <QString>
#include <QStringList>

/*
    THE COLOUR SPACE LOOKUP TABLE. G::ICCSpaceColumn holds one of these ids, and the
    table view, the "colorspace" query field and the catalog index all go through it.

    THE IDS ARE A PUBLISHED FORMAT: they are stored in the catalog index
    (image.colorspace, schema 27) and the library snapshot, and saved queries compare by
    the NAMES below. Add to the end, never renumber, never rename. 0 is "nothing known"
    and shows blank.

    What an image's id says (decided in Metadata::readMetadata and the parsers):

      1. An embedded profile decides, identified by its COLORANTS -- its primaries as the
         profile itself states them -- not by its description, which every vendor
         spells differently ("sRGB IEC61966-2.1", "sRGB built-in", "Display"). The
         description is only the fallback for a profile with no colorants. A profile
         that matches none of the named spaces is "Other profile".
      2. No profile: the EXIF ColorSpace tag (0xA001), which is also what a raw's
         embedded previews were encoded in. 1 = sRGB, 2 = Adobe RGB (some bodies),
         0xFFFF = Uncalibrated, which only means "not sRGB": a camera writes it in Adobe
         RGB mode and says so elsewhere -- InteropIndex "R03" (JPEG), the Canon (0x00B4)
         or Nikon (0x001E) MakerNote -- and a RAW that says nothing more is taken as
         Adobe RGB (the only other space a camera offers). Uncalibrated is left only on
         a JPEG/TIFF with no profile and no R03, e.g. an edited export. A DNG's own
         PreviewColorSpace tag is read before EXIF (fromDngPreview).
      3. HEIC nclx boxes carry numeric primaries instead of a profile (fromNclx).

    Header-only for the names, so the query grammar (core, no lcms) can use the table;
    the profile classification is in colorspaces.cpp.
*/
namespace ColorSpaces
{
    enum Id : int {
        None = 0,
        sRGB = 1,
        AdobeRGB = 2,
        DisplayP3 = 3,
        ProPhotoRGB = 4,
        Rec2020 = 5,
        Gray = 6,
        CMYK = 7,
        OtherProfile = 8,
        Uncalibrated = 9,
        Count
    };

    /* The names, id 1 first: names().at(id - 1). What the table view shows and what
       a query compares (Query::Field::indexed). */
    inline const QStringList &names()
    {
        static const QStringList n{"sRGB", "Adobe RGB", "Display P3", "ProPhoto RGB",
                                   "Rec. 2020", "Gray", "CMYK", "Other profile",
                                   "Uncalibrated"};
        return n;
    }

    inline QString name(int id)
    {
        return (id >= 1 && id < Count) ? names().at(id - 1) : QString();
    }

    /* An embedded ICC profile (the whole profile, not its first bytes). */
    int fromIcc(const QByteArray &profile);

    /* The EXIF ColorSpace tag value; -1 when the tag was absent. */
    int fromExif(int tag);

    /* A HEIC nclx box's colour primaries code (ITU-T H.273). */
    int fromNclx(int primaries);

    /* A DNG's PreviewColorSpace (50970): 1 Gray, 2 sRGB, 3 Adobe RGB, 4 ProPhoto; -1 or
       0 (unknown) gives None. Decides a DNG before EXIF does. */
    int fromDngPreview(int tag);
}

#endif // COLORSPACES_H
