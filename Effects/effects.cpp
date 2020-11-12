#include "effects.h"

#define M_EPSILON 1.0e-6
#define M_SQ2PI 2.50662827463100024161235523934010416269302368164062

Effects::Effects()
{
    rgbToEv[1] = -831;
    rgbToEv[2] = -805;
    rgbToEv[3] = -779;
    rgbToEv[4] = -754;
    rgbToEv[5] = -730;
    rgbToEv[6] = -707;
    rgbToEv[7] = -685;
    rgbToEv[8] = -663;
    rgbToEv[9] = -642;
    rgbToEv[10] = -622;
    rgbToEv[11] = -603;
    rgbToEv[12] = -584;
    rgbToEv[13] = -566;
    rgbToEv[14] = -548;
    rgbToEv[15] = -531;
    rgbToEv[16] = -515;
    rgbToEv[17] = -499;
    rgbToEv[18] = -484;
    rgbToEv[19] = -470;
    rgbToEv[20] = -456;
    rgbToEv[21] = -442;
    rgbToEv[22] = -429;
    rgbToEv[23] = -416;
    rgbToEv[24] = -404;
    rgbToEv[25] = -393;
    rgbToEv[26] = -381;
    rgbToEv[27] = -370;
    rgbToEv[28] = -360;
    rgbToEv[29] = -350;
    rgbToEv[30] = -340;
    rgbToEv[31] = -331;
    rgbToEv[32] = -322;
    rgbToEv[33] = -314;
    rgbToEv[34] = -305;
    rgbToEv[35] = -297;
    rgbToEv[36] = -290;
    rgbToEv[37] = -282;
    rgbToEv[38] = -275;
    rgbToEv[39] = -269;
    rgbToEv[40] = -262;
    rgbToEv[41] = -256;
    rgbToEv[42] = -250;
    rgbToEv[43] = -244;
    rgbToEv[44] = -238;
    rgbToEv[45] = -233;
    rgbToEv[46] = -228;
    rgbToEv[47] = -223;
    rgbToEv[48] = -218;
    rgbToEv[49] = -213;
    rgbToEv[50] = -209;
    rgbToEv[51] = -204;
    rgbToEv[52] = -200;
    rgbToEv[53] = -196;
    rgbToEv[54] = -192;
    rgbToEv[55] = -188;
    rgbToEv[56] = -185;
    rgbToEv[57] = -181;
    rgbToEv[58] = -178;
    rgbToEv[59] = -175;
    rgbToEv[60] = -171;
    rgbToEv[61] = -168;
    rgbToEv[62] = -165;
    rgbToEv[63] = -162;
    rgbToEv[64] = -159;
    rgbToEv[65] = -156;
    rgbToEv[66] = -153;
    rgbToEv[67] = -151;
    rgbToEv[68] = -148;
    rgbToEv[69] = -145;
    rgbToEv[70] = -143;
    rgbToEv[71] = -140;
    rgbToEv[72] = -138;
    rgbToEv[73] = -135;
    rgbToEv[74] = -133;
    rgbToEv[75] = -130;
    rgbToEv[76] = -128;
    rgbToEv[77] = -126;
    rgbToEv[78] = -123;
    rgbToEv[79] = -121;
    rgbToEv[80] = -118;
    rgbToEv[81] = -116;
    rgbToEv[82] = -114;
    rgbToEv[83] = -111;
    rgbToEv[84] = -109;
    rgbToEv[85] = -107;
    rgbToEv[86] = -104;
    rgbToEv[87] = -102;
    rgbToEv[88] = -100;
    rgbToEv[89] = -97;
    rgbToEv[90] = -95;
    rgbToEv[91] = -93;
    rgbToEv[92] = -90;
    rgbToEv[93] = -88;
    rgbToEv[94] = -85;
    rgbToEv[95] = -83;
    rgbToEv[96] = -80;
    rgbToEv[97] = -78;
    rgbToEv[98] = -75;
    rgbToEv[99] = -73;
    rgbToEv[100] = -70;
    rgbToEv[101] = -68;
    rgbToEv[102] = -65;
    rgbToEv[103] = -63;
    rgbToEv[104] = -60;
    rgbToEv[105] = -58;
    rgbToEv[106] = -55;
    rgbToEv[107] = -53;
    rgbToEv[108] = -50;
    rgbToEv[109] = -47;
    rgbToEv[110] = -45;
    rgbToEv[111] = -42;
    rgbToEv[112] = -39;
    rgbToEv[113] = -37;
    rgbToEv[114] = -34;
    rgbToEv[115] = -31;
    rgbToEv[116] = -28;
    rgbToEv[117] = -26;
    rgbToEv[118] = -23;
    rgbToEv[119] = -20;
    rgbToEv[120] = -18;
    rgbToEv[121] = -15;
    rgbToEv[122] = -12;
    rgbToEv[123] = -9;
    rgbToEv[124] = -7;
    rgbToEv[125] = -4;
    rgbToEv[126] = -1;
    rgbToEv[127] = 2;
    rgbToEv[128] = 4;
    rgbToEv[129] = 7;
    rgbToEv[130] = 10;
    rgbToEv[131] = 12;
    rgbToEv[132] = 15;
    rgbToEv[133] = 18;
    rgbToEv[134] = 20;
    rgbToEv[135] = 23;
    rgbToEv[136] = 26;
    rgbToEv[137] = 28;
    rgbToEv[138] = 31;
    rgbToEv[139] = 33;
    rgbToEv[140] = 36;
    rgbToEv[141] = 38;
    rgbToEv[142] = 41;
    rgbToEv[143] = 43;
    rgbToEv[144] = 46;
    rgbToEv[145] = 48;
    rgbToEv[146] = 50;
    rgbToEv[147] = 53;
    rgbToEv[148] = 55;
    rgbToEv[149] = 57;
    rgbToEv[150] = 60;
    rgbToEv[151] = 62;
    rgbToEv[152] = 64;
    rgbToEv[153] = 66;
    rgbToEv[154] = 68;
    rgbToEv[155] = 70;
    rgbToEv[156] = 72;
    rgbToEv[157] = 74;
    rgbToEv[158] = 76;
    rgbToEv[159] = 78;
    rgbToEv[160] = 80;
    rgbToEv[161] = 82;
    rgbToEv[162] = 83;
    rgbToEv[163] = 85;
    rgbToEv[164] = 87;
    rgbToEv[165] = 88;
    rgbToEv[166] = 90;
    rgbToEv[167] = 92;
    rgbToEv[168] = 93;
    rgbToEv[169] = 95;
    rgbToEv[170] = 96;
    rgbToEv[171] = 97;
    rgbToEv[172] = 99;
    rgbToEv[173] = 100;
    rgbToEv[174] = 101;
    rgbToEv[175] = 103;
    rgbToEv[176] = 104;
    rgbToEv[177] = 105;
    rgbToEv[178] = 106;
    rgbToEv[179] = 107;
    rgbToEv[180] = 108;
    rgbToEv[181] = 109;
    rgbToEv[182] = 110;
    rgbToEv[183] = 111;
    rgbToEv[184] = 112;
    rgbToEv[185] = 113;
    rgbToEv[186] = 114;
    rgbToEv[187] = 115;
    rgbToEv[188] = 116;
    rgbToEv[189] = 117;
    rgbToEv[190] = 118;
    rgbToEv[191] = 119;
    rgbToEv[192] = 120;
    rgbToEv[193] = 120;
    rgbToEv[194] = 121;
    rgbToEv[195] = 122;
    rgbToEv[196] = 123;
    rgbToEv[197] = 124;
    rgbToEv[198] = 125;
    rgbToEv[199] = 126;
    rgbToEv[200] = 127;
    rgbToEv[201] = 128;
    rgbToEv[202] = 129;
    rgbToEv[203] = 130;
    rgbToEv[204] = 132;
    rgbToEv[205] = 133;
    rgbToEv[206] = 134;
    rgbToEv[207] = 136;
    rgbToEv[208] = 137;
    rgbToEv[209] = 139;
    rgbToEv[210] = 140;
    rgbToEv[211] = 142;
    rgbToEv[212] = 144;
    rgbToEv[213] = 146;
    rgbToEv[214] = 148;
    rgbToEv[215] = 150;
    rgbToEv[216] = 153;
    rgbToEv[217] = 155;
    rgbToEv[218] = 158;
    rgbToEv[219] = 161;
    rgbToEv[220] = 164;
    rgbToEv[221] = 167;
    rgbToEv[222] = 170;
    rgbToEv[223] = 174;
    rgbToEv[224] = 177;
    rgbToEv[225] = 181;
    rgbToEv[226] = 186;
    rgbToEv[227] = 190;
    rgbToEv[228] = 195;
    rgbToEv[229] = 200;
    rgbToEv[230] = 205;
    rgbToEv[231] = 210;
    rgbToEv[232] = 216;
    rgbToEv[233] = 222;
    rgbToEv[234] = 229;
    rgbToEv[235] = 236;
    rgbToEv[236] = 243;
    rgbToEv[237] = 250;
    rgbToEv[238] = 258;
    rgbToEv[239] = 267;
    rgbToEv[240] = 275;
    rgbToEv[241] = 284;
    rgbToEv[242] = 294;
    rgbToEv[243] = 304;
    rgbToEv[244] = 314;
    rgbToEv[245] = 325;
    rgbToEv[246] = 337;
    rgbToEv[247] = 349;
    rgbToEv[248] = 361;
    rgbToEv[249] = 374;
    rgbToEv[250] = 388;
    rgbToEv[251] = 402;
    rgbToEv[252] = 417;
    rgbToEv[253] = 432;
    rgbToEv[254] = 448;
    rgbToEv[255] = 465;

    evToRgb[-500] = 16;
    evToRgb[-490] = 18;
    evToRgb[-480] = 20;
    evToRgb[-470] = 22;
    evToRgb[-460] = 23;
    evToRgb[-450] = 24;
    evToRgb[-440] = 25;
    evToRgb[-430] = 26;
    evToRgb[-420] = 26;
    evToRgb[-410] = 27;
    evToRgb[-400] = 27;
    evToRgb[-390] = 28;
    evToRgb[-380] = 28;
    evToRgb[-370] = 29;
    evToRgb[-360] = 29;
    evToRgb[-350] = 29;
    evToRgb[-340] = 30;
    evToRgb[-330] = 31;
    evToRgb[-320] = 31;
    evToRgb[-310] = 32;
    evToRgb[-300] = 33;
    evToRgb[-290] = 34;
    evToRgb[-280] = 36;
    evToRgb[-270] = 37;
    evToRgb[-260] = 38;
    evToRgb[-250] = 40;
    evToRgb[-240] = 42;
    evToRgb[-230] = 44;
    evToRgb[-220] = 46;
    evToRgb[-210] = 48;
    evToRgb[-200] = 51;
    evToRgb[-190] = 54;
    evToRgb[-180] = 56;
    evToRgb[-170] = 59;
    evToRgb[-160] = 63;
    evToRgb[-150] = 66;
    evToRgb[-140] = 69;
    evToRgb[-130] = 73;
    evToRgb[-120] = 77;
    evToRgb[-110] = 81;
    evToRgb[-100] = 85;
    evToRgb[-90] = 89;
    evToRgb[-80] = 93;
    evToRgb[-70] = 98;
    evToRgb[-60] = 102;
    evToRgb[-50] = 107;
    evToRgb[-40] = 111;
    evToRgb[-30] = 116;
    evToRgb[-20] = 121;
    evToRgb[-10] = 126;
    evToRgb[0] = 131;
    evToRgb[10] = 136;
    evToRgb[20] = 140;
    evToRgb[30] = 145;
    evToRgb[40] = 150;
    evToRgb[50] = 155;
    evToRgb[60] = 160;
    evToRgb[70] = 165;
    evToRgb[80] = 170;
    evToRgb[90] = 175;
    evToRgb[100] = 179;
    evToRgb[110] = 184;
    evToRgb[120] = 188;
    evToRgb[130] = 193;
    evToRgb[140] = 197;
    evToRgb[150] = 201;
    evToRgb[160] = 205;
    evToRgb[170] = 209;
    evToRgb[180] = 213;
    evToRgb[190] = 217;
    evToRgb[200] = 220;
    evToRgb[210] = 223;
    evToRgb[220] = 227;
    evToRgb[230] = 230;
    evToRgb[240] = 232;
    evToRgb[250] = 235;
    evToRgb[260] = 238;
    evToRgb[270] = 240;
    evToRgb[280] = 242;
    evToRgb[290] = 244;
    evToRgb[300] = 246;
    evToRgb[310] = 247;
    evToRgb[320] = 249;
    evToRgb[330] = 250;
    evToRgb[340] = 251;
    evToRgb[350] = 252;
    evToRgb[360] = 252;
    evToRgb[370] = 253;
    evToRgb[380] = 254;
    evToRgb[390] = 254;
    evToRgb[400] = 254;
    evToRgb[410] = 254;
    evToRgb[420] = 254;
    evToRgb[430] = 254;
    evToRgb[440] = 254;
    evToRgb[450] = 254;
    evToRgb[460] = 254;
    evToRgb[470] = 254;
    evToRgb[480] = 254;
    evToRgb[490] = 254;
    evToRgb[500] = 255;

}

