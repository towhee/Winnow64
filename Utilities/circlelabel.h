#ifndef CIRCLELABEL_H
#define CIRCLELABEL_H

#include <QtWidgets>
#include "global.h"

class CircleLabel : public QLabel
{
    Q_OBJECT
public:
    CircleLabel(QWidget *parent = 0);
    void setDiameter(int diameter);
    void setTextColor(QColor textColor);
    void setBackgroundColor(QColor labelColor);
    void setText(QString rating);

protected:
    void paintEvent(QPaintEvent *event);

private:
    int diameter;
    QColor textColor;
    QColor backgroundColor;
    QString text;
};

#endif // CIRCLELABEL_H
