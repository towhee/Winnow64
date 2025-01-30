#include "irb.h"
#include "Main/global.h"

/* IRB chain structure

*/

IRB::IRB()
{

}

void IRB::readThumb(MetadataParameters &p,  ImageMetadata &m)
{
/*
    Read a Image Resource Block looking for embedded thumb
        - see https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
    This is a recursive function, iterating through consecutive resource blocks until
    the embedded jpg preview is found (irbID == 1036)
*/

        // Photoshop IRBs use big endian

        foundTifThumb = false;

        // if no signature then end of IRB
        p.file.seek(p.offset);
        QByteArray irbSignature("8BIM");
        QByteArray signature = p.file.read(4);
        if (signature != irbSignature) return;

        // Get the IRB ID (we're looking for 1036 = thumb)
        uint irbID = static_cast<uint>(Utilities::get16(p.file.read(2)));
        if (irbID == 1036) foundTifThumb = true;

        if (p.report) {
            p.rpt.setFieldAlignment(QTextStream::AlignLeft);
            p.rpt.setFieldWidth(14);
            p.rpt << p.offset;
            p.rpt.setFieldAlignment(QTextStream::AlignRight);
            p.rpt.setFieldWidth(6);
            p.rpt << irbID;
            p.rpt.reset();
            p.rpt << "\n";
        }
        /*
        qDebug() << "IRB::readThumb" << m.fPath
                 << "p.offset =" << p.offset
                 << "irbID =" << irbID
                    ;
                    //*/

        // read the pascal string which we don't care about
        uint pascalStringLength = static_cast<uint>(Utilities::get16(p.file.read(1)));
        // uint pascalStringLength = static_cast<uint>(Utilities::get16(p.file.read(2)));
        if (pascalStringLength > 0) p.file.read(pascalStringLength);
        // if position is odd pad to even offset
        if ((p.file.pos() & 1) == 1) p.file.read(1);

        // get the length of the IRB data block
        quint32 dataBlockLength = Utilities::get32(p.file.read(4));
        // round to even 2 bytes
        dataBlockLength % 2 == 0 ? dataBlockLength : dataBlockLength++;
        quint32 blockLengthPos = p.file.pos();

        // found the thumb IRB, collect offsets and return
        if (foundTifThumb) {
            // find start of jpeg
            bool foundJpeg = false;
            while (!foundJpeg) {
                quint16 jpgStart = Utilities::get16(p.file.read(2));
                if (jpgStart == 0xFFD8) foundJpeg = true;
                if (p.file.pos() - blockLengthPos > 1000) break;
            }
            int hdrLength = p.file.pos() - blockLengthPos - 2;
            if (!foundJpeg) {
                /*
                qDebug() << "IRB::readThumb not found"
                         << "IRB ID 1036 offset =" << p.offset
                         << "dataBlockLength =" << dataBlockLength
                         << "searched =" << hdrLength << "bytes"
                            ; //*/
                return;
            }
            m.offsetThumb = static_cast<quint32>(p.file.pos()) - 2;
            m.lengthThumb = dataBlockLength - hdrLength;
            m.thumbFormat = G::ImageFormat::Jpg;
            m.isEmbeddedThumbMissing = false;
            if (p.report) p.rpt << "Embedded IRB Jpg thumbnail found.\n";
            /*
            qDebug() << "IRB::readThumb"
                     << m.row
                     << "IRB ID 1036 offset =" << p.offset
                     << "dataBlockLength =" << dataBlockLength
                     << "hdrLength =" << hdrLength
                     << "m.offsetThumb =" << m.offsetThumb
                     << "m.lengthThumb =" << m.lengthThumb
                        ;
                        //*/
            return;
        }

        // // get the length of the IRB data block
        // quint32 dataBlockLength = Utilities::get32(p.file.read(4));
        // // round to even 2 bytes
        // dataBlockLength % 2 == 0 ? dataBlockLength : dataBlockLength++;

        // // found the thumb, collect offsets and return
        // if (foundTifThumb) {
        //     m.offsetThumb = static_cast<quint32>(p.file.pos()) + 28;
        //     m.lengthThumb = dataBlockLength - 28;
        //     m.thumbFormat = G::ImageFormat::Jpg;
        //     if (p.report) p.rpt << "Embedded IRB Jpg found.\n";
        //     /*
        //     qDebug() << "IRB::readThumb"
        //              << "m.offsetThumb =" << m.offsetThumb
        //              << "m.lengthThumb =" << m.lengthThumb
        //                 ;
        //                 //*/
        //     return;
        // }

        // did not find the thumb, try again
        p.file.read(dataBlockLength);
        p.offset = static_cast<quint32>(p.file.pos());
        readThumb(p, m);
}
