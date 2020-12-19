#include "coloranalysis.h"
#include "Effects/effects.h"

ColorAnalysis::ColorAnalysis(QString &fPath, QObject *parent) : fPath(fPath)
{
    QImage img(fPath);
    QVector<int> hues(360, 0);
    Effects effects;
    effects.hueCount(img, hues);
    qDebug() << fPath;
    qDebug().noquote() << "Hue    Count";
    for (int i = 0; i < hues.size(); i++) {
        qDebug().noquote()
//                << hues[i]
//                << QString::number(i).rightJustified(3)
//                << QString::number(hues[i]).rightJustified(9)
                   ;
    }
}
