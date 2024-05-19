#ifndef LOG_H
#define LOG_H

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


// Thread-safe queue for error messages
class ErrorQueue {
public:
    void enqueue(const QString& error);
    QString dequeue();
    bool isEmpty() const;

private:
    std::queue<QString> errors;
    mutable QMutex mutex;
};

// Error logging thread
class ErrorLog : public QThread
{
    Q_OBJECT
public:
    ErrorLog(QObject* parent = nullptr);
    ~ErrorLog() override;

    void log(const QString& msg);
    void stop();

protected:
    void run() override;

private:
    ErrorQueue errorQueue;
    QFile file;
    QTextStream out;
};

#endif // LOG_H
