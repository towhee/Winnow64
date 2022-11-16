#include "canoncr3.h"

/*
Canon CR3 files reside in a HEIF container.  The HEIF must start with the ftyp box. The first
box inside ftyp is the moov box, which contains all the metadata and the offset and length of
the full size embedded JPG in the mdat.

Canon uses uuid boxes (custom objects), which are followed by a unique code.

"85c0b687820f11e08111f4ce462b6a48"  Container for metadata boxes
  CMT1 = IDF0               EXIF tags
  CMT2 = ExifIFD            EXIF tags
  CMT3 = MakerNOteCanon     Canon tags
  CMT4 = GPSInfo            GPS tags
  CNCV = CompressionVersion
  CNOP = CanonCNOP          Canon CNOP tags
  CNTH = CanonCNTH          Canon CNTH tags
  trak = track 1            Width, Height, Offset and length to embedded jpg in mdat
  trak = n                  Subsequent tracks (not used by Winnow)
  THMB = ThumbnailImage     JPG

"be7acfcb97a942e89c71999491e3afac"  Container for XMP
"EAF42B5E1C984B88B9FBB7DC406E4D16"  Container for mid size image preview jpg
*/

CanonCR3::CanonCR3()
{
}

bool CanonCR3::parse(MetadataParameters *p,
                     ImageMetadata *m,
                     IFD *ifd,
                     Exif *exif,
                     Jpeg *jpeg,
                     GPS *gps)
{
    if (G::isLogger) G::log("CanonCR3::parse");
    this->p = p;
    this->m = m;
    this->ifd = ifd;
    this->exif = exif;
    this->jpeg = jpeg;
    this->gps = gps;
    quint32 offset = 0;
    quint32 length;
    QString type;
    eof = p->file.size();
    trakCount = 0;

    QFileInfo info(p->file);
    if (p->report) {
//        p->rpt << "\nISO/IEC 14496-12 boxes";
        MetaReport::header("ISO/IEC 14496-12 boxes", p->rpt);
        p->rpt << "\nType     Offset    Hex   Length    Hex  Comment";
    }

    // iterate box structures
    while (offset < eof) {
        p->file.seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
    }

    if (p->report) p->rpt << "\n";

    parseIfd0();
    parseExif();

    return true;
}

bool CanonCR3::nextHeifBox(quint32 &length, QString &type)
{
    if (G::isLogger) G::log("CanonCR3::nextHeifBox");
    qint64 offset = p->file.pos();
    length = u.get32(p->file.read(4), true);
//    if (length < 2) length = static_cast<quint32>(eof - offset);
    type = p->file.read(4);
    if (p->report) {
        p->rpt << "\n";
        p->rpt.setFieldWidth(6);
        p->rpt.setFieldAlignment(QTextStream::AlignLeft);
        p->rpt << type;
        p->rpt.setFieldWidth(9);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(offset, 10).toUpper();
        p->rpt.setFieldWidth(7);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(offset, 16).toUpper();
        p->rpt.setFieldWidth(9);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(length, 10).toUpper();
        p->rpt.setFieldWidth(7);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(length, 16).toUpper();
        p->rpt.setFieldWidth(2);
        p->rpt << " ";
    }
    return (length > 0);
}

bool CanonCR3::getHeifBox(QString &type, quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::getHeifBox");
    if (p->report) {
        p->rpt << "\n";
        p->rpt.setFieldWidth(6);
        p->rpt.setFieldAlignment(QTextStream::AlignLeft);
        p->rpt << type;
        p->rpt.setFieldWidth(9);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(offset, 10).toUpper();
        p->rpt.setFieldWidth(7);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(offset, 16).toUpper();
        p->rpt.setFieldWidth(9);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(length, 10).toUpper();
        p->rpt.setFieldWidth(7);
        p->rpt.setFieldAlignment(QTextStream::AlignRight);
        p->rpt << QString::number(length, 16).toUpper();
        p->rpt.setFieldWidth(2);
        p->rpt << " ";
    }
    if (type == "ftyp") return ftypBox(offset, length);
    if (type == "CMT1") return cmt1Box(offset, length);
    if (type == "CMT2") return cmt2Box(offset, length);
    if (type == "CMT3") return cmt3Box(offset, length);
    if (type == "CMT4") return cmt4Box(offset, length);
    if (type == "co64") return co64Box(offset, length);
    if (type == "colr") return colrBox(offset, length);
    if (type == "dinf") return dinfBox(offset, length);
    if (type == "dref") return drefBox(offset, length);
    if (type == "hdlr") return hdlrBox(offset, length);
    if (type == "hvcC") return hvcCBox(offset, length);
    if (type == "idat") return idatBox(offset, length);
    if (type == "iinf") return iinfBox(offset, length);
    if (type == "iloc") return ilocBox(offset, length);
    if (type == "iprp") return iprpBox(offset, length);
    if (type == "iref") return irefBox(offset, length);
    if (type == "irot") return irotBox(offset, length);
    if (type == "ispe") return ispeBox(offset, length);
    if (type == "ipma") return ipmaBox(offset, length);
    if (type == "mdat") return mdatBox(offset, length);
    if (type == "mdia") return mdiaBox(offset, length);
    if (type == "meta") return metaBox(offset, length);
    if (type == "moov") return moovBox(offset, length);
    if (type == "pixi") return pixiBox(offset, length);
    if (type == "pitm") return pitmBox(offset, length);
    if (type == "minf") return minfBox(offset, length);
    if (type == "stbl") return stblBox(offset, length);
    if (type == "stsc") return stscBox(offset, length);
    if (type == "stsd") return stsdBox(offset, length);
    if (type == "stts") return sttsBox(offset, length);
    if (type == "stsz") return stszBox(offset, length);
    if (type == "THMB") return thmbBox(offset, length);
    if (type == "trak") return trakBox(offset, length);
    if (type == "tkhd") return tkhdBox(offset, length);
    if (type == "url ") return  urlBox(offset, length);
    if (type == "urn ") return  urnBox(offset, length);
    if (type == "uuid") return uuidBox(offset, length);
    if (type == "vmhd") return vmhdBox(offset, length);

    // err
//    qDebug() << "CanonCR3::getHeifBox"
//             << "offset:" << offset
//             << "length:" << length
//             << "type:" << type
//             << "(Undefined)"
//                ;
    G::error("CanonCR3::getHeifBox", m->fPath, "Box type " + type + " unknown.");
    offset += length;
    return true;
//    return false;
}

