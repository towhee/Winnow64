#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QtWidgets>

//#include <QMutex>
//#include <QMutexLocker>

class Logger: public QObject
{
    Q_OBJECT
public:
    Logger();
    void log(const QString functionName = "", const QString comment = "");
    void restart();
    void skipLine();

private:
    QMutex mutex;
    QElapsedTimer t;
    bool sendLogToConsole = true;
    QString prevFunctionName = "";
    QString prevComment = "";
};

#endif // LOGGER_H
