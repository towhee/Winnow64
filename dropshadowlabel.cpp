#include "dropshadowlabel.h"
#include <QPainter>
#include <QDebug>

    DropShadowLabel::DropShadowLabel(QWidget *parent) : QLabel(parent)
    {

    }

    void DropShadowLabel::paintEvent(QPaintEvent *event)
    {
        QPainter painter(this);

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QRect rect1(1,1,this->width()+100,this->height()-2);
        QRect rect2(3,3,this->width()+100,this->height()-2);
        painter.setPen(Qt::black);
        painter.drawText(rect2, text());
        painter.setPen(Qt::white);
        painter.drawText(rect1, text());
    }