/* DEFINES ***********************************************************************************/

#define MAX_3(a, b, c) ( ( a > b && a > c) ? a : (b > c ? b : c) )
#define MIN_3(a, b, c) ( ( a < b && a < c) ? a : (b < c ? b : c) )

/* PIXEL OPERATIONS **************************************************************************/

QRgb Effects::convertFromPremult(QRgb p)
{
    int alpha = qAlpha(p);
    return(!alpha ? 0 : qRgba(255 * qGreen(p) / alpha,
                              255 * qGreen(p) / alpha,
                              255 * qBlue(p)/alpha,
                              alpha));
}

QRgb Effects::convertToPremult(QRgb p)
{
    uint a = p >> 24;
    uint t = (p & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    p = ((p >> 8) & 0xff) * a;
    p = (p + ((p >> 8) & 0xff) + 0x80);
    p &= 0xff00;
    p |= t | (a << 24);
    return(p);
}

void Effects::averagePixel(QVector<QVector<QRgb>> &v, const int &x, const int &y,
                           const int &w, const int &h)
{
/*
Return the average of the pixels left, right, above and below the pixel at x,y
*/
    if (x < 0 || x > w - 1 || y < 0 || y > h - 1) {
        /*
        qDebug() << __FUNCTION__ << "WARNING: OUT OF RANGE"
                 << "x =" << x
                 << "y =" << y
                    ;
                    */
        return;
    }
    int x1, x2, y1, y2;
    int r, g, b, a;
    x - 1 < 0 ? x1 = 0 : x1 = x - 1;
    y - 1 < 0 ? y1 = 0 : y1 = y - 1;
    x + 1 > w - 1 ? x2 = w - 1 : x2 = x + 1;
    y + 1 > h - 1 ? y2 = h - 1 : y2 = y + 1;
    /*
    qDebug() << __FUNCTION__
             << "x =" << x
             << "y =" << y
             << "x1 =" << x1
             << "x2 =" << x2
             << "y1 =" << y1
             << "y2 =" << y2
             << "w =" << w
             << "h =" << h
                ;
//                */

    r = (qRed(v[y][x1]) + qRed(v[y][x2]) + qRed(v[y1][x]) + qRed(v[y2][x])) / 4;
    g = (qGreen(v[y][x1]) + qGreen(v[y][x2]) + qGreen(v[y1][x]) + qGreen(v[y2][x])) / 4;
    b = (qBlue(v[y][x1]) + qBlue(v[y][x2]) + qBlue(v[y1][x]) + qBlue(v[y2][x])) / 4;
    a = (qAlpha(v[y][x1]) + qAlpha(v[y][x2]) + qAlpha(v[y1][x]) + qAlpha(v[y2][x])) / 4;
    v[y][x] = qRgba(r, g, b, a);
}

void Effects::swapPoints(Point &p1, Point &p2)
{
    Point tmp;
    tmp.x = p1.x;
    tmp.y = p1.y;
    p1.x = p2.x;
    p1.y = p2.y;
    p2.x = tmp.x;
    p2.y = tmp.y;
}



int Effects::luminosity(QRgb &p)
{
//    int min = std::min({qRed(p), qGreen(p), qBlue(p)});
//    int max = std::max({qRed(p), qGreen(p), qBlue(p)});
    int min = MIN_3(qRed(p), qGreen(p), qBlue(p));
    int max = MAX_3(qRed(p), qGreen(p), qBlue(p));
    return (min + max) / 2;
}

void Effects::brightenPixel(QRgb &p, double deltaEv)
{
    int alpha = qAlpha(p);
    // QColor for pixel p
    QColor c(p);
    // convert to HSV
    c.toHsv();
    // get the luminance 0-255
    int lum = luminosity(p);  // or int lum = c.value();
    // get the exposure value rounded to nearest 10
    int ev = 10 * (qRound(rgbToEv[lum] * 1.0 / 10));
    // convert deltaEv ie 1.5 to 150 for lookup table evToRgb
    int deltaEv100 = qRound(deltaEv * 10);
    deltaEv100 *= 10;
    // new EV
    int newEv = ev + deltaEv100;
    if (newEv > 500) newEv = 500;
    if (newEv < -500) newEv = -500;
    // new luminance from new exposure value (0-255)
    int v = evToRgb[newEv];
    // plug new luminance into HSV
    int h = c.hue();
    int s = c.saturation();
    c.setHsv(h, s, v);
    // convert back to RGB
    c.toRgb();
    /*
    qDebug() << __FUNCTION__
             << "qRed(p) =" << qRed(p)
             << "qGreen(p) =" << qGreen(p)
             << "qBlue(p) =" << qBlue(p)
             << "lum =" << lum
             << "ev =" << ev
             << "deltaEv100 =" << deltaEv100
             << "newEv =" << newEv
             << "h =" << h
             << "s =" << s
             << "v =" << v
             << "c.red() =" << c.red()
             << "c.green() =" << c.green()
             << "c.blue() =" << c.blue()
                ;
//                */

    // update pixel p
    p = qRgba(c.red(), c.green(), c.blue(), alpha);
}

/* VECTOR OPERATIONS *************************************************************************/

void Effects::imageToVector(QImage &img, QVector<QVector<QRgb>> &v)
{
    for (int y = 0; y < img.height(); ++y) {
        v[y].resize(img.width());         // source = img
        memcpy(&v[y][0], img.scanLine(y), img.bytesPerLine());
    }
}

void Effects::vectorToImage(QImage &img, QVector<QVector<QRgb> > &v)
{
//    qDebug() << __FUNCTION__;
    for (int y = 0; y < img.height(); ++y) {
        memcpy(img.scanLine(y), &v[y][0], img.bytesPerLine());
    }
}

void Effects::zeroVector(QImage &img, QVector<QVector<QRgb>> &v)
{
    for (int y = 0; y < img.height(); ++y) {
//        qDebug() << __FUNCTION__ << "Zeroing row =" << y;
        memset(&v[y], 0, static_cast<uint>(img.width()) * sizeof(int));
    }
}

/* 2D EFFECTS ********************************************************************************/

#define ConvolveAccumulate(weight, pixel) \
    r += (weight * qRed(pixel)); \
    g += (weight * qGreen(pixel)); \
    b += (weight * qBlue(pixel));

QImage Effects::convolve(QImage &img, int mDim, double *matrix)
{
    // matrix descriptors
    int i, mX, mY;

    int x, y, w, h;
    int edge = mDim / 2;
    QRgb *dest, *src, *s, **scanblock;
    double *m, *normalizeMatrix, normalize;
    uint mLen = static_cast<uint>(mDim * mDim);
    bool overflow = false;

    if(!(mDim % 2)){
        qWarning("convolve(): kernel width must be an odd number!");
        return(img);
    }

    w = img.width();
    h = img.height();
    if (w < 3 || h < 3) {
        qWarning("convolve(): Image is too small!");
        return(img);
    }

    if(img.format() == QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32);
    else if(img.depth() < 32){
        img = img.convertToFormat(img.hasAlphaChannel() ?
                                  QImage::Format_ARGB32 :
                                  QImage::Format_RGB32);
    }
    QImage buffer(w, h, img.format());

    scanblock = new QRgb* [static_cast<uint>(mDim)];
    normalizeMatrix = new double[mLen];

    // create normalized matrix
    normalize = 0.0;
    for (i = 0; i < static_cast<int>(mLen); ++i) {
        normalize += matrix[i];
    }
    if (std::abs(normalize) <=  M_EPSILON) normalize = 1.0;
    normalize = 1.0 / normalize;
    for (i = 0; i < mDim*mDim; ++i) {
        normalizeMatrix[i] = normalize*matrix[i];
        if (normalizeMatrix[i] < 0.0) overflow = true;
    }

    // apply
    double r, g, b;
    for (y = 0; y < h; ++y) {
        src = reinterpret_cast<QRgb *>(img.scanLine(y));
        dest = reinterpret_cast<QRgb *>(buffer.scanLine(y));
        /* Read in scanlines to pixel neighborhood. If the scanline is outside
           the image use the top or bottom edge.  */
        for (x = y - edge, i = 0; x <= y + edge; ++i, ++x){
            scanblock[i] = reinterpret_cast<QRgb *>(img.scanLine((x < 0) ? 0 : (x > h - 1) ? h - 1 : x));
        }

        // Start processing scanlines.
        // Process the left part where the neighborhood extends off the right edge of the image
        for (x = 0; x - edge < 0 ; ++x){
            r = g = b = 0.0;
            m = normalizeMatrix;
            for (mY = 0; mY < mDim; ++mY) {
                s = scanblock[mY];
                mX = -edge;
                while(x + mX < 0) {
                    ConvolveAccumulate(*m, *s)
                    ++mX;
                    ++m;
                }
                while(mX <= edge){
                    ConvolveAccumulate(*m, *s)
                    ++mX;
                    ++m;
                    ++s;
                }
            }
            r = r < 0.0 ? 0.0 : r > 255.0 ? 255.0 : r + 0.5;
            g = g < 0.0 ? 0.0 : g > 255.0 ? 255.0 : g + 0.5;
            b = b < 0.0 ? 0.0 : b > 255.0 ? 255.0 : b + 0.5;
            *dest++ = qRgba(static_cast<unsigned char>(r),
                            static_cast<unsigned char>(g),
                            static_cast<unsigned char>(b),
                            qAlpha(*src++));
        }

        // Process the middle part where the entire neighborhood is on the image.
        for(; x + edge < w; ++x){
            m = normalizeMatrix;
            r = g = b = 0.0;
            for(mY = 0; mY < mDim; ++mY){
                s = scanblock[mY] + (x - edge);
                for (mX = -edge; mX <= edge; ++mX, ++m, ++s) {
                    ConvolveAccumulate(*m, *s)
                }
            }
            r = r < 0.0 ? 0.0 : r > 255.0 ? 255.0 : r+0.5;
            g = g < 0.0 ? 0.0 : g > 255.0 ? 255.0 : g+0.5;
            b = b < 0.0 ? 0.0 : b > 255.0 ? 255.0 : b+0.5;
            *dest++ = qRgba(static_cast<unsigned char>(r),
                            static_cast<unsigned char>(g),
                            static_cast<unsigned char>(b),
                            qAlpha(*src++));
        }

        // Process the right part where the neighborhood extends off the right edge of the image
        for(; x < w; ++x){
            r = g = b = 0.0;
            m = normalizeMatrix;
            for(mY = 0; mY < mDim; ++mY){
                s = scanblock[mY];
                s += x - edge;
                mX = -edge;
                while(x + mX < w){
                    ConvolveAccumulate(*m, *s)
                    ++mX;
                    ++m;
                    ++s;
                }
                --s;
                while(mX <= edge){
                    ConvolveAccumulate(*m, *s)
                    ++mX;
                    ++m;
                }
            }
            r = r < 0.0 ? 0.0 : r > 255.0 ? 255.0 : r + 0.5;
            g = g < 0.0 ? 0.0 : g > 255.0 ? 255.0 : g + 0.5;
            b = b < 0.0 ? 0.0 : b > 255.0 ? 255.0 : b + 0.5;
            *dest++ = qRgba(static_cast<unsigned char>(r),
                            static_cast<unsigned char>(g),
                            static_cast<unsigned char>(b),
                            qAlpha(*src++));
        }
    }

    delete[] scanblock;
    delete[] normalizeMatrix;
    return(buffer);
}

void Effects::blurLine(QVector<QVector<QRgb> > &q, Point &p1, Point &p2,
                       const int &w, const int &h, const int &width)
{
    if (p1.x > p2.x) swapPoints(p1, p2);

    bool isVertical = p1.x == p2.x;
    double m = 0;
    double b;
    if (isVertical) {
        if (p1.y > p2.y) swapPoints(p1, p2);
        b = p1.x;
    }
    else {
        m = static_cast<double>(p2.y - p1.y) / (p2.x - p1.x);
        b = p1.y - m * p1.x;
    }
    /*
    qDebug() << __FUNCTION__
             << "w =" << w
             << "h =" << h
             << "p1.x =" << p1.x
             << "p1.y =" << p1.y
             << "p2.x =" << p2.x
             << "p2.y =" << p2.y
             << "m =" << m
             << "b =" << b
                ;
//                */
    // line
    if (isVertical) {
        for (int y = p1.y; y <= p2.y; ++y) {
            int x = p1.x;
            for (int d = 0; d < width; ++d) {
                averagePixel(q, x, y, w, h);
                if (d > 0) {
                    averagePixel(q, x-d, y, w, h);
                    averagePixel(q, x+d, y, w, h);
                }
            }
        }
    }
    else {
        for (int x = p1.x; x <= p2.x; ++x) {
            int y = static_cast<int>(m * x + b);
            averagePixel(q, x, y, w, h);
            for (int d = 1; d < width; ++d) {
                if (qFuzzyCompare(m, 0)) {   // horizontal line
                    averagePixel(q, x, y-d, w, h);
                    averagePixel(q, x, y+d, w, h);
                }
                else if (m > 0) {
                    averagePixel(q, x+d, y, w, h);
                    averagePixel(q, x, y+d, w, h);
                }
                else {
                    averagePixel(q, x-d, y, w, h);
                    averagePixel(q, x, y-d, w, h);
                }
            }
        }
    }
}

void Effects::blur(QImage &img, int radius)
{
//    qDebug() << __FUNCTION__;
//    QElapsedTimer t;
//    t.start();

    int m = radius;
    int w, h;               // image width, height
    int a, r, g, b;
    int n;                  // sample size
    int *as, *rs, *gs, *bs;

    if(radius < 1 || img.isNull() || img.width() < (radius << 1))
        return;

    w = img.width();
    h = img.height();

    if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);

    // create QVector of img for pixel wrangling
    QVector<QVector<QRgb>> s(h);  // source, destination
    QVector<QVector<QRgb>> d(h);  // source, destination
    imageToVector(img, s);
    imageToVector(img, d);

    as = new int[w];
    rs = new int[w];
    gs = new int[w];
    bs = new int[w];

    int y1, y2;
    for (int y = 0; y < h; ++y) {
        y - m < 0 ? y1 = 0 : y1 = y - m;
        y + m > h ? y2 = h : y2 = y + m;
        memset(as, 0, w * sizeof(int));
        memset(rs, 0, w * sizeof(int));
        memset(gs, 0, w * sizeof(int));
        memset(bs, 0, w * sizeof(int));
        for (int x = 0; x < w; ++x) {
            for (int yy = y1; yy < y2; ++yy) {
                QRgb pixel = s[yy][x];
                as[x] += qAlpha(pixel);
                rs[x] += qRed(pixel);
                gs[x] += qGreen(pixel);
                bs[x] += qBlue(pixel);
                n++;
            }
        }
        for (int x = 0; x < w; ++x) {
            int x1, x2;
            x - m < 0 ? x1 = 0 : x1 = x - m;
            x + m > w ? x2 = w : x2 = x + m;
            a = 0; r = 0; g = 0; b = 0;

            for (int xx = x1; xx < x2; ++xx) {
                a += as[xx];
                r += rs[xx];
                g += gs[xx];
                b += bs[xx];
            }
            n = (y2 - y1) * (x2 - x1);
            d[y][x] = qRgba(r/n, g/n, b/n, a/n);
            }
    }
    vectorToImage(img, d);

    delete[] as;
    delete[] rs;
    delete[] gs;
    delete[] bs;

//    qDebug() << __FUNCTION__
//             << "Elapsed time =" << QString("%L1").arg(t.nsecsElapsed());
}

