#include "logger.h"

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
