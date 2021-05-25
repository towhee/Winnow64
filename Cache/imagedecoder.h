#ifndef IMAGEDECODER_H
#define IMAGEDECODER_H

#include <QObject>
#include <QThread>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Metadata/imagemetadata.h"
#include "ImageFormats/Jpeg/jpeg.h"
#ifdef Q_OS_WIN
#include "Utilities/icc.h"
#include "ImageFormats/Heic/heic.h"
#endif

class ImageDecoder : public QThread
{
    Q_OBJECT

public:
    ImageDecoder(QObject *parent, int id, Metadata *metadata);
    void decode(G::ImageFormat format,
                QString fPath,
                ImageMetadata m,
                QByteArray ba = nullptr);
    void setReady();

    int threadId;
    QImage image;
    QByteArray ba;
    QString fPath;
    G::ImageFormat imageFormat;

    enum Status {
        Ready,
        Busy,
        Done
    } status;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void done(int threadId);

private:
    void decodeJpg();
    void decodeTif();
    void decodeHeic();
    void decodeUsingQt();
    void rotate();
    void colorManage();
    DataModel *dm;
    Metadata *metadata;
    ImageMetadata m;
    MetadataParameters p;
    unsigned char *buf;
    QString ext;
};

#endif // IMAGEDECODER_H
