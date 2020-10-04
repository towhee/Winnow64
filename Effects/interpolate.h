#ifndef INTERPOLATE_H
#define INTERPOLATE_H

#include <QtWidgets>
#include <cmath>

/**
 * Performs interpolation between a pixel's colors and it's neighbors.
 * Handles 32bpp and 8bpp source images transparently.
 *
 * @short Inline interpolation
 * @author Daniel M. Duley
 */

class InlineInterpolate
{
public:
    /**
     * Constructs an interpolation object. Do this outside your main loop then
     * call the inline interpolate methods when you need them.
     *
     * @param image The image whose pixels you will be interpolating. Once
     * you have created an interpolation object you can change the image's
     * pixel data, but do not resize or reset it.
     * @param background The background color to use if a pixel's neighbor
     * is off the edge of the image.
     */
    InlineInterpolate(QImage *image, unsigned int background){
        img = image; ptr = img->bits();
        colorTable = img->colorTable();
        w=img->width(); h=img->height();
        bg=background; truecolor = img->depth() > 8;
    }
    static inline QRgb interpolate255(QRgb x, unsigned int a, QRgb y, unsigned int b);
    /**
     * Interpolates the pixel at the given x,y position. If the pixel's
     * neighbor is off the edge of the image the last pixel is extended.
     */
    inline unsigned int interpolate(float x_offset, float y_offset);
    /**
     * Interpolates the pixel at the given x,y position. If the pixel's
     * neighbor is off the edge of the image the background color is used.
     */
    inline unsigned int interpolateBackground(float x_offset, float y_offset);
private:
    int w, h;
    unsigned int p, q, r, s, bg;
    unsigned char *ptr;
    QVector<QRgb> colorTable;
    bool truecolor;
    QImage *img;
};

inline QRgb InlineInterpolate::interpolate255(QRgb x, unsigned int a, QRgb y, unsigned int b)
{
    unsigned int t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;
    x = (x + ((x >> 8) & 0xff00ff) + 0x800080);
    x &= 0xff00ff00;
    x |= t;
    return(x);
}

inline unsigned int InlineInterpolate::interpolate(float x_offset,
                                                   float y_offset)
{
    int x = qBound(0, (int)x_offset, w-2);
    int y = qBound(0, (int)y_offset, h-2);

    if(truecolor){
        p = *(((QRgb *)ptr)+(y*w)+x);
        q = *(((QRgb *)ptr)+(y*w)+x+1);
        r = *(((QRgb *)ptr)+((y+1)*w)+x);
        s = *(((QRgb *)ptr)+((y+1)*w)+x+1);
    }
    else{
        p = colorTable[*(ptr+(y*w)+x)];
        q = colorTable[*(ptr+(y*w)+x+1)];
        r = colorTable[*(ptr+((y+1)*w)+x)];
        s = colorTable[*(ptr+((y+1)*w)+x+1)];
    }
    x_offset -= std::floor(x_offset); y_offset -= std::floor(y_offset);
    unsigned int alpha = (unsigned int)(255*x_offset);
    unsigned int beta = (unsigned int)(255*y_offset);

    p = InlineInterpolate::interpolate255(p, 255-alpha, q, alpha);
    r = InlineInterpolate::interpolate255(r, 255-alpha, s, alpha);
    return(InlineInterpolate::interpolate255(p, 255-beta, r, beta));
}

inline unsigned int InlineInterpolate::interpolateBackground(float x_offset,
                                                             float y_offset)
{
    int x = (int)x_offset;
    int y = (int)y_offset;
    p = q = r = s = bg;

    if(truecolor){
        if(y >= 0 && y < h && x >= 0 && x < w){
            p = *(((QRgb *)ptr)+(y*w)+x);
            if(y+1 < h) r = *(((QRgb *)ptr)+((y+1)*w)+x);
            if(x+1 < w){
                q = *(((QRgb *)ptr)+(y*w)+x+1);
                if(y+1 < h) q = *(((QRgb *)ptr)+((y+1)*w)+x+1);
            }
        }
    }
    else{
        if(y >= 0 && y < h && x >= 0 && x < w){
            p = colorTable[*(ptr+(y*w)+x)];
            if(y+1 < h) r = colorTable[*(ptr+((y+1)*w)+x)];
            if(x+1 < w){
                q = colorTable[*(ptr+(y*w)+x+1)];
                if(y+1 < h) s = colorTable[*(ptr+((y+1)*w)+x+1)];
            }
        }
    }
    x_offset -= std::floor(x_offset); y_offset -= std::floor(y_offset);
    unsigned int alpha = (unsigned int)(255*x_offset);
    unsigned int beta = (unsigned int)(255*y_offset);

    p = interpolate255(p, 255-alpha, q, alpha);
    r = interpolate255(r, 255-alpha, s, alpha);
    return(interpolate255(p, 255-beta, r, beta));
}

#endif // INTERPOLATE_H
