#ifndef FOCUSPOINTTRAINER_H
#define FOCUSPOINTTRAINER_H

#include <QtWidgets>

class FocusPointTrainer : public QObject
{
    Q_OBJECT
public:
    explicit FocusPointTrainer(QObject *parent = nullptr);

signals:

public slots:
    void focus(QImage image, float x, float y);

};

#endif // FOCUSPOINTTRAINER_H
