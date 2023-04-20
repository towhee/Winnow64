#ifndef SUBJECT_H
#define SUBJECT_H

#include <QtWidgets>
#include <QObject>
#include "Main/global.h"
#include "opencv2/opencv.hpp"

class Subject: public QObject
{
    Q_OBJECT
public:
    Subject();
    void eyes(QImage &image, QList<QPointF> &pts);

};

#endif // SUBJECT_H
