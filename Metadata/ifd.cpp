#include "ifd.h"
#include <QDebug>

IFD::IFD()
{

}

quint32 IFD::readIFD(MetadataParameters &p, bool isBigEnd)
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
            << "Num    Offset       hex  tagId   hex  tagType  tagCount    tagValue   tagDescription\n";
    }
    quint32 pos;
    QString tagDescription;
    for (int i = 0; i < tags; i++){
        pos = static_cast<quint32>(p.file.pos());
        tagId = Utilities::get16(p.file.read(2), isBigEnd);
        tagType = Utilities::get16(p.file.read(2), isBigEnd);
        tagCount = Utilities::get32(p.file.read(4), isBigEnd);
        // check for orientation and save offset for subsequent writing
//        if (p.hdr == "IFD0" && tagId == 274) m.orientationOffset = static_cast<uint>(p.file.pos());
        if (tagType == 3 && tagCount == 1 && isBigEnd) {
            tagValue = Utilities::get16(p.file.read(2), isBigEnd);
            p.file.read(2);
        }
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
    quint32 offset = p.file.pos();
    quint32 nextIFDOffset = Utilities::get32(p.file.read(4), isBigEnd);
    if (p.report) {
        p.rpt << "   ";
        p.rpt.setFieldWidth(10);
        p.rpt.setFieldAlignment(QTextStream::AlignRight);
        p.rpt << QString::number(offset, 10).toUpper();
        p.rpt << QString::number(offset, 16).toUpper();
        p.rpt.reset();
        p.rpt << "    nextIFDOffset = ";
        p.rpt << QString::number(nextIFDOffset, 10).toUpper();
        p.rpt << " / " << QString::number(nextIFDOffset, 16).toUpper();
        p.rpt << " (+ start offset if JPG)";
        p.rpt << "\n";
    }
    return(nextIFDOffset);
}

QList<quint32> IFD::getSubIfdOffsets(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    QList<quint32> offsets;
    file.seek(subIFDaddr);
    for (int i = 0; i < count; i++) {
        offsets.append(Utilities::get32(file.read(4), isBigEnd)); // big endian by default
    }
    return offsets;
}

quint32 IFD::readIFD_B(MetadataParameters &p, bool isBigEnd)
{
    /* hash is the hash for the image format ie jpeg, nikon nef etc
       the hash has a description for each tagID and is used for reporting
    */

    uint tagId, tagType = 0;
    quint32 tagCount, tagValue = 0;
    IFDData ifdData;
    ifdDataHash.clear();

    p.buf.seek(p.offset);
    int tags = static_cast<int>(Utilities::get16(p.buf.read(2), isBigEnd));

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
        pos = static_cast<quint32>(p.buf.pos());
        tagId = Utilities::get16(p.buf.read(2), isBigEnd);
        tagType = Utilities::get16(p.buf.read(2), isBigEnd);
        tagCount = Utilities::get32(p.buf.read(4), isBigEnd);
        // check for orientation and save offset for subsequent writing
//        if (p.hdr == "IFD0" && tagId == 274) m.orientationOffset = static_cast<uint>(p.buf.pos());
        if (tagType == 3) {
            if (isBigEnd && tagCount > 1){
                p.buf.read(2);
                tagValue = Utilities::get16(p.buf.read(2), isBigEnd);
            }
            else {
                tagValue = Utilities::get16(p.buf.read(2), isBigEnd);
                p.buf.read(2);
            }
        }
        else tagValue = Utilities::get32(p.buf.read(4), isBigEnd);

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
    quint32 nextIFDOffset = Utilities::get32(p.buf.read(4), isBigEnd);
    if (p.report) {
        p.rpt << "   ";
        p.rpt.setFieldWidth(10);
        p.rpt.setFieldAlignment(QTextStream::AlignRight);
        p.rpt << QString::number(p.buf.pos(), 10).toUpper();
        p.rpt << QString::number(p.buf.pos(), 16).toUpper();
        p.rpt.reset();
        p.rpt << "    nextIFDOffset = ";
        p.rpt << QString::number(nextIFDOffset, 10).toUpper();
        p.rpt << " / " << QString::number(nextIFDOffset, 16).toUpper();
        p.rpt << " (+ start offset)";
        p.rpt << "\n";
    }
    return(nextIFDOffset);
}

bool IFD::writeIFDCount(MetadataParameters &p, ImageMetadata &m, quint16 count)
{
    // p.offset has been set to the next IFD offset
    p.file.seek(p.offset);
    qint64 ret = p.file.write(Utilities::put16(count, m.isBigEnd));
    if (ret == 2) return true;
    else {
        QString msg = "IFD::writeIFDCount failed. ";
        msg += QString::number(ret) + "bytes written (should be 4) for " + m.fPath;
        qWarning(msg.toLatin1());
        return false;
    }
}

bool IFD::writeIFDItem(MetadataParameters &p, ImageMetadata &m,
                       quint16 tagId, quint16 tagType, quint32 tagCount, quint32 tagValue)
{
    // p.offset has been set to the next IFD offset
//    p.file.seek(p.offset);
    qint64 ret;
    ret = p.file.write(Utilities::put16(tagId, m.isBigEnd));
    if (ret != 2) return false;
    ret = p.file.write(Utilities::put16(tagType, m.isBigEnd));
    if (ret != 2) return false;
    ret = p.file.write(Utilities::put32(tagCount, m.isBigEnd));
    if (ret != 4) return false;
    if (tagType == 3 && tagCount == 1) {
        ret = p.file.write(Utilities::put16(tagValue, m.isBigEnd));
        if (ret != 2) return false;
        ret = p.file.write(Utilities::put16(0, m.isBigEnd));
        if (ret != 2) return false;
    }
    else
        ret = p.file.write(Utilities::put32(tagValue, m.isBigEnd));
    if (ret != 4) return false;
    return true;
}

QList<quint32> IFD::getSubIfdOffsets_B(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    QList<quint32> offsets;
    file.seek(subIFDaddr);
    for (int i = 0; i < count; i++) {
        offsets.append(Utilities::get32(file.read(4), isBigEnd)); // big endian by default
    }
    return offsets;
}

