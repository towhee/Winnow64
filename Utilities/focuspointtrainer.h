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
    void focus(QString srcPath, float x, float y, QString type, QImage srcImage);

private:
    void appendToCSV(QString &type, float x, float y, QString &srcPath);
};

#endif // FOCUSPOINTTRAINER_H
