#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <QWidget>
#include <QMainWindow>
#include <QPainter>
#include <QLabel>
#include "Main/global.h"

class ProgressBar : public QWidget
{
    Q_OBJECT
public:
    explicit ProgressBar(QWidget *parent);

    void clearImageCacheProgress();
    void setBackgroundColor(const QColor &bg);
    void updateImageCacheProgress(int fromItem, int toItem, int items, QLinearGradient gradient);
    // void updateDoneItem(bool isDone, int item, int items, QColor doneColor);
    void updateCursor(int item, int items);

    QColor cursorColor;
    QColor bgColor;
    QColor targetColor;
    QColor imageCacheColor;
    QColor metaReadCacheColor;

    QLinearGradient getGradient(QColor c1);
    QLinearGradient bgGradient;
    QLinearGradient metaReadGradient;
    QLinearGradient imageCacheGradient;
    QLinearGradient cursorGradient;
    QLinearGradient targetColorGradient;

public slots:
    void clearMetadataProgress(QColor bgColor);
    void updateMetadataCacheProgress(int item, int items);

private:
    QPixmap state;
    bool paintingActive = false;

    int counter = 0;
    int icHtOffset;           // the offset from the top of pnt to the progress bar
    int icHt;                 // the height of the progress bar
    int mrHtOffset;       // the offset from the top of pnt to the progress bar for meta progress
    int mrHt;             // the height of the progress bar for meta progress
    int minCursorWidth = 2; // the minimum width in pixels of the current file position on the bar
};

#endif // PROGRESSBAR_H
