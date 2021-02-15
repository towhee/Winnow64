#include "exiftool.h"

ExifTool::ExifTool()
{
//    QUrl etUrl(qApp->applicationDirPath() + "/et.exe");
//    exifToolPath = etUrl.path();
    exifToolPath = qApp->applicationDirPath() + "/et.exe";
}

int ExifTool::execute(QStringList &args)
{
    /* all args that are a path to an image should be converted to a url
       ie  QUrl("D:/Pictures/Zenfolio/2021-02-12_0006.jpg").path();  */
    Utilities::log(__FUNCTION__, exifToolPath);
    qDebug() << __FUNCTION__ << args;
    return QProcess::execute(exifToolPath, args);
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
        args += dst.at(i) + "\n";       // dst = path of destination file
        args += "-execute\n";
    }
    // time to quit
    args += "-stay_open\n";
    args += "False\n";

    QProcess et;
    et.start(exifToolPath, stayOpen);   // exifToolPath = path to ExifTool.exe or ExifTool.app
    et.waitForStarted(3000);
    // stdin
    et.write(args);
    if (et.waitForFinished(30000)) qDebug() << __FUNCTION__ << "ExifTool exit code =" << et.exitCode();
    else qDebug() << __FUNCTION__ << "et.waitForFinished failed";

    return et.exitCode();
}

