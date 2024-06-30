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


// void IssueQueue::enqueue(const QString& msg) {
//     QMutexLocker locker(&mutex);
//     qDebug() << "IssueQueue::enqueue" << msg;
//     messages.push(msg);
// }

// QString IssueQueue::dequeue() {
//     QMutexLocker locker(&mutex);
//     if (!messages.empty()) {
//         QString error = messages.front();
//         messages.pop();
//         return error;
//     }
//     return QString();
// }

// bool IssueQueue::isEmpty() const {
//     QMutexLocker locker(&mutex);
//     return messages.empty();
// }

// IssueLog::IssueLog(PopUp *popUp, QObject* parent) : QThread(parent)
// {
//     // qDebug() << "IssueLog::IssueLog";

//     issueLogPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
//                    + "/Winnow/Log/WinnowErrorLog.txt";
//     // if (issueLogFile.isOpen()) issueLogFile.close();
//     if (!open()) {
//         qDebug() << "IssueLog::IssueLog" << "Warning: Failed to open issue log file." << issueLogPath;
//         QString popupMsg = "The issue log file is open, preventing Winnow from writing issues<br>"
//                            "to file.<p>";
//         QString exitMsg = "<h2>Press <font color=\"red\"><b>Esc</b></font> to continue</h2><p>";
//         popupMsg += exitMsg;
//         popUp->showPopup(popupMsg, 3000, true, 0.75, Qt::AlignLeft);
//     }
//     else {
//         qDebug() << "IssueLog::IssueLog trim(14);";
//         trim(14);
//         start();
//     }
// }

// IssueLog::~IssueLog() {}

// bool IssueLog::open()
// {
//     // qDebug() << "ErrorLog::open";
//     issueLogFile.setFileName(issueLogPath);
//     if (issueLogFile.open(QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text)) {
//         out.setDevice(&issueLogFile);
//         return true;
//     }
//     else {
//         return false;
//     }
// }

// void IssueLog::log(const QString& msg) {
//     // qDebug() << "ErrorLog::log" << msg;
//     if (isRunning()) issueQueue.enqueue(msg);
// }

// QString IssueLog::logText()
// {
//     if (!issueLogFile.isOpen()) return "IssueLog file was not open.";
//     qint64 pos = issueLogFile.pos();
//     issueLogFile.seek(0);
//     QString s = issueLogFile.readAll();
//     issueLogFile.seek(pos);
//     return s;
// }

// void IssueLog::trim(int daysToKeep)
// {
//     qDebug() << "IssueLog::trim";
//     if (issueLogFile.isOpen()) {
//         qDebug() << "IssueLog::trim issueLogFile.isOpen()";
//         QString tempFilePath = issueLogPath + "temp";
//         QFile temp(tempFilePath);
//         QTextStream in(&issueLogFile);
//         in.seek(0);
//         QString line;
//         QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
//         QDate date;
//         if (temp.open(QIODevice::ReadWrite)) {
//             qDebug() << "IssueLog::trim temp.isOpen()";
//             QTextStream out(&temp);
//             while (!in.atEnd()) {
//                 line = in.readLine() + "\n";
//                 date = QDate::fromString(line.left(10), "yyyy-MM-dd");
//                 // lines to keep

//                 int daysTo = date.daysTo(QDate::currentDate());
//                 bool okToKeep = date.daysTo(QDate::currentDate()) <= daysToKeep;
//                 // qDebug() << "IssueLog::trim"
//                 //          << line
//                 //          << "daysTo =" << daysTo
//                 //          << "daysToKeep" << daysToKeep
//                 //          << "okToKeep" << okToKeep
//                 //     ;

//                 if (date.daysTo(QDate::currentDate()) < daysToKeep) {
//                     out << line;
//                 }
//             }
//         }
//         issueLogFile.close();
//         issueLogFile.remove();
//         temp.close();
//         temp.rename(issueLogPath);
//         QFile::remove(tempFilePath);
//         open();
//     }
// }

