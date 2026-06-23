#include "sony.h"
#include "Main/global.h"
#include "ImageFormats/Raw/cameramatrix.h"
#include <QSet>

Sony::Sony()
{
    sonyMakerHash[258] = "Image quality";
    sonyMakerHash[260] = "Flash exposure compensation in EV";
    sonyMakerHash[261] = "Teleconverter Model";
    sonyMakerHash[274] = "White Balance Fine Tune Value";
    sonyMakerHash[276] = "Camera Settings";
    sonyMakerHash[277] = "White balance";
    sonyMakerHash[278] = "Unknown";
    sonyMakerHash[3584] = "PrintIM information";
    sonyMakerHash[4096] = "Multi Burst Mode";
    sonyMakerHash[4097] = "Multi Burst Image Width";
    sonyMakerHash[4098] = "Multi Burst Image Height";
    sonyMakerHash[4099] = "Panorama";
    sonyMakerHash[8192] = "Unknown";
    sonyMakerHash[8193] = "JPEG preview image";
    sonyMakerHash[8194] = "Unknown";
    sonyMakerHash[8195] = "Unknown";
    sonyMakerHash[8196] = "Contrast";
    sonyMakerHash[8197] = "Saturation";
    sonyMakerHash[8198] = "Sharpness";
    sonyMakerHash[8199] = "Brightness";
    sonyMakerHash[8200] = "LongExposureNoiseReduction ";
    sonyMakerHash[8201] = "HighISONoiseReduction ";
    sonyMakerHash[8202] = "High Definition Range Mode";
    sonyMakerHash[8203] = "Multi Frame Noise Reduction";
    sonyMakerHash[8212] = "WBShiftAB_GM";
    sonyMakerHash[8220] = "AFAreaModeSetting";
    sonyMakerHash[12288] = "Shot Information IFD";
    sonyMakerHash[36944] = "ShutterCount Subdir";
    sonyMakerHash[45056] = "File Format";
    sonyMakerHash[45057] = "Sony Model ID";
    sonyMakerHash[45088] = "Color Reproduction";
    sonyMakerHash[45089] = "Color Temperature";
    sonyMakerHash[45090] = "Color Compensation Filter";
    sonyMakerHash[45091] = "Scene Mode";
    sonyMakerHash[45092] = "Zone Matching";
    sonyMakerHash[45093] = "Dynamic Range Optimizer";
    sonyMakerHash[45094] = "Image stabilization";
    sonyMakerHash[45095] = "Lens identifier";
    sonyMakerHash[45096] = "Minolta MakerNote";
    sonyMakerHash[45097] = "Color Mode";
    sonyMakerHash[45099] = "Full Image Size";
    sonyMakerHash[45100] = "Preview image size";
    sonyMakerHash[45120] = "Macro";
    sonyMakerHash[45121] = "Exposure Mode";
    sonyMakerHash[45122] = "Focus Mode";
    sonyMakerHash[45123] = "AF Mode";
    sonyMakerHash[45124] = "AF Illuminator";
    sonyMakerHash[45127] = "JPEG Quality";
    sonyMakerHash[45128] = "Flash Level";
    sonyMakerHash[45129] = "Release Mode";
    sonyMakerHash[45130] = "Shot number in continuous burst mode";
    sonyMakerHash[45131] = "Anti-Blur";
    sonyMakerHash[45134] = "Long Exposure Noise Reduction";
    sonyMakerHash[45135] = "Dynamic Range Optimizer";
    sonyMakerHash[45138] = "Intelligent Auto";
    sonyMakerHash[45140] = "White balance 2";

    sonyCypherHash[0] = 0;
    sonyCypherHash[1] = 1;
    sonyCypherHash[2] = 50;
    sonyCypherHash[3] = 177;
    sonyCypherHash[4] = 10;
    sonyCypherHash[5] = 14;
    sonyCypherHash[6] = 135;
    sonyCypherHash[7] = 40;
    sonyCypherHash[8] = 2;
    sonyCypherHash[8] = 251;
    sonyCypherHash[9] = 204;
    sonyCypherHash[10] = 202;
    sonyCypherHash[11] = 173;
    sonyCypherHash[12] = 27;
    sonyCypherHash[13] = 220;
    sonyCypherHash[14] = 8;
    sonyCypherHash[15] = 237;
    sonyCypherHash[16] = 100;
    sonyCypherHash[17] = 134;
    sonyCypherHash[18] = 240;
    sonyCypherHash[19] = 79;
    sonyCypherHash[20] = 140;
    sonyCypherHash[21] = 108;
    sonyCypherHash[22] = 184;
    sonyCypherHash[23] = 203;
    sonyCypherHash[24] = 105;
    sonyCypherHash[25] = 196;
    sonyCypherHash[26] = 44;
    sonyCypherHash[27] = 3;
    sonyCypherHash[27] = 252;
    sonyCypherHash[28] = 151;
    sonyCypherHash[29] = 182;
    sonyCypherHash[30] = 147;
    sonyCypherHash[31] = 124;
    sonyCypherHash[32] = 20;
    sonyCypherHash[33] = 243;
    sonyCypherHash[34] = 226;
    sonyCypherHash[35] = 62;
    sonyCypherHash[36] = 48;
    sonyCypherHash[37] = 142;
    sonyCypherHash[38] = 215;
    sonyCypherHash[39] = 96;
    sonyCypherHash[40] = 28;
    sonyCypherHash[41] = 161;
    sonyCypherHash[42] = 171;
    sonyCypherHash[43] = 55;
    sonyCypherHash[44] = 236;
    sonyCypherHash[45] = 117;
    sonyCypherHash[46] = 190;
    sonyCypherHash[47] = 35;
    sonyCypherHash[48] = 21;
    sonyCypherHash[49] = 106;
    sonyCypherHash[50] = 89;
    sonyCypherHash[51] = 63;
    sonyCypherHash[52] = 208;
    sonyCypherHash[53] = 185;
    sonyCypherHash[54] = 150;
    sonyCypherHash[55] = 181;
    sonyCypherHash[56] = 80;
    sonyCypherHash[57] = 39;
    sonyCypherHash[58] = 136;
    sonyCypherHash[59] = 227;
    sonyCypherHash[60] = 129;
    sonyCypherHash[61] = 148;
    sonyCypherHash[62] = 224;
    sonyCypherHash[63] = 192;
    sonyCypherHash[64] = 4;
    sonyCypherHash[65] = 92;
    sonyCypherHash[66] = 198;
    sonyCypherHash[67] = 232;
    sonyCypherHash[68] = 95;
    sonyCypherHash[69] = 75;
    sonyCypherHash[70] = 112;
    sonyCypherHash[71] = 56;
    sonyCypherHash[72] = 159;
    sonyCypherHash[73] = 130;
    sonyCypherHash[74] = 128;
    sonyCypherHash[75] = 81;
    sonyCypherHash[76] = 43;
    sonyCypherHash[77] = 197;
    sonyCypherHash[78] = 69;
    sonyCypherHash[79] = 73;
    sonyCypherHash[80] = 155;
    sonyCypherHash[81] = 33;
    sonyCypherHash[82] = 82;
    sonyCypherHash[83] = 83;
    sonyCypherHash[84] = 84;
    sonyCypherHash[85] = 133;
    sonyCypherHash[86] = 11;
    sonyCypherHash[87] = 93;
    sonyCypherHash[88] = 97;
    sonyCypherHash[89] = 218;
    sonyCypherHash[90] = 123;
    sonyCypherHash[91] = 85;
    sonyCypherHash[92] = 38;
    sonyCypherHash[93] = 36;
    sonyCypherHash[94] = 7;
    sonyCypherHash[95] = 110;
    sonyCypherHash[96] = 54;
    sonyCypherHash[97] = 91;
    sonyCypherHash[98] = 71;
    sonyCypherHash[99] = 183;
    sonyCypherHash[100] = 217;
    sonyCypherHash[101] = 74;
    sonyCypherHash[102] = 162;
    sonyCypherHash[103] = 223;
    sonyCypherHash[104] = 191;
    sonyCypherHash[105] = 18;
    sonyCypherHash[106] = 37;
    sonyCypherHash[107] = 188;
    sonyCypherHash[108] = 30;
    sonyCypherHash[109] = 127;
    sonyCypherHash[110] = 86;
    sonyCypherHash[111] = 234;
    sonyCypherHash[112] = 16;
    sonyCypherHash[113] = 230;
    sonyCypherHash[114] = 207;
    sonyCypherHash[115] = 103;
    sonyCypherHash[116] = 77;
    sonyCypherHash[117] = 60;
    sonyCypherHash[118] = 145;
    sonyCypherHash[119] = 131;
    sonyCypherHash[120] = 225;
    sonyCypherHash[121] = 49;
    sonyCypherHash[122] = 179;
    sonyCypherHash[123] = 111;
    sonyCypherHash[124] = 244;
    sonyCypherHash[125] = 5;
    sonyCypherHash[126] = 138;
    sonyCypherHash[127] = 70;
    sonyCypherHash[128] = 200;
    sonyCypherHash[129] = 24;
    sonyCypherHash[130] = 118;
    sonyCypherHash[131] = 104;
    sonyCypherHash[132] = 189;
    sonyCypherHash[133] = 172;
    sonyCypherHash[134] = 146;
    sonyCypherHash[135] = 42;
    sonyCypherHash[136] = 19;
    sonyCypherHash[137] = 233;
    sonyCypherHash[138] = 15;
    sonyCypherHash[139] = 163;
    sonyCypherHash[140] = 122;
    sonyCypherHash[141] = 219;
    sonyCypherHash[142] = 61;
    sonyCypherHash[143] = 212;
    sonyCypherHash[144] = 231;
    sonyCypherHash[145] = 58;
    sonyCypherHash[146] = 26;
    sonyCypherHash[147] = 87;
    sonyCypherHash[148] = 175;
    sonyCypherHash[149] = 32;
    sonyCypherHash[150] = 66;
    sonyCypherHash[151] = 178;
    sonyCypherHash[152] = 158;
    sonyCypherHash[153] = 195;
    sonyCypherHash[154] = 139;
    sonyCypherHash[155] = 242;
    sonyCypherHash[156] = 213;
    sonyCypherHash[157] = 211;
    sonyCypherHash[158] = 164;
    sonyCypherHash[159] = 126;
    sonyCypherHash[160] = 31;
    sonyCypherHash[161] = 152;
    sonyCypherHash[162] = 156;
    sonyCypherHash[163] = 238;
    sonyCypherHash[164] = 116;
    sonyCypherHash[165] = 165;
    sonyCypherHash[166] = 166;
    sonyCypherHash[167] = 167;
    sonyCypherHash[168] = 216;
    sonyCypherHash[169] = 94;
    sonyCypherHash[170] = 176;
    sonyCypherHash[171] = 180;
    sonyCypherHash[172] = 52;
    sonyCypherHash[173] = 206;
    sonyCypherHash[174] = 168;
    sonyCypherHash[175] = 121;
    sonyCypherHash[176] = 119;
    sonyCypherHash[177] = 90;
    sonyCypherHash[178] = 193;
    sonyCypherHash[179] = 137;
    sonyCypherHash[180] = 174;
    sonyCypherHash[181] = 154;
    sonyCypherHash[182] = 17;
    sonyCypherHash[183] = 51;
    sonyCypherHash[184] = 157;
    sonyCypherHash[185] = 245;
    sonyCypherHash[186] = 57;
    sonyCypherHash[187] = 25;
    sonyCypherHash[188] = 101;
    sonyCypherHash[189] = 120;
    sonyCypherHash[190] = 22;
    sonyCypherHash[191] = 113;
    sonyCypherHash[192] = 210;
    sonyCypherHash[193] = 169;
    sonyCypherHash[194] = 68;
    sonyCypherHash[195] = 99;
    sonyCypherHash[196] = 64;
    sonyCypherHash[197] = 41;
    sonyCypherHash[198] = 186;
    sonyCypherHash[199] = 160;
    sonyCypherHash[200] = 143;
    sonyCypherHash[201] = 228;
    sonyCypherHash[202] = 214;
    sonyCypherHash[203] = 59;
    sonyCypherHash[204] = 132;
    sonyCypherHash[205] = 13;
    sonyCypherHash[206] = 194;
    sonyCypherHash[207] = 78;
    sonyCypherHash[208] = 88;
    sonyCypherHash[209] = 221;
    sonyCypherHash[210] = 153;
    sonyCypherHash[211] = 34;
    sonyCypherHash[212] = 107;
    sonyCypherHash[213] = 201;
    sonyCypherHash[214] = 187;
    sonyCypherHash[215] = 23;
    sonyCypherHash[216] = 6;
    sonyCypherHash[217] = 229;
    sonyCypherHash[218] = 125;
    sonyCypherHash[219] = 102;
    sonyCypherHash[220] = 67;
    sonyCypherHash[221] = 98;
    sonyCypherHash[222] = 246;
    sonyCypherHash[223] = 205;
    sonyCypherHash[224] = 53;
    sonyCypherHash[225] = 144;
    sonyCypherHash[226] = 46;
    sonyCypherHash[227] = 65;
    sonyCypherHash[228] = 141;
    sonyCypherHash[229] = 109;
    sonyCypherHash[230] = 170;
    sonyCypherHash[231] = 9;
    sonyCypherHash[232] = 115;
    sonyCypherHash[233] = 149;
    sonyCypherHash[234] = 12;
    sonyCypherHash[235] = 241;
    sonyCypherHash[236] = 29;
    sonyCypherHash[237] = 222;
    sonyCypherHash[238] = 76;
    sonyCypherHash[239] = 47;
    sonyCypherHash[240] = 45;
    sonyCypherHash[241] = 247;
    sonyCypherHash[242] = 209;
    sonyCypherHash[243] = 114;
    sonyCypherHash[244] = 235;
    sonyCypherHash[245] = 239;
    sonyCypherHash[246] = 72;
    sonyCypherHash[247] = 199;
    sonyCypherHash[248] = 248;
}