void Effects::blurOriginal(QImage &img, int radius)
{
    qDebug() << __FUNCTION__;
    QElapsedTimer t;
    t.start();
    QRgb *p1, *p2;
    int x, y, w, h, mx, my, mw, mh, mt, xx, yy;
    int a, r, g, b;
    int *as, *rs, *gs, *bs;

    if(radius < 1 || img.isNull() || img.width() < (radius << 1))
        return;

    w = img.width();
    h = img.height();

    if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);
    QImage buffer(w, h, img.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);

    as = new int[w];
    rs = new int[w];
    gs = new int[w];
    bs = new int[w];

    QVector<QRgb> colorTable;
    if(img.format() == QImage::Format_Indexed8)
        colorTable = img.colorTable();

    for(y = 0; y < h; y++){
        my = y - radius;
        mh = (radius << 1) + 1;
        if(my < 0){
            mh += my;
            my = 0;
        }
        if((my + mh) > h)
            mh = h - my;

        p1 = (QRgb *)buffer.scanLine(y);
        memset(as, 0, w * sizeof(int));
        memset(rs, 0, w * sizeof(int));
        memset(gs, 0, w * sizeof(int));
        memset(bs, 0, w * sizeof(int));

        if(img.format() == QImage::Format_ARGB32_Premultiplied){
            QRgb pixel;
            for(yy = 0; yy < mh; yy++){
                p2 = (QRgb *)img.scanLine(yy + my);
                for(x = 0; x < w; x++, p2++){
                    pixel = convertFromPremult(*p2);
                    as[x] += qAlpha(pixel);
                    rs[x] += qGreen(pixel);
                    gs[x] += qGreen(pixel);
                    bs[x] += qBlue(pixel);
                }
            }
        }
        else if(img.format() == QImage::Format_Indexed8){
            QRgb pixel;
            unsigned char *ptr;
            for(yy = 0; yy < mh; yy++){
                ptr = (unsigned char *)img.scanLine(yy + my);
                for(x = 0; x < w; x++, ptr++){
                    pixel = colorTable[*ptr];
                    as[x] += qAlpha(pixel);
                    rs[x] += qGreen(pixel);
                    gs[x] += qGreen(pixel);
                    bs[x] += qBlue(pixel);
                }
            }
        }
        else{
            for(yy = 0; yy < mh; yy++){
                p2 = (QRgb *)img.scanLine(yy + my);
                for(x = 0; x < w; x++, p2++){
                    as[x] += qAlpha(*p2);
                    rs[x] += qGreen(*p2);
                    gs[x] += qGreen(*p2);
                    bs[x] += qBlue(*p2);
                }
            }
        }

        for(x = 0; x < w; x++){
            a = r = g = b = 0;
            mx = x - radius;
            mw = (radius << 1) + 1;
            if(mx < 0){
                mw += mx;
                mx = 0;
                }
            if((mx + mw) > w)
                mw = w - mx;
            mt = mw * mh;
            for(xx = mx; xx < (mw + mx); xx++){
                a += as[xx];
                r += rs[xx];
                g += gs[xx];
                b += bs[xx];
            }
            a = a / mt;
            r = r / mt;
            g = g / mt;
            b = b / mt;
            *p1++ = qRgba(r, g, b, a);
        }
    }
    delete[] as;
    delete[] rs;
    delete[] gs;
    delete[] bs;

    img = buffer;
    qDebug() << __FUNCTION__
             << "Elapsed time =" << QString("%L1").arg(t.nsecsElapsed());
}

