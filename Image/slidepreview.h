#ifndef SLIDEPREVIEW_H
#define SLIDEPREVIEW_H

#include <QString>
#include <QImage>
#include <QByteArray>

class DataModel;
class Metadata;

/*
    THE BEST AVAILABLE PREVIEW FOR A SLIDE, WITHOUT DEVELOPING ANYTHING.

    A slideshow shows pictures; it never edits them, so it must never pay for a sensor
    decode or a develop render. The best picture it can show without one is, in order:

      1. the devPreview (DevPreviewCache) -- the image's develop recipe, or a raw's
         default render, already rendered, oriented and cropped;
      2. the file's embedded / full JPEG (raws, JPEGs);
      3. anything else QImage reads directly.

    TIFF and HEIC return a null image from decode(): the caller falls back to
    Pixmap::load, which has the Winnow decoders for them.

    TWO HALVES, because the datamodel may only be read on the GUI thread. gather() reads
    every column the decode needs into a Job; decode() touches only the Job, the file and
    DevPreviewCache (thread-safe), so it can run on a pool thread for the random-order
    prefetch (MW::slideShowPrefetchRandom).

    This mirrors what ImageDecoder serves the ImageCache outside Develop (loadDevPreview,
    then the embedded JPG), so a slide decoded here and one the cache decoded look the
    same. Documentation: "Slide Show" in notes/Documentation.md.
*/
namespace SlidePreview {

struct Job
{
    QString rowKey;             // the model key (a version key for a version)
    QString srcPath;            // the file on disk
    QString ext;
    QString devKey;             // DevPreviewCache key; empty = do not look
    QString displayDims;        // G::CroppedDimensionsColumn, for a reduced devPreview
    bool hasJpg = false;
    quint32 offsetFull = 0;
    quint32 lengthFull = 0;
    bool rotate = false;
    int orientation = 0;
    int rotationDegrees = 0;
    bool colorManage = false;
    QByteArray iccBuf;
    bool valid = false;
};

/* GUI thread. */
Job gather(DataModel *dm, Metadata *metadata, const QString &rowKey);

/* Any thread. fromDevPreview (when non-null) reports which source served the slide.
   maxEdge > 0 scales a larger image down to that long edge and tags it with the size
   it stands for (DevPreviewCache::kDisplaySizeText), so the loupe still presents it at
   full size. */
QImage decode(const Job &job, bool *fromDevPreview = nullptr, int maxEdge = 0);

}   // namespace SlidePreview

#endif // SLIDEPREVIEW_H