bool Sony::parse(MetadataParameters &p,
                 ImageMetadata &m,
                 IFD *ifd,
                 Exif *exif,
                 Jpeg *jpeg,
                 GPS *gps)
{
    //file.open in Metadata::readMetadata
    // first two bytes is the endian order (skip next 2 bytes)
    Utilities u;
    quint16 order = u.get16(p.file.read(4));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // get offset to first IFD and read it
    quint32 offsetIfd0 = u.get32(p.file.read(4), isBigEnd);
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p);

    // pull data reqd from IFD0
    // cameras earlier than a1 referenced downsized preview here
    m.offsetFull = ifd->ifdDataHash.value(513).tagValue;
    m.lengthFull = ifd->ifdDataHash.value(514).tagValue;
    // get jpeg full size preview dimensions
    p.offset = m.offsetFull;
    jpeg->getWidthHeight(p, m.widthPreview, m.heightPreview);
//    if (lengthFullJPG) verifyEmbeddedJpg(offsetFull, lengthFull);
    m.offsetThumb = ifd->ifdDataHash.value(273).tagValue;
    m.lengthThumb = ifd->ifdDataHash.value(279).tagValue;

//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumb, lengthThumb);
    m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue, ifd->ifdDataHash.value(271).tagCount);
    m.model = "Sony " + u.getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount);
    m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue, ifd->ifdDataHash.value(315).tagCount);
    m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue, ifd->ifdDataHash.value(33432).tagCount);
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);

    // get the offset for GPSIFD
    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue;

    quint32 offsetEXIF;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;

    /* Sony provides an offset in IFD0 to subIFDs, but there is only one, which is
       at the offset ifd->ifdDataHash.value(330).tagValue */

    // SubIFD0
    quint32 offset = ifd->ifdDataHash.value(330).tagValue;
    p.hdr = "SubIFD0";
    p.offset = offset;
    ifd->readIFD(p);

