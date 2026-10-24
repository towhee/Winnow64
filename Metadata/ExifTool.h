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
       right profile and must not have the source's copied over it. upright sets
       Orientation = 1 IN THE SAME WRITE, for a dst whose pixels are already rotated. */
    void copyAllTags(QString src, QString dst, bool excludeIcc = false,
                     bool upright = false);
    void copyICC(QString src, QString dst);
    QString readTag(QString fPath, QString tag);
    void writeTitle(QString dst, QString val);
    void writeXMP(QString dst, QString tag, QString val);
    void readXMP(QString dst, QString tag, QString &val);
    void addThumb(QString src, QString dst);
    /*  Replace dst's EXIF thumbnail (IFD1) with thumbSrc and its PreviewImage, where it
        has one, with previewSrc, and WAIT for ExifTool's answer. True only when it
        reports the file updated; err gets its stderr otherwise. See the definition. */
    bool replaceEmbeddedImages(const QString &thumbSrc, const QString &previewSrc,
                               const QString &dst, QString &err);
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
