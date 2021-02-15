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
    int copyAll(QString src, QString dst);
    int copyAll(const QStringList &src, QStringList &dst);

private:
    QString exifToolPath;
};

#endif // EXIFTOOL_H