//    if (m.orientation == 6 || m.orientation == 8) {
//        m.width = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
//        m.height = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
//    }
//    else {
        m.width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
        m.height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
//    }

    /* RAW sensor unpack info (read from SubIFD0, which holds the full-res CFA). Captured here,
       before the IFD hash is overwritten by later reads, so SonyRaw::UnpackCfa can use it on
       the cache path instead of re-walking the file. Only the uncompressed case is unpackable
       today; compressed ARW still falls back to the embedded JPG. Sony E-mount is RGGB,
       little-endian, with BlackLevel kept in the makernote (decoder applies its default).

       Gated on G::useRaw: when raw decoding is off, previews are used, so this work (and the
       later DataModel store) is skipped to keep the MetaRead path fast. If useRaw is toggled on
       after a folder is read, SonyRaw::UnpackCfa falls back to its self-walk, so nothing breaks. */
    if (G::useRaw &&
        ifd->ifdDataHash.contains(273) && ifd->ifdDataHash.contains(279) &&
        m.width > 0 && m.height > 0) {
        RawSensorInfo &ri = m.rawInfo;
        ri.isRaw = true;
        ri.stripOffset = ifd->ifdDataHash.value(273).tagValue;
        ri.stripLength = ifd->ifdDataHash.value(279).tagValue;
        ri.width = m.width;
        ri.height = m.height;
        ri.bitsPerSample = ifd->ifdDataHash.contains(258)
                               ? static_cast<int>(ifd->ifdDataHash.value(258).tagValue) : 14;
        ri.compression = ifd->ifdDataHash.contains(259)
                             ? static_cast<int>(ifd->ifdDataHash.value(259).tagValue) : 1;
        ri.samplesPerPixel = ifd->ifdDataHash.contains(277)
                                 ? static_cast<int>(ifd->ifdDataHash.value(277).tagValue) : 1;
        ri.littleEndianSamples = !isBigEnd;
        /* cfaPlaneColor left at its RGGB default (Sony E-mount). */
        if (ifd->ifdDataHash.contains(50717))      // 0xC61D WhiteLevel
            ri.white = ifd->ifdDataHash.value(50717).tagValue;

        /* BlackLevel (0x7310, SHORT x4, RGGB order) and the as-shot WB_RGGBLevels (0x7313,
           SSHORT x4, R G1 G2 B) live in this SubIFD (Sony's "SR2" tags, stored in the clear on
           ARW). Their four values exceed 4 bytes, so the IFD entry holds an offset -- seek to
           it and read the four samples. */
        if (ifd->ifdDataHash.contains(0x7310)) {
            p.file.seek(ifd->ifdDataHash.value(0x7310).tagValue);
            for (int i = 0; i < 4; ++i)
                ri.black[i] = u.get16(p.file.read(2), isBigEnd);
        }
        if (ifd->ifdDataHash.contains(0x7313)) {
            p.file.seek(ifd->ifdDataHash.value(0x7313).tagValue);
            const int r  = static_cast<qint16>(u.get16(p.file.read(2), isBigEnd));
            const int g1 = static_cast<qint16>(u.get16(p.file.read(2), isBigEnd));
            const int g2 = static_cast<qint16>(u.get16(p.file.read(2), isBigEnd));
            const int b  = static_cast<qint16>(u.get16(p.file.read(2), isBigEnd));
            /* RawImage.camMul is (R, G, B, G2); these are gains to apply (R,B > G for daylight),
               so RawColor uses them directly as the as-shot white balance. */
            if (r > 0 && g1 > 0 && b > 0) {
                ri.camMul[0] = r;  ri.camMul[1] = g1;
                ri.camMul[2] = b;  ri.camMul[3] = g2;
            }
        }

        /* Colour: per-model XYZ->camera matrix for RawColor (combined with the as-shot WB
           above to produce accurate colour). */
        ri.hasColorMatrix = xyzToCamForModel(m.model, ri.xyzToCam);
    }

    // IFD 1:
    p.hdr = "IFD1";
    p.offset = nextIFDOffset;
    if (nextIFDOffset) {
        nextIFDOffset = ifd->readIFD(p);
        m.offsetThumb = ifd->ifdDataHash.value(513).tagValue;
        m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;
    }
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    // IFD 2:
    p.hdr = "IFD2";
    p.offset = nextIFDOffset;
    if (nextIFDOffset) {
        nextIFDOffset = ifd->readIFD(p);
        m.offsetFull = ifd->ifdDataHash.value(513).tagValue;
        m.lengthFull = ifd->ifdDataHash.value(514).tagValue;
        p.offset = m.offsetFull;
        jpeg->getWidthHeight(p, m.widthPreview, m.heightPreview);
    }

    // get the offset for ExifIFD and read it
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p);

    // IFD EXIF: dimensions
