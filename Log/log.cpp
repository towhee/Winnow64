#include "log.h";

// Logger temp until integrate
Logger::Logger()
{

}

void Logger::log(const QString functionName, const QString comment)
{
    QMutexLocker locker(&mutex);
    QString microSec = QString("%L1").arg(t.nsecsElapsed() / 1000);
    QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " ";
    QString e = microSec.rightJustified(11, ' ') + " ";
    QString f = prevFunctionName.leftJustified(51, ' ');
    QString c = prevComment;
    if (sendLogToConsole) {
        QString msg = e + f + c;
        qDebug().noquote() << msg;
    }
    else {
        QString msg = d + e + f + c + "\n";
        //if (logFile.isOpen()) logFile.write(msg.toUtf8());
    }
    prevFunctionName = functionName;
    prevComment = comment;
    t.restart();
}

void Logger::restart()
{
    t.restart();
}

void Logger::skipLine()
{
    qDebug();
}
// End Logger temp until integrate


void ErrorQueue::enqueue(const QString& error) {
    QMutexLocker locker(&mutex);
    errors.push(error);
}

QString ErrorQueue::dequeue() {
    QMutexLocker locker(&mutex);
    if (!errors.empty()) {
        QString error = errors.front();
        errors.pop();
        return error;
    }
    return QString();
}

bool ErrorQueue::isEmpty() const {
    QMutexLocker locker(&mutex);
    return errors.empty();
}

ErrorLoggerThread::ErrorLoggerThread(QObject* parent) : QThread(parent) {
    file.setFileName("error.log");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open error log file!";
    } else {
        out.setDevice(&file);
    }
}

ErrorLoggerThread::~ErrorLoggerThread() {
    if (file.isOpen()) {
        file.close();
    }
}

void ErrorLoggerThread::addError(const QString& error) {
    errorQueue.enqueue(error);
}

void ErrorLoggerThread::stop() {
    requestInterruption(); // Signal the thread to stop
    wait(); // Wait for the thread to finish
}

void ErrorLoggerThread::run() {
    while (!isInterruptionRequested()) {
        if (!errorQueue.isEmpty()) {
            QString error = errorQueue.dequeue();
            if (!error.isEmpty() && file.isOpen()) {
                // out << error << std::endl;
            }
        }
        // Sleep for a short duration to avoid busy waiting
        msleep(100);
    }
}