void Effects::raisePixel(QVector<QVector<QRgb> > &s, const int &ground, const int &adj,
                         const int &x, const int &y, const double &blend)
/*
Not being used - see #define CrunchRaisedPixel which is faster than calling function
*/
{
    int range = 255;
    double scale = 1.0 / range;
    int r, g, b, a;

    int z2 = ground * (range - adj);
    /* red */
    r = qRed(s[y][x]);
    int z1 = r * adj;
    int rnew = static_cast<int>(scale * (z1 + z2));
    int rDelta = static_cast<int>(blend * (rnew - r));
    r += rDelta;
    /* green */
    g = qGreen(s[y][x]);
    z1 = g * adj;
    int gnew = static_cast<int>(scale * (z1 + z2));
    int gDelta = static_cast<int>(blend * (gnew - g));
    g += gDelta;
    /* blue */
    b = static_cast<int>(blend * scale * (z1 + z2));
    b = qBlue(s[y][x]);
    z1 = b * adj;
    int bnew = static_cast<int>(scale * (z1 + z2));
    int bDelta = static_cast<int>(blend * (bnew - b));
    b += bDelta;
    /* result */
    s[y][x] = qRgba(r, g, b, a);
}

void Effects::raise(QImage &img, int m, double taper, int blurWidth, bool raise)
{
#define CrunchRaisedPixel \
    int z2 = ground * (range - adj); \
    /* red */ \
    r = qRed(s[y][x]); \
    int z1 = r * adj; \
    int rnew = static_cast<int>(scale * (z1 + z2)); \
    int rDelta = static_cast<int>(blend * (rnew - r)); \
    r += rDelta; \
    /* green */ \
    g = qGreen(s[y][x]); \
    z1 = g * adj; \
    int gnew = static_cast<int>(scale * (z1 + z2)); \
    int gDelta = static_cast<int>(blend * (gnew - g)); \
    g += gDelta; \
    /* blue */ \
    b = static_cast<int>(blend * scale * (z1 + z2)); \
    b = qBlue(s[y][x]); \
    z1 = b * adj; \
    int bnew = static_cast<int>(scale * (z1 + z2)); \
    int bDelta = static_cast<int>(blend * (bnew - b)); \
    b += bDelta; \
    /* result */ \
    s[y][x] = qRgba(r, g, b, a);

#define AntialiasPixel \
    /* pixel on line requires definition of x, y */ \
    int u = x; \
    int v = y; \
    r = (qRed(s[v][u-1]) + qRed(s[v][u+1]) + qRed(s[v-1][u]) + qRed(s[v+1][u])) / 4; \
    g = (qGreen(s[v][u-1]) + qGreen(s[v][u+1]) + qGreen(s[v-1][u]) + qGreen(s[v+1][u])) / 4; \
    b = (qBlue(s[v][u-1]) + qBlue(s[v][u+1]) + qBlue(s[v-1][u]) + qBlue(s[v+1][u])) / 4; \
    s[v][u] = qRgba(r, g, b, a); \
    /* antialias right pixel */ \
    u++; \
    r = (qRed(s[v][u-1]) + qRed(s[v][u+1]) + qRed(s[v-1][u]) + qRed(s[v+1][u])) / 4; \
    g = (qGreen(s[v][u-1]) + qGreen(s[v][u+1]) + qGreen(s[v-1][u]) + qGreen(s[v+1][u])) / 4; \
    b = (qBlue(s[v][u-1]) + qBlue(s[v][u+1]) + qBlue(s[v-1][u]) + qBlue(s[v+1][u])) / 4; \
    s[v][u] = qRgba(r, g, b, a); \
    /* antialias left pixel  */ \
    u--; \
    u--; \
    r = (qRed(s[v][u-1]) + qRed(s[v][u+1]) + qRed(s[v-1][u]) + qRed(s[v+1][u])) / 4; \
    g = (qGreen(s[v][u-1]) + qGreen(s[v][u+1]) + qGreen(s[v-1][u]) + qGreen(s[v+1][u])) / 4; \
    b = (qBlue(s[v][u-1]) + qBlue(s[v][u+1]) + qBlue(s[v-1][u]) + qBlue(s[v+1][u])) / 4; \
    s[v][u] = qRgba(r, g, b, a); \


//    qDebug() << __FUNCTION__ << img.format();
//    QElapsedTimer t;
//    t.start();

    int fg, bg, ground;     // foreground, background, ground to use
    int a = 0, r, g, b;
    int w = img.width();
    int h = img.height();
    int accentuate = 135;
    int highlight = 190;
    int shadow = 190;
    int trough = 135;
    int adj = 0;
    int range = 255;
    double scale = 1.0 / range;
    int transition = static_cast<int>(m * (1 - taper));
    double blendIncrement = 1.0 / (m - transition);
    /*
    qDebug() << __FUNCTION__
             << "m" << m
             << "taper" << taper
             << "transition" << transition
             << "blendIncrement" << blendIncrement;
//             */

    if (w <= (m << 1)) {
        qDebug() << __FUNCTION__ << "return";
        return;
    }

    if (raise) {
        fg = 255;
        bg = 0.0;
    }
    else {
        fg = 0.0;
        bg = 255;
    }

    // create QVector of img for pixel wrangling
    QVector<QVector<QRgb>> s(h);
    imageToVector(img, s);
    /*
    for (int y = 0; y < h; ++y) {
        s[y].resize(w);         // source = img
        memcpy(&s[y][0], img.scanLine(y), static_cast<size_t>(bpl));
    }
//    */

    double blend;
    // left margin top corner
    adj = highlight;
    ground = fg;
    for (int y = 0; y < m; y++) {
        blend = 1;
        for (int x = 0; x < y; x++) {
            if (x > transition) blend -= blendIncrement;
            if (blendIncrement < 0) blendIncrement = 0;
            CrunchRaisedPixel
        }
    }

    // top margin
    blend = 1;
    adj = accentuate;   // brightness adjustment
    ground = fg;
    for (int y = 0; y < m; y++) {
        if (y > transition) blend -= blendIncrement;
        for (int x = y ; x < w - y; x++) {
            CrunchRaisedPixel
        }
    }

    // right margin top corner
    adj = shadow;
    ground = bg;
    for (int y = 0; y < m; y++) {
        blend = 0;
        for (int x = w - y; x < w; x++) {
            if (x < (w - transition)) blend += blendIncrement;
            else blend = 1;
            CrunchRaisedPixel
        }
    }

    // left margin
    adj = highlight;
    ground = fg;
    for (int y = m; y < (h - m); y++) {
        blend = 1;
        for (int x = 0; x < m; x++) {
            if (x > transition)  blend -= blendIncrement;
            CrunchRaisedPixel
        }
    }

    // right margin
    adj = shadow;
    ground = bg;
    for (int y = m; y < (h - m); y++) {
        blend = 0;
        for (int x = (w - m); x < w; x++) {
            if (x < (w - transition)) blend += blendIncrement;
            else blend = 1;
            /*
            if (y == m)
            qDebug() << __FUNCTION__
                     << "y =" << y
                     << "x =" << x
                     << "w =" << w
                     << "m =" << m
                     << "transition =" << transition
                     << "blendIncrement =" << blendIncrement
                     << "blend =" << blend
                        ;
                        */
            CrunchRaisedPixel
        }
    }

    // left margin bottom corner
    adj = highlight;
    ground = fg;
    for (int y = (h - m); y < h; y++) {
        blend = 1;
        for (int x = 0; x < h - y; x++) {
            if (x > transition) blend -= blendIncrement;
            CrunchRaisedPixel
        }
    }

    // bottom margin
    blend = 0;
    adj = trough;
    ground = bg;
    for (int y = (h - m); y < h; y++) {
        if (y < (h - transition)) blend += blendIncrement;
        else blend = 1;
        for (int x = h - y; x < (w + y - h); x++) {
            /*
            if (y == h - m)
            qDebug() << __FUNCTION__
                     << "y =" << y
                     << "x =" << x
                     << "w =" << w
                     << "m =" << m
                     << "transition =" << transition
                     << "blendIncrement =" << blendIncrement
                     << "blend =" << blend
                        ;
//                        */
            CrunchRaisedPixel
        }
    }

    // right margin bottom corner
    adj = shadow;
    ground = bg;
    for (int y = h - m ; y < h; y++) {
        blend = 0;
        for (int x = (w + y - h); x < w; x++) {
            if (x < (w - transition)) blend += blendIncrement;
            else blend = 1;
            /*
            qDebug() << __FUNCTION__
                     << "y =" << y
                     << "x =" << x
                     << "h =" << h
                     << "w - transition =" << w - transition
                     << "blend =" << blend
                        ;
                        */
            CrunchRaisedPixel
        }
    }

    Point p1, p2;
    // blur top left corner
    p1.x = 0; p1.y = 0; p2.x = m; p2.y = m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // blur top right corner
    p1.x = w - 1; p1.y = 0; p2.x = w - m; p2.y = m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // blur bottom left corner
    p1.x = 0; p1.y = h - 1; p2.x = m; p2.y = h - m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // blur bottom right corner
    p1.x = w - 1; p1.y = h - 1; p2.x = w - m; p2.y = h - m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // blur top
    p1.x = m; p1.y = m; p2.x = w - m; p2.y = m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // blur left
    p1.x = m; p1.y = m; p2.x = m; p2.y = h - m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // blur right
    p1.x = w - m; p1.y = m; p2.x = w - m; p2.y = h - m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // blur bottom
    p1.x = m; p1.y = h - m; p2.x = w - m; p2.y = h - m;
    blurLine(s, p1, p2, w, h, blurWidth);

    // build destination image
    vectorToImage(img, s);

//    qDebug() << __FUNCTION__
//             << "Elapsed time =" << QString("%L1").arg(t.nsecsElapsed());

#undef CrunchRaisedPixel
#undef AntialiasPixel
}