//    m.width = ifd->ifdDataHash.value(40962).tagValue;
//    m.height = ifd->ifdDataHash.value(40963).tagValue;

    // EXIF: created datetime
    QString createdExif;
    createdExif = u.getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // Exif: get shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue,
                                      isBigEnd);
        if (x < 1) {
            int t = qRound(1/x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = x;
        } else {
            uint t = static_cast<uint>(x);
            m.exposureTime = QString::number(t);
            m.exposureTimeNum = t;
        }
        m.exposureTime += " sec";
    } else {
        m.exposureTime = "";
    }

    // Exif: aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33437).tagValue,
                                      isBigEnd);
        m.aperture = "f/" + QString::number(x, 'f', 1);
        m.apertureNum = (qRound(x * 10) / 10.0);
    } else {
        m.aperture = "";
        m.apertureNum = 0;
    }

    //Exif: ISO
    if (ifd->ifdDataHash.contains(34855)) {
        quint32 x = ifd->ifdDataHash.value(34855).tagValue;
        m.ISONum = static_cast<int>(x);
        m.ISO = QString::number(m.ISONum);
//        ISO = "ISO " + QString::number(x);
    } else {
        m.ISO = "";
        m.ISONum = 0;
    }

    // EXIF: Exposure compensation
    if (ifd->ifdDataHash.contains(37380)) {
        // tagType = 10 signed rational
        double x = u.getReal_s(p.file,
                                      ifd->ifdDataHash.value(37380).tagValue,
                                      isBigEnd);
        m.exposureCompensation = QString::number(x, 'f', 1) + " EV";
        m.exposureCompensationNum = x;
    } else {
        m.exposureCompensation = "";
        m.exposureCompensationNum = 0;
    }

    // Exif: focal length
    if (ifd->ifdDataHash.contains(37386)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(37386).tagValue,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // Exif: lens
    m.lens = u.getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
        ifd->ifdDataHash.value(42036).tagCount);

    // Exif: read makernoteIFD

    if (ifd->ifdDataHash.contains(37500)) {
        quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue;
        p.hdr = "IFD Sony Maker Note";
        p.offset = makerOffset;
        p.hash = &sonyMakerHash;
        ifd->readIFD(p);
    }

    // MakerNote: focus (width, height, x, y) each 16 bits

    if (ifd->ifdDataHash.contains(8231)) {
        quint32 focusOffset = ifd->ifdDataHash.value(8231).tagValue;
        p.file.seek(focusOffset);
        int w = u.get16(p.file.read(2), isBigEnd);
        int h = u.get16(p.file.read(2), isBigEnd);
        int x = u.get16(p.file.read(2), isBigEnd);
        int y = u.get16(p.file.read(2), isBigEnd);
        m.focusX = x * 1.0 / w;
        m.focusY = y * 1.0 / h;
    }

    /* Decipher/encipher Sony tag 0x2010, 0x900b, 0x9050 and 0x940x data
       MakernoteSubIFD with shuttercount is in 0x9050 subdirectory at offset 0x3A (58)  */

    if (sonyMakerHash.contains(36944)) {
        quint32 shutterCountOffset = ifd->ifdDataHash.value(36944).tagValue + 58;
        p.file.seek(shutterCountOffset);
        int d1 = sonyCypherHash[u.get8(p.file.read(1))];
        int d2 = sonyCypherHash[u.get8(p.file.read(1))] * 256;
        int d3 = sonyCypherHash[u.get8(p.file.read(1))] * 256 * 256;
        int d4 = sonyCypherHash[u.get8(p.file.read(1))] * 256 * 256 * 256;
        m.shutterCount = d1 + d2 + d3 + d4;
    }

    // read GPSIFD
    if (offsetGPS) {
        p.file.seek(offsetGPS);
        p.hdr = "IFD GPS";
        p.hash = &gps->hash;
        p.offset = offsetGPS;
        ifd->readIFD(p);

        if (ifd->ifdDataHash.contains(1)) {  // 1 = GPSLatitudeRef
            // process GPS info
            QString gpsCoord = gps->decode(p.file, ifd->ifdDataHash, isBigEnd);
            m.gpsCoord = gpsCoord;
        }
    }

    // Sony does not embed xmp in raw files

    // jpg preview metadata report information
    if (p.report) {
        p.offset = m.offsetFull;
        Jpeg jpeg;
        IPTC iptc;
        GPS gps;
        jpeg.parse(p, m, ifd, &iptc, exif, &gps);
    }

    return true;

}

