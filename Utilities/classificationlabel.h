#ifndef CLASSIFICATIONLABEL_H
#define CLASSIFICATIONLABEL_H

#include <QtWidgets>
#include "Main/global.h"

class ClassificationLabel : public QLabel
{
    Q_OBJECT
public:
    ClassificationLabel(QWidget *parent = 0);
    void setDiameter(int diameter);
    void setColorClass(QString colorClass);
    void setRating(QString rating);
    void setPick(bool isPick);
    void setRatingColorVisibility(bool showRatingAndColor);
    void refresh();

protected:
    void paintEvent(QPaintEvent *event);

private:
    int diameter;
    bool showRatingAndColor;
    bool isPick;
    QString rating;
    QString colorClass;
    QColor pickColor;
    QColor textColor;
    QColor backgroundColor;
};

#endif // CLASSIFICATIONLABEL_H