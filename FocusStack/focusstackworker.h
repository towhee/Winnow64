#ifndef FOCUSSTACKWORKER_H
#define FOCUSSTACKWORKER_H

#include <QObject>
#include <QFileInfo>
#include <QFile>
#include <QColor>
#include "FocusStack/focusstack.h"
#include "FocusStack/options.h"


class FocusStackWorker : public QObject
{
    Q_OBJECT
public:
    FocusStackWorker(QStringList selection)
        : m_selection(std::move(selection)) {}

signals:
    void updateStatus(bool keepBase, QString msg, QString src);
    void updateProgress(int value, int total, QColor color = Qt::blue);
    void clearProgress();
    void finished(bool success, QString outputPath, QString depthmapPath);

public slots:
    void process();


private:
    QStringList m_selection;
};

#endif // FOCUSSTACKWORKER_H