bool CanonCR3::ftypBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::ftypBox");
    if (length == 0) {
        // err
        G::error("CanonCR3::ftypBox", m->fPath, "ftyp not found.");
        qDebug() << "CanonCR3::ftypBox" << "ftyp not found";
        return false;
    }

    bool isCR3 = false;
    /* compatible brands 32bits each to end of ftype box
       ftyp box = length + type + major brand + minor version + compatible brands (each 4 bytes) */
    int compatibleBrands = static_cast<int>(length) - 16;
    p->file.seek(16);
    for (int i = 0; i < compatibleBrands; i++) {
        QString brand = p->file.read(4);
        if (brand == "crx ") {
            isCR3 = true;
            break;
        }
    }
    if (!isCR3) {
        // err
        G::error("CanonCR3::ftypBox", m->fPath, "crx not found");
        qDebug() << "CanonCR3::ftypBox" << "crx not found";
        return false;
    }
    offset += length;
    return true;
}

bool CanonCR3::metaBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::metaBox");
    QString type;
//    heif.metaOffset = offset;
//    heif.metaLength = length;
    quint32 metaEnd = offset + length;
    offset += 12;

    qDebug() << "CanonCR3::metaBox"
             << "offset =" << offset
             << "length =" << length;

    while (offset < metaEnd) {
        p->file.seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
    }

    offset = metaEnd;

    return true;
}

bool CanonCR3::hdlrBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::hdlrBox");
    offset += length;
    return true;
}

bool CanonCR3::dinfBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::dingBox");
    offset += length;
    return true;
}

bool CanonCR3::drefBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::drefBox");
    QString type;
    quint32 drefEnd = offset + length;
    p->file.seek(offset + 12);
    quint32 entry_count = u.get32(p->file.read(4));
    qDebug();
    qDebug() << "CanonCR3::drefBox" << "entry_count =" << entry_count;

    for (quint32 i = 0; i < entry_count; i++) {
        if (offset > drefEnd) return false;
        nextHeifBox(length, type);
        qDebug() << "CanonCR3::drefBox" << "entry#" << i;
        if (type == "url ") getHeifBox(type, offset, length);
//        if (type == "urln") getHeifBox(type, offset, length);
    }

    offset = drefEnd;
    return true;
}

bool CanonCR3::urlBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::urlBox");
    QString location = "";
    uint urlLength = length - 12;
    p->file.seek(offset + 12);
    if (urlLength > 0) {
        location = p->file.read(length - 12);
    }
    qDebug() << "CanonCR3::urlBox"
             << "    location =" << location;

    offset += length;
    return true;
}

bool CanonCR3::urnBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::urnBox");
    QString name = "";
    QString location = "";
    uint urnLength = length - 12;
    p->file.seek(offset + 12);
    if (urnLength > 0) {
        name = u.getCString(p->file);
        if (p->file.pos() < offset + length) location = u.getCString(p->file);
    }
    qDebug() << "CanonCR3::urnBox"
             << "    name =" << name
             << "location =" << location;

    offset += length;
    return true;
}

