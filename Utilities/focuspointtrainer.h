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
    void focus(QString path, float x, float y, QString type, QImage image);

};

#endif // FOCUSPOINTTRAINER_H
