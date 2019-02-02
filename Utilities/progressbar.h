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
    explicit ProgressBar(QWidget *parent);

    void clearProgress();
    void updateProgress(int fromItem, int toItem, int items, QColor doneColor, QString source);
    void updateCursor(int item, int items, QColor currentColor, QColor imageCacheColor);

signals:

public slots:

private:
    QLinearGradient getGradient(QColor c1);
    int counter = 0;
    int htOffset;       // the offset from the top of pnt to the progress bar
    int ht;             // the height of the progress bar
};

#endif // PROGRESSBAR_H
