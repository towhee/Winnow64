#ifndef PIXMAP_H
#define PIXMAP_H

#include <QObject>
#include <QtWidgets>
#include <QHash>
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
//#ifdef Q_OS_WIN
#include "Utilities/icc.h"
//#endif
#include "ImageFormats/Jpeg/jpeg.h"
#ifdef Q_OS_WIN
// rgh remove heic
#include "ImageFormats/Heic/heic.h"
#endif

class Pixmap : public QObject
{
    Q_OBJECT
public:
    explicit Pixmap(QObject *parent, DataModel *dm, Metadata *metadata);
    bool load(QString &fPath, QPixmap &pm, QString src = "");
    bool load(QString &fPath, QImage &image, QString src = "");
    /*
       Decode a file that is NOT necessarily in the datamodel, scaling so its long side
       == longSide (0 = no resize). Loads the file's own metadata, so it works for
       arbitrary files (e.g. FindDuplicatesDlg target images). Video is handled by the
       caller via FrameDecoder. Absorbed from the former AutonomousImage class.

       colorManage is opt-in (and still gated by the global G::colorManage): leave it off
       for comparison thumbnails so they stay apples-to-apples with the datamodel's non
       colour-managed icons; turn it on for previews shown to the user.
    */
    bool loadIndependent(QString &fPath, QImage &image, int longSide, QString src = "",
                         bool colorManage = false);

signals:
    void setValDm(int dmRow, int dmCol, QVariant value,
                  int instance, QString src = "",
                  int role = Qt::EditRole, int align = Qt::AlignLeft);

private:
    DataModel *dm;
    Metadata *metadata;
    int instance;

    bool loadFromHeic(QString &fPath, QImage &image);

    // helpers for loadIndependent (decode files not in the datamodel)
    bool loadFromJpgData(QString &fPath, QImage &image, uint offset, uint length);
    bool loadFromTiff(QString &fPath, QImage &image, ImageMetadata *m);
    bool loadFromEntireFile(QString &fPath, QImage &image);
    void applyOrientation(QImage &image, int orientation, int rotationDegrees);
};

#endif // PIXMAP_H
