#include "irb.h"

IRB::IRB()
{

}

bool IRB::read(MetadataParameters &p,  ImageMetadata &m)
{
    /*
    Read a Image Resource Block looking for embedded thumb
        - see https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
    This is a recursive function, iterating through consecutive resource blocks until
    the embedded jpg preview is found (irbID == 1036)
    */
        {
        #ifdef ISDEBUG
        G::track(__FUNCTION__, path);
        #endif
        }

        // Photoshop IRBs use big endian
//        quint32 oldOrder = order;
//        quint32 order = 0x4D4D;

//        // check signature to make sure this is the start of an IRB
//        p.file.seek(offset);
//        QByteArray irbSignature("8BIM");
//        QByteArray signature = p.file.read(4);
//        if (signature != irbSignature) {
//            order = oldOrder;
//            return foundTifThumb;
//        }

//        // Get the IRB ID (we're looking for 1036 = thumb)
//        uint irbID = static_cast<uint>(Utilities::get16(p.file.read(2)));
//        if (irbID == 1036) foundTifThumb = true;
//    //    qDebug() << __FUNCTION__ << fPath << "irbID =" << irbID;

//        // read the pascal string which we don't care about
//        uint pascalStringLength = static_cast<uint>(Utilities::get16(p.file.read(2)));
//        if (pascalStringLength > 0) p.file.read(pascalStringLength);

//        // get the length of the IRB data block
//        quint32 dataBlockLength = Utilities::get32(p.file.read(4));
//        // round to even 2 bytes
//        dataBlockLength % 2 == 0 ? dataBlockLength : dataBlockLength++;

//        // reset order as going to return or recurse next
//        order = oldOrder;

//        // found the thumb, collect offsets and return
//        if (foundTifThumb) {
//            m.offsetThumb = static_cast<quint32>(p.file.pos()) + 28;
//            m.lengthThumb = dataBlockLength - 28;
//            return foundTifThumb;
//        }

//        // did not find the thumb, try again
//        p.file.read(dataBlockLength);
//        offset = static_cast<quint32>(p.file.pos());
//        readIRB(offset);

//        // make the compiler happy
        return false;
}