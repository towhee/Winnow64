#include "Main/mainwindow.h"

void MW::openLog()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    qDebug() << __PRETTY_FUNCTION__ << path;
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
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    if (G::logFile.isOpen()) G::logFile.close();
}

void MW::clearLog()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    if (!G::logFile.isOpen()) openLog();
    G::logFile.resize(0);
}

void MW::openErrLog()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    QDir dir(path);
    if (!dir.exists()) dir.mkdir(path);
    if (G::errlogFile.isOpen()) G::errlogFile.close();
    QString fPath = path + "/WinnowErrorLog.txt";
//    qDebug() << __PRETTY_FUNCTION__ << fPath;
    G::errlogFile.setFileName(fPath);
    // erase content if over one week since last modified
    QFileInfo info(G::errlogFile);
    QDateTime lastModified = info.lastModified();
    QDateTime oneWeekAgo = QDateTime::currentDateTime().addDays(-7);
    if (lastModified < oneWeekAgo) clearErrLog();
    if (G::errlogFile.open(QIODevice::ReadWrite)) {
//        QString errString(G::errlogFile.readAll());
//        qDebug() << __PRETTY_FUNCTION__ << errString;
    }
}

void MW::closeErrLog()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    if (G::errlogFile.isOpen()) G::errlogFile.close();
}

void MW::clearErrLog()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    if (!G::errlogFile.isOpen()) openLog();
    G::errlogFile.resize(0);
}