bool CanonCR3::colrBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::colrBox");
    p->file.seek(offset + 8);

    QString colrType = p->file.read(4);
    qDebug() << "CanonCR3::colrBox" << "colrType =" << colrType;
    QByteArray colorProfile;
    if (colrType == "nclx") {
        quint16 colour_primaries = u.get16(p->file.read(2));
        quint16 transfer_characteristics = u.get16(p->file.read(2));
        quint16 matrix_coefficients = u.get16(p->file.read(2));
        quint8 x = u.get8(p->file.read(1));
        quint8 full_range_flag = (x & 0b10000000) >> 7;             // first 1 bit
        qDebug() << "CanonCR3::colrBox"
                 << "colour_primaries =" << colour_primaries
                 << "transfer_characteristics =" << transfer_characteristics
                 << "matrix_coefficients =" << matrix_coefficients
                 << "full_range_flag =" << full_range_flag;
    }
    else if (colrType == "rICC" || colrType == "prof") {
        colorProfile = p->file.read(length - 8);
        qDebug() << "CanonCR3::colrBox" << colorProfile;
    }
    else {
        // err
        G::error("CanonCR3::colrBox", m->fPath, "Color type " + colrType + " is not recognized");
        offset += length;
        return false;
    }

    offset += length;
    return true;
}

bool CanonCR3::pixiBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::pixiBox");
    p->file.seek(offset + 12);
    quint8 num_channels = u.get8(p->file.read(1));

    for (quint8 i = 0; i < num_channels; i++) {
        quint8 bits_per_channel = u.get8(p->file.read(1));
        qDebug() << "CanonCR3::pixiBox" << "channel#" << i << "bits_per_channel =" << bits_per_channel;
    }

    offset += length;
    return true;
}

bool CanonCR3::irotBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::irotBox");
    p->file.seek(offset + 8);
    quint8 x = u.get8(p->file.read(1));
    quint8 angle = (x & 0b00000011);             // first 1 bit
    quint16 angle_degrees = angle * 90;
    qDebug() << "CanonCR3::irotBox" << "angle =" << angle << "angle degrees =" << angle_degrees;

    offset += length;
    return true;
}

bool CanonCR3::pitmBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::pitmBox");
    p->file.seek(offset + 16);
    pitmId = u.get16(p->file.read(2));
    qDebug() << "CanonCR3::pitmBox"
             << "pitmId =" << pitmId;

    offset += length;
    return true;
}

bool CanonCR3::ilocBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::ilocBox");
    p->file.seek(offset + 12);
    QByteArray c = p->file.read(1);
    ilocOffsetSize = u.get4_1st(c);
    ilocLengthSize = u.get4_2nd(c);
    c = p->file.read(1);
    ilocBaseOffsetSize = u.get4_1st(c);
    ilocItemCount = u.get16(p->file.read(2));
    qDebug();
    qDebug()  << "CanonCR3::ilocBox"
             << "ilocOffsetSize =" << ilocOffsetSize
             << "ilocLengthSize =" << ilocLengthSize
             << "ilocBaseOffsetSize =" << ilocBaseOffsetSize
             << "ilocItemCount =" << ilocItemCount;
    if (ilocItemCount > 100) {
        qDebug() << "CanonCR3::ilocBox" << "*** Quiting because ilocItemCount =" << ilocItemCount;
        offset += length;
        return false;
    }
    for (int i = 0; i < ilocItemCount; i++) {
        quint16 item_ID = u.get16(p->file.read(2));
        quint16 data_reference_index = u.get16(p->file.read(2));
        quint32 base_offset = 0;
        if (ilocBaseOffsetSize == 0) base_offset = u.get8(p->file.read(2));
        if (ilocBaseOffsetSize == 1) base_offset = u.get8(p->file.read(1));
        if (ilocBaseOffsetSize == 2) base_offset = u.get16(p->file.read(2));
        if (ilocBaseOffsetSize == 4) base_offset = u.get32(p->file.read(4));
        quint16 extent_count = u.get16(p->file.read(2));
        qDebug() << "CanonCR3::ilocBox" << "Item:" << i
                 << "itemId" << item_ID
                 << "data_reference_index" << data_reference_index
                 << "base_offset" << base_offset
                 << "extent_count" << extent_count;
        if (extent_count > 100) {
            QString err = "Quiting because extent_count has reached = " +
                    QString::number(extent_count);
            G::error("CanonCR3::ilocBox", m->fPath, err);
            qDebug() << "CanonCR3::ilocBox" << err;
            offset += length;
            return false;
        }
        for (int j = 0; j < extent_count; j++) {
            quint32 extent_offset = 0;
            if (ilocOffsetSize == 0) extent_offset = u.get8(p->file.read(2));
            if (ilocOffsetSize == 1) extent_offset = u.get8(p->file.read(1));
            if (ilocOffsetSize == 2) extent_offset = u.get16(p->file.read(2));
            if (ilocOffsetSize == 4) extent_offset = u.get32(p->file.read(4));
            quint32 extent_length = 0;
            if (ilocLengthSize == 0) extent_length = u.get8(p->file.read(2));
            if (ilocLengthSize == 1) extent_length = u.get8(p->file.read(1));
            if (ilocLengthSize == 2) extent_length = u.get16(p->file.read(2));
            if (ilocLengthSize == 4) extent_length = u.get32(p->file.read(4));
            qDebug() <<"CanonCR3::ilocBox" << "    Extent:" << i
                     << "extent_offset" << extent_offset
                     << "extent_length" << extent_length;
        }
    }
    qDebug() << "CanonCR3::ilocBox" << "p->file.pos()" << p->file.pos();
    //    if (hdlrType != "pict") {
    //        // err
    //        return false;
    //    }

    offset += length;
    return true;
}

