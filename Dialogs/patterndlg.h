#ifndef PATTERNDLG_H
#define PATTERNDLG_H

#include <QtWidgets>
#include <QDialog>
#include "Utilities/utilities.h"

class PatternDlgView : public QGraphicsView
{
    Q_OBJECT

public:
    PatternDlgView(QPixmap &pm, QPixmap &tile, QWidget *parent = nullptr);
    void findPatternInSample(QPixmap &sample, QRect &sampleRect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void msg(QString txt);

private:
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QPixmap &pm;
    QPixmap &tile;
    QGraphicsRectItem *imageRect;
    QGraphicsRectItem *patternRect;
    QMatrix matrix;
    qreal zoom;
    QRubberBand *rubberBand;
    QPoint origin;

    void debugPxArray(QImage &i, int originX, int originY, int size);
};

class PatternDlg : public QDialog
{
    Q_OBJECT

public:
    explicit PatternDlg(QWidget *parent, QPixmap &pm);
    ~PatternDlg();
    QPixmap tile;

private slots:
    void updateMsg(QString txt);
    void save();
    void quit();

signals:
    void saveTile(QString name, QPixmap *tile);

private:
    QPixmap pm;
    QVBoxLayout *layout;
    QHBoxLayout *vLayout;
    QHBoxLayout *btnLayout;
    QHBoxLayout *msgLayout;
    QFrame *vFrame;
    QFrame *btnFrame;
    QFrame *msgFrame;
    QPushButton *saveBtn = new QPushButton("Save Tile");
    QPushButton *exitBtn = new QPushButton("Quit");
    PatternDlgView *v;
    QLabel *msg;
};

#endif // PATTERNDLG_H
