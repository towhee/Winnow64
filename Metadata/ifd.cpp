#include "ifd.h"
#include <QDebug>

IFD::IFD()
{

}

quint32 IFD::readIFD(MetadataParameters &p, ImageMetadata &m, bool isBigEnd)
{
    /* hash is the hash for the image format ie jpeg, nikon nef etc
       the hash has a description for each tagID and is used for reporting
    */

    uint tagId, tagType = 0;
    quint32 tagCount, tagValue = 0;
    IFDData ifdData;
    ifdDataHash.clear();

    p.file.seek(p.offset);
    int tags = static_cast<int>(Utilities::get16(p.file.read(2), isBigEnd));

    // iterate through IFD0, looking for the subIFD tag
    if (p.report) {
        MetaReport::header(p.hdr, p.rpt);
        p.rpt << "IFDOffset  Hex: "
            << QString::number(p.offset, 16).toUpper()
            << "   Dec: " << p.offset
            << "   Tag count: " << QString::number(tags)
            << "\n"
            << "Num    Offset   /   hex  tagId   hex  tagType  tagCount    tagValue   tagDescription\n";
    }
    quint32 pos;
    QString tagDescription;
    for (int i = 0; i < tags; i++){
//        if (report) pos = QString::number(p.file.pos(), 16).toUpper();
        pos = static_cast<quint32>(p.file.pos());
        tagId = Utilities::get16(p.file.read(2), isBigEnd);
        tagType = Utilities::get16(p.file.read(2), isBigEnd);
        tagCount = Utilities::get32(p.file.read(4), isBigEnd);
        // check for orientation and save offset for subsequent writing
        if (p.hdr == "IFD0" && tagId == 274) m.orientationOffset = static_cast<uint>(p.file.pos());
        if (tagType == 3) tagValue = Utilities::get16(p.file.read(4), isBigEnd);
        else tagValue = Utilities::get32(p.file.read(4), isBigEnd);

        ifdData.tagType = tagType;
        ifdData.tagCount = tagCount;
        ifdData.tagValue = tagValue;

        ifdDataHash.insert(tagId, ifdData);

        if (p.report) {
            (p.hash->contains(tagId)) ? tagDescription = p.hash->value(tagId)
            : tagDescription = "Undefined tag";

            // Num    Offset (dec/hex)  tagId   hex  tagType  tagCount    tagValue   tagDescription

            // Num
            p.rpt.setFieldWidth(3);
            p.rpt.setFieldAlignment(QTextStream::AlignRight);
            p.rpt << QString::number(i, 10).toUpper();
            // Offset (dec/hex)
            p.rpt.setFieldWidth(10);
            p.rpt.setFieldAlignment(QTextStream::AlignRight);
            p.rpt << QString::number(pos, 10).toUpper();
            p.rpt << QString::number(pos, 16).toUpper();
            // tagID
            p.rpt.setFieldWidth(7);
            p.rpt << tagId;
            // tagID in hex
            p.rpt.setFieldWidth(6);
            p.rpt << QString::number(tagId, 16).toUpper();
            // tagType
            p.rpt.setFieldWidth(9);
            p.rpt << tagType;
            // tagCount
            p.rpt.setFieldWidth(10);
            p.rpt << tagCount;
            // tagValue
            p.rpt.setFieldWidth(12);
            p.rpt << tagValue;
            p.rpt.setFieldWidth(3);
            p.rpt << "   ";
            // tagDescription
            p.rpt.setFieldWidth(50);
            p.rpt.setFieldAlignment(QTextStream::AlignLeft);
            p.rpt << tagDescription;
            p.rpt.reset();
            p.rpt << "\n";
        }
        // quit if more than 200 tags - prob error
        if (i>200) break;
    }
    quint32 nextIFDOffset = Utilities::get32(p.file.read(4), false);
    if (p.report) {
        p.rpt << "   ";
        p.rpt.setFieldWidth(10);
        p.rpt.setFieldAlignment(QTextStream::AlignRight);
        p.rpt << QString::number(p.file.pos(), 10).toUpper();
        p.rpt << QString::number(p.file.pos(), 16).toUpper();
        p.rpt.reset();
        p.rpt << "    nextIFDOffset = ";
        p.rpt << QString::number(nextIFDOffset, 10).toUpper();
        p.rpt << " / " << QString::number(nextIFDOffset, 16).toUpper();
        p.rpt << " (+ start offset)";
        p.rpt << "\n";
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