#ifndef EFFECTS_H
#define EFFECTS_H

#include <QtWidgets>

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
    void blurOriginal(QImage &img, int radius);
    void boxBlur(QImage &img, int radius);
    void boxBlur2D(QImage &img, int radius);
    void boxBlur1(QImage &img, int radius);
    void raise(QImage &img, int m, double taper = 0.5, int blurWidth = 3, bool raise = false);
    void brightness(QImage &img, qreal evDelta);
    void emboss(QImage &img, int azimuth, double size, double exposure, double contrast,
                double inflection, double startEV, double midEV, double endEV,
                double umbra, bool isUmbraGradient);
    void stroke(QImage &img, double width, QColor color, double opacity, bool inner);
    void glow(QImage &img, double width, QColor color, double blurRadius);

    QImage convolve(QImage &img, int mDim, double *matrix);
    void hueCount(QImage &img, QVector<int> &hues);

    void test(QImage &img);
    void brightenPixel(QRgb &p, double deltaEv, double opacity = 1);


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
    double embossEV(int &m, int d, double &contrast, double exposure,
                    double inflection, double startEV, double midEV, double endEV,
                    double &umbra, bool isUmbra, bool isUmbraGradient);

//    void brightenPixel(QRgb &p, double deltaEv);

    // Vector ops
    void imageToVector(QImage &img, QVector<uint> &v);
    void vectorToImage(QImage &img, QVector<uint> &v);
    void imageToVector2D(QImage &img, QVector<QVector<QRgb>> &v);
    void vector2DToImage(QImage &img, QVector<QVector<QRgb>> &v);
    void zeroVector(QImage &img, QVector<QVector<QRgb>> &v);
    void arrayToImage(QImage &img, int *a, quint32 bytes);

    // Image ops
    void sumRgb(QRgb &a, QRgb &b);      // no function ???
//    QImage convolve(QImage &img, int matrix_size, double *matrix);
    void blurLine(QVector<QVector<QRgb> > &q, Point &p1, Point &p2,
                  const int &w, const int &h, const int &width);
    void setOpacity(QImage &img, double opacity);

    // 2D
    struct Pt {
        int x;
        int y;
    };

    int ptToSegment(int x, int y, int x1, int y1, int x2, int y2);
    void surrounding8Px(QList<Pt> &sPts, Pt &p, const int &w, const int &h);
    void surrounding4Px(QList<Pt> &sPts, Pt &p, const int &w, const int &h);
    void transparentEdgeMap(QImage &im, int depth,
                            QVector<QVector<QRgb>> &s,
                            QMultiMap<int, Pt> &edge);

    // 3D
    typedef double Vector[3];
    typedef double Vector2D[2];
    void crossProduct(Vector &a, Vector &b, Vector &n);
    void lightVector(int azimuth, Vector &light);
    void normalize(Vector &p);
    void slopeUnitVector(Vector &a, Vector &b, int angle);

};

#endif // EFFECTS_H
