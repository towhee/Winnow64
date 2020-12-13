#include "effects.h"

#define M_EPSILON 1.0e-6
#define M_SQ2PI 2.50662827463100024161235523934010416269302368164062

Effects::Effects()
{
}

/* DEFINES ***********************************************************************************/

#define MAX_3(a, b, c) ( ( a > b && a > c) ? a : (b > c ? b : c) )
#define MIN_3(a, b, c) ( ( a < b && a < c) ? a : (b < c ? b : c) )

/* 2D OPERATIONS *****************************************************************************/

int Effects::ptToSegment(int x, int y, int x1, int y1, int x2, int y2)
{
    int a = x - x1;
    int b = y - y1;
    int c = x2 - x1;
    int d = y2 - y1;

    int dot = a * c + b * d;
    int len_sq = c * c + d * d;
    double param = -1;
    if (len_sq != 0) //in case of 0 length line
        param = static_cast<double>(dot) / len_sq;

    int xx, yy;

    if (param < 0) {
      xx = x1;
      yy = y1;
    }
    else if (param > 1) {
      xx = x2;
      yy = y2;
    }
    else {
      xx = x1 + param * c;
      yy = y1 + param * d;
    }

    int dx = x - xx;
    int dy = y - yy;
    double minDistance = qSqrt(dx * dx + dy * dy);
    return static_cast<int>(minDistance);
}

void Effects::surrounding4Px(QList<Pt> &sPts, Pt &p, const int &w, const int &h)
{
    // sample point
    Pt sp;
    if (p.x > 0) {
        sp.x = p.x - 1;
        sp.y = p.y;
        sPts << sp;
    }
    if (p.x < w) {
        sp.x = p.x + 1;
        sp.y = p.y;
        sPts << sp;
    }
    if (p.y > 0) {
        sp.x = p.x - 1;
        sp.y = p.y - 1;
        sPts << sp;
    }
    if (p.y < h) {
        sp.x = p.x + 1;
        sp.y = p.y + 1;
        sPts << sp;
    }
}

void Effects::surrounding8Px(QList<Pt> &sPts, Pt &p, const int &w, const int &h)
{
    Pt sp;
    for (int x = p.x-1; x < p.x+2; x++) {
        if (x < 0) x = 0;
        if (x >= w) continue;
        for (int y = p.y-1; y < p.y+2; y++) {
            if (y < 0) y = 0;
            if (y >= h) continue;
            if (x == p.x && y == p.y) continue;
            sp.x = x;
            sp.y = y;
            sPts << sp;
        }
    }
}

