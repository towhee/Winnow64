#include "Utilities/classificationlabel.h"
#include "Main/global.h"
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
    rejectColor = QColor(Qt::red);
    pickBackgroundColor = QColor(Qt::green);
    textColor = QColor(Qt::white);
    diameter = 20;
    isPick = true;
    borderPen.setWidth(3);
}

void ClassificationLabel::setRatingColorVisibility(bool showRatingAndColor)
{
    this->showRatingAndColor = showRatingAndColor;
}

void ClassificationLabel::setPick(bool isPick)
{
    this->isPick = isPick;
}

void ClassificationLabel::setReject(bool isReject)
{
    this->isReject = isReject;
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
//    refresh();
//    if (isVisible()) repaint();
}

void ClassificationLabel::refresh()
{
    if (isPick || isReject)
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
        QRect pickRect(1, 1, diameter-2, diameter-2);
//        QPen pickPen(pickColor, 3);
        borderPen.setColor(pickColor);
        painter.setPen(borderPen);
        painter.drawEllipse(pickRect);
    }

    if (isReject) {
        QRect pickRect(1, 1, diameter-2, diameter-2);
        borderPen.setColor(rejectColor);
        painter.setPen(borderPen);
        painter.drawEllipse(pickRect);
    }

    if (showRatingAndColor || isPick || isReject) {
        // draw the color class circle
        QRect colorRect(3, 3, diameter-6, diameter-6);
        painter.setBrush(backgroundColor);
        painter.setPen(backgroundColor);
        painter.drawEllipse(colorRect);
    }
    if (rating == "1" || rating == "2" || rating == "3" || rating == "4" || rating == "5") {
//    if (showRatingAndColor || isPick || isReject) {
        // rating text
        QRect textRect(0, -1, diameter, diameter);
        QPen ratingTextPen(textColor);
        QFont font = painter.font();
        if ((rating == "" && colorClass == "" && isPick) || (isPick && !showRatingAndColor)) {
            ratingTextPen.setColor(pickColor);
            setText("√");
        }
        painter.setPen(ratingTextPen);
        int fontHeight = static_cast<int>(diameter - 6);
        if (fontHeight < 6) fontHeight = 6;
        font.setPixelSize(fontHeight);
    //    font.setBold(true);
        painter.setFont(font);
        painter.drawText(textRect, Qt::AlignCenter, text());
    }
}
