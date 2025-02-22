#ifndef CLASSIFICATIONLABEL_H
#define CLASSIFICATIONLABEL_H

#include <QtWidgets>

class ClassificationLabel : public QLabel
{
    Q_OBJECT
public:
    ClassificationLabel(QWidget *parent = nullptr);
    void setDiameter(int diameter);
    void setColorClass(QString colorClass);
    void setRating(QString rating);
    void setPick(bool isPick);
    void setReject(bool isReject);
    void setRatingColorVisibility(bool showRatingAndColor);
    void refresh();

protected:
    void paintEvent(QPaintEvent *event);

private:
    int diameter;
    bool showRatingAndColor;
    bool isPick;
    bool isReject;
    QString rating;
    QString colorClass;
    QColor pickColor;
    QColor rejectColor;
    QColor pickBackgroundColor;
    QColor textColor;
    QColor backgroundColor;
    QPen borderPen;
};

#endif // CLASSIFICATIONLABEL_H
