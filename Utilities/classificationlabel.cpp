#include "Utilities/classificationlabel.h"
#include <QPainter>
#include <QDebug>

ClassificationLabel::ClassificationLabel(QWidget *parent)
    : QLabel(parent)
{
    setScaledContents(true);
    setAttribute(Qt::WA_TranslucentBackground);
    setAlignment(Qt::AlignCenter);

    // defaults
    pickColor = QColor(Qt::green);
    textColor = QColor(Qt::white);
    diameter = 20;
    isPick = true;
}

void ClassificationLabel::setRatingColorVisibility(bool showRatingAndColor)
{
    this->showRatingAndColor = showRatingAndColor;
}

void ClassificationLabel::setPick(bool isPick)
{
    this->isPick = isPick;
}

void ClassificationLabel::setRating(QString rating)
{
    this->rating = rating;
    setText(rating);
}

void ClassificationLabel::setColorClass(QString colorClass)
{
    this->colorClass = colorClass;
    textColor = Qt::white;
    backgroundColor = G::labelNoneColor;
    if (colorClass == "Red") backgroundColor = G::labelRedColor;
    if (colorClass == "Yellow") backgroundColor = G::labelYellowColor;
    if (colorClass == "Yellow") textColor = Qt::black;
    if (colorClass == "Green") backgroundColor = G::labelGreenColor;
    if (colorClass == "Blue") backgroundColor = G::labelBlueColor;
    if (colorClass == "Purple") backgroundColor = G::labelPurpleColor;
}

void ClassificationLabel::setDiameter(int diameter)
{
    this->diameter = diameter;
    setFixedSize(diameter, diameter);
    repaint();
}

void ClassificationLabel::refresh()
{
    if (isPick)
        setVisible(true);
    else {
        if ((G::labelColors.contains(colorClass) || G::ratings.contains(rating))
         && showRatingAndColor)
            setVisible(true);
        else
            setVisible(false);
    }
    if (!showRatingAndColor) {
        backgroundColor  = G::labelNoneColor;
    }
    if (isVisible()) repaint();
}

void ClassificationLabel::paintEvent(QPaintEvent *event)
{
    QVariant x = event->type();     // suppress compiler warning

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // draw the pick circle
    if (isPick) {
        QRect pickRect(0, 0, diameter, diameter);
        painter.setBrush(pickColor);
        painter.drawEllipse(pickRect);
    }

    if (showRatingAndColor || isPick) {
        // draw the color class circle
        QRect colorRect(2, 2, diameter-4, diameter-4);
        painter.setBrush(backgroundColor);
        painter.setPen(backgroundColor);
        painter.drawEllipse(colorRect);
    }
    if (showRatingAndColor || isPick) {
        // rating text
        QRect textRect(0, -1, diameter, diameter);
        QPen ratingTextPen(textColor);
        QFont font = painter.font();
        if ((rating == "" && colorClass == "" && isPick) || isPick && !showRatingAndColor) {
            ratingTextPen.setColor(pickColor);
//            font.setFamily("Webdings Regular");
//            setText(QChar(0x2713));           // checkmark unicode
            setText("√");
        }
        painter.setPen(ratingTextPen);
        int fontHeight = (int)diameter - 6;
        font.setPixelSize(fontHeight);
    //    font.setBold(true);
        painter.setFont(font);
        painter.drawText(textRect, Qt::AlignCenter, text());
    }
}
