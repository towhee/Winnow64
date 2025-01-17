#ifndef LOG_H
#define LOG_H

#include "popup.h"
#include <QtWidgets>
#include <QObject>
#include <iostream>

#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QMutex>
#include <queue>

// Logger temp until integrate

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
// End Logger temp until integrate


// Thread-safe queue for issue messages
class IssueQueue {
public:
    void enqueue(const QString& msg);
    QString dequeue();
    bool isEmpty() const;

private:
    std::queue<QString> messages;
    mutable QMutex mutex;
};

// Issue logging thread
class IssueLog : public QThread
{
    Q_OBJECT
public:
    IssueLog(QObject* parent = nullptr);
    ~IssueLog() override;

    void log(const QString& msg);
    QString logText();
    void stop();
    bool failedToOpen = false;

protected:
    void run() override;

private:
    bool open();
    void trim(int daysToKeep);
    IssueQueue issueQueue;
    QString issueLogPath;
    QFile issueLogFile;
    QTextStream out;
    // PopUp popUp;
};

#endif // LOG_H
