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
    void setBackgroundColor(QColor bg);
    void updateProgress(int fromItem, int toItem, int items, QColor doneColor, QString comment);
    void updateCursor(int item, int items, QColor currentColor, QColor imageCacheColor);
    void saveProgressState();
    void recoverProgressState();

signals:

public slots:

private:
    QPixmap state;
    QLinearGradient getGradient(QColor c1);
    int counter = 0;
    int htOffset;           // the offset from the top of pnt to the progress bar
    int ht;                 // the height of the progress bar
    int minCursorWidth = 2; // the minimum width in pixels of the current file position on the bar
};

#endif // PROGRESSBAR_H
