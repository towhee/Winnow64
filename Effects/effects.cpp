#include "effects.h"

#define M_EPSILON 1.0e-6
#define M_SQ2PI 2.50662827463100024161235523934010416269302368164062

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
    unsigned int a = p >> 24;
    unsigned int t = (p & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    p = ((p >> 8) & 0xff) * a;
    p = (p + ((p >> 8) & 0xff) + 0x80);
    p &= 0xff00;
    p |= t | (a << 24);
    return(p);
}

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

QImage Effects::convolve(QImage &img, int mDim, double *matrix)
{
#define ConvolveAccumulate(weight, pixel) \
    r+=((weight))*(qRed((pixel))); \
    g+=((weight))*(qGreen((pixel))); \
    b+=((weight))*(qBlue((pixel)));

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
    for (i = 0; i < static_cast<int>(mLen); ++i)
        normalize += matrix[i];
    if (std::abs(normalize) <=  M_EPSILON) normalize = 1.0;
    normalize = 1.0 / normalize;
    for (i = 0; i < mDim*mDim; ++i){
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

void Effects::averagePixel(QVector<QVector<QRgb>> &v, const int &x, const int &y,
                           const int &w, const int &h)
{
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


    qDebug() << __FUNCTION__ << img.format();
    QElapsedTimer t;
    t.start();

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

    qDebug() << __FUNCTION__
             << "Elapsed time =" << QString("%L1").arg(t.nsecsElapsed());

#undef CrunchRaisedPixel
#undef AntialiasPixel
}

void Effects::brighten(QImage &img, int delta)
{
    qDebug() << __FUNCTION__ << "delta =" << delta;
    QElapsedTimer t;
    t.start();

    if (delta == 100) return;
    if (delta > 400) return;
    if (delta < 10) return;

    // bDelta               // change in brightness
    int w = img.width();
    int h = img.height();

    if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);

    int a, r, g, b;

    // create QVector of img for pixel wrangling
    QVector<QVector<QRgb>> s(h);  // source, destination
    imageToVector(img, s);

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
        {0.0707106781186546, -0.997496867163},
        {0.170105923113651, -0.985425783568531},
        {0.221107742275936, -0.975249386724051},
        {0.261600245006004, -0.965176311257585},
        {0.296167158862675, -0.955136123288828},
        {0.326750428788845, -0.945110658751824},
        {0.354402252998618, -0.935093066528409},
        {0.379771854842118, -0.925080179373538},
        {0.403294299765187, -0.915070329415673},
        {0.425278465115475, -0.905062554251929},
        {0.445953240206091, -0.895056259432718},
        {0.465493958013857, -0.885051057878919},
        {0.484038527532704, -0.875046686676758},
        {0.501697793172207, -0.865042960971383},
        {0.518562465924874, -0.855039746990696},
        {0.534707920905983, -0.845036945535757},
        {0.550197614251566, -0.835034481486774},
        {0.565085576749102, -0.82503229691336},
        {0.579418272416389, -0.815030346422762},
        {0.593236009474094, -0.80502859394139},
        {0.606574029011821, -0.795027010439499},
        {0.619463357147401, -0.785025572291548},
        {0.631931480697382, -0.775024260074234},
        {0.644002889147744, -0.765023057671701},
        {0.655699513955934, -0.755021951600053},
        {0.667041088037625, -0.745020930490937},
        {0.678045442506019, -0.735019984692129},
        {0.688728753574125, -0.725019105955307},
        {0.699105749500316, -0.715018287189636},
        {0.709189885220236, -0.705017522265446},
        {0.71899349063599, -0.695016805856573},
        {0.728527897270377, -0.685016133312787},
        {0.737803547029672, -0.67501550055568},
        {0.746830086074954, -0.665014903993344},
        {0.755616446224674, -0.65501434044973},
        {0.764170915857467, -0.645013807106143},
        {0.772501201927119, -0.635013301452148},
        {0.780614484415939, -0.625012821244522},
        {0.788517464325281, -0.615012364472479},
        {0.796216406117405, -0.605011929328244},
        {0.803717175373873, -0.595011514181905},
        {0.811025272313462, -0.585011117560063},
        {0.818145861713133, -0.575010738127277},
        {0.82508379969247, -0.565010374670268},
        {0.831843657754317, -0.555010026084141},
        {0.838429744417042, -0.545009691360415},
        {0.844846124726348, -0.535009369576713},
        {0.851096637894882, -0.525009059887568},
        {0.857184913283687, -0.515008761516577},
        {0.863114384911623, -0.505008473749333},
        {0.868888304654201, -0.495008195927248},
        {0.874509754272774, -0.485007927442194},
        {0.879981656397582, -0.475007667731551},
        {0.885306784572509, -0.465007416274068},
        {0.890487772456979, -0.455007172585892},
        {0.895527122268489, -0.445006936217312},
        {0.90042721254019, -0.435006706749567},
        {0.905190305258799, -0.425006483792285},
        {0.90981855244141, -0.415006266980893},
        {0.91431400220275, -0.405006055974463},
        {0.91867860435915, -0.395005850453793},
        {0.922914215610575, -0.385005650119472},
        {0.927022604337358, -0.375005454690439},
        {0.931005455045033, -0.365005263902306},
        {0.934864372486614, -0.355005077506237},
        {0.938600885489373, -0.345004895267539},
        {0.942216450509855, -0.335004716964717},
        {0.945712454939102, -0.325004542388345},
        {0.949090220177483, -0.315004371340235},
        {0.952351004497025, -0.305004203632518},
        {0.955496005707204, -0.295004039086888},
        {0.958526363638789, -0.285003877533974},
        {0.961443162459061, -0.275003718812528},
        {0.964247432830259, -0.265003562768983},
        {0.966940153922374, -0.255003409256771},
        {0.969522255290094, -0.245003258135906},
        {0.97199461862312, -0.235003109272402},
        {0.974358079377971, -0.225002962537988},
        {0.976613428299028, -0.215002817809487},
        {0.978761412835476, -0.205002674968652},
        {0.980802738460668, -0.195002533901624},
        {0.98273806989947, -0.185002394498731},
        {0.984568032268956, -0.175002256654127},
        {0.986293212137191, -0.165002120265473},
        {0.987914158504481, -0.155001985233684},
        {0.989431383711063, -0.1450018514627},
        {0.990845364274896, -0.135001718859238},
        {0.99215654166284, -0.125001587332457},
        {0.99336532299818, -0.115001456793907},
        {0.994472081707293, -0.105001327157156},
        {0.995477158107806, -0.0950011983377404},
        {0.99638085994055, -0.0850010702528495},
        {0.997183462847201, -0.075000942821168},
        {0.997885210795393, -0.0650008159628296},
        {0.998486316452876, -0.0550006895990195},
        {0.998986961512012, -0.0450005636520003},
        {0.999387296965831, -0.0350004380448284},
        {0.999687443336585, -0.0250003127013018},
        {0.999887490857643, -0.0150001875457154},
        {-6.1257422745431E-17, -1}
    };

//    for (int i = 0; i < 102; ++i) {
//        qDebug().noquote() << __FUNCTION__
//                           << i
//                           << "lookup[i][0] =" << lookup[i][0]
//                           << "lookup[i][1] =" << lookup[i][1]
//                           ;
//    }
//    return;
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
        qDebug().noquote() << __FUNCTION__
                 << "i =" << i
                 << "Azimuth =" << QString::number(azimuth).leftJustified(4) << "   "
                 << "n[0] =" << QString::number(n[0], 'f', 3).rightJustified(6)
                 << "n[1] =" << QString::number(n[1], 'f', 3).rightJustified(6)
                 << "n[2] =" << QString::number(n[2], 'f', 3).rightJustified(6) << "   "
                 << "l[0] =" << QString::number(l[0], 'f', 3).rightJustified(6)
                 << "l[1] =" << QString::number(l[1], 'f', 3).rightJustified(6)
                 << "l[2] =" << QString::number(l[2], 'f', 3).rightJustified(6) << "   "
                 << "Illumination =" << QString::number(DotProduct(n, l), 'f', 3).rightJustified(6)
                 ;
        double illumination = DotProduct(n, l) * 100;
        for (int x = 0; x < w; ++x) {
            s[y][x] = QColor(s[y][x]).lighter(illumination).rgba();
        }
    }
    vectorToImage(img, s);

}
