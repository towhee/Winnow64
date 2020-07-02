#ifndef PATTERNDLG_H
#define PATTERNDLG_H

#include <QtWidgets>
#include <QDialog>

class PatternDlg : public QDialog
{
    Q_OBJECT

public:
    explicit PatternDlg(QWidget *parent, QPixmap &pm, QPixmap &tile ,QString &name);
    ~PatternDlg();

private slots:
    void findPattern();
    void save();
    void zoomIn();
    void zoomOut();

private:
    QPixmap &pm;
    QPixmap &tile;
    QString &name;
    QVBoxLayout *layout;
    QHBoxLayout *btnLayout;
    QFrame *btnFrame;
    QPushButton *saveBtn = new QPushButton("Save Tile and Exit");
    QPushButton *patternBtn = new QPushButton("Find Pattern");
    QPushButton *plusBtn = new QPushButton("+");
    QPushButton *minusBtn = new QPushButton("-");
    QGraphicsView *v;
    QGraphicsScene *scene;
    QGraphicsRectItem *imageRect;
    QGraphicsRectItem *patternRect;
    QMatrix matrix;
    qreal zoom;
    QLabel *zoomText;
};

#endif // PATTERNDLG_H
