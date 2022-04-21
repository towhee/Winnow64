#include "Utilities/dropshadowlabel.h"
#include <QPainter>
#include <QDebug>

DropShadowLabel::DropShadowLabel(QWidget *parent) : QLabel(parent)
{
//    QFont font;
//    font.setBold(true);
//    font.setFamily("Monaco");
//    font.setPixelSize(24);
//    setFont(font);

    setFont(QFont("Tahoma", 50));
}

void DropShadowLabel::paintEvent(QPaintEvent* /*event*/)
{
    if (G::isLogger) G::log(__FUNCTION__); 

    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRect rect1(0, 0, this->width()*2 + 100, this->height()*2 + 20);
    QRect rect2(2, 2, this->width()*2 + 100, this->height()*2 + 20);

    QFont font = painter.font();
//    font.setPixelSize(16);
//    font.setBold(true);
    painter.setFont(font);

    painter.setPen(Qt::black);
    painter.drawText(rect2, text());
    painter.setPen(Qt::white);
    painter.drawText(rect1, text());
}

