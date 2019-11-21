#include "ifd.h"
#include <QDebug>

IFD::IFD()
{

}

quint32 IFD::readIFD(QFile &file, quint32 &offset, ImageMetadata &m,
                     QHash<quint32, QString> &hash,
                     bool report, QTextStream &rpt, QString &hdr, bool isBigEnd)
{
    /* hash is the hash for the image format ie jpeg, nikon nef etc
       the hash has a description for each tagID and is used for reporting
    */


    uint tagId, tagType = 0;
    quint32 tagCount, tagValue = 0;
    IFDData_ ifdData;
    ifdDataHash.clear();

    file.seek(offset);
    int tags = static_cast<int>(Utilities::get16(file.read(2), isBigEnd));

    // iterate through IFD0, looking for the subIFD tag
    if (report) {
        MetaReport::header(hdr, rpt);
        rpt << "IFDOffset  Hex: "
            << QString::number(offset, 16).toUpper()
            << "   Dec: " << offset
            << "   Tag count: " << QString::number(tags)
            << "\n"
            << "Num    Offset   /   hex  tagId   hex  tagType  tagCount    tagValue   tagDescription\n";
    }
    quint32 pos;
    QString tagDescription;
    for (int i = 0; i < tags; i++){
//        if (report) pos = QString::number(file.pos(), 16).toUpper();
        pos = static_cast<quint32>(file.pos());
        tagId = Utilities::get16(file.read(2), isBigEnd);
        tagType = Utilities::get16(file.read(2), isBigEnd);
        tagCount = Utilities::get32(file.read(4), isBigEnd);
        // check for orientation and save offset for subsequent writing
        if (hdr == "IFD0" && tagId == 274) m.orientationOffset = static_cast<uint>(file.pos());
        if (tagType == 3) tagValue = Utilities::get16(file.read(4), isBigEnd);
        else tagValue = Utilities::get32(file.read(4), isBigEnd);

        ifdData.tagType = tagType;
        ifdData.tagCount = tagCount;
        ifdData.tagValue = tagValue;

        ifdDataHash.insert(tagId, ifdData);

        if (report) {
            (hash.contains(tagId)) ? tagDescription = hash.value(tagId)
            : tagDescription = "Undefined tag";

            // Num    Offset (dec/hex)  tagId   hex  tagType  tagCount    tagValue   tagDescription

            // Num
            rpt.setFieldWidth(3);
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt << QString::number(i, 10).toUpper();
            // Offset (dec/hex)
            rpt.setFieldWidth(10);
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt << QString::number(pos, 10).toUpper();
            rpt << QString::number(pos, 16).toUpper();
            // tagID
            rpt.setFieldWidth(7);
            rpt << tagId;
            // tagID in hex
            rpt.setFieldWidth(6);
            rpt << QString::number(tagId, 16).toUpper();
            // tagType
            rpt.setFieldWidth(9);
            rpt << tagType;
            // tagCount
            rpt.setFieldWidth(10);
            rpt << tagCount;
            // tagValue
            rpt.setFieldWidth(12);
            rpt << tagValue;
            rpt.setFieldWidth(3);
            rpt << "   ";
            // tagDescription
            rpt.setFieldWidth(50);
            rpt.setFieldAlignment(QTextStream::AlignLeft);
            rpt << tagDescription;
            rpt.reset();
            rpt << "\n";
        }
        // quit if more than 200 tags - prob error
        if (i>200) break;
    }
    quint32 nextIFDOffset = Utilities::get32(file.read(4), false);
    if (report) {
        rpt << "   ";
        rpt.setFieldWidth(10);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt << QString::number(file.pos(), 10).toUpper();
        rpt << QString::number(file.pos(), 16).toUpper();
        rpt.reset();
        rpt << "    nextIFDOffset = ";
        rpt << QString::number(nextIFDOffset, 10).toUpper();
        rpt << " / " << QString::number(nextIFDOffset, 16).toUpper();
        rpt << " (+ start offset)";
        rpt << "\n";
    }
    return(nextIFDOffset);
}

QList<quint32> IFD::getSubIfdOffsets(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QList<quint32> offsets;
    file.seek(subIFDaddr);
    for (int i = 0; i < count; i++) {
        offsets.append(Utilities::get32(file.read(4), isBigEnd)); // big endian by default
    }
    return offsets;
}