void Effects::brighten(QImage &img, qreal evDelta)
{
    qDebug() << __FUNCTION__ << "delta =" << evDelta;
    QElapsedTimer t;
    t.start();
/*
    if (evdelta == 100) return;
    if (evdelta > 400) return;
    if (evdelta < 10) return;
    */

    // bDelta               // change in brightness
    int w = img.width();
    int h = img.height();

    if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);

//    int a, r, g, b;

    // create QVector of img for pixel wrangling
    QVector<QVector<QRgb>> s(h);  // source, destination
    imageToVector(img, s);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            brightenPixel(s[y][x], evDelta);
        }
    }

    /*
    // lighten
    if (delta > 100) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                s[y][x] = QColor(s[y][x]).lighter(delta).rgba();
            }
        }
    }
    // darken
    else {
        delta = 10000 / delta;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                s[y][x] = QColor(s[y][x]).darker(delta).rgba();
            }
        }
    }
    */
    vectorToImage(img, s);

    qDebug() << __FUNCTION__
             << "Elapsed time =" << QString("%L1").arg(t.nsecsElapsed());
}

/* 3D OPERATIONS *****************************************************************************/

#define DotProduct(a, b) \
    (a[0] * b[0] + a[1] * b[1] + a[2] * b[2])

#define CrossProduct(a, b, n) \
    n[0] = a[1] * b[2] - a[2] * b[1]; \
    n[1] = a[2] * b[0] - a[0] * b[2]; \
    n[2] = a[0] * b[1] - a[2] * b[1];