bool CanonCR3::infeBox(quint32 &offset, quint32 &/*length*/)
{
    if (G::isLogger) G::log("CanonCR3::infeBox");
    p->file.seek(offset + 12);
    quint16 item_ID = u.get16(p->file.read(2));
    quint16 item_protection_index = u.get16(p->file.read(2));
    QString item_name = u.getCString(p->file);
    QString content_type = u.getCString(p->file);
    QString content_encoding;
    if (content_type.length()) content_encoding = u.getCString(p->file);
    else content_encoding = "";

    qDebug() << "   " << "CanonCR3::infeBox"
             << "item_ID =" << item_ID
             << "item_protection_index =" << item_protection_index
             << "item_name =" << item_name
             << "content_type =" << content_type
             << "content_encoding =" << content_encoding;

    return true;
}

bool CanonCR3::iinfBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::iinfBox");
    p->file.seek(offset + 14);
    quint16 entry_count = u.get16(p->file.read(2));
//    qDebug() << "CanonCR3::iinfBox" << "iint entry count =" << entry_count << p->file.pos();
    if (entry_count == 0) {
        // err
        QString err = "No iint entries found.";
        G::error("CanonCR3::iinfBox", m->fPath, err);
        offset += length;
        return false;
    }

    quint32 infeOffset = static_cast<quint32>(p->file.pos());
    quint32 infeLength;
    for (int i = 0; i < entry_count; i++) {
        QString infeType;
        nextHeifBox(infeLength, infeType);
        qDebug() << "CanonCR3::iinfBox" << "   Item Info Entry " << i;
        infeBox(infeOffset, infeLength);
        infeOffset += infeLength;
    }

    offset += length;
    return true;
}

// add a large version sitrBoxL
bool CanonCR3::sitrBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::iinfBox");
    p->file.seek(offset + 8);
    quint16 from_item_ID = u.get16(p->file.read(2));
    quint16 reference_count = u.get16(p->file.read(2));

    qDebug() << "CanonCR3::iinfBox"
             << "from_item_ID =" << from_item_ID
             << "reference_count =" << reference_count;

    for (int i = 0; i < reference_count; i++) {
        quint32 to_item_ID = u.get32(p->file.read(4));
        qDebug() << "CanonCR3::iinfBox" << i << ": to_item_ID =" << to_item_ID;
    }
    offset += length;
    return true;
}

bool CanonCR3::irefBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::irefBox");
    irefOffset = offset;
    irefLength = length;
    quint32 irefEndOffset = offset + length;
    p->file.seek(offset + 8);
    uint version = u.get8(p->file.read(1));
    quint32 sitrOffset = offset + 12;
    qDebug() << "CanonCR3::irefBox" << "offset =" << offset
             << "length =" << length << "sitrOffset" << sitrOffset;
    while (sitrOffset < irefEndOffset) {
        quint32 sitrLength;
        QString type;
        p->file.seek(sitrOffset);
        nextHeifBox(sitrLength, type);
        if (version == 0) sitrBox(sitrOffset, sitrLength);
//        else sitrBoxL(sitrOffset, sitrLength);
        qDebug() << "CanonCR3::irefBox" << "irefEndOffset =" << irefEndOffset << "sitrOffset" << sitrOffset;
    }

    offset += length;
    return true;
}

