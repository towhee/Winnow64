#include "log.h"

// Logger temp until integrate
Logger::Logger()
{

}

void Logger::log(const QString functionName, const QString comment)
{
    QMutexLocker locker(&mutex);
    qint64 nsec = t.nsecsElapsed();
    QString microSec = "";
    // less than 10 seconds
    if (nsec < 10000000000)  microSec = QString("%L1").arg(nsec / 1000);
    QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " ";
    QString e = microSec.rightJustified(11, ' ') + " ";
    QString f = prevFunctionName.leftJustified(51, ' ');
    QString c = prevComment;
    if (sendLogToConsole) {
        QString msg = "";
        if (prevFunctionName != "") msg = e + f + c;
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

// CHATGPT WITH REVISIONS

void IssueQueue::enqueue(const QString& msg) {
    QMutexLocker locker(&mutex);
    // qDebug() << "IssueQueue::enqueue" << msg;
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

IssueLog::IssueLog(QObject* parent) : QThread(parent)
{
    issueLogPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/Log/WinnowIssueLog.txt";
    if (!open()) {
        failedToOpen = true;
        QString errMsg = "The issue log file failed to open, preventing Winnow from writing issues<br>"
                         "to file";
        qWarning() << "IssueLog::IssueLog" << errMsg;
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

    // Check if the file exists
    if (!QFile::exists(issueLogPath)) {
        QFile newFile(issueLogPath);
        if (!newFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // Failed to create the file
            qWarning() << "Failed to create file:" << issueLogPath
                       << "\nError:" << newFile.errorString();
            return false;
        }
        newFile.close();
    }

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
