#ifndef HEIC_H
#define HEIC_H

#include <QtWidgets>
#include <QtCore>
#include <QtXmlPatterns>
#include <iostream>
#include <iomanip>

#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/metadata.h"

class Heic : public QObject
{
    Q_OBJECT

public:
    Heic(QFile &file);

    quint32 metaOffset;
    quint32 metaLength;
    quint16 pitmId;
    int ilocOffsetSize;
    int ilocLengthSize;
    int ilocBaseOffsetSize;
    int ilocItemCount;
    int ilocExtentCount;
    quint32 irefOffset;
    quint32 irefLength;

private:
    bool nextHeifBox(quint32 &length, QString &type);
    bool getHeifBox(QString &type, quint32 &offset, quint32 &length);

    bool dinfBox(quint32 &offset, quint32 &length);  // Data Information Box (Container)
    bool drefBox(quint32 &offset, quint32 &length);  // Data Reference Box
    bool ftypBox(quint32 &offset, quint32 &length);  // File Type Box
    bool hdlrBox(quint32 &offset, quint32 &length);  // Metadata Handler Box
    bool hvcCBox(quint32 &offset, quint32 &length);  // HEVC Configuration Item Property Box
    bool ilocBox(quint32 &offset, quint32 &length);  // Item Location Box
    bool iinfBox(quint32 &offset, quint32 &length);  // Item Information Box
    bool infeBox(quint32 &offset, quint32 &length);  // Item Info Entry
    bool ipmaBox(quint32 &offset, quint32 &length);  // Image Property Association Box
    bool iprpBox(quint32 &offset, quint32 &length);  // Item Properties Box
    bool irefBox(quint32 &offset, quint32 &length);  // Item Reference Box
    bool ispeBox(quint32 &offset, quint32 &length);  // Image Spacial Extent Box
    bool mdatBox(quint32 &offset, quint32 &length);  // Media Data Box
    bool idatBox(quint32 &offset, quint32 &length);  // Media Data Box
    bool metaBox(quint32 &offset, quint32 &length);  // Metadata Box (Container)
    bool pitmBox(quint32 &offset, quint32 &length);  // Primary Item Box
    bool sitrBox(quint32 &offset, quint32 &length);  // Single Item Type Reference Box
    bool sitrBoxL(quint32 &offset, quint32 &length); // Single Item Type Reference Box Large
    bool urlBox(quint32 &offset, quint32 &length);   // Data Entry Url Box
    bool urnBox(quint32 &offset, quint32 &length);   // Data Entry Urn Box
    bool colrBox(quint32 &offset, quint32 &length);  // Color Information Box
    bool irotBox(quint32 &offset, quint32 &length);  // Color Information Box
    bool pixiBox(quint32 &offset, quint32 &length);  // Color Information Box

    QFile &file;
    qint64 eof;
};

#endif // HEIC_H
