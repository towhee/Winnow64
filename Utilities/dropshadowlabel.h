#ifndef DROPSHADOWLABEL_H
#define DROPSHADOWLABEL_H

#include <QObject>
#include <QWidget>
#include <QLabel>

class DropShadowLabel : public QLabel
{
    Q_OBJECT

public:
    DropShadowLabel(QWidget *parent = 0);

protected:
    void paintEvent(QPaintEvent *event);
};

#endif // DROPSHADOWLABEL_H
