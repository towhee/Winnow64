#ifndef EXIFTOOL_H
#define EXIFTOOL_H

#include <QtWidgets>

class ExifTool : public QObject
{
    Q_OBJECT

public:
    ExifTool();
    int execute(QStringList &args);
    /* -TagsFromFile src -all:all dst. excludeIcc adds --icc_profile:all, for a dst whose
       pixels are NOT in the source's colour space: the caller has already embedded the
       right profile and must not have the source's copied over it. */
    void copyAllTags(QString src, QString dst, bool excludeIcc = false);
    void copyICC(QString src, QString dst);
    QString readTag(QString fPath, QString tag);
    void writeTitle(QString dst, QString val);
    void writeXMP(QString dst, QString tag, QString val);
    void readXMP(QString dst, QString tag, QString &val);
    void addThumb(QString src, QString dst);
    void writeOrientation(QString src, QString orientation);
    int copyAll(QString src, QString dst);
    int copyAll(const QStringList &src, QStringList &dst);
    void stayOpen();
    void setOverWrite(bool overWrite);
    int close();

private:
    bool ensureRunning(const QString &where);

    QString exifToolPath;
    QProcess process;
    bool ready = false;
    bool isOverWrite = false;
    QBuffer result;
};

#endif // EXIFTOOL_H