#define Normalize(p) \
    double m = sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]); \
    p[0] /= m; \
    p[1] /= m; \
    p[2] /= m;


void Effects::crossProduct(Vector &a, Vector &b, Vector &n)
{
    /* The vector n is normal to the place defined by vectos a and b
       0 = x, 1 = y, 2 = z
    */
    n[0] = a[1] * b[2] - a[2] * b[1];
    n[1] = a[2] * b[0] - a[0] * b[2];
    n[2] = a[0] * b[1] - a[2] * b[1];
}

void Effects::normalize(Vector &p)
{
    double m = sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
    p[0] /= m;
    p[1] /= m;
    p[2] /= m;
}

void Effects::lightVector(int azimuth, Vector &light)
{
    double rads = azimuth * 0.017453;     // 3.14159 / 180 = 0.017453
    light[0] = -sin(rads);
    light[1] = cos(rads);
    light[2] = -.707;                    // assume light is 45 degrees in z axis
}

void Effects::slopeUnitVector(Vector &a, Vector &b, int angle)
{
    double rads = angle * 0.017453;      // 3.14159 / 180 = 0.017453
    if (a[0] == 0) {
        b[1] = 0;
        b[0] = cos(rads);
    }
    else {
        b[0] = 0;
        b[1] = cos(rads);
    }
    b[2] = -sin(rads);
    qDebug().noquote() << __FUNCTION__
             << "b[0] =" << QString::number(b[0], 'f', 3).rightJustified(6)
             << "b[1] =" << QString::number(b[1], 'f', 3).rightJustified(6)
             << "b[2] =" << QString::number(b[2], 'f', 3).rightJustified(6);
    Normalize(b);
}

