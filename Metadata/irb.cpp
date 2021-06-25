#include "irb.h"

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
//    if (G::isLogger) G::log(__FUNCTION__);

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
//        /*
        qDebug() << __FUNCTION__ << m.fPath
                 << "p.offset =" << p.offset
                 << "irbID =" << irbID
                    ;
                    //*/

        // read the pascal string which we don't care about
        uint pascalStringLength = static_cast<uint>(Utilities::get16(p.file.read(2)));
        if (pascalStringLength > 0) p.file.read(pascalStringLength);

        // get the length of the IRB data block
        quint32 dataBlockLength = Utilities::get32(p.file.read(4));
        // round to even 2 bytes
        dataBlockLength % 2 == 0 ? dataBlockLength : dataBlockLength++;

        // found the thumb, collect offsets and return
        if (foundTifThumb) {
            m.offsetThumb = static_cast<quint32>(p.file.pos()) + 28;
            m.lengthThumb = dataBlockLength - 28;
            m.thumbFormat = G::ImageFormat::Jpg;
            /*
            qDebug() << __FUNCTION__
                     << "m.offsetThumb =" << m.offsetThumb
                     << "m.lengthThumb =" << m.lengthThumb
                        ;
                        //*/
            return;
        }

        // did not find the thumb, try again
        p.file.read(dataBlockLength);
        p.offset = static_cast<quint32>(p.file.pos());
        readThumb(p, m);
}
