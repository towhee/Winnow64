#include "Metadata/ExifTool.h"

/*

*/

ExifTool::ExifTool()
{
    if (G::isFileLogger) Utilities::log("ExifTool::ExifTool", exifToolPath);
#ifdef Q_OS_WIN
    exifToolPath = qApp->applicationDirPath() + "/et.exe";
#endif
#ifdef Q_OS_MAC
    exifToolPath = qApp->applicationDirPath() + "/ExifTool/exiftool";
#endif
    // confirm exifToolPath exists
    if (!QFile(exifToolPath).exists())
        qWarning() << "WARNING" << "ExifTool::ExifTool" << exifToolPath << "is missing";
    QStringList startArgs;
    startArgs << "-stay_open";
    startArgs << "True";
    startArgs << "-@";
    startArgs << "-";
    startArgs << "-execute";
    process.start(exifToolPath, startArgs);   // exifToolPath = path to ExifTool.exe or ExifTool.app
    process.waitForStarted(3000);
//    qDebug() << "ExifTool::ExifTool" << process.errorString();
}

int ExifTool::execute(QStringList &args)
{
    /* all args that are a path to an image should be converted to a url
       ie  QUrl("D:/Pictures/Zenfolio/2021-02-12_0006.jpg").path();  */
    if (G::isFileLogger) Utilities::log("ExifTool::execute", exifToolPath);
    return QProcess::execute(exifToolPath, args);
}

void ExifTool::setOverWrite(bool overWrite)
{
    isOverWrite = overWrite;
}

int ExifTool::close()
{
    QByteArray closeArgs;
    closeArgs += "-stay_open\n";
    closeArgs += "False\n";
    process.write(closeArgs);
    if (!process.waitForFinished(30000)) {
        // qDebug() << "ExifTool::close" << "ExifTool exit code =" << process.exitCode();
        qWarning() << "WARNING" << "ExifTool::close"  << "process.waitForFinished failed";
        if (G::isFileLogger) Utilities::log("ExifTool::close", "process.waitForFinished failed");
        return -1;
    }

    return process.exitCode();
}

void ExifTool::copyAllTags(QString src, QString dst)
{
    QByteArray args;
    args += "-TagsFromFile\n";
    args += src.toUtf8() + "\n";
    args += "-all:all\n";
    if (isOverWrite) args += "-overwrite_original\n";
    args += dst.toUtf8() + "\n";
    args += "-execute\n";
    process.write(args);    
}

void ExifTool::copyICC(QString src, QString dst)
{
    QByteArray args;
    args += "-TagsFromFile\n";
    args += src.toUtf8() + "\n";
    args += "-icc_profile\n";
    if (isOverWrite) args += "-overwrite_original\n";
    args += dst.toUtf8() + "\n";
    args += "-execute\n";
    process.write(args);
}

// not used
void ExifTool::writeTitle(QString dst, QString val)
{
    QByteArray args;

}

void ExifTool::writeOrientation(QString src, QString orientation)
{
/*
    1 = Horizontal (normal)
    2 = Mirror horizontal
    3 = Rotate 180
    4 = Mirror vertical
    5 = Mirror horizontal and rotate 270 CW
    6 = Rotate 90 CW
    7 = Mirror horizontal and rotate 90 CW
    8 = Rotate 270 CW
*/
    QByteArray args;
    args.append("-orientation#=" + orientation.toUtf8() + "\n");
    if (isOverWrite) args += "-overwrite_original\n";
    args.append(src.toUtf8() + "\n");
    args.append("-execute\n");
    process.write(args);
}

// not used as ExifTool can only read results into a file, going to try using my own xmp.setItem
void ExifTool::readXMP(QString dst, QString tag, QString &val)
{
    QByteArray args;
    args.append("-" + tag.toUtf8() + "=" + val.toUtf8() + "\n");
//    args = args + "-" + tag + "=" + val + "\n");
    args.append(dst.toUtf8() + "\n");
    args.append("-execute\n");
    process.write(args);
}

void ExifTool::writeXMP(QString dst, QString tag, QString val)
{
    QByteArray args;
    args.append('-' + tag.toUtf8() + '=' + val.toUtf8() + '\n');
    args.append(dst.toUtf8() + "\n");
    args.append("-execute\n");
    process.write(args);
}

void ExifTool::addThumb(QString src, QString dst)
{
//    '-ThumbnailImage<=thumb.jpg' dst.jpg
    QByteArray args;
//    args += "-thumbnailimage<=:/thumb.jpg\n";
    args.append("-thumbnailimage<=" + src.toUtf8() + "\n");
    if (isOverWrite) args += "-overwrite_original\n";
    args.append(dst.toUtf8() + "\n");
    args.append("-execute\n");
    process.write(args);
    process.waitForBytesWritten(1000);
}

int ExifTool::copyAll(QString src, QString dst)
{
    QStringList args;
    args << "-TagsFromFile";
    args << src;
    args << "-all:all";
    args << dst;
    return execute(args);
}

