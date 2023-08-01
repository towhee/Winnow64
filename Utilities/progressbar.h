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

    void clearImageCacheProgress();
    void setBackgroundColor(const QColor &bg);
    void updateImageCacheProgress(int fromItem, int toItem, int items, QLinearGradient gradient);
    void updateDoneItem(bool isDone, int item, int items, QColor doneColor);
    void updateCursor(int item, int items);
    void saveProgressState();
    void recoverProgressState();

    QColor progressCurrentColor = QColor(158,200,158);           // Light green
    QColor progressBgColor = QColor(150,150,150);                // Light gray
    QColor progressAppBgColor = QColor(85,85,85);                // Darker gray
    QColor progressAddFileInfoColor = QColor(85,100,115);        // Slate blue
    QColor progressAddMetadataColor = QColor(100,100,150);       // Purple
    QColor progressBuildFiltersColor = QColor(75,75,125);        // Darker purple
    QColor progressTargetColor = QColor(125,125,125);            // Gray
    QColor progressImageCacheColor = QColor(108,150,108);        // Green
    QColor progressMetadateCacheColor = QColor(Qt::red);         // Red

    QLinearGradient getGradient(QColor c1);
    QLinearGradient bgGradient;
    QLinearGradient imageCacheColorGradient;
    QLinearGradient currentColorGradient;
    QLinearGradient targetColorGradient;

public slots:
    void resetMetadataProgress();
    void updateMetadataCacheProgress(int item, int items);

private:
    QPixmap state;

    int counter = 0;
    int htOffset;           // the offset from the top of pnt to the progress bar
    int ht;                 // the height of the progress bar
    int minCursorWidth = 2; // the minimum width in pixels of the current file position on the bar
};

#endif // PROGRESSBAR_H
