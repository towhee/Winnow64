#ifndef FOCUSMEASURE_H
#define FOCUSMEASURE_H

#include <QObject>
#include <QImage>
#include <QVector>
#include <QMap>
#include <QtMath>
#include <QDebug>
#include <QPainter>
#include <QMetaEnum>
#include <QThread>
#include <opencv2/opencv.hpp>
#include "focusstackutilities.h"

class FocusMeasure : public QObject
{
    Q_OBJECT
public:
    enum Method {
        LaplacianVariance,   // variance of Laplacian
        SobelEnergy,         // sum of squared gradients
        Tenengrad,           // Sobel magnitude thresholded
        EmulateZerene,       // Zerene-style focus metric
        Petteri              // Petteri focus_stack
    };
    Q_ENUM(Method)
    QStringList methods{"LaplacianVariance",
                        "SobelEnergy",
                        "Tenengrad",
                        "EmulateZerene",
                        "Petteri"
                        };

    explicit FocusMeasure(QObject *parent = nullptr);

    void setMethod(Method m);
    void setDownsample(int f);
    void setSaveResults(bool enabled);
    void setOutputFolder(const QString &path);

public slots:
    QMap<int, QImage> computeFocusMaps(const QMap<int, QImage> &stack);

signals:
    void updateStatus(bool keepBase, QString msg, QString src);
    void progress(QString msg, int current, int total);

private:
    Method method = LaplacianVariance;
    int downsample = 1;
    bool saveResults = true;
    QString outputFolder;

    bool computeFocusMaps_Petteri(const QString& alignedPath,
                                  const QString& outFolder,
                                  int downsample,
                                  bool saveResults,
                                  float gaussRadius,    // Petteri’s blur radius (sigma)
                                  float energyThresh);  // Petteri’s pre-threshold
    QImage toGray(const QImage &img);
    QImage focusMapLaplacian(const QImage &gray);
    QImage focusMapSobel(const QImage &gray);
    QImage focusMapTenengrad(const QImage &gray);
    QImage focusMapZerene(const QImage &gray);
    QImage focusMapPetteri(const QImage &gray);
    void saveFocusImage(const QImage &map, const QString &path);
};

#endif // FOCUSMEASURE_H
