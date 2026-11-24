#include "log.h"
#include <QSaveFile>

// global.h is heavy to include; this is the one call this file makes into it
namespace G {
void issue(QString type, QString msg, QString src, int sfRow, QString fPath);
}

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
        trim(14);       // a failure to reopen after it sets failedToOpen
        start();
    }
}

IssueLog::~IssueLog() {
    stop(); // Ensure the thread is stopped on destruction
}

bool IssueLog::open() {
    issueLogFile.setFileName(issueLogPath);

    // A fresh profile has no AppData/Log folder yet; QFile will not create it
    QDir().mkpath(QFileInfo(issueLogPath).absolutePath());

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
/*
    Keep the last daysToKeep days. The kept lines replace the log through QSaveFile, an
    atomic replace: the old log stays whole until the new one is complete, so a failure
    at any step leaves it as it was. It used to be deleted before the trimmed copy was
    renamed in, and a failed rename lost it. A line with no date continues the dated
    line above it (a multi-line message) and is kept or dropped with it.

    A failure is an issue like any other. Raised here, in the constructor, before this
    log is G::issueLog, it waits and is written after the session marker.
*/
    if (!issueLogFile.isOpen()) return;
    issueLogFile.seek(0);
    QTextStream in(&issueLogFile);
    const QDate today = QDate::currentDate();
    QString kept;
    bool keep = false;
    bool dropped = false;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QDate date = QDate::fromString(line.left(10), "yyyy-MM-dd");
        if (date.isValid()) keep = date.daysTo(today) <= daysToKeep;
        if (keep) kept += line + '\n';
        else dropped = true;
    }
    if (!dropped) return;                   // nothing to trim: the file is left alone

    issueLogFile.close();                   // Windows cannot replace an open file
    QSaveFile save(issueLogPath);
    bool ok = save.open(QIODevice::WriteOnly | QIODevice::Text);
    const QByteArray bytes = kept.toUtf8();
    if (ok) ok = save.write(bytes) == bytes.size();
    if (ok) ok = save.commit();
    if (!ok)
        G::issue("Warning", "The issue log could not be trimmed: " + save.errorString(),
                 "IssueLog::trim", -1, issueLogPath);
    if (!open()) failedToOpen = true;
}

void IssueLog::stop() {
    requestInterruption();
    wait();
}

void IssueLog::run() {
    /* Write everything queued on each wake. It used to write ONE issue per 2 s wake, so
       a burst (hundreds of warnings while a folder loads) took hours to reach the file,
       and whatever was still queued at quit was lost. The final drain after the loop
       catches issues logged during shutdown. */
    auto drain = [this] {
        bool wrote = false;
        while (!issueQueue.isEmpty()) {
            QString issue = issueQueue.dequeue();
            if (!issue.isEmpty() && issueLogFile.isOpen()) {
                out << issue << '\n';
                wrote = true;
            }
        }
        if (wrote) out.flush();
    };
    while (!isInterruptionRequested()) {
        drain();
        msleep(2000);
    }
    drain();
}
