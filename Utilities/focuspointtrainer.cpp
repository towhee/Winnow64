#include "focuspointtrainer.h"

FocusPointTrainer::FocusPointTrainer(QObject *parent)
    : QObject{parent}
{}

void FocusPointTrainer::focus(QString path, float x, float y, QString type, QImage image)
{
    qDebug() << "FocusPointTrainer::focus"
             << type
             << x
             << y
             << path
                ;
    QImage im = image.scaled(QSize(256,256),Qt::KeepAspectRatio, Qt::SmoothTransformation);

}
