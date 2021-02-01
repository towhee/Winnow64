#include "sony.h"

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
                 Jpeg *jpeg)
{
    //file.open in Metadata::readMetadata
    // first two bytes is the endian order (skip next 2 bytes)
    quint16 order = Utilities::get16(p.file.read(4));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // get offset to first IFD and read it
    quint32 offsetIfd0 = Utilities::get32(p.file.read(4), isBigEnd);
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p, m);

    // pull data reqd from IFD0
    m.offsetFull = ifd->ifdDataHash.value(513).tagValue;
    m.lengthFull = ifd->ifdDataHash.value(514).tagValue;
    // get jpeg full size preview dimensions
    p.offset = m.offsetFull;
    jpeg->getWidthHeight(p, m.widthFull, m.heightFull);
//    if (lengthFullJPG) verifyEmbeddedJpg(offsetFull, lengthFull);
    m.offsetThumb = ifd->ifdDataHash.value(273).tagValue;
    m.lengthThumb = ifd->ifdDataHash.value(279).tagValue;

//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumb, lengthThumb);
    m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount);
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);

    quint32 offsetEXIF;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;

    /* Sony provides an offset in IFD0 to subIFDs, but there is only one, which is
       at the offset ifd->ifdDataHash.value(330).tagValue */

    // SubIFD0
    quint32 offset = ifd->ifdDataHash.value(330).tagValue;
    p.hdr = "SubIFD0";
    p.offset = offset;
    ifd->readIFD(p, m);
    m.width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);

    // IFD 1:
    p.hdr = "IFD1";
    p.offset = nextIFDOffset;
    if (nextIFDOffset) ifd->readIFD(p, m);

    m.offsetThumb = ifd->ifdDataHash.value(513).tagValue;
    m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    // get the offset for ExifIFD and read it
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p, m);

    // IFD EXIF: dimensions
//    m.width = ifd->ifdDataHash.value(40962).tagValue;
//    m.height = ifd->ifdDataHash.value(40963).tagValue;

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // Exif: get shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue,
                                      isBigEnd);
        if (x <1 ) {
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
        double x = Utilities::getReal(p.file,
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
        double x = Utilities::getReal_s(p.file,
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
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(37386).tagValue,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // Exif: lens
    m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
        ifd->ifdDataHash.value(42036).tagCount);

    // Exif: read makernoteIFD

    if (ifd->ifdDataHash.contains(37500)) {
        quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue;
        p.hdr = "IFD Sony Maker Note";
        p.offset = makerOffset;
        p.hash = &sonyMakerHash;
        ifd->readIFD(p, m);
    }

    /* Decipher/encipher Sony tag 0x2010, 0x900b, 0x9050 and 0x940x data
       MakernoteSubIFD with shuttercount is in 0x9050 subdirectory at offset 0x3A (58)  */

    if (sonyMakerHash.contains(36944)) {
        quint32 shutterCountOffset = ifd->ifdDataHash.value(36944).tagValue + 58;
        p.file.seek(shutterCountOffset);
        int d1 = sonyCypherHash[Utilities::get8(p.file.read(1))];
        int d2 = sonyCypherHash[Utilities::get8(p.file.read(1))] * 256;
        int d3 = sonyCypherHash[Utilities::get8(p.file.read(1))] * 256 * 256;
        int d4 = sonyCypherHash[Utilities::get8(p.file.read(1))] * 256 * 256 * 256;
        m.shutterCount = d1 + d2 + d3 + d4;
    }

    // Sony does not embed xmp in raw files

    return true;

}
