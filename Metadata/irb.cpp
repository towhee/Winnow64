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
    Iterates through consecutive resource blocks until the embedded jpg preview
    is found (irbID == 1036). Bounded to defuse malicious IRB chains.
*/

        // Photoshop IRBs use big endian

        foundTifThumb = false;

        const int kMaxIRBs = 1024;
        QByteArray irbSignature("8BIM");

        for (int i = 0; i < kMaxIRBs; ++i) {
            // if no signature then end of IRB
            p.file.seek(p.offset);
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

            // Pascal string length is a single byte per the Photoshop file format spec.
            uint pascalStringLength = static_cast<uint>(Utilities::get8(p.file.read(1)));
            if (pascalStringLength > 0) p.file.read(pascalStringLength);
            // if position is odd pad to even offset
            if ((p.file.pos() & 1) == 1) p.file.read(1);

            // get the length of the IRB data block
            quint32 dataBlockLength = Utilities::get32(p.file.read(4));
            // round to even 2 bytes
            if (dataBlockLength % 2) dataBlockLength++;
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
                if (!foundJpeg) return;
                m.offsetThumb = static_cast<quint32>(p.file.pos()) - 2;
                m.lengthThumb = dataBlockLength - hdrLength;
                m.thumbFormat = G::ImageFormat::Jpg;
                m.isEmbeddedThumbMissing = false;
                if (p.report) p.rpt << "Embedded IRB Jpg thumbnail found.\n";
                return;
            }

            // Advance past the data block by seeking, not reading, to avoid attacker-sized allocations.
            p.offset = static_cast<quint32>(p.file.pos() + dataBlockLength);
        }
}
