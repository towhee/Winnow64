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
    ImageDecoder(QObject *parent, int id);
    void decode(G::ImageFormat format,
                QString fPath,
                ImageMetadata m = mNull,
                QByteArray ba = nullptr);
    void setReady();

    int threadId;
    QImage image;
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
    void afterDecode();
    static ImageMetadata mNull;
    DataModel *dm;
    Metadata *metadata;
    QByteArray ba;
    ImageMetadata m;
};

#endif // IMAGEDECODER_H