/* ------------------------------------------------------------------------------------------
   SonyRaw::UnpackCfa  --  uncompressed ARW sensor unpack
   ------------------------------------------------------------------------------------------ */

namespace {

/* TIFF tag types -> byte size. */
int tiffTypeSize(quint16 t)
{
    switch (t) {
    case 1: case 2: case 6: case 7: return 1;   // BYTE/ASCII/SBYTE/UNDEFINED
    case 3: case 8:                 return 2;   // SHORT/SSHORT
    case 4: case 9: case 11:        return 4;   // LONG/SLONG/FLOAT
    case 5: case 10: case 12:       return 8;   // RATIONAL/SRATIONAL/DOUBLE
    default:                        return 1;
    }
}

/* One IFD entry: type, count, and the raw 4 value-bytes (offset or inline value). */
struct RawTag {
    quint16 type = 0;
    quint32 count = 0;
    QByteArray val;     // exactly 4 bytes as stored in the entry
};

/*
    Self-contained TIFF/EP walk used as a FALLBACK when the metadata read did not populate
    ImageMetadata::rawInfo (e.g. an independent decode of a file whose parser ran without the
    raw fields). Locates the uncompressed full-resolution CFA IFD and fills a RawSensorInfo;
    the shared unpack below then reads the strip. Returns false if no uncompressed CFA IFD is
    present (compressed ARW -> caller falls back to the embedded JPG).
*/
bool readSonyTiffSensorInfo(QFile &file, RawSensorInfo &info)
{
    if (!file.seek(0)) return false;
    const QByteArray hdr = file.read(8);
    if (hdr.size() < 8) return false;
    const bool big = (quint8(hdr[0]) == 'M' && quint8(hdr[1]) == 'M');

    /* Endian-aware readers over a byte buffer. */
    auto g16 = [&](const QByteArray &b, int i) -> quint32 {
        const quint8 x = quint8(b[i]), y = quint8(b[i+1]);
        return big ? (quint32(x) << 8 | y) : (quint32(y) << 8 | x);
    };
    auto g32 = [&](const QByteArray &b, int i) -> quint32 {
        const quint8 x = quint8(b[i]), y = quint8(b[i+1]), z = quint8(b[i+2]), w = quint8(b[i+3]);
        return big ? (quint32(x)<<24 | quint32(y)<<16 | quint32(z)<<8 | w)
                   : (quint32(w)<<24 | quint32(z)<<16 | quint32(y)<<8 | x);
    };
    auto readAt = [&](quint32 off, int n) -> QByteArray {
        if (!file.seek(off)) return QByteArray();
        return file.read(n);
    };

    /* Read one IFD: fill tags, append any SubIFD offsets, return the next-IFD offset. */
    auto readIfd = [&](quint32 off, QHash<quint16, RawTag> &tags,
                       QList<quint32> &subs, quint32 &next) -> bool {
        const QByteArray cnt = readAt(off, 2);
        if (cnt.size() < 2) return false;
        const int n = int(g16(cnt, 0));
        const QByteArray body = readAt(off + 2, n * 12 + 4);
        if (body.size() < n * 12 + 4) return false;
        for (int i = 0; i < n; ++i) {
            const int e = i * 12;
            const quint16 tag = quint16(g16(body, e));
            RawTag t;
            t.type  = quint16(g16(body, e + 2));
            t.count = g32(body, e + 4);
            t.val   = body.mid(e + 8, 4);
            tags.insert(tag, t);
            if (tag == 330) {                       // SubIFDs
                const int sz = tiffTypeSize(t.type) * int(t.count);
                const QByteArray ob = (sz <= 4) ? t.val : readAt(g32(t.val, 0), int(t.count) * 4);
                for (quint32 k = 0; k < t.count && int((k+1)*4) <= ob.size(); ++k)
                    subs << g32(ob, int(k * 4));
            }
        }
        next = g32(body, n * 12);
        return true;
    };

    /* First value of a tag as an unsigned scalar (SHORT or LONG, inline). */
    auto scalar = [&](const RawTag &t) -> quint32 {
        return t.type == 3 ? g16(t.val, 0) : g32(t.val, 0);
    };

    /* ASCII string value of a tag (inline if <= 4 bytes, else at the offset). */
    auto readAscii = [&](const RawTag &t) -> QString {
        const int n = int(t.count);
        if (n <= 0) return QString();
        QByteArray s = (tiffTypeSize(t.type) * n <= 4) ? t.val.left(n)
                                                       : readAt(g32(t.val, 0), n);
        const int z = s.indexOf('\0');
        if (z >= 0) s = s.left(z);
        return QString::fromLatin1(s);
    };

    /* Four 16-bit values of a tag (stored at an offset since 4*2 > 4 bytes). */
    auto read4 = [&](const RawTag &t, int out[4]) -> bool {
        const QByteArray b = readAt(g32(t.val, 0), 8);
        if (b.size() < 8) return false;
        for (int i = 0; i < 4; ++i) out[i] = qint16(g16(b, i * 2));
        return true;
    };

    /* Collect IFD0 + its SubIFDs + the next-IFD chain, then pick the raw CFA IFD. */
    QList<quint32> queue { g32(hdr, 4) };
    QSet<quint32> seen;
    QHash<quint16, RawTag> rawIfd;      // tags of the chosen raw IFD
    QString model;                      // tag 272 (from IFD0) for the colour-matrix lookup
    quint64 bestPixels = 0;

    while (!queue.isEmpty()) {
        const quint32 off = queue.takeFirst();
        if (off == 0 || seen.contains(off)) continue;
        seen.insert(off);

        QHash<quint16, RawTag> tags;
        QList<quint32> subs;
        quint32 next = 0;
        if (!readIfd(off, tags, subs, next)) continue;
        for (quint32 s : subs) queue << s;
        if (next) queue << next;

        if (model.isEmpty() && tags.contains(272))   // Model (IFD0), for the colour matrix
            model = "Sony " + readAscii(tags[272]);

        /* Raw CFA candidate: uncompressed, single-sample, with a strip. */
        const bool hasStrip = tags.contains(273) && tags.contains(279) &&
                              tags.contains(256) && tags.contains(257);
        const quint32 comp = tags.contains(259) ? scalar(tags[259]) : 0;
        const quint32 spp  = tags.contains(277) ? scalar(tags[277]) : 1;
        if (hasStrip && comp == 1 && spp == 1) {
            const quint64 px = quint64(scalar(tags[256])) * scalar(tags[257]);
            if (px > bestPixels) { bestPixels = px; rawIfd = tags; }
        }
    }

    if (rawIfd.isEmpty()) return false;

    info.isRaw = true;
    info.width = int(scalar(rawIfd[256]));
    info.height = int(scalar(rawIfd[257]));
    info.bitsPerSample = rawIfd.contains(258) ? int(scalar(rawIfd[258])) : 14;
    info.compression = 1;                       // candidate selection required comp == 1
    info.samplesPerPixel = 1;
    info.stripOffset = scalar(rawIfd[273]);
    info.stripLength = scalar(rawIfd[279]);
    info.littleEndianSamples = !big;
    if (rawIfd.contains(33422) && rawIfd[33422].val.size() >= 4)
        for (int i = 0; i < 4; ++i) info.cfaPlaneColor[i] = quint8(rawIfd[33422].val[i]);
    if (rawIfd.contains(50717)) info.white = scalar(rawIfd[50717]);   // 0xC61D WhiteLevel

    /* Colour, so the fallback path produces the SAME result as the plumbed path: BlackLevel
       (0x7310), as-shot WB_RGGBLevels (0x7313, R G1 G2 B -> camMul R,G,B,G2), and the per-model
       XYZ->camera matrix. Without these the render would be the green-tinted identity result. */
    int v4[4];
    if (rawIfd.contains(0x7310) && read4(rawIfd[0x7310], v4))
        for (int i = 0; i < 4; ++i) info.black[i] = quint32(v4[i] < 0 ? 0 : v4[i]);
    else if (rawIfd.contains(50714))
        for (int i = 0; i < 4; ++i) info.black[i] = scalar(rawIfd[50714]);
    if (rawIfd.contains(0x7313) && read4(rawIfd[0x7313], v4) &&
        v4[0] > 0 && v4[1] > 0 && v4[3] > 0) {
        info.camMul[0] = v4[0]; info.camMul[1] = v4[1];   // R, G1
        info.camMul[2] = v4[3]; info.camMul[3] = v4[2];   // B, G2
    }
    info.hasColorMatrix = xyzToCamForModel(model, info.xyzToCam);
    return true;
}

} // namespace

