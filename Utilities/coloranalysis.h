#ifndef COLORANALYSIS_H
#define COLORANALYSIS_H

#include <QtWidgets>

class ColorAnalysis : public QObject
{
    Q_OBJECT

public:
    ColorAnalysis(QObject *parent = nullptr);
    void process(QStringList &fPathList);

public slots:
    void abortHueReport();

private:
    bool abort = false;
};

#endif // COLORANALYSIS_H