bool CanonCR3::hvcCBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::hvcCBox");
    qDebug() << "CanonCR3::hvcCBox" << "p->file position:" << p->file.pos();
    uint configurationVersion = u.get8(p->file.read(1));
    quint8 x = u.get8(p->file.read(1));
    int general_profile_space = (x & 0b11000000) >> 6;             // first 2 bits
    int general_tier_flag =     (x & 0b00100000) >> 5;             // 3rd bit
    int general_profile_idc =   (x & 0b00011111);                  // last 5 bits
    quint32 general_profile_compatibility_flags = u.get32(p->file.read(4));
    auto general_constraint_indicator_flags = u.get48(p->file.read(6));
    quint8 general_level_idc  = u.get8(p->file.read(1));
    quint16 min_spatial_segmentation_idc = u.get16(p->file.read(2));
    // 4 bits reserved, last 12 = min_spatial_segmentation_idc
    min_spatial_segmentation_idc = min_spatial_segmentation_idc & 0b0000111111111111;

    quint8 parallelismType = u.get8(p->file.read(1));
    // 6 bits reserved, last 2 = parallelismType
    parallelismType = parallelismType & 0b00000011;

    quint8 chroma_format_idc = u.get8(p->file.read(1));
    // 6 bits reserved, last 2 = chroma_format_idc
    chroma_format_idc = chroma_format_idc & 0b00000011;

    quint8 bit_depth_luma_minus8 = u.get8(p->file.read(1));
    // 5 bits reserved, last 3 = bit_depth_luma_minus8
    bit_depth_luma_minus8 = bit_depth_luma_minus8 & 0b00000111;
    quint8 bit_depth_chroma_minus8 = u.get8(p->file.read(1));
    // 5 bits reserved, last 3 = bit_depth_chroma_minus8
    bit_depth_chroma_minus8 = bit_depth_chroma_minus8 & 0b00000111;

    quint16 avgFrameRate = u.get16(p->file.read(2));

    x = u.get8(p->file.read(1));
    int constantFrameRate = (x & 0b11000000) >> 6;             // first 2 bits
    int numTemporalLayers =     (x & 0b00111000) >> 3;         // next 3 bits
    int temporalIdNested =     (x & 0b00000100) >> 5;          // next bit
    int lengthSizeMinusOne =   (x & 0b00000011);               // last 2 bits

    quint8 numOfArrays = u.get8(p->file.read(1));

    qDebug() << "CanonCR3::hvcCBox" << "configurationVersion =" << configurationVersion;
    qDebug() << "CanonCR3::hvcCBox" << "general_profile_space =" << general_profile_space;
    qDebug() << "CanonCR3::hvcCBox" << "general_tier_flag =" << general_tier_flag;
    qDebug() << "CanonCR3::hvcCBox" << "general_profile_idc =" << general_profile_idc;
    qDebug() << "CanonCR3::hvcCBox" << "general_profile_compatibility_flags =" << general_profile_compatibility_flags;
    qDebug() << "CanonCR3::hvcCBox" << "general_constraint_indicator_flags =" << general_constraint_indicator_flags;
    qDebug() << "CanonCR3::hvcCBox" << "general_level_idc =" << general_level_idc;
    qDebug() << "CanonCR3::hvcCBox" << "min_spatial_segmentation_idc =" << min_spatial_segmentation_idc;
    qDebug() << "CanonCR3::hvcCBox" << "parallelismType =" << parallelismType;
    qDebug() << "CanonCR3::hvcCBox" << "chroma_format_idc =" << chroma_format_idc;
    qDebug() << "CanonCR3::hvcCBox" << "bit_depth_luma_minus8 =" << bit_depth_luma_minus8;
    qDebug() << "CanonCR3::hvcCBox" << "bit_depth_chroma_minus8 =" << bit_depth_chroma_minus8;
    qDebug() << "CanonCR3::hvcCBox" << "avgFrameRate =" << avgFrameRate;
    qDebug() << "CanonCR3::hvcCBox" << "constantFrameRate =" << constantFrameRate;
    qDebug() << "CanonCR3::hvcCBox" << "numTemporalLayers =" << numTemporalLayers;
    qDebug() << "CanonCR3::hvcCBox" << "temporalIdNested =" << temporalIdNested;
    qDebug() << "CanonCR3::hvcCBox" << "lengthSizeMinusOne =" << lengthSizeMinusOne;
    qDebug() << "CanonCR3::hvcCBox" << "numOfArrays =" << numOfArrays;

    for (int i = 0; i < numOfArrays; i++) {
        x = u.get8(p->file.read(1));
        int array_completeness = (x & 0b10000000) >> 6;             // first bit
                                                                    // 2nd bit reserved
        int NAL_unit_type = (x & 0b00111111);                         // last 6 bits
        quint16 numNalus = u.get16(p->file.read(2));
        qDebug() << "CanonCR3::hvcCBox" << "    array # =" << i;
        qDebug() << "CanonCR3::hvcCBox" << "    array_completeness =" << array_completeness;
        qDebug() << "CanonCR3::hvcCBox" << "    NAL_unit_type =" << NAL_unit_type;
        qDebug() << "CanonCR3::hvcCBox" << "    numNalus =" << numNalus;

        for (quint16 j = 0; j < numNalus; j++) {
            quint16 nalUnitLength = u.get16(p->file.read(2));
            QByteArray nalUnit = p->file.read(nalUnitLength);
            qDebug() << "CanonCR3::hvcCBox" << "        nalUnitLength =" << nalUnitLength;
        }
    }

    qDebug() << "CanonCR3::hvcCBox"
             << "offset:" << offset
             << "length:" << length
             << "offset + length" << offset + length
             << "p->file position:" << p->file.pos();

    offset += length;
    return true;
}

