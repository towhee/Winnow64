#ifndef CANON_H
#define CANON_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "Metadata/metareport.h"

class Canon : public QObject
{
    Q_OBJECT

public:
    Canon();
    bool parse(QFile &file,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               bool report,
               QTextStream &rpt,
               QString &xmpString);
private:
    QHash<quint32, QString> canonMakerHash, canonFileInfoHash;
};

#endif // CANON_H
