#include "Metadata/ExifTool.h"

ExifTool::ExifTool()
{
    exifToolPath = qApp->applicationDirPath() + "/et.exe";
    QStringList startArgs;
    startArgs << "-stay_open";
    startArgs << "True";
    startArgs << "-@";
    startArgs << "-";
    startArgs << "-execute";
    process.start(exifToolPath, startArgs);   // exifToolPath = path to ExifTool.exe or ExifTool.app
    process.waitForStarted(3000);
}

int ExifTool::execute(QStringList &args)
{
    /* all args that are a path to an image should be converted to a url
       ie  QUrl("D:/Pictures/Zenfolio/2021-02-12_0006.jpg").path();  */
    Utilities::log(__FUNCTION__, exifToolPath);
    qDebug() << __FUNCTION__ << args;
    return QProcess::execute(exifToolPath, args);
}

void ExifTool::overwrite()
{
    isOverWrite = true;
}

int ExifTool::close()
{
    QByteArray closeArgs;
    closeArgs += "-stay_open\n";
    closeArgs += "False\n";
    process.write(closeArgs);
    if (process.waitForFinished(30000))
        qDebug() << __FUNCTION__ << "ExifTool exit code =" << process.exitCode();
    else qWarning("ExifTool::copyAll process.waitForFinished failed");

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

void ExifTool::addThumb(QString src, QString dst)
{
//    '-ThumbnailImage<=thumb.jpg' dst.jpg
    QByteArray args;
//    args += "-thumbnailimage<=:/thumb.jpg\n";
    args += "-thumbnailimage<=" + src.toUtf8() + "\n";
    if (isOverWrite) args += "-overwrite_original\n";
    args += dst.toUtf8() + "\n";
    args += "-execute\n";
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
        args += src.at(i) + "\n";       // src = path of source file
        args += "-all:all\n";
        args += "-icc_profile\n";
        args += dst.at(i) + "\n";       // dst = path of destination file
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
        qDebug() << __FUNCTION__ << "ExifTool exit code =" << process.exitCode();
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
qDebug() << __FUNCTION__ << args << started;
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
qDebug() << __FUNCTION__ << args;
bool success = et->waitForFinished(3000);
qDebug() << __FUNCTION__ << "success =" << success;
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
if (et.waitForFinished(3000)) qDebug() << __FUNCTION__ << "ExifTool exit code =" << et.exitCode();
else qDebug() << __FUNCTION__ << "et.waitForFinished failed";
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
if (et->waitForFinished(3000)) qDebug() << __FUNCTION__ << "ExifTool exit code =" << et->exitCode();
else qDebug() << __FUNCTION__ << "et.waitForFinished failed";
//    */
