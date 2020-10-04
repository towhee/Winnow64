#ifndef EFFECT_H
#define EFFECT_H

#include <QtWidgets>
#include "interpolate.h"

/*
This code is from D:\Programming\ImageEditingSourceCode\qimageblitz-master
GitHub: https://github.com/mylxiaoyi/qimageblitz
*/

namespace GraphicEffect {

inline QRgb convertFromPremult(QRgb p)
{
    int alpha = qAlpha(p);
    return(!alpha ? 0 : qRgba(255*qRed(p)/alpha,
                              255*qGreen(p)/alpha,
                              255*qBlue(p)/alpha,
                              alpha));
}

inline QRgb convertToPremult(QRgb p)
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

QImage blur(QImage &img, int radius)
{
    QRgb *p1, *p2;
    int x, y, w, h, mx, my, mw, mh, mt, xx, yy;
    int a, r, g, b;
    int *as, *rs, *gs, *bs;

    if(radius < 1 || img.isNull() || img.width() < (radius << 1))
        return(img);

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
                    rs[x] += qRed(pixel);
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
                    rs[x] += qRed(pixel);
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
                    rs[x] += qRed(*p2);
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

    return(buffer);
}

QImage sharpen(QImage &img, int radius)
{
    if(img.isNull() || radius < 1)
        return(img);
    if(img.depth() != 32){
        img = img.convertToFormat(img.hasAlphaChannel() ?
                                  QImage::Format_ARGB32 :
                                  QImage::Format_RGB32);
    }
    else if(img.format() == QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32);

    QImage buffer(img.width(), img.height(), img.format());
    int a, r, g, b, x, y;
    int w = img.width();
    int h = img.height();
    QRgb *src, *dest;

    memcpy(buffer.scanLine(0), img.scanLine(0), img.bytesPerLine());
    for(y=1; y < h-1; ++y){
        src = (QRgb *)img.scanLine(y);
        dest = (QRgb *)buffer.scanLine(y);
        *dest++ = *src++;
        for(x=1; x < w-1; ++x){
            r = qRed(*src)*5;
            g = qGreen(*src)*5;
            b = qBlue(*src)*5;
            a = qAlpha(*src)*5;

            r -= qRed(*(src-1));
            g -= qGreen(*(src-1));
            b -= qBlue(*(src-1));
            a -= qAlpha(*(src-1));
            r -= qRed(*(src+1));
            g -= qGreen(*(src+1));
            b -= qBlue(*(src+1));
            a -= qAlpha(*(src+1));

            r -= qRed(*(src-w));
            g -= qGreen(*(src-w));
            b -= qBlue(*(src-w));
            a -= qAlpha(*(src-w));
            r -= qRed(*(src+w));
            g -= qGreen(*(src+w));
            b -= qBlue(*(src+w));
            a -= qAlpha(*(src+w));

            r = (r & ((~r) >> 16));
            r = ((r | ((r & 256) - ((r & 256) >> 8))));
            g = (g & ((~g) >> 16));
            g = ((g | ((g & 256) - ((g & 256) >> 8))));
            b = (b & ((~b) >> 16));
            b = ((b | ((b & 256) - ((b & 256) >> 8))));
            a = (a & ((~a) >> 16));
            a = ((a | ((a & 256) - ((a & 256) >> 8))));

            *dest = qRgba(r, g, b, a);
            ++src; ++dest;
        }
        *dest++ = *src++;
    }
    memcpy(buffer.scanLine(h-1), img.scanLine(h-1), img.bytesPerLine());
    return(buffer);
}

/*
 * I was looking for a quick way to do edge detection and found
 * the Sobel method. This is a gradient method that applies two 3x3
 * matrixes. These matrixes are:
 *
 * x:  -1, 0, 1      y:  1,  2,  1
 *     -2, 0, 2          0,  0,  0
 *     -1, 0, 1         -1, -2, -1
 *
 * After the matrix is applied you simply calculate the magnitude by
 * |x| + |y|.
 *
 * The one problem with this is most descriptions of the algorithm I've
 * seen assume grayscale data so your working with the intensity of the
 * pixel. We do each color channel separately. This is probably wrong,
 * but all the implementations I've seen do this...
 * (mosfet)
 */

// Accumulates the results of applying x and y Sobel masks
#define SOBEL(xm, ym, pixel) \
    xR += qRed((pixel))*(xm); \
    xG += qGreen((pixel))*(xm); \
    xB += qBlue((pixel))*(xm); \
    yR += qRed((pixel))*(ym); \
    yG += qGreen((pixel))*(ym); \
    yB += qBlue((pixel))*(ym);

// rgh convert this to work with alpha channel as well
QImage edge(QImage &img)
{
    int x, y, w = img.width(), h = img.height();
    if(w < 3 || h < 3){
        qWarning("Blitz::edge(): Image is too small!");
        return(img);
    }
    if(img.isNull())
        return(img);

    if(img.depth() != 32){
        img = img.convertToFormat(img.hasAlphaChannel() ?
                                  QImage::Format_ARGB32 :
                                  QImage::Format_RGB32);
    }
    else if(img.format() == QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32);

    QImage buffer(w, h, QImage::Format_RGB32);
    QRgb *dest;
    QRgb *s, *scanblock[3];

    int xR, xG, xB, yR, yG, yB;
    for (y = 0; y < h; ++y) {
        scanblock[1] = reinterpret_cast<QRgb *>(img.scanLine(y));
        dest = reinterpret_cast<QRgb *>(buffer.scanLine(y));
        if (y == 0) {
            scanblock[0] = reinterpret_cast<QRgb *>(img.scanLine(y));
            scanblock[2] = reinterpret_cast<QRgb *>(img.scanLine(y + 1));
        }
        else if (y == h - 1) {
            scanblock[0] = reinterpret_cast<QRgb *>(img.scanLine(y - 1));
            scanblock[2]  = reinterpret_cast<QRgb *>(img.scanLine(y));
        }
        else{
            scanblock[0] = reinterpret_cast<QRgb *>(img.scanLine(y - 1));
            scanblock[2] = reinterpret_cast<QRgb *>(img.scanLine(y + 1));
        }

        // x == 0, double over first pixel
        xR = xG = xB = yR = yG = yB = 0;
        s = scanblock[0];
        SOBEL(-1, 1, *s) SOBEL(0, 2, *s) ++s; SOBEL(1, 1, *s)
        s = scanblock[1];
        SOBEL(-2, 0, *s) SOBEL(0, 0, *s) ++s; SOBEL(2, 0, *s)
        s = scanblock[2];
        SOBEL(-1, -1, *s) SOBEL(0, -2, *s) ++s; SOBEL(1, -1, *s)
        xR = qAbs(xR) + qAbs(yR);
        xG = qAbs(xG) + qAbs(yG);
        xB = qAbs(xB) + qAbs(yB);
        *dest++ = qRgb(qMin(xR, 255), qMin(xG, 255), qMin(xB, 255));

        // x == 1, process middle of image
        for(x=1; x < w-1; ++x){
            xR = xG = xB = yR = yG = yB = 0;
            s = scanblock[0];
            SOBEL(-1, 1, *s) ++s; SOBEL(0, 2, *s) ++s; SOBEL(1, 1, *s)
            s = scanblock[1];
            SOBEL(-2, 0, *s) ++s; SOBEL(0, 0, *s) ++s; SOBEL(2, 0, *s)
            s = scanblock[2];
            SOBEL(-1, -1, *s) ++s; SOBEL(0, -2, *s) ++s; SOBEL(1, -1, *s)
            ++scanblock[0]; ++scanblock[1]; ++scanblock[2];
            xR = qAbs(xR)+qAbs(yR);
            xG = qAbs(xG)+qAbs(yG);
            xB = qAbs(xB)+qAbs(yB);
            *dest++ = qRgb(qMin(xR, 255), qMin(xG, 255), qMin(xB, 255));
        }

        // x == w-1, double over last pixel
        xR = xG = xB = yR = yG = yB = 0;
        s = scanblock[0];
        SOBEL(-1, 1, *s) ++s; SOBEL(0, 2, *s) SOBEL(1, 1, *s)
        s = scanblock[1];
        SOBEL(-2, 0, *s) ++s; SOBEL(0, 0, *s) SOBEL(2, 0, *s)
        s = scanblock[2];
        SOBEL(-1, -1, *s) ++s; SOBEL(0, -2, *s) SOBEL(1, -1, *s)
        xR = qAbs(xR)+qAbs(yR);
        xG = qAbs(xG)+qAbs(yG);
        xB = qAbs(xB)+qAbs(yB);
        *dest++ = qRgb(qMin(xR, 255), qMin(xG, 255), qMin(xB, 255));
    }
    return(buffer);
}

// Normalize
typedef struct
{
    quint32 red, green, blue, alpha;
} IntegerPixel;

typedef struct
{
    quint16 red, green, blue, alpha;
} ShortPixel;

typedef struct
{
    // Yes, a normal pixel can be used instead but this is easier to read
    // and no shifts to get components.
    quint8 red, green, blue, alpha;
} CharPixel;

typedef struct
{
    quint32 red, green, blue, alpha;
} HistogramListItem;

bool normalize(QImage &img)
{
    if(img.isNull())
        return(false);

    IntegerPixel intensity;
    HistogramListItem *histogram;
    CharPixel *normalize_map;
    ShortPixel high, low;

    uint threshold_intensity;
    int i, count;
    QRgb pixel, *dest;
    unsigned char r, g, b;

    if(img.depth() < 32){
        img = img.convertToFormat(img.hasAlphaChannel() ?
                                  QImage::Format_ARGB32 :
                                  QImage::Format_RGB32);
    }
    count = img.width()*img.height();

    histogram = new HistogramListItem[256];
    normalize_map = new CharPixel[256];

    // form histogram
    memset(histogram, 0, 256*sizeof(HistogramListItem));
    dest = (QRgb *)img.bits();

    if(img.format() == QImage::Format_ARGB32_Premultiplied){
        for(i=0; i < count; ++i, ++dest){
            pixel = convertFromPremult(*dest);
            histogram[qRed(pixel)].red++;
            histogram[qGreen(pixel)].green++;
            histogram[qBlue(pixel)].blue++;
            histogram[qAlpha(pixel)].alpha++;
        }
    }
    else{
        for(i=0; i < count; ++i){
            pixel = *dest++;
            histogram[qRed(pixel)].red++;
            histogram[qGreen(pixel)].green++;
            histogram[qBlue(pixel)].blue++;
            histogram[qAlpha(pixel)].alpha++;
        }
    }

    // find the histogram boundaries by locating the .01 percent levels.
    threshold_intensity = count/1000;

    memset(&intensity, 0, sizeof(IntegerPixel));
    for(low.red=0; low.red < 256; ++low.red){
        intensity.red += histogram[low.red].red;
        if(intensity.red > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(high.red=255; high.red >= 0; --high.red){
        intensity.red += histogram[high.red].red;
        if(intensity.red > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(low.green=low.red; low.green < high.red; ++low.green){
        intensity.green += histogram[low.green].green;
        if(intensity.green > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(high.green=high.red; high.green != low.red; --high.green){
        intensity.green += histogram[high.green].green;
        if(intensity.green > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(low.blue=low.green; low.blue < high.green; ++low.blue){
        intensity.blue += histogram[low.blue].blue;
        if(intensity.blue > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(high.blue=high.green; high.blue != low.green; --high.blue){
        intensity.blue += histogram[high.blue].blue;
        if(intensity.blue > threshold_intensity)
            break;
    }

    delete[] histogram;

    // stretch the histogram to create the normalized image mapping.
    for(i=0; i < 256; i++){
        if(i < low.red)
            normalize_map[i].red = 0;
        else{
            if(i > high.red)
                normalize_map[i].red = 255;
            else if(low.red != high.red)
                normalize_map[i].red = (255*(i-low.red))/
                    (high.red-low.red);
        }

        if(i < low.green)
            normalize_map[i].green = 0;
        else{
            if(i > high.green)
                normalize_map[i].green = 255;
            else if(low.green != high.green)
                normalize_map[i].green = (255*(i-low.green))/
                    (high.green-low.green);
        }

        if(i < low.blue)
            normalize_map[i].blue = 0;
        else{
            if(i > high.blue)
                normalize_map[i].blue = 255;
            else if(low.blue != high.blue)
                normalize_map[i].blue = (255*(i-low.blue))/
                    (high.blue-low.blue);
        }
    }

    // write
    dest = (QRgb *)img.bits();
    if(img.format() == QImage::Format_ARGB32_Premultiplied){
        for(i=0; i < count; ++i, ++dest){
            pixel = convertFromPremult(*dest);
            r = (low.red != high.red) ? normalize_map[qRed(pixel)].red :
                qRed(pixel);
            g = (low.green != high.green) ? normalize_map[qGreen(pixel)].green :
                qGreen(pixel);
            b = (low.blue != high.blue) ?  normalize_map[qBlue(pixel)].blue :
                qBlue(pixel);
            *dest = convertToPremult(qRgba(r, g, b, qAlpha(pixel)));
        }
    }
    else{
        for(i=0; i < count; ++i){
            pixel = *dest;
            r = (low.red != high.red) ? normalize_map[qRed(pixel)].red :
                qRed(pixel);
            g = (low.green != high.green) ? normalize_map[qGreen(pixel)].green :
                qGreen(pixel);
            b = (low.blue != high.blue) ?  normalize_map[qBlue(pixel)].blue :
                qBlue(pixel);
            *dest++ = qRgba(r, g, b, qAlpha(pixel));
        }
    }

    delete[] normalize_map;
    return(true);
}

bool grayscale(QImage &img, bool reduceDepth)
{
    if(img.isNull()) return(false);

    if(img.format() == QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32);
    else if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);

    if(reduceDepth && img.format() ==  QImage::Format_RGB32){
        // 32bpp w/ conversion to palette
        int y, w = img.width(), h = img.height();
        QImage buffer(w, h, QImage::Format_Indexed8);

        QVector<QRgb> cTable(256);
        unsigned int *src = cTable.data();
        for(y=0; y < 256; ++y)
         *src++ = qRgb(y, y, y);
        buffer.setColorTable(cTable);

        src = (unsigned int *)img.scanLine(0);
        unsigned char *end, *dest;
        unsigned int pixel;
        for(y=0; y < h; ++y){
            dest = buffer.scanLine(y);
            end = dest+w;
            while(dest != end){
                pixel = *src++;
                *dest++ = qGray(qRed(pixel), qGreen(pixel), qBlue(pixel));
            }
        }
        img = buffer;
    }
    else {
        // 8bpp or 32bpp w/ no conversion
        int count = img.depth() > 8 ? img.width()*img.height() : img.colorCount();

        QVector<QRgb> cTable;
        if(img.depth() == 8)
            cTable = img.colorTable();

        unsigned int *data = (img.depth() > 8) ?
            (unsigned int *)img.scanLine(0) : cTable.data();
        unsigned int *end = data+count;
        unsigned int pixel;
        unsigned char c;
        while(data != end){
            pixel = *data;
            c = qGray(qRed(pixel), qGreen(pixel), qBlue(pixel));
            *data++ = qRgba(c, c, c, qAlpha(pixel));
        }
        if(img.depth() == 8) img.setColorTable(cTable);
    }
    return(true);
}

// black and white edges
QImage charcoal(QImage &img)
{
    QImage buffer(edge(img));
    buffer = blur(buffer, 1);
    normalize(buffer);
    buffer.invertPixels();
    grayscale(buffer, true);
    return(buffer);
}



// not useful
QImage implode(QImage &img, float amount)
{

    float distance, radius, factor;
    float x_center, x_distance, x_scale;
    float y_center, y_distance, y_scale;
    int x, y, w, h;
    QRgb *dest;

    w = img.width();
    h = img.height();

    if(img.format() == QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32);
    else if(img.depth() < 8)
        img = img.convertToFormat(QImage::Format_Indexed8);

    QImage buffer(w, h, img.hasAlphaChannel() ?
                  QImage::Format_ARGB32 : QImage::Format_RGB32);

    x_scale = y_scale = 1.0;
    x_center = 0.5*w;
    y_center = 0.5*h;
    radius = x_center;
    if(w > h)
        y_scale = (float)(w/h);
    else if(w < h){
        x_scale = (float)(h/w);
        radius = y_center;
    }

    InlineInterpolate interpolate(&img, 0);
    if(img.depth() > 8){
        QRgb *src;
        for(y=0; y < h; ++y){
            src = (QRgb *)img.scanLine(y);
            dest = (QRgb *)buffer.scanLine(y);
            y_distance = y_scale*(y-y_center);
            for(x=0; x < w; ++x){
                x_distance = x_scale*(x-x_center);
                distance = x_distance*x_distance + y_distance*y_distance;
                if(distance >= (radius*radius))
                    *dest = src[x];
                else{
                    factor = 1.0;
                    if(distance > 0.0)
                        factor = std::pow(std::sin(((float)M_PI)*
                                                   std::sqrt(distance)/
                                                   radius/2), -amount);
                    *dest = interpolate.
                        interpolate(factor*x_distance/x_scale+x_center,
                                    factor*y_distance/y_scale+y_center);
                }
                ++dest;
            }
        }
    }
    else{
        QVector<QRgb> cTable(img.colorTable());
        unsigned char *src;
        for(y=0; y < h; ++y){
            src = img.scanLine(y);
            dest = (QRgb *)buffer.scanLine(y);
            y_distance = y_scale*(y-y_center);
            for(x=0; x < w; ++x){
                x_distance = x_scale*(x-x_center);
                distance = x_distance*x_distance + y_distance*y_distance;
                if(distance >= (radius*radius))
                    *dest = cTable.at(src[x]);
                else{
                    factor = 1.0;
                    if(distance > 0.0)
                        factor = std::pow(std::sin(((float)M_PI)*
                                                   std::sqrt(distance)/
                                                   radius/2), -amount);
                    *dest = interpolate.
                        interpolate(factor*x_distance/x_scale+x_center,
                                    factor*y_distance/y_scale+y_center);
                }
                ++dest;
            }
        }
    }
    return(buffer);
}

} // end namespace


#endif // EFFECT_H
