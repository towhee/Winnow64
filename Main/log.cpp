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
    if (G::logFile.open(QIODevice::ReadWrite)) {
        G::logFile.readAll();
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
//    qDebug() << "MW::openErrLog" << fPath;
    G::errlogFile.setFileName(fPath);
    // erase content if over one week since last modified
    QFileInfo info(G::errlogFile);
    QDateTime lastModified = info.lastModified();
    QDateTime oneWeekAgo = QDateTime::currentDateTime().addDays(-7);
    if (lastModified < oneWeekAgo) clearErrLog();
    if (G::errlogFile.open(QIODevice::ReadWrite)) {
//        QString errString(G::errlogFile.readAll());
//        qDebug() << "MW::openErrLog" << errString;
    }
}

void MW::closeErrLog()
{
    if (G::isLogger) G::log("MW::closeErrLog");
    if (G::errlogFile.isOpen()) G::errlogFile.close();
}

void MW::clearErrLog()
{
    if (G::isLogger) G::log("MW::clearErrLog");
    if (!G::errlogFile.isOpen()) openLog();
    G::errlogFile.resize(0);
}

