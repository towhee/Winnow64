#include "blur.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

QImage Blur::blur(QImage &src, double factor)
{
#define Blur(weight) \
    total_red+=(weight)*qRed(*s); \
    total_green+=(weight)*qGreen(*s); \
    total_blue+=(weight)*qBlue(*s); \
    total_opacity+=(weight)*qAlpha(*s); \
    s++;

#define Blur256(weight) \
    total_red+=(weight)*qRed(*(cTable+(*s))); \
    total_green+=(weight)*qGreen(*(cTable+(*s))); \
    total_blue+=(weight)*qBlue(*(cTable+(*s))); \
    total_opacity+=(weight)*qAlpha(*(cTable+(*s))); \
    s++;

    if(src.width() < 3 || src.height() < 3)
        return(src);

    double quantum, total_blue, total_green, total_opacity, total_red, weight;

    int x, y;
    unsigned int *q;

    QImage dest(src.width(), src.height(), QImage::Format_RGB32);
    /*
    weight=((100.0-factor)/2)+1;
    quantum = qMax(weight+12.0, 1.0);
    if(src.depth() > 8){ // DirectClass source image
        unsigned int *p, *s;
        for(y=0; y < src.height(); ++y){
            p = (unsigned int *)src.scanLine(qMin(qMax(y-1,0),src.height()-3));
            q = (unsigned int *)dest.scanLine(y);
            // blur this row of pixels.
            *q++=(*(p+src.width()));
            for(x=1; x < src.width()-1; ++x){
                // compute weighted average of target pixel color components.
                total_red=0.0;
                total_green=0.0;
                total_blue=0.0;
                total_opacity=0.0;
                s=p;
                Blur(1); Blur(2); Blur(1);
                s=p+src.width();
                Blur(2); Blur(weight); Blur(2);
                s=p+2*src.width();
                Blur(1); Blur(2); Blur(1);
                *q = qRgba((unsigned char)((total_red+(quantum/2))/quantum),
                           (unsigned char)((total_green+(quantum/2))/quantum),
                           (unsigned char)((total_blue+(quantum/2))/quantum),
                           (unsigned char)((total_opacity+(quantum/2))/quantum));
                p++;
                q++;
            }
            p++;
            *q++=(*p);
        }
    }
    else{ // PsudeoClass source image
        unsigned char *p, *p2, *p3, *s;
        unsigned int *cTable = src.colorTable();
        int scanLineIdx;
        for(y=0; y < src.height(); ++y){
            scanLineIdx = qMin(qMax(y-1,0),src.height()-3);
            p = (unsigned char *)src.scanLine(scanLineIdx);
            p2 = (unsigned char *)src.scanLine(scanLineIdx+1);
            p3 = (unsigned char *)src.scanLine(scanLineIdx+2);
            q = (unsigned int *)dest.scanLine(y);
            // blur this row of pixels.
            *q++=(*(cTable+(*p2)));
            for(x=1; x < src.width()-1; ++x){
                // compute weighted average of target pixel color components.
                total_red=0.0;
                total_green=0.0;
                total_blue=0.0;
                total_opacity=0.0;
                s=p;
                Blur256(1); Blur256(2); Blur256(1);
                s=p2;
                Blur256(2); Blur256(weight); Blur256(2);
                s=p3;
                Blur256(1); Blur256(2); Blur256(1);
                *q = qRgba((unsigned char)((total_red+(quantum/2))/quantum),
                           (unsigned char)((total_green+(quantum/2))/quantum),
                           (unsigned char)((total_blue+(quantum/2))/quantum),
                           (unsigned char)((total_opacity+(quantum/2))/quantum));
                p++;
                p2++;
                p3++;
                q++;
            }
            p++;
            *q++=(*(cTable+(*p)));
        }
    }
    */
    return(dest);
}