bool CanonCR3::trakBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::trakBox");
    // container for track
    trakCount++;
    // if 1st instance of track then drill in to get offset and length of embedded JPG in mdat
    if (trakCount == 1) {
        if (p->report) p->rpt << "Track 1 with info on full size JPG in mdat";
        offset += 8;
    }
    // otherwise skip over track
    else {
        if (p->report) p->rpt << "Skip";
        offset += length;
    }
    return true;
}

bool CanonCR3::tkhdBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::tkhdBox");
    // container for track - skip
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::mdiaBox(quint32 &offset, quint32 &/*length*/)
{
    if (G::isLogger) G::log("CanonCR3::mdiaBox");
    // container for media info - drill into
    offset += 8;       // temp for testing
    return true;
}

bool CanonCR3::mdhdBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::mdhdBox");
    // container for media info - drill into
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::minfBox(quint32 &offset, quint32 &/*length*/)
{
    if (G::isLogger) G::log("CanonCR3::minfBox");
    // container
    offset += 8;       // temp for testing
    return true;
}

bool CanonCR3::vmhdBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::vmhdBox");
    // container
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::stblBox(quint32 &offset, quint32 &/*length*/)
{
    if (G::isLogger) G::log("CanonCR3::stblBox");
    // container
    offset += 8;       // temp for testing
    return true;
}

bool CanonCR3::stsdBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::stsdBox");
    // embedded image width and height (not used)
    if (p->report) p->rpt << "Width and Height of full size JPG in mdat (not used)";
    p->file.seek(offset + 48);
    m->width = u.get16(p->file.read(2));    // width
    m->height = u.get16(p->file.read(2));   // height
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::sttsBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::sttsBox");
    //
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::stscBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::stscBox");
    //
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::stszBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::stszBox");
    // length of embedded JPG
    if (p->report) p->rpt << "Length of full size JPG in mdat";
    p->file.seek(offset + 20);
    m->lengthFull = u.get32(p->file.read(4));
//    qDebug() << "CanonCR3::stszBox" << "m->lengthFull =" << m->lengthFull;
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::co64Box(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::co64Box");
    // length of embedded JPG
    if (p->report) p->rpt << "Absolute offset to full size JPG in mdat";
    p->file.seek(offset + 20);
    m->offsetFull = u.get32(p->file.read(4));
//    qDebug() << "CanonCR3::co64Box" << "m->offsetFull =" << m->offsetFull;
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::ispeBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::ispeBox");
    p->file.seek(offset + 12);
    quint32 image_width = u.get32(p->file.read(4));
    quint32 image_height = u.get32(p->file.read(4));
    qDebug() << "CanonCR3::ispeBox" << "image_width =" << image_width;
    qDebug() << "CanonCR3::ispeBox" << "image_height =" << image_height << "\n";

    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::ipmaBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::ipmaBox");
    p->file.seek(offset + 8);
    quint16 x = u.get16(p->file.read(2));
    auto version = (x & 0b1111000000000000) >> 12;       // first 4 bits
    auto flags =   (x & 0b0000111111111111);             // last 12 bits

    p->file.seek(offset + 12);
    quint32 entry_count = u.get32(p->file.read(4));
//    qDebug() << "CanonCR3::ipmaBox" << "entry_count =" << entry_count;

    for (quint16 i = 0; i < entry_count; i++ ) {
        quint32 item_ID;
        if (version < 1) {
            item_ID = u.get16(p->file.read(2));
        }
        else {
            item_ID = u.get32(p->file.read(4));
        }
        quint8 association_count = u.get8(p->file.read(1));
//        qDebug() << "    entry#" << i
//                 << "item_ID =" << item_ID
//                 << "association_count =" << association_count;

        for (int j = 0; j < association_count; j++) {
            int x = u.get8(p->file.peek(1));
//            int essential =  (x & 0b10000000) >> 7;
            quint16 property_index;
            if (flags & 1) {
                property_index = u.get16(p->file.read(2));
                property_index  = property_index & 0b0111111111111111;
            }
            else {
                property_index = u.get16(p->file.read(1));
                property_index  = property_index & 0b01111111;
            }
//            qDebug() << "        association#" << j
//                     << "essential =" << essential
//                     << "property_index =" << property_index;
        }
    }

//    qDebug() << "CanonCR3::ipmaBox"
//             << "offset:" << offset
//             << "length:" << length
//             << "offset + length" << offset + length
//             << "p->file position:" << p->file.pos();

    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::iprpBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::iprpBox");
    QString type;
    quint32 iprpEnd = offset + length;
    p->file.seek(offset + 8);

    // should be an ipco box next
    quint32 ipcoLength;
    QString ipcoType;
    nextHeifBox(ipcoLength, ipcoType);

    if (ipcoType != "ipco") {
        // err
        QString err = "ipco not found in iprp box";
        G::error("CanonCR3::iprpBox", m->fPath, err);
        qDebug() << "CanonCR3::iprpBox" << "ipco not found in iprp box";
        return false;
    }

    offset += 16;

    while (offset < iprpEnd) {
        p->file.seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
    }

    offset = iprpEnd;

    return true;
}

