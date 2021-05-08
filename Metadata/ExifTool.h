#ifndef EXIFTOOL_H
#define EXIFTOOL_H

#include <QtWidgets>
#include "Utilities/utilities.h"

class ExifTool : public QObject
{
    Q_OBJECT

public:
    ExifTool();
    int execute(QStringList &args);
    void copyAllTags(QString src, QString dst);
    void copyICC(QString src, QString dst);
    void addThumb(QString src, QString dst);
    int copyAll(QString src, QString dst);
    int copyAll(const QStringList &src, QStringList &dst);
    void stayOpen();
    void overwrite();
    int close();

private:
    QString exifToolPath;
    QProcess process;
    bool isOverWrite = false;
};

#endif // EXIFTOOL_H
