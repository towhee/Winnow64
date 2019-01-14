#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <QWidget>
#include <QMainWindow>
#include <QPainter>
#include <QLabel>

class ProgressBar : public QWidget
{
    Q_OBJECT
public:
//    explicit ProgressBar(QWidget *parent,
//                         QLabel &cacheLabel,
//                         QPixmap &pBar,
//                         QColor bgColor);

    explicit ProgressBar(QWidget *parent);


    void clearProgress();
    void updateProgress(int fromItem, int toItem, int items, QColor doneColor, QString source);
//    void updateProgress(QVector<bool> isProgress, QColor doneColor);
//    void updateTarget(int fromItem, int toItem, QColor targetColor);
//    void updateTarget(QVector<bool> isProgress, QColor targetColor);

signals:

public slots:

private:
    QLinearGradient getGradient(QColor c1);
    int counter = 0;
    int htOffset;       // the offset from the top of pnt to the progress bar
    int ht;             // the height of the progress bar
};

#endif // PROGRESSBAR_H