void Effects::transparentEdgeMap(QImage &img, int depth,
                                 QVector<QVector<QRgb>> &s,
                                 QMultiMap<int, Pt> &edge)
{
    int w = img.width();
    int h = img.height();
    int x, y;

    // create QVector edge map (em) used to tally pixels added to QMultiMap edge
    QVector<QVector<int>> em(h);
    for (y = 0; y < h; y++) {
        em[y].resize(w);
        // start with zeros, change to d (distance from edge)
        em[y].fill(0);
    }

    bool prevTransparent;
    Pt pt;

    // scan left to right
    for (y = 0; y < h; y++) {
        prevTransparent = true;
        for (x = 0; x < w; x++) {
            pt.x= x;
            pt.y= y;
            // opaque to transparent edge
            if (qAlpha(s[y][x]) == 0 && !prevTransparent)  {
                pt.x= x-1;
                // do not need to check if already tallied on first pass
                edge.insertMulti(1, pt);
                em[y][x-1] = 1;
                prevTransparent = true;
            }
            // transparent to opaque edge
            if (qAlpha(s[y][x]) > 0 && prevTransparent)  {
                edge.insertMulti(1, pt);
                em[y][x] = 1;
                prevTransparent = false;
            }
            // right and bottom of image
            if (x == w - 1 && qAlpha(s[y][x]) > 0) {
                edge.insertMulti(1, pt);
                em[y][x] = 1;
            }
            if (y == h - 1 && qAlpha(s[y][x]) > 0) {
                edge.insertMulti(1, pt);
                em[y][x] = 1;
            }
        }
    }

    // scan top to bottom
    for (x = 0; x < w; x++) {
        prevTransparent = true;
        for (y = 0; y < h; y++) {
            pt.x= x;
            pt.y= y;
            // opaque to transparent edge
            if (qAlpha(s[y][x]) == 0 && !prevTransparent)  {
                pt.y= y-1;
                // check if already tallied
                if (em[y-1][x] == 0) {
                    edge.insertMulti(1, pt);
                    em[y-1][x] = 1;
                }
                prevTransparent = true;
            }
            // transparent to opaque edge
            if (qAlpha(s[y][x]) > 0 && prevTransparent)  {
                // check if already tallied
                if (em[y][x] == 0) {
                    edge.insertMulti(1, pt);
                    em[y][x] = 1;
                }
                prevTransparent = false;
            }
        }
    }

    // iterate to find points pts for distances > 1
    int d = 1;
    bool morePtsToProcess = true;
    // list of pixels for a distance from the edge
    QList<Pt> pts;
    // list of pixels surrounding a pixel
    QList<Pt> sPts;
    while (morePtsToProcess) {
        morePtsToProcess = false;
        pts.clear();
        // get list of points for the previous distance from the edge
        pts = edge.values(d);
//        qDebug() << __FUNCTION__ << "d =" << d << "pts.size() =" << pts.size();
        d++;
        if (d > depth) break;
        for (int i = 0; i < pts.size(); i++) {
            sPts.clear();
            // get a list of the 8 surrounding pixels (except for image edges)
            pt = pts.at(i);
//            surrounding4Px(sPts, pt, w, h);
            surrounding8Px(sPts, pt, w, h);
            // for each surrounding pixel
            for (int j = 0; j < sPts.size(); j++) {
                x = sPts.at(j).x;
                y = sPts.at(j).y;
                // if not already tallied and not transparent then tally
                if (em[y][x] == 0 && qAlpha(s[y][x]) > 0) {
                    // add new pixel
                    edge.insertMulti(d, sPts.at(j));
                    // tally new pixel
                    em[y][x] = d;
                    morePtsToProcess = true;
                }
            }
        }
    }

    /*
    // show result
    qDebug() << __FUNCTION__ << "edge.lastKey() ="  << edge.lastKey();
    int step = 220 / edge.lastKey();
    for (d = 1; d <= edge.lastKey(); d++) {
        pts.clear();
        pts = edge.values(d);
        for (int i = 0; i < pts.size(); i++) {
            int red = d * step + 20;
            if (red>255) red = 255;
            x = pts.at(i).x;
            y = pts.at(i).y;
            s[y][x] = qRgba(red, 0, 0, qAlpha(s[y][x]));
        }
    }
    */
    return;
}

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