bool CanonCR3::mdatBox(quint32 &offset, quint32 &/*length*/)
{
    if (G::isLogger) G::log("CanonCR3::mdatBox");
    if (p->report) p->rpt << "Full size embedded JPG starting at " << offset + 16;
    offset = 999999999;
    return true;
}

bool CanonCR3::idatBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::idatBox");
//    qDebug() << "CanonCR3::idatBox" << "offset =" << offset;
    offset += length;       // temp for testing
    return true;
}

bool CanonCR3::freeBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::freeBox");
    // free space - ignore
    offset += length;
    return true;
}

bool CanonCR3::uuidBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::uuidBox");
    // Canon custom object
    p->file.seek(offset + 8);
    QByteArray thisUUID = p->file.read(16);
    QString s;

    s = "85c0b687820f11e08111f4ce462b6a48";
    QByteArray metaUUID = QByteArray::fromHex(s.toLatin1());
    if (thisUUID == metaUUID) {
        if (p->report) p->rpt << s << " Tiff format metadata";
        offset += 24;
        return true;
    }
    s = "be7acfcb97a942e89c71999491e3afac";
    QByteArray xmpUUID = QByteArray::fromHex(s.toLatin1());
    if (thisUUID == xmpUUID) {
        if (p->report) p->rpt << s << " XMP data";
        offset += length;
        return true;
    }
    s = "eaf42b5E1c984b88b9fbb7dc406E4d16";
    QByteArray prvdUUID = QByteArray::fromHex(s.toLatin1());
//    qDebug() << "CanonCR3::uuidBox" << prvdUUID.toHex() << thisUUID.toHex();
    // get preview if no full size jpg found in track 1
    if (thisUUID == prvdUUID && m->offsetFull == 0) {
        if (p->report) p->rpt << s << " Preview embedded JPG";
        QString type;
        offset += 32;
        m->offsetFull = offset + 24;
        p->file.seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
        m->lengthFull = length - 24;
        return true;
    }
    offset += length;
    return true;
}

bool CanonCR3::thmbBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::thmbBox");
    // embedded JPG thumbnail
    if (p->report) p->rpt << "Embedded JPG thumbnail";
    p->file.read(4);     // skip
    p->file.read(2);     // thumbnail width
    p->file.read(2);     // thumbnail height
    m->lengthThumb = u.get32(p->file.read(4));
    p->file.read(4);     // skip
    m->offsetThumb = static_cast<quint32>(p->file.pos());
    offset += length;
    return true;
}

bool CanonCR3::cmt1Box(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::cmt1Box");
    // IFD0
    if (p->report) p->rpt << "IFD0 data";
    ifd0Offset = offset;
    offset += length;
    return true;
}

bool CanonCR3::cmt2Box(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::cmt2Box");
    // IFD EXIF
    if (p->report) p->rpt << "Exif data";
    exifOffset = offset + 16;
    offset += length;
    return true;
}

bool CanonCR3::cmt3Box(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::cmt3Box");
    // Canon maker notes IFD
    if (p->report) p->rpt << "Canon maker notes";
    offset += length;
    return true;
}
bool CanonCR3::cmt4Box(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::cmt4Box");
    // Exif GPS IFD
    if (p->report) p->rpt << "GPS data";
    offset += length;
    return true;
}

bool CanonCR3::moovBox(quint32 &offset, quint32 &length)
{
    if (G::isLogger) G::log("CanonCR3::moovBox");
/* container for metadata moov
  UUID = Canon custom object
  CMT1 = IDF0               EXIF tags
  CMT2 = ExifIFD            EXIF tags
  CMT3 = MakerNOteCanon     Canon tags
  CMT4 = GPSInfo            GPS tags
  CNCV = CompressionVersion
  CNOP = CanonCNOP          Canon CNOP tags
  CNTH = CanonCNTH          Canon CNTH tags
  trak = track 1            Width, Height, Offset and length to embedded jpg in mdat
  trak = n                  Subsequent tracks (not used by Winnow)
  THMB = ThumbnailImage     JPG

*/
    if (p->report) p->rpt << "Container with metadata for image";
    QString type;
    quint32 moovEnd = offset + length;

//    qDebug() << "CanonCR3::moovBox" << "offset:" << offset << "length:" << length << "type:" << type;

    offset += 8;
    // uuid container
    p->file.seek(offset);
    nextHeifBox(length, type);
    getHeifBox(type, offset, length);
//    quint32 uuidEnd = offset + length;
//    if (type == "uuid") uuidBox(offset, length);
//    offset += 24;

    while (offset < moovEnd) {
        p->file.seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
    }

    offset = moovEnd;

    return true;
}

