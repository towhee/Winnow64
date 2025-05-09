#include "focuspointtrainer.h"

FocusPointTrainer::FocusPointTrainer(QObject *parent)
    : QObject{parent}
{}

void FocusPointTrainer::focus(QImage image, float x, float y)
{
    qDebug() << "FocusPointTrainer::focus" << image.width() << image.height()
             << x << y;
}
