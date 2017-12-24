#include "imagealign.h"
#include "global.h"
#include <QDebug>

ImageAlign::ImageAlign(int thumbSize, qreal targetPct, QObject *parent) : QObject(parent)
{
    this->thumbSize = thumbSize;
    this->targetPct = targetPct;
    targetSize = thumbSize * targetPct;
    target = new QImage;
    baseThumb = new QImage;
    alignThumb = new QImage;
}

QPointF ImageAlign::alignImage(QImage *baseImage, QImage *image, QPointF &basePosPct)
{
//    qreal posX = (qreal)basePos.x() / baseImage->width();
//    qreal posY = (qreal)basePos.y() / baseImage->height();

    *baseThumb = baseImage->scaled(thumbSize, thumbSize, Qt::KeepAspectRatio);
    *alignThumb = image->scaled(thumbSize, thumbSize, Qt::KeepAspectRatio);

    QRect targetRect(basePosPct.x() * baseThumb->width() - targetSize / 2,
                     basePosPct.y() * baseThumb->height() - targetSize / 2,
                     targetSize, targetSize);
    qDebug() << targetSize << basePosPct.x() << basePosPct.y() << targetRect
             << alignThumb->width() << alignThumb->height();
    *target = baseThumb->copy(targetRect);

//    TestAlignDlg *dlg = new TestAlignDlg(target, alignThumb);
//    dlg->exec();
//    delete dlg;


    uint w = alignThumb->width() - targetSize;
    uint h = alignThumb->height() - targetSize;

    ulong diff;
    ulong lowestDiff = (ulong)999999999999999;
    QPoint bestAlignPt;
    int t = targetSize;
    uint count = 0;

//    qDebug() << "w, h, t" << w << h << t;

//    QTime tmr;
//    tmr.start();

    // iterate entire image
    for (uint y = 0; y < h; ++y) {
        for (uint x = 0; x < w; ++x) {
            // iterate target inside image
            diff = 0;
            for (int yt = 0; yt < t; ++yt) {
                QRgb *row = (QRgb*)alignThumb->scanLine(y + yt);
                QRgb *targetRow = (QRgb*)target->scanLine(yt);
                for (int xt = 0; xt < t; ++xt) {
                    QRgb px = row[x + xt];
                    QRgb pxT = targetRow[xt];
/*                    int pxR = qRed(px);
                    int pxRT = qRed(pxT);
                    int pxG = qGreen(px);
                    int pxGT = qGreen(pxT);
                    int pxB = qBlue(px);
                    int pxBT = qBlue(pxT);   */
                    diff += qAbs(qRed(px) - qRed(pxT));
                    diff += qAbs(qGreen(px) - qGreen(pxT));
                    diff += qAbs(qBlue(px) - qBlue(pxT));
                    if (diff > lowestDiff) break;
                    count++;
                }
                if (diff > lowestDiff) break;
            }
            if (diff < lowestDiff && diff > 0) {
                lowestDiff = diff;
                bestAlignPt = QPoint(x + t / 2, y + t / 2);
            }
//            if (x == 74 || x == 84 || x == 94)
//                qDebug() << "x =" << x << "y =" << y << "diff" << diff
//                         << "lowestDiff" << lowestDiff
//                         << "bestAlignPt" <<bestAlignPt;
        }
    }
//    qDebug() << "bestAlignPt.x()" << bestAlignPt.x() << "alignThumb->width()" << alignThumb->width();
    qreal alignX = (qreal)(bestAlignPt.x()) / alignThumb->width();
    qreal alignY = (qreal)(bestAlignPt.y()) / alignThumb->height();
//    qDebug() << "ALIGN:" << count << lowestDiff << bestAlignPt << alignX << alignY;
    return QPointF(alignX, alignY);
}

