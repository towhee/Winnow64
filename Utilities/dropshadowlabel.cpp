#include "Utilities/dropshadowlabel.h"
#include <QPainter>
#include <QDebug>

DropShadowLabel::DropShadowLabel(QWidget *parent) : QLabel(parent)
{
    setFont(QFont("Tahoma", 50));
}

void DropShadowLabel::paintEvent(QPaintEvent* /*event*/)
{
    {
    #ifdef ISDEBUG
    qDebug() << "DropShadowLabel::paintEvent";
    #endif
    }

    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRect rect1(0, 0, this->width() + 100, this->height() + 20);
    QRect rect2(2, 2, this->width() + 100, this->height() + 20);

    painter.setPen(Qt::black);
    painter.drawText(rect2, text());
    painter.setPen(Qt::white);
    painter.drawText(rect1, text());
}