QRgb Effects::averagePixelInBox(QVector<QVector<QRgb>> &q, const int &x, const int &y,
                           const int &w, const int &h, const int &o)
{
/*
Return the average of the pixels in the box defined by offset o centered on x,y
(x-o, y-o) to (x+o, y+o)
*/
    // return if coordinate out of range
    if (x < 0 || x > w - 1 || y < 0 || y > h - 1) {
        /*
        qDebug() << __FUNCTION__ << "WARNING: OUT OF RANGE"
                 << "x =" << x
                 << "y =" << y
                    ;
                    */
        return 0;
    }

    int x1, x2, y1, y2;
    int r, g, b, a;
    r = g = b = a = 0;
    x - o < 0 ? x1 = 0 : x1 = x - o;
    y - o < 0 ? y1 = 0 : y1 = y - o;
    x + o > w - 1 ? x2 = w - 1 : x2 = x + o;
    y + o > h - 1 ? y2 = h - 1 : y2 = y + o;
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
    int count = 0;
    for (int v = y1; v <= y2; v++) {
        for (int u = x1; u <= x2; u++) {
            count++;
            r += qRed(q[v][u]);
            g += qGreen(q[v][u]);
            b += qBlue(q[v][u]);
            a += qAlpha(q[v][u]);
        }
    }
    r /= count;
    g /= count;
    b /= count;
    a /= count;
    return qRgba(r, g, b, a);
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

void Effects::brightenPixel(QRgb &p, double deltaEv, double opacity)
{
/*
p = pixel from original image, deltaEV = change in exposure value

Changes the luminance (brightness) of a pixel by an exposure value (f/stop) emulating how
exposure works in lightroom.  Luminance (lum) is on a scale 0-255, where 128 = 0EV.

Since EV reflects what our brain interprets from the amount of light, which is not linear. This
table shows the relationship:

    EV      Lum (0-255)
    -5           17
    -4           24
    -3           35
    -2           54
    -1           83
     0          128
     1          179
     2          223
     3          246
     4          252

Two quadratic formulas for neg/pos ev:

    lum = a * ev² + b * ev + c
    ev  = (b + (b² - 4a(128 - lum))½) / 2a

        Negative EV         Positive EV
    a       5.426              -7.9286
    b     -47.229             -63.214
    c     128                 128

*/
    // if change is to ev 0 then just use original
    if (deltaEv > -0.01 && deltaEv < 0.01) return;

    int alpha = qAlpha(p) * opacity;
    // QColor for pixel p
    QColor rgb(p);
    // convert to HSL
    QColor hsl = rgb.toHsl();
    // get the luminance 0-255 of the pixel
    double lum = hsl.lightnessF() * 255;
    // get the exposure value of the pixel
    double ev;
    if (lum < 26)
        ev = -4;
    else if (lum < 128)
        ev = (-47.229 + qSqrt(2230.578441 - 21.704 * (128 - lum))) / 10.852;
    else if (lum > 253)
        ev = 4;
    else
        ev = (-63.214 + qSqrt(3996.00996 + 31.7144 * (128 - lum))) / (-15.8572);
    // new EV
    double newEv = ev + deltaEv;
    if (newEv > 4) newEv = 4;
    if (newEv < -4) newEv = -4;
    // new luminance from new exposure value
    double l;
    if (newEv < 0)
        l = (5.426 * newEv * newEv + 47.229 * newEv + 128) / 255;
    else
        l = (-7.9286 * newEv * newEv + 63.214 * newEv + 128) / 255;
    // plug new luminance into HSL
    hsl.setHslF(hsl.hslHueF(), hsl.hslSaturationF(), l);
    // convert back to RGB
    rgb =  hsl.toRgb();
    // update pixel p
    p = qRgba(rgb.red(), rgb.green(), rgb.blue(), alpha);
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

    // define line y = mx + b;
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

    // vector to store average results
    QVector<QRgb> temp;

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
        int offset = static_cast<int>(width/2);
        for (int x = p1.x; x <= p2.x; ++x) {
            int y = static_cast<int>(m * x + b);
            if (offset > 0) {
//                // tapered offset
//                int to = 0;
                for (int d = -offset; d <= offset; ++d) {
//                    if (d <= 0) to++;
//                    else to--;
                    // horizontal line
                    if (qFuzzyCompare(m, 0) && y+d >= 0 && y+d < h)
                        temp << averagePixelInBox(q, x, y + d, w, h, offset);
                    // everything else
                    else if (x+d >= 0 && x+d < w)
                        temp << averagePixelInBox(q, x + d, y, w, h, offset);
                }
            }
        }
        // update img vector from temp averages
        int i = 0;
        int len = temp.size();
        /*
        qDebug() << __FUNCTION__
                 << "width =" << width
                 << "p1.x =" << p1.x
                 << "p2.x =" << p2.x
                 << "temp.size() =" << temp.size()
                    ;
//        */
        for (int x = p1.x; x <= p2.x; ++x) {
            int y = static_cast<int>(m * x + b);
            if (offset > 0) {
                for (int d = -offset; d <= offset; ++d) {
                    // horizontal line
                    if (qFuzzyCompare(m, 0) && y+d >= 0 && y+d < h)
                        q[y+d][x] = temp.at(i);
                    // everything else
                    else if (x+d >= 0 && x+d < w)
                        if (i < len) q[y][x+d] = temp.at(i++);
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

void Effects::brightness(QImage &img, qreal evDelta)
{
//    qDebug() << __FUNCTION__ << "delta =" << evDelta;
    QElapsedTimer t;
    t.start();

    // bDelta               // change in brightness
    int w = img.width();
    int h = img.height();

    if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);

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

//    qDebug() << __FUNCTION__
//             << "Elapsed time =" << QString("%L1").arg(t.nsecsElapsed());
}

double Effects::embossEV(int &m, int d, double &contrast, double exposure,
                         double inflection, double startEV, double midEV, double endEV,
                         double &umbra, bool isUmbra, bool isUmbraGradient)
{
    /*
    This is called from Effects::emboss to return the brightness (ev) of a pixel.

    The emboss effect works by adjusting the brightness of the pixels across a margin or
    boundary around the perimeter of the object.  In the diagram the margin is the x axis,
    which is normalized to 0 to 1.  The y axis is the brightness, measured in ev (exposure
    value). The * represent the ev at each pixel across the margin.  The inflection point
    is where the two lines either peak or trough.

                |     * inflection pt xi, midEV
                |    * *
                |   *   *
             0,0|--*-----*---1,0
                | *       *
                |*         *
      0,startEV *           * 1,endEV

      for the formula of a line y = ax + b, where a = slope, b = y intercept

          if xi = 0  b = start  slope = (start - end)
          if x < xi  b = start  slope = (start - mid) / xi
          if x > xi  b = mid    slope = (mid - end) / (1 - xi)

    ev       = measure of brightness in exposure values (see Effects::brightenPixel)
    m        = the margin in pixels to emboss (ie the width of a border)
    d        = the current distance through the margin in pixels
    x        = d normalized between 0 and 1
    dd       = m - d = the pixels remaining to the edge of the margin
    slope    = the slope of the line, either before or after the inflection point
    exposure = the amount of brightening to add
    contrast = darken neg ev and brighten pos ev
    contour  = (n/a) the function to use to determine the amount of brightening for each pixel
               ie flat, ridge, trough gradient...
    startEV  = the ev at the start of the margin
    midEV    = the ev at the inflection point
    endEV    = the ev at the end of the margin
    umbra    = the ev amount to adjust the areas not exposed to light.  This is determined
               by the slope and isUmbra
    isUmbra  = not exposed directly to light source (see light direction = azimuth)
    isUmbraGradient = create a smooth gradient between startEv to midEV to endEV
    */

    double ev = exposure;
    double x = static_cast<double>(d) / m;
    double xi = inflection;
    double slope;

    // between start and inflection point
    if (xi > 0 && x < xi) {
        slope = x / xi;
        ev += (midEV - startEV) * slope + startEV;
        if (!isUmbraGradient) slope = 0;
        if (isUmbra && midEV > startEV) ev += umbra * (1 - slope);
        if (!isUmbra && midEV < startEV) ev += umbra * (1 - slope);
    }
    // inflection point to the end
    else {
        slope = (x - xi) / (1 - xi);
        ev += (endEV - midEV) * slope + midEV;
        if (!isUmbraGradient) slope = 1;
        if (isUmbra && endEV > midEV) ev += umbra * slope;
        if (!isUmbra && endEV < midEV) ev += umbra * slope;
    }
    /*
    if (rpt) qDebug().noquote() << __FUNCTION__
         << "d =" << QString::number(d).leftJustified(4)
         << "m =" << QString::number(m).leftJustified(3)
         << "xi =" << QString::number(xi, 'f', 2).rightJustified(6)
         << "start =" << QString::number(start, 'f', 2).rightJustified(6)
         << "mid =" << QString::number(mid, 'f', 2).rightJustified(6)
         << "end =" << QString::number(end, 'f', 2).rightJustified(6)
         << "x =" << QString::number(x, 'f', 3).rightJustified(6)
         << "ev =" << QString::number(ev, 'f', 3).rightJustified(6)
                ;
//                */

    // contrast
    ev *= contrast;

    return ev;
}

void Effects::emboss(QImage &img, int azimuth, double size, double exposure, double contrast,
                     double inflection, double startEV, double midEV, double endEV,
                     double umbra, bool isUmbraGradient)
{
    /*
    The emboss effect works by adjusting the brightness of the pixels across a margin or
    boundary width around the perimeter of the object.

                                    inflection xi
                     startEV           midEV           endEV
                        v                v               v

                              <            d             >

                        |                |               |
     Outside boundary   |     *          |               |  Inside boundary
                        |     ^          |               |
                              |
                              Current pixel (x,y)

                        <              margin            >

    For each pixel, Effects::embossEV is called.
    */

    // border or graphics object with no transparent pixels along outer border
    size /= 100;                // emboss distance from edge (% of long side)
    int w = img.width();
    int h = img.height();
    int ls = w > h ? w : h;     // long side in pixels
    int m = ls * size;          // emboss width or margin in pixels
    double alpha = 1.0;

    double ev = 0;
    int x = 0;
    int y = 0;
    bool isUmbra;

    /*
    qDebug() << __FUNCTION__
             << "size =" << size
             << "w =" << w
             << "h =" << h
             << "ls =" << ls
             << "m =" << m
             << "highlight =" << highlight
             << "shadow =" << shadow
             << "contour =" << contour
             << "white =" << white
             << "black =" << black
                ;
                if (x==200)
                qDebug().noquote()
                     << __FUNCTION__
                     << "x =" << QString::number(x).leftJustified(4)
                     << "y =" << QString::number(y).leftJustified(4)
                     << "h =" << QString::number(h).leftJustified(4)
                     << "w =" << QString::number(w).leftJustified(4)
                     << "m =" << QString::number(m).leftJustified(3)
                     << "dd =" << QString::number(dd).leftJustified(3)
                     << "sft =" << QString::number(sft).leftJustified(3)
                     << "evTop  =" << QString::number(evTop, 'f', 3).rightJustified(6)
                     << "ev =" << QString::number(ev, 'f', 3).rightJustified(6)
//                      << "(dd)/sft =" << static_cast<double>(dd)/sft
                     << "(s[y][x] =" << QString::number(s[y][x], 16)
                     << "white =" << QString::number(white, 'f', 3).rightJustified(6)
                     << "black =" << QString::number(black, 'f', 3).rightJustified(6)
                        ;
    */

    // create QVector of img for pixel wrangling
    QVector<QVector<QRgb>> s(h);
    imageToVector(img, s);

    // top
    (azimuth > 270 || azimuth < 90) ? isUmbra = false : isUmbra = true;
    for (y = 0; y < m; y++) {
        ev = embossEV(m, y, contrast, exposure, inflection, startEV, midEV, endEV, umbra, isUmbra, isUmbraGradient);
        for (x = y; x < w - y; x++) {
            brightenPixel(s[y][x], ev, alpha);
        }
    }

    // right
    (azimuth > 0 && azimuth < 180) ? isUmbra = false : isUmbra = true;
    for (x = w - m; x < w; x++) {
        ev = embossEV(m, w-x-1, contrast, exposure, inflection, startEV, midEV, endEV, umbra, isUmbra, isUmbraGradient);
        for (y = w - x; y < h - w + x; y++) {
            brightenPixel(s[y][x], ev, alpha);
        }
    }

    // bottom
    (azimuth > 90 && azimuth < 270) ? isUmbra = false : isUmbra = true;
    for (y = h - m; y < h; y++) {
        ev = embossEV(m, h-y-1, contrast, exposure, inflection, startEV, midEV, endEV, umbra, isUmbra, isUmbraGradient);
        for (x = h - y; x < w - h + y + 1; x++) {
            brightenPixel(s[y][x], ev, alpha);
        }
    }

    // left
    (azimuth > 180 && azimuth < 360) ? isUmbra = false : isUmbra = true;
    for (x = 0; x < m; x++) {
        ev = embossEV(m, x, contrast, exposure, inflection, startEV, midEV, endEV, umbra, isUmbra, isUmbraGradient);
        for (y = x + 1; y < h - x; y++) {
            brightenPixel(s[y][x], ev, alpha);
        }
    }

    // build destination image
    vectorToImage(img, s);
}

void Effects::stroke(QImage &img, double width, QColor color, bool inner)
{
/*
Draws a solid boundary around an object in an image, where the boundary is transparency.
*/
    // create QVector (s) of img for pixel wrangling
    QVector<QVector<QRgb>> s(img.height());
    imageToVector(img, s);

    // get edge map for img
    QMultiMap<int, Pt> edge;
    int depth = static_cast<int>(width);
    transparentEdgeMap(img, depth, s, edge);

    // erase img so can paint back effect only
    img.fill(Qt::transparent);

    // paint in the stroke for the assigned width and color
    QList<Pt> pts;
    int w;
    double aPct;
    width > edge.lastKey() ? w = edge.lastKey() : w = static_cast<int>(width);
    for (int d = 1; d <= w; d++) {
        aPct = 1.0;
        if (d == 1) aPct = 0.5;
        if (d == 2) aPct = 0.75;
        if (d == w - 1) aPct = 0.75;
        if (d == w) aPct = 0.5;
        pts.clear();
        pts = edge.values(d);
        for (int i = 0; i < pts.size(); i++) {
            int x = pts.at(i).x;
            int y = pts.at(i).y;
            s[y][x] = qRgba(color.red(), color.green(), color.blue(), qAlpha(s[y][x]) * aPct);
        }
    }

    // anti-alias
//    pts.clear();
//    pts = edge.values(1);
//    for (int i = 0; i < pts.size(); i++) {
//        int x = pts.at(i).x;
//        int y = pts.at(i).y;
//        if (qAlpha(s[y][x]) > 192)
//            s[y][x] = qRgba(color.red(), color.green(), color.blue(), qAlpha(s[y][x]) * 0.33);
//    }


    vectorToImage(img, s);
}

void Effects::glow(QImage &img, double width, QColor color, double blurRadius)
{
    // create QVector (s) of img for pixel wrangling
    QVector<QVector<QRgb>> s(img.height());
    imageToVector(img, s);

    // get edge map for img
    QMultiMap<int, Pt> edge;
    int depth = static_cast<int>(width);
    transparentEdgeMap(img, depth, s, edge);

    // erase img so can paint back effect only
    img.fill(Qt::transparent);

    // paint in the stroke for the assigned width and color
    QList<Pt> pts;
    int w;
    width > edge.lastKey() ? w = edge.lastKey() : w = static_cast<int>(width);
    for (int d = 1; d <= w; d++) {
        pts.clear();
        pts = edge.values(d);
        for (int i = 0; i < pts.size(); i++) {
            int x = pts.at(i).x;
            int y = pts.at(i).y;
            s[y][x] = qRgba(color.red(), color.green(), color.blue(), qAlpha(s[y][x]));
        }
    }

    // convert back to QImage
    vectorToImage(img, s);

    // blur the stroke
    int radius = static_cast<int>(blurRadius);
    blurOriginal(img, radius);
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
