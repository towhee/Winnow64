#include "Main/mainwindow.h"

void MW::openLog()
{
    if (G::isLogger) G::log("MW::openLog");
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
    if (G::logFile.open(QIODevice::ReadWrite)) {
//        G::logFile.readAll();
    }
}

void MW::closeLog()
{
    if (G::isLogger) G::log("MW::closeLog");
    if (G::logFile.isOpen()) G::logFile.close();
}

void MW::clearLog()
{
    if (G::isLogger) G::log("MW::clearLog");
    if (!G::logFile.isOpen()) openLog();
    G::logFile.resize(0);
}

void MW::openErrLog()
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
    G::errlogFile.open(QIODevice::ReadWrite);
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
