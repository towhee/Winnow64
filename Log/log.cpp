#include "log.h"

/*
void MW::startLog()
{
    if (!G::isLogger) return;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    qDebug() << "MW::openLog" << path;
    QDir dir(path);
    if (!dir.exists()) dir.mkdir(path);
    if (G::logFile.isOpen()) G::logFile.close();
    QString fPath = path + "/WinnowLog.txt";
    G::logFile.setFileName(fPath);
    // erase content if over one week since last modified
    QFileInfo info(G::logFile);
    QDateTime lastModified = info.lastModified();
    QDateTime oneWeekAgo = QDateTime::currentDateTime().addDays(-7);
    if (lastModified < oneWeekAgo) clearLog();
    qDebug() << "MW::openLog" << fPath;
//    if (G::logFile.open(QIODevice::ReadWrite)) {
//        if (G::isLogger) G::log("MW::startLog");
////        G::logFile.readAll();
//    }
}

void MW::closeLog()
{
    if (G::isLogger) G::log("MW::closeLog");
    if (G::logFile.isOpen()) G::logFile.close();
}

void MW::clearLog()
{
    if (G::isLogger) G::log("MW::clearLog");
//    if (!G::logFile.isOpen()) startLog();
    G::logFile.resize(0);
}

void MW::startErrLog()
{
    if (G::isLogger) G::log("MW::openErrLog");
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    QDir dir(path);
    if (!dir.exists()) dir.mkdir(path);
    if (G::errlogFile.isOpen()) G::errlogFile.close();
    QString fPath = path + "/WinnowErrorLog.txt";
    G::errlogFile.setFileName(fPath);
    QFileInfo info(G::errlogFile);
    QDateTime lastModified = info.lastModified();
    QDateTime oneWeekAgo = QDateTime::currentDateTime().addDays(-7);
    trimErrLog(G::errlogFile, 3);
//    G::errlogFile.open(QIODevice::ReadWrite);
}

void MW::closeErrLog()
{
    if (G::isLogger) G::log("MW::closeErrLog");
    if (G::errlogFile.isOpen()) G::errlogFile.close();
}

void MW::trimErrLog(QFile &errorLog, int daysToKeep)
{
    QString errFilePath = QFileInfo(errorLog).filePath();
    QString tempFilePath = errFilePath + "temp";
    QFile temp(tempFilePath);
    if (G::errlogFile.open(QIODevice::ReadWrite)) {
        QTextStream in(&errorLog);
        QString line;
        QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
        QDate date;
        if (temp.open(QIODevice::ReadWrite)) {
            QTextStream out(&temp);
            while (!in.atEnd()) {
                line = in.readLine() + "\n";
                date = QDate::fromString(line.left(10), "yyyy-MM-dd");
                // lines to keep
                if (date.daysTo(QDate::currentDate()) < daysToKeep) {
                    out << line;
                }
            }
        }
    }
    errorLog.close();
    errorLog.remove();
    temp.close();
    temp.rename(errFilePath);
    QFile::remove(tempFilePath);
}

*/

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
     qDebug() << "ErrorQueue::enqueue" << error;
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

ErrorLog::ErrorLog(QObject* parent) : QThread(parent)
{
    QString fPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/Winnow/Log/WinnowErrorLog.txt";
    file.setFileName(fPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "ErrorLog::ErrorLog" << "Warning: Failed to open error log file." << fPath;
    }
    else {
        out.setDevice(&file);
    }
    // crashing
    // start();
}

ErrorLog::~ErrorLog() {
    if (file.isOpen()) {
        file.close();
    }
}

void ErrorLog::log(const QString& msg) {
    qDebug() << "ErrorLog::log" << msg;
    errorQueue.enqueue(msg);
}

void ErrorLog::stop() {
    requestInterruption(); // Signal the thread to stop
    wait(); // Wait for the thread to finish
}

void ErrorLog::run() {
    while (!isInterruptionRequested()) {
        if (!errorQueue.isEmpty()) {
            QString error = errorQueue.dequeue();
            if (!error.isEmpty() && file.isOpen()) {
                qDebug() << "ErrorLog::run out error" << error;
                out << error << '\n';
                out.flush();
            }
        }
        // Sleep for a short duration to avoid busy waiting
        msleep(1000);
    }
}
