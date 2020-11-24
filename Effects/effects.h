#ifndef EFFECTS_H
#define EFFECTS_H

#include <QtWidgets>
#include "Main/global.h"

enum Contour {contour_flat,
              contour_ridge,
              contour_trough,
              contour_gradient_brighter,
              contour_gradient_darker,
              contour_offset_ridge,
              contour_offset_trough
             };

class Effects
{
public:
    Effects();
    void blur(QImage &img, int radius);
    void blurOriginal(QImage &img, int radius);
    void raise(QImage &img, int m, double taper = 0.5, int blurWidth = 3, bool raise = false);
    void brighten(QImage &img, qreal evDelta);
    void emboss(QImage &img, int azimuth, double size, double highlight, double shadow,
                int contour, double white, double black, double soften, bool shade);
    QImage convolve(QImage &img, int mDim, double *matrix);

    void test(QImage &img);
    void brightenPixel(QRgb &p, double deltaEv);

private:
    struct Point {
        int x;
        int y;
    };
    // Pixel ops
    QRgb convertToPremult(QRgb p);
    QRgb convertFromPremult(QRgb p);
    void swapPoints(Point &p1, Point &p2);
    void raisePixel(QVector<QVector<QRgb> > &s, const int &ground, const int &adj,
                    const int &x, const int &y, const double &blend);
    void averagePixel(QVector<QVector<QRgb> > &v, const int &x, const int &y,
                      const int &w, const int &h);
    QRgb averagePixelInBox(QVector<QVector<QRgb> > &v, const int &x, const int &y,
                      const int &w, const int &h, const int &r);
    int luminosity(QRgb &p);
    double embossEV(int &m, int d, int &dd, int &sft, double aspectEv, int contour,
                    double &white, double &black, double shadowEv = 0, bool shade = false,
                    bool rpt = false);

//    void brightenPixel(QRgb &p, double deltaEv);

    // Vector ops
    void imageToVector(QImage &img, QVector<QVector<QRgb>> &v);
    void vectorToImage(QImage &img, QVector<QVector<QRgb>> &v);
    void zeroVector(QImage &img, QVector<QVector<QRgb>> &v);

    // Image ops
    void sumRgb(QRgb &a, QRgb &b);      // no function ???
//    QImage convolve(QImage &img, int matrix_size, double *matrix);
    void blurLine(QVector<QVector<QRgb> > &q, Point &p1, Point &p2,
                  const int &w, const int &h, const int &width);

    // 2D
    int ptToSegment(int x, int y, int x1, int y1, int x2, int y2);

    // 3D
    typedef double
    Vector[3];
    typedef double Vector2D[2];
    void crossProduct(Vector &a, Vector &b, Vector &n);
    void lightVector(int azimuth, Vector &light);
    void normalize(Vector &p);
    void slopeUnitVector(Vector &a, Vector &b, int angle);

};

#endif // EFFECTS_H

//Effects::test Azimuth = 0        n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.000 l[1] =  1.000 l[2] = -0.707     Illumination =  1.000
//Effects::test Azimuth = 15       n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.259 l[1] =  0.966 l[2] = -0.707     Illumination =  0.966
//Effects::test Azimuth = 30       n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.500 l[1] =  0.866 l[2] = -0.707     Illumination =  0.866
//Effects::test Azimuth = 45       n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.707 l[1] =  0.707 l[2] = -0.707     Illumination =  0.707
//Effects::test Azimuth = 60       n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.866 l[1] =  0.500 l[2] = -0.707     Illumination =  0.500
//Effects::test Azimuth = 75       n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.966 l[1] =  0.259 l[2] = -0.707     Illumination =  0.259
//Effects::test Azimuth = 90       n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -1.000 l[1] =  0.000 l[2] = -0.707     Illumination =  0.000
//Effects::test Azimuth = 105      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.966 l[1] = -0.259 l[2] = -0.707     Illumination = -0.259
//Effects::test Azimuth = 120      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.866 l[1] = -0.500 l[2] = -0.707     Illumination = -0.500
//Effects::test Azimuth = 135      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.707 l[1] = -0.707 l[2] = -0.707     Illumination = -0.707
//Effects::test Azimuth = 150      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.500 l[1] = -0.866 l[2] = -0.707     Illumination = -0.866
//Effects::test Azimuth = 165      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.259 l[1] = -0.966 l[2] = -0.707     Illumination = -0.966
//Effects::test Azimuth = 180      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] = -0.000 l[1] = -1.000 l[2] = -0.707     Illumination = -1.000
//Effects::test Azimuth = 195      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.259 l[1] = -0.966 l[2] = -0.707     Illumination = -0.966
//Effects::test Azimuth = 210      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.500 l[1] = -0.866 l[2] = -0.707     Illumination = -0.866
//Effects::test Azimuth = 225      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.707 l[1] = -0.707 l[2] = -0.707     Illumination = -0.707
//Effects::test Azimuth = 240      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.866 l[1] = -0.500 l[2] = -0.707     Illumination = -0.500
//Effects::test Azimuth = 255      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.966 l[1] = -0.259 l[2] = -0.707     Illumination = -0.259
//Effects::test Azimuth = 270      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  1.000 l[1] = -0.000 l[2] = -0.707     Illumination = -0.000
//Effects::test Azimuth = 285      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.966 l[1] =  0.259 l[2] = -0.707     Illumination =  0.259
//Effects::test Azimuth = 300      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.866 l[1] =  0.500 l[2] = -0.707     Illumination =  0.500
//Effects::test Azimuth = 315      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.707 l[1] =  0.707 l[2] = -0.707     Illumination =  0.707
//Effects::test Azimuth = 330      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.500 l[1] =  0.866 l[2] = -0.707     Illumination =  0.866
//Effects::test Azimuth = 345      n[0] =  0.000 n[1] =  1.000 n[2] =  0.000     l[0] =  0.259 l[1] =  0.966 l[2] = -0.707     Illumination =  0.966