int ExifTool::copyAll(const QStringList &src, QStringList &dst)
{
/*

*/
    // initial arguments to keep ExifTool open
    QStringList stayOpen;
    stayOpen << "-stay_open";
    stayOpen << "True";
    stayOpen << "-@";
    stayOpen << "-";

    // stdin stream with copy tags for each pair of files in the lists
    QByteArray args;
    for (int i = 0; i < src.length(); i++) {
        args += "-TagsFromFile\n";
        args += src.at(i).toUtf8() + "\n";       // src = path of source file
        args += "-all:all\n";
        args += "-icc_profile\n";
        args += dst.at(i).toUtf8() + "\n";       // dst = path of destination file
        args += "-execute\n";
    }
    // time to quit
    args += "-stay_open\n";
    args += "False\n";

    QProcess process;
    process.start(exifToolPath, stayOpen);   // exifToolPath = path to ExifTool.exe or ExifTool.app
    process.waitForStarted(3000);
    // stdin
    process.write(args);
    if (process.waitForFinished(30000))
        qDebug() << "ExifTool::copyAll" << "ExifTool exit code =" << process.exitCode();
    else qWarning("ExifTool::copyAll process.waitForFinished failed");

    return process.exitCode();
}

/* this works: Yes
QString exifToolPath = qApp->applicationDirPath() + "/et.exe";
QString src = "D:/Pictures/Zenfolio/2021-02-12_0006.jpg";
QString dst = "D:/Pictures/Zenfolio/pbase2048/2021-02-12_0006_Zen2048.JPG";
QStringList args;
args << "-TagsFromFile";
args << src;
args << "-all:all";
args << dst;
QProcess::execute(exifToolPath, args);
return;
//    */

/* this works
QString exifToolPath = qApp->applicationDirPath() + "/et.exe";
QProcess et;
QStringList args;
args << "-CreateDate";
args << "D:/Pictures/Zenfolio/2021-02-12_0006.jpg";
et.setArguments(args);
et.setProgram(exifToolPath);
bool started = et.startDetached();
qDebug() << "ExifTool::" << args << started;
return;
//    */

/* this works
QString exifToolPath = qApp->applicationDirPath() + "/et.exe";
QProcess *et = new QProcess;
QStringList args;
args += "-TagsFromFile";
args += "D:/Pictures/Zenfolio/2021-02-12_0006.jpg";
args += "-all:all";
args += "D:/Pictures/Zenfolio/pbase2048/2021-02-12_0006_Zen2048.jpg";
et->setArguments(args);
et->setProgram(exifToolPath);
et->setStandardOutputFile("D:/output.txt");
et->start();
qDebug() << "ExifTool::" << args;
bool success = et->waitForFinished(3000);
qDebug() << "ExifTool::" << "success =" << success;
delete et;
return;
//    */

/* This works
QString exifToolPath = qApp->applicationDirPath() + "/et.exe";
QProcess et;
QByteArray args;
args += "-TagsFromFile\n";
args += "D:/Pictures/Zenfolio/2021-02-12_0006.jpg\n";
args += "-all:all\n";
args += "D:/Pictures/Zenfolio/pbase2048/2021-02-12_0006_Zen2048.jpg\n";
args += "-execute\n";
args += "-stay_open\n";
args += "False\n";

QStringList stayOpen;
stayOpen << "-stay_open";
stayOpen << "True";
stayOpen << "-@";
stayOpen << "-";

//    et->setArguments(stayOpen);
//    et->setProgram(exifToolPath);
et.start(exifToolPath, stayOpen);
et.waitForStarted(3000);
et.write(args);
if (et.waitForFinished(3000)) qDebug() << "ExifTool::" << "ExifTool exit code =" << et.exitCode();
else qDebug() << "ExifTool::" << "et.waitForFinished failed";
//    */

    /* This works
QString exifToolPath = qApp->applicationDirPath() + "/et.exe";
QProcess *et = new QProcess;
QByteArray args;
args += "-TagsFromFile\n";
args += "D:/Pictures/Zenfolio/2021-02-12_0006.jpg\n";
args += "-all:all\n";
args += "D:/Pictures/Zenfolio/pbase2048/2021-02-12_0006_Zen2048.jpg\n";
args += "-execute\n";
args += "-stay_open\n";
args += "False\n";

QStringList stayOpen;
stayOpen << "-stay_open";
stayOpen << "True";
stayOpen << "-@";
stayOpen << "-";

//    et->setArguments(stayOpen);
//    et->setProgram(exifToolPath);
et->start(exifToolPath, stayOpen);
et->waitForStarted(3000);
et->write(args);
if (et->waitForFinished(3000)) qDebug() << "ExifTool::" << "ExifTool exit code =" << et->exitCode();
else qDebug() << "ExifTool::" << "et.waitForFinished failed";
//    */
