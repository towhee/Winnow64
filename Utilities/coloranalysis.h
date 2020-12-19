#ifndef COLORANALYSIS_H
#define COLORANALYSIS_H

#include <QtWidgets>

class ColorAnalysis : public QObject
{
    Q_OBJECT

public:
    ColorAnalysis(QString &fPath, QObject *parent = nullptr);

private:
    QString &fPath;
};

#endif // COLORANALYSIS_H