bool SonyRaw::UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw)
{
/*
    Unpack an uncompressed Sony ARW CFA mosaic. Prefer the RawSensorInfo plumbed onto
    ImageMetadata by Sony::parse (the cache path); fall back to a self-contained TIFF walk when
    it is absent (e.g. an independent decode without raw metadata). Both paths converge on the
    shared unpack below, so behaviour is identical either way.
*/
    RawSensorInfo info;
    if (m.rawInfo.isValid()) {
        info = m.rawInfo;
    }
    else if (!readSonyTiffSensorInfo(file, info)) {
        errMsg = "Sony raw: no uncompressed CFA IFD (compressed ARW not supported yet).";
        return false;
    }

    const int W   = info.width;
    const int H   = info.height;
    const int bps = info.bitsPerSample;

    if (W <= 0 || H <= 0 || bps <= 0 || bps > 16) {
        errMsg = "Sony raw: bad CFA dimensions/bps.";
        return false;
    }
    if (info.compression != 1) {
        errMsg = "Sony raw: compressed ARW not supported yet.";
        return false;
    }
    /* This path handles 16-bit-per-sample storage only (the uncompressed ARW case). */
    if (info.stripLength < quint32(W) * quint32(H) * 2) {
        errMsg = "Sony raw: strip smaller than 16-bit storage (packed/compressed unsupported).";
        return false;
    }

    raw.pattern = cfaPatternFromPlaneColor(info.cfaPlaneColor);
    if (raw.pattern == CfaPattern::Unknown) raw.pattern = CfaPattern::RGGB;

    /* Colour data for RawColor (identity/1s when the parser did not fill it -> approx colour). */
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) raw.xyzToCam[i][j] = info.xyzToCam[i][j];
    for (int i = 0; i < 4; ++i) raw.camMul[i] = info.camMul[i];

    raw.white = info.white ? quint16(info.white) : quint16((1u << bps) - 1);

    /* BlackLevel is absent from the ARW IFD (Sony keeps it in the makernote). Use a
       reasonable default until the makernote value is plumbed (see real-RawColor task). */
    const quint16 blackDefault = 512;
    const bool haveBlack = info.black[0] || info.black[1] || info.black[2] || info.black[3];
    for (int i = 0; i < 4; ++i)
        raw.black[i] = haveBlack ? quint16(info.black[i]) : blackDefault;

    /* Read the sensor strip and unpack little/big-endian uint16 samples (masked to bps). */
    if (!file.seek(info.stripOffset)) {
        errMsg = "Sony raw: seek to sensor strip failed.";
        return false;
    }
    const QByteArray strip = file.read(int(quint32(W) * quint32(H) * 2));
    if (strip.size() < W * H * 2) {
        errMsg = "Sony raw: could not read full sensor strip.";
        return false;
    }

    raw.width = W;
    raw.height = H;
    raw.cfa.resize(size_t(W) * size_t(H));
    const quint16 mask = quint16((1u << bps) - 1);
    const uchar *s = reinterpret_cast<const uchar *>(strip.constData());
    const size_t count = size_t(W) * size_t(H);
    if (!info.littleEndianSamples) {
        for (size_t i = 0; i < count; ++i)
            raw.cfa[i] = quint16((quint16(s[2*i]) << 8 | s[2*i + 1]) & mask);
    } else {
        for (size_t i = 0; i < count; ++i)
            raw.cfa[i] = quint16((quint16(s[2*i + 1]) << 8 | s[2*i]) & mask);
    }

    return true;
}