void Effects::test(QImage &img)
{
    qDebug() << __FUNCTION__;

    /*
    // vectors a and b define the plane
    Vector a = {1.0, 0.0, 0.0};
//    Vector b = {0.0, 0.0, -0.707};
    Vector b;
    slopeUnitVector(a, b, 45);
    // normal vector n
    Vector n;
    CrossProduct(a, b, n)
    Normalize(n);

    // light vector l
    Vector l;
    lightVector(315, l);
    for (int azimuth = 0; azimuth < 360; azimuth += 15) {
        lightVector(azimuth, l);
        qDebug().noquote() << __FUNCTION__
                 << "Azimuth =" << QString::number(azimuth).leftJustified(4) << "   "
                 << "n[0] =" << QString::number(n[0], 'f', 3).rightJustified(6)
                 << "n[1] =" << QString::number(n[1], 'f', 3).rightJustified(6)
                 << "n[2] =" << QString::number(n[2], 'f', 3).rightJustified(6) << "   "
                 << "l[0] =" << QString::number(l[0], 'f', 3).rightJustified(6)
                 << "l[1] =" << QString::number(l[1], 'f', 3).rightJustified(6)
                 << "l[2] =" << QString::number(l[2], 'f', 3).rightJustified(6) << "   "
                 << "Illumination =" << QString::number(DotProduct(n, l), 'f', 3).rightJustified(6)
                 ;
    }
    */

    Vector n;
    Vector2D lookup[101] = {
        {-6.1257422745431E-17, -1},
        {0.0999999999999999, -0.99498743710662},
        {0.23970742216082, -0.970845173939189},
        {0.310748164766664, -0.950492281975057},
        {0.366682783520693, -0.930346030393808},
        {0.414024909898246, -0.91026555135507},
        {0.455541561613055, -0.890214516643679},
        {0.492735342987967, -0.870179223936385},
        {0.526535184748707, -0.850153338652296},
        {0.557564642663404, -0.830133524953444},
        {0.586267051995121, -0.810117857934851},
        {0.612971332254572, -0.790105148593562},
        {0.637929676489445, -0.770094622662743},
        {0.661340578933826, -0.750085754201126},
        {0.683363637278313, -0.730078173379916},
        {0.704129466134566, -0.710071612530068},
        {0.723746565277693, -0.690065873122808},
        {0.742306215719597, -0.670060804781217},
        {0.759886055853644, -0.650056291500354},
        {0.776552748960547, -0.630052242343298},
        {0.792364009742321, -0.610048585003744},
        {0.807370168925421, -0.590045261254879},
        {0.821615398610478, -0.570042223669567},
        {0.835138684236081, -0.550039433215863},
        {0.847974604411431, -0.530036857466797},
        {0.860153963065996, -0.510034469248757},
        {0.871704306668523, -0.490032245607929},
        {0.882650350988381, -0.47003016701068},
        {0.893014335919264, -0.450028216718325},
        {0.902816322543995, -0.430026380293276},
        {0.912074443411325, -0.41002464520553},
        {0.920805114597179, -0.390023000516221},
        {0.929023216308789, -0.370021436621276},
        {0.936742247404689, -0.350019945042012},
        {0.943974458134391, -0.330018518252964},
        {0.950730964569642, -0.310017149539309},
        {0.957021847545883, -0.290015832877905},
        {0.962856238415484, -0.270014562837608},
        {0.968242393502124, -0.250013334494939},
        {0.9731877588142, -0.230012143362462},
        {0.977699026307051, -0.210010985327542},
        {0.981782182765135, -0.190009856599405},
        {0.98544255219561, -0.170008753663457},
        {0.988684832476354, -0.150007673241086},
        {0.991513126877369, -0.130006612254389},
        {0.993930970970399, -0.110005567794724},
        {0.995941355353021, -0.0900045370944612},
        {0.997546744537541, -0.0700035175012976},
        {0.99874909228907, -0.0500025064547549},
        {0.999549853639078, -0.0300015014640601},
        {0.999949993748687, -0.01000050008752},
        {-0.999949993748687, -0.01000050008752},
        {-0.999549853639078, -0.0300015014640656},
        {-0.998749092289071, -0.0500025064547489},
        {-0.99754674453754, -0.070003517501303},
        {-0.995941355353021, -0.0900045370944611},
        {-0.993930970970399, -0.110005567794724},
        {-0.991513126877369, -0.130006612254389},
        {-0.988684832476354, -0.150007673241085},
        {-0.985442552195609, -0.170008753663462},
        {-0.981782182765134, -0.190009856599411},
        {-0.977699026307052, -0.210010985327537},
        {-0.973187758814199, -0.230012143362467},
        {-0.968242393502124, -0.250013334494939},
        {-0.962856238415484, -0.270014562837607},
        {-0.957021847545881, -0.290015832877909},
        {-0.950730964569644, -0.310017149539304},
        {-0.94397445813439, -0.330018518252969},
        {-0.936742247404689, -0.350019945042012},
        {-0.929023216308789, -0.370021436621276},
        {-0.920805114597177, -0.390023000516226},
        {-0.912074443411325, -0.41002464520553},
        {-0.902816322543993, -0.430026380293279},
        {-0.893014335919264, -0.450028216718324},
        {-0.882650350988379, -0.470030167010684},
        {-0.871704306668523, -0.490032245607929},
        {-0.860153963065996, -0.510034469248757},
        {-0.847974604411431, -0.530036857466797},
        {-0.835138684236081, -0.550039433215863},
        {-0.821615398610478, -0.570042223669567},
        {-0.807370168925418, -0.590045261254882},
        {-0.792364009742319, -0.610048585003747},
        {-0.776552748960547, -0.630052242343298},
        {-0.759886055853641, -0.650056291500356},
        {-0.742306215719595, -0.670060804781219},
        {-0.723746565277693, -0.690065873122808},
        {-0.704129466134565, -0.710071612530069},
        {-0.683363637278311, -0.730078173379918},
        {-0.661340578933825, -0.750085754201127},
        {-0.637929676489443, -0.770094622662744},
        {-0.612971332254571, -0.790105148593563},
        {-0.586267051995119, -0.810117857934852},
        {-0.557564642663403, -0.830133524953445},
        {-0.526535184748706, -0.850153338652296},
        {-0.492735342987965, -0.870179223936387},
        {-0.455541561613052, -0.89021451664368},
        {-0.414024909898243, -0.910265551355071},
        {-0.366682783520691, -0.930346030393809},
        {-0.31074816476666, -0.950492281975058},
        {-0.239707422160815, -0.97084517393919},
        {-0.100000000000002, -0.99498743710662}
    };

    // light vector l
    int azimuth = 0;
    Vector l;
    lightVector(azimuth, l);

    int w = img.width();
    int h = img.height();

    if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);

    int a, r, g, b;

    // create QVector of img for pixel wrangling
    QVector<QVector<QRgb>> s(h);  // source, destination
    imageToVector(img, s);

    // shade
    for (int y = 0; y < h; ++y) {
        int i = static_cast<double>(y) / h * 101;
        n[0] = 0;
        n[1] = lookup[i][0];
        n[2] = lookup[i][1];
        double illum = DotProduct(n, l);
        int illumination = illum * 100;
        illumination += (illumination - 100) * 2;
        for (int x = 0; x < w; ++x) {
            s[y][x] = QColor(s[y][x]).lighter(illumination).rgba();
        }
        qDebug().noquote() << __FUNCTION__
                 << "i =" << i
                 << "Azimuth =" << QString::number(azimuth).leftJustified(4) << "   "
                 << "n[0] =" << QString::number(n[0], 'f', 3).rightJustified(6)
                 << "n[1] =" << QString::number(n[1], 'f', 3).rightJustified(6)
                 << "n[2] =" << QString::number(n[2], 'f', 3).rightJustified(6) << "   "
                 << "l[0] =" << QString::number(l[0], 'f', 3).rightJustified(6)
                 << "l[1] =" << QString::number(l[1], 'f', 3).rightJustified(6)
                 << "l[2] =" << QString::number(l[2], 'f', 3).rightJustified(6) << "   "
                 << "Illumination =" << QString::number(illum, 'f', 3).rightJustified(6)
                 << "Illumination =" << QString::number(illumination).rightJustified(6)
                 ;
    }
    vectorToImage(img, s);

}
