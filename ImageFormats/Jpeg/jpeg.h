#ifndef JPEG_H
#define JPEG_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/iptc.h"
#include "Metadata/xmp.h"
#include "Metadata/metareport.h"

class Jpeg : public QObject
{
    Q_OBJECT

public:
    Jpeg();
    bool getMetadata(QFile &file,
                     quint32 startOffset,
                     ImageMetadata &m,
                     IFD *ifd,
                     IPTC *iptc,
                     Exif *exif,
//                     QHash<QString,quint32> &segmentHash,
                     bool report,
                     QTextStream &rpt,
                     QString &xmpString);
    
private:
    void initSegCodeHash();
    void getJpgSegments(QFile &file, qint64 offset, ImageMetadata &m,
                        bool &report, QTextStream &rpt);
    bool getDimensions(QFile &file, quint32 offset, ImageMetadata &m);

    QHash<QString, quint32> segmentHash;
    QHash<quint32, QString> segCodeHash;
    quint32 order;
    QString err;
    bool isXmp;
};

#endif // JPEG_H