// void IssueLog::stop() {
//     requestInterruption();      // Signal the thread to stop
//     wait();                     // Wait for the thread to finish
// }

// void IssueLog::run() {
//     // qDebug() << "IssueLog::run";
//     while (!isInterruptionRequested()) {
//         if (!issueQueue.isEmpty()) {
//             QString issue = issueQueue.dequeue();
//             if (!issue.isEmpty() && issueLogFile.isOpen()) {
//                 out << issue << '\n';
//                 out.flush();
//             }
//         }
//         // Sleep for a short duration to avoid busy waiting
//         msleep(2000);
//     }
// }

// CHATGPT WITH REVISIONS

void IssueQueue::enqueue(const QString& msg) {
    QMutexLocker locker(&mutex);
    qDebug() << "IssueQueue::enqueue" << msg;
    messages.push(msg);
}

QString IssueQueue::dequeue() {
    QMutexLocker locker(&mutex);
    if (!messages.empty()) {
        QString error = messages.front();
        messages.pop();
        return error;
    }
    return QString();
}

bool IssueQueue::isEmpty() const {
    QMutexLocker locker(&mutex);
    return messages.empty();
}

IssueLog::IssueLog(PopUp *popUp, QObject* parent) : QThread(parent)
{
    issueLogPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/Winnow/Log/WinnowErrorLog.txt";
    if (!open()) {
        // qDebug() << "IssueLog::IssueLog" << "Warning: Failed to open issue log file." << issueLogPath;
        QString popupMsg = "The issue log file is open, preventing Winnow from writing issues<br>"
                           "to file.<p>";
        QString exitMsg = "<h2>Press <font color=\"red\"><b>Esc</b></font> to continue</h2><p>";
        popupMsg += exitMsg;
        popUp->showPopup(popupMsg, 3000, true, 0.75, Qt::AlignLeft);
    } else {
        trim(14);
        start();
    }
}

IssueLog::~IssueLog() {
    stop(); // Ensure the thread is stopped on destruction
}

bool IssueLog::open() {
    issueLogFile.setFileName(issueLogPath);
    if (issueLogFile.open(QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text)) {
        out.setDevice(&issueLogFile);
        return true;
    }
    return false;
}

void IssueLog::log(const QString& msg) {
    if (isRunning()) {
        issueQueue.enqueue(msg);
    }
}

QString IssueLog::logText() {
    if (!issueLogFile.isOpen()) return "IssueLog file was not open.";
    qint64 pos = issueLogFile.pos();
    issueLogFile.seek(0);
    QString s = issueLogFile.readAll();
    issueLogFile.seek(pos);
    return s;
}

void IssueLog::trim(int daysToKeep) {
    if (issueLogFile.isOpen()) {
        QString tempFilePath = issueLogPath + "temp";
        QFile temp(tempFilePath);
        QTextStream in(&issueLogFile);
        in.seek(0);
        QString line;
        QDate currentDate = QDate::currentDate();
        if (temp.open(QIODevice::ReadWrite)) {
            QTextStream out(&temp);
            while (!in.atEnd()) {
                line = in.readLine() + "\n";
                QDate date = QDate::fromString(line.left(10), "yyyy-MM-dd");
                if (date.isValid() && date.daysTo(currentDate) <= daysToKeep) {
                    out << line;
                }
            }
            issueLogFile.close();
            issueLogFile.remove();
            temp.close();
            temp.rename(issueLogPath);
            open();
        }
    }
}

void IssueLog::stop() {
    requestInterruption();
    wait();
}

void IssueLog::run() {
    while (!isInterruptionRequested()) {
        if (!issueQueue.isEmpty()) {
            QString issue = issueQueue.dequeue();
            if (!issue.isEmpty() && issueLogFile.isOpen()) {
                out << issue << '\n';
                out.flush();
            }
        }
        msleep(2000);
    }
}
