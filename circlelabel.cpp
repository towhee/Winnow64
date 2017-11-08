#include "circlelabel.h"
#include <QPainter>
#include <QDebug>

CircleLabel::CircleLabel(QWidget *parent)
    : QLabel(parent)
{
    setFixedSize(18, 18);
    setScaledContents(true);
    setAttribute(Qt::WA_TranslucentBackground);
//    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setAlignment(Qt::AlignRight | Qt::AlignBottom);
//    setAlignment(Qt::AlignCenter | Qt::AlignHCenter);

    textColor = QColor(Qt::white);
}

void CircleLabel::setText(QString rating)
{
    text = rating;
    repaint();
}

void CircleLabel::setTextColor(QColor textColor)
{
    this->textColor = textColor;
    repaint();
}

void CircleLabel::setBackgroundColor(QColor labelColor)
{
    backgroundColor = labelColor;
    repaint();
}

void CircleLabel::setDiameter(int diameter)
{
    this->diameter = diameter;
    repaint();
}

void CircleLabel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRect rect(0, 0, 18, 18);
//    QRect rect((width() - diameter) / 2, (height() - diameter) / 2, diameter, diameter);
    painter.setBrush(backgroundColor);
//    painter.setBrush(QColor(255,0,0,128));

    painter.setPen(backgroundColor);
    painter.drawEllipse(rect);
    QPen ratingTextPen(textColor);
    ratingTextPen.setWidth(2);
    painter.setPen(ratingTextPen);
    QFont font = painter.font();
    font.setPixelSize(12);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, text);
}
