#ifndef BLUR_H
#define BLUR_H

//#include <QObject>
#include <QWidget>

//class Blur  : public QObject
//{
//    Q_OBJECT
class Blur
{
public:
    static QImage blur(QImage &src, double factor=50.0);
};

#endif // BLUR_H