void CanonCR3::parseIfd0()
{
    if (G::isLogger) G::log("CanonCR3::parseIfd0");
    p->hdr = "IFD0";
    p->offset = ifd0Offset + 16;
    p->hash = &exif->hash;
    ifd->readIFD(*p);

    quint32 startOffset = ifd0Offset + 8;
    m->widthPreview = ifd->ifdDataHash.value(256).tagValue;
    m->heightPreview = ifd->ifdDataHash.value(257).tagValue;
    m->width = m->widthPreview;
    m->height = m->heightPreview;
    m->make = u.getString(p->file, ifd->ifdDataHash.value(271).tagValue + startOffset, ifd->ifdDataHash.value(271).tagCount);
    m->model = u.getString(p->file, ifd->ifdDataHash.value(272).tagValue + startOffset, ifd->ifdDataHash.value(272).tagCount);
    m->orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m->copyright = u.getString(p->file, ifd->ifdDataHash.value(33432).tagValue + startOffset, ifd->ifdDataHash.value(33432).tagCount);
    /*
    qDebug() << "CanonCR3::parseIfd0"
             << m->widthPreview
             << m->heightPreview
             << m->make
             << m->model
             << m->orientation
             << m->copyright
                ;
//                */
}

void CanonCR3::parseExif()
{
    if (G::isLogger) G::log("CanonCR3::parseExif");
    p->hdr = "IFD Exif";
    p->offset = exifOffset;
    ifd->readIFD(*p);

    quint32 startOffset = exifOffset - 8;

    // EXIF: created datetime
    QString createdExif;
    createdExif = u.getString(p->file, ifd->ifdDataHash.value(36868).tagValue + startOffset,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m->createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p->file,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      false);
        if (x < 1 ) {
            int t = qRound(1/x);
            m->exposureTime = "1/" + QString::number(t);
            m->exposureTimeNum = x;
        } else {
            uint t = static_cast<uint>(x);
            m->exposureTime = QString::number(t);
            m->exposureTimeNum = t;
        }
        m->exposureTime += " sec";
    } else {
        m->exposureTime = "";
    }

    // aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = u.getReal(p->file,
                                      ifd->ifdDataHash.value(33437).tagValue + startOffset,
                                      false);
        m->aperture = "f/" + QString::number(x, 'f', 1);
        m->apertureNum = (qRound(x * 10) / 10.0);
    } else {
        m->aperture = "";
        m->apertureNum = 0;
    }

    //ISO
    if (ifd->ifdDataHash.contains(34855)) {
        quint32 x = ifd->ifdDataHash.value(34855).tagValue;
        m->ISONum = static_cast<int>(x);
        m->ISO = QString::number(m->ISONum);
    } else {
        m->ISO = "";
        m->ISONum = 0;
    }

    // Exposure compensation
    if (ifd->ifdDataHash.contains(37380)) {
        // tagType = 10 signed rational
        double x = u.getReal_s(p->file,
                                      ifd->ifdDataHash.value(37380).tagValue + startOffset,
                                      false);
        m->exposureCompensation = QString::number(x, 'f', 1) + " EV";
        m->exposureCompensationNum = x;
    } else {
        m->exposureCompensation = "";
        m->exposureCompensationNum = 0;
    }

    // focal length
    if (ifd->ifdDataHash.contains(37386)) {
        double x = u.getReal(p->file,
                                      ifd->ifdDataHash.value(37386).tagValue + startOffset,
                                      false);
        m->focalLengthNum = static_cast<int>(x);
        m->focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m->focalLength = "";
        m->focalLengthNum = 0;
    }

    // IFD Exif: lens
    m->lens = u.getString(p->file, ifd->ifdDataHash.value(42036).tagValue + startOffset,
            ifd->ifdDataHash.value(42036).tagCount);

    // IFD Exif: camera serial number
    m->cameraSN = u.getString(p->file, ifd->ifdDataHash.value(42033).tagValue + startOffset,
            ifd->ifdDataHash.value(42033).tagCount);

    // IFD Exif: lens serial nember
    m->lensSN = u.getString(p->file, ifd->ifdDataHash.value(42037).tagValue + startOffset,
            ifd->ifdDataHash.value(42037).tagCount);

    // jpg preview metadata report information
    if (p->report) {
        p->offset = m->offsetFull;
        Jpeg jpeg;
        IPTC iptc;
//        GPS *gps;
        jpeg.parse(*p, *m, ifd, &iptc, exif, gps);
    }

    /*
    qDebug() << "CanonCR3::parseExif"
             << m.createdDate
             << m.exposureTime
             << m.aperture
             << m.ISO
             << m.exposureCompensation
             << m.focalLength
             << m.lens
             << m.cameraSN
             << m.lensSN
                ;
//                */

}
