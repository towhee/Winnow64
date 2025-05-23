#include "utilities.h"
#include "Main/global.h"

bool Utilities::integrityCheck(const QString &path1, const QString &path2)
{
    if (G::isLogger) G::log("Utilities::integrityCheck");
    QFile file1(path1);
    file1.open(QIODevice::ReadOnly);
    QByteArray ba1 = file1.readAll();
    QFile file2(path2);
    file2.open(QIODevice::ReadOnly);
    QByteArray ba2 = file2.readAll();
    file1.close();
    file2.close();
    return ba1 == ba2;
}

QSize Utilities::fitScreen(QSize preferred)
{
    if (G::isLogger) G::log("Utilities::fitScreen");
    int w = preferred.width();
    int h = preferred.height();
    int headerH = static_cast<int>(30 * G::actDevicePixelRatio);
    int screenW = static_cast<int>(G::displayVirtualHorizontalPixels * 0.95);
    int screenH = static_cast<int>(G::displayVirtualVerticalPixels * 0.95 - headerH);
    if (screenW < w) w = screenW;
    if (screenH < h) h = screenH;
    return QSize(w, h);
}

QStringList Utilities::listFromPath(QString path)
{
    QStringList segments = path.split("/");
    if (segments.at(0) == "" && segments.count() > 0) segments.remove((0));
    return segments;
}

QString Utilities::getDriveName(QString path)
{
    QString name = "";
    QStringList segments = listFromPath(path);
    //qDebug() << "Utilities::getDriveName  path =" << path << "segments =" << segments;
    if (segments.count() == 0) return "";
    #ifdef Q_OS_WIN
        return segments.at(0);
    #endif
    #ifdef Q_OS_MAC
        if (segments.at(0) == "Volumes" && segments.count() >= 2) {
            return segments.at(1);
        }
    #endif    // 1st segment should be Volumes
    return "";
}

QString Utilities::getDrive(QString path)
{
/*
    Returns the drive portion of a file system path ie /Volumes/Untitled
    The path must use the delimiter /.
    The path must include the drive - the absolute path.
*/
    QString drivePath = "";
    if (path == "") return "";
#ifdef Q_OS_WIN
    int firstSlash = path.indexOf("/");
    drivePath = path.left(firstSlash+1);
    if (firstSlash == -1) return "";
#endif
#ifdef Q_OS_MAC
    if (path.left(9) == "/Volumes/") {
        drivePath = path.left(path.indexOf("/", 10));
    }
    else {
        if (path.at(0) == '/') drivePath = "/";
    }
#endif
//    qDebug() << "Utilities::getDrive" << path << drivePath;
    return drivePath;
}

QString Utilities::getFileName(QString srcPath)
{
    if (G::isLogger) G::log("Utilities::getFileName");
    QFileInfo fileInfo(srcPath);
    return fileInfo.fileName();
}

QString Utilities::getFileBase(QString srcPath)
{
    if (G::isLogger) G::log("Utilities::getFileName");
    QFileInfo fileInfo(srcPath);
    return fileInfo.baseName();
}

QString Utilities::getSuffix(QString srcPath)
{
    if (G::isLogger) G::log("Utilities::getFileName");
    QFileInfo fileInfo(srcPath);
    return fileInfo.suffix().toLower();
}

QString Utilities::getFolderPath(QString fPath)
{
    if (G::isLogger) G::log("Utilities::getFolderPath");
    return QFileInfo((fPath)).dir().absolutePath();
}

QString Utilities::assocXmpPath(QString fPath)
{
    if (G::isLogger) G::log("Utilities::assocXmpPath");
    return QFileInfo((fPath)).dir().absolutePath() + "/" + QFileInfo((fPath)).baseName() + ".xmp";
}

QString Utilities::replaceFileName(QString srcPath, QString newName)
{
    if (G::isLogger) G::log("Utilities::replaceFileName");
    QFileInfo fileInfo(srcPath);
    QString sourceFolderPath = fileInfo.absoluteDir().absolutePath();
    QString sourceBaseName = fileInfo.baseName();
    QString sourceSuffix = fileInfo.suffix();
    return sourceFolderPath + "/" + newName + "." + sourceSuffix;
}

QString Utilities::replaceSuffix(QString srcPath, QString newSuffix)
{
    if (G::isLogger) G::log("Utilities::replaceSuffix");
    QFileInfo fileInfo(srcPath);
    QString sourceFolderPath = fileInfo.absoluteDir().absolutePath();
    QString sourceBaseName = fileInfo.baseName();
    QString sourceExtension = fileInfo.suffix();
    return sourceFolderPath + "/" + sourceBaseName + "." + newSuffix;
}

QStringList Utilities::getSidecarPaths(QString srcPath)
{
    if (G::isLogger) G::log("Utilities::getSidecarPaths");
    QStringList extensions;
    extensions << ".xmp" << ".txt";
    QStringList sidecarPaths;
    QFileInfo fileInfo(srcPath);
    QString sourceFolderPath = fileInfo.absoluteDir().absolutePath();
    QString sourceBaseName = fileInfo.baseName();
    foreach (QString ext, extensions) {
        QString sideCarPath = sourceFolderPath + "/" + sourceBaseName + ext;
        if (QFile(sideCarPath).exists()) sidecarPaths << sideCarPath;
    }
    return sidecarPaths;
}

void Utilities::uniqueInList(QString &name, const QStringList &list, QString delimiter)
{
/*
    Checks to see if the name already exists in the list.  If so, the delimiter and a
    number are appended to the name, repeating until a unique name is found.

    Usage:  Utilities::uniqueInList(name, list);
*/
    if (G::isLogger) G::log("Utilities::uniqueInList");
    int count = 0;
    QString originalName = name;
    do {
        if (list.contains(name)) {
            name = originalName + delimiter + QString::number(++count);
        }
    } while (list.contains(name));
}

void Utilities::uniqueFolderPath(QString &path, QString delimiter)
{
/*
    Checks to see if the folder pointed to by path exists.  If so, the delimiter and a
    number are appended to the path, repeating until a unique name is found.

    Usage:  Utilities::uniqueFolder(dPath);
    if dPath = ".../zen2048" exists then dPath changed to ".../zen2048_1"
*/
    if (G::isLogger) G::log("Utilities::uniqueFolderPath");
    int count = 0;
    bool folderAlreadyExists = true;
    if (path.right(1) != "/") path += "/";
    QFileInfo info(path);
    QString folderPath = info.dir().absolutePath();
    do {
        QDir testFile(path);
        if (testFile.exists()) {
            path = folderPath + delimiter + QString::number(++count);
        }
        else folderAlreadyExists = false;
    } while (folderAlreadyExists);
}

void Utilities::uniqueFilePath(QString &path, QString delimiter)
{
/*
    Checks to see if the file pointed to by path exists.  If so, the delimiter and a
    number are appended to the path, repeating until a unique name is found.

    Usage:  Utilities::uniqueFolder(fPath);
    if fPath = ".../zen2048.json" exists then fPath changed to ".../zen2048_1.json"
*/
    if (G::isLogger) G::log("Utilities::uniqueFilePath");
    int count = 0;
    bool fileAlreadyExists = true;
    QFileInfo info(path);
    QString folderPath = info.dir().absolutePath() + "/";
    QString base = info.baseName() + delimiter;
    QString suffix = "." + info.suffix();
    do {
        QFile testFile(path);
        if (testFile.exists()) {
            path = folderPath + base + QString::number(++count) + suffix;
        }
        else fileAlreadyExists = false;
    } while (fileAlreadyExists);
}

bool Utilities::comboBoxContainsText(QComboBox* comboBox, const QString& text)
{
    for (int i = 0; i < comboBox->count(); ++i) {
        if (comboBox->itemText(i) == text) {
            return true;
        }
    }
    return false;
}

bool Utilities::clipboardHasUrls()
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData->urls().count()) return true;
    return false;
}

void Utilities::backup(QString fPath, QString subfolderName)
{
    QString fName = QFileInfo(fPath).fileName();
    QString backupFolder = QFileInfo(fPath).dir().path() + "/" + subfolderName;
    QString backupPath = backupFolder + "/" + fName;
    if (!QDir(backupFolder).exists()) QDir().mkpath(backupFolder);
    uniqueFilePath(backupPath);
    QFile::copy(fPath, backupPath);
}

QString Utilities::inputText(QString title, QString description,
                             QStringList doNotUse, QString old)
{
    QString input = old;
    InputDlg d(title, description, input, doNotUse);
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(d.winId());
    #endif
    d.exec();
    return input;
}

void Utilities::log(QString function, QString msg)
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    QDir dir(path);
    if (!dir.exists()) dir.mkdir(path);
    QFile fLog(path + "/WinnowLog.txt");
    if (fLog.isOpen()) fLog.close();
    if (fLog.open(QIODevice::ReadWrite)) {
        fLog.readAll();
        QString t = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString f = function;
        QString txt = t + "  " + f + "  " + msg + "\n";
        fLog.write(txt.toUtf8());
        fLog.close();
    }
}

void Utilities::setOpacity(QWidget *widget, qreal opacity)
{
    QGraphicsOpacityEffect * effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(opacity);
    widget->setGraphicsEffect(effect);
}

QString Utilities::formatMemory(qulonglong bytes, int precision, bool useBinary)
{
/*
    Binary is the default.
    • Use binary (1024) if you’re showing memory (RAM, image size, cache size).
    • Use SI (1000) if you’re showing disk space, drive capacities, or network speeds
*/
    // Define units and base depending on the format
    // G::log("Mac::formatMemory");
    QStringList units = useBinary
        ? QStringList{"bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"}
        : QStringList{"bytes", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};

    int base = useBinary ? 1024 : 1000;

    double size = static_cast<double>(bytes);
    int unitIndex = 0;

    while (size >= base && unitIndex < units.size() - 1) {
        size /= base;
        ++unitIndex;
    }

    if (unitIndex == 0) {
        return QString("%1 %2").arg(bytes).arg(units[unitIndex]);
    } else {
        return QString("%1 %2").arg(size, 0, 'f', precision).arg(units[unitIndex]);
    }
}

QString Utilities::enquote(QString &s)
{
    return QChar('\"') + s + QChar('\"');
}

void Utilities::hideCursor()
{
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

QString Utilities::centeredRptHdr(QChar padChar, QString title, int width)
{
    QString hdr;
    QTextStream rpt;
    rpt.setString(&hdr);
    int hdrWidth = width;
    int titleWidth = title.size() + 4;
    int padWidth = (hdrWidth - titleWidth) / 2;
    rpt.reset();
    rpt << "\n";
    rpt.setPadChar(padChar);
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << padChar;
    rpt.reset();
    rpt.setFieldWidth(titleWidth);
    rpt.setFieldAlignment(QTextStream::AlignCenter);
    rpt << title;
    rpt.setPadChar(padChar);
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << padChar;
    return hdr;
}

bool Utilities::isScreenValid(const QScreen *screen)
{
    foreach(QScreen *scrn, qApp->screens()) {
        // if (scrn->name() == screen->name()) return true;
        if (scrn == screen) return true;
    }
    return false;
}

const Qt::KeyboardModifiers Utilities::modifiersMask = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

bool Utilities::modifiers(Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers val)
{
/*
    modifiers = current system keyboard modifiers, which could include modifiers other than
    control, alt, shift and meta (ie keyboard etc).  We want to make sure that we only
    compare val (the comparitor) to the first 4 modifiers.
*/
    Qt::KeyboardModifiers masked = modifiers &= modifiersMask;
    return val == masked;
}

void Utilities::saveByteArrayAsFile(QString fPath, QByteArray &ba)
{
/*
This is useful to save a bytearray and then examine using HxD hex editor or Qt Creator
*/
    QFile f(fPath);
    f.open(QIODevice::WriteOnly);
    f.write(ba);
    f.close();
}

// Convert
QString Utilities::stringListToString(QStringList list)
{
    QString s;
    int n = list.size();
    for (int i = 0; i < n; i++) {
        if (i && i < n) s += ", ";
        s += list.at(i).trimmed();
    }
    return s;
}

QString Utilities::pixmapToString(QPixmap &pixmap)
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    pixmap.save(&buffer, "PNG");
    QString imgBase64 = QString::fromLatin1(byteArray.toBase64().data());
    QString imgHtml = "<img src=\"data:image/png;base64," + imgBase64 + "\"/>";
    return imgHtml;
}

QString Utilities::pngToString(QString fPath)
{
    // Load the pixmap from the file
    QPixmap pixmap(fPath);
    if (pixmap.isNull()) {
        return QString(); // Return an empty string if the image cannot be loaded
    }

    // Serialize the pixmap to a byte array
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    buffer.close();

    // Convert the byte array to a base64-encoded string
    QString base64String = QString::fromLatin1(byteArray.toBase64().data());
    // QString base64String = byteArray.toBase64();
    QString imgHtml = "<img src=\"data:image/png;base64," + base64String + "\"/>";
    return imgHtml;
}

// numbers: big endian = "MM" = 0x4D4D   little endian = "II" = 0x4949

int Utilities::get4_1st(QByteArray c)
{
    if (c == "") return 0;
    return static_cast<int>(c[0] >> 4);
}

int Utilities::get4_2nd(QByteArray c)
{
    if (c == "") return 0;
    return static_cast<int>(c[0]&0xF);
}

quint8 Utilities::get8(QByteArray c)
{
    if (c == "") return 0;
    return c[0]&0xFF;
}

QByteArray Utilities::put8(quint8 x)
{
    QByteArray c;
    c.resize(1);
    c[0] = static_cast<char>(x);
    return c;
}

quint16 Utilities::get16(QByteArray c, bool isBigEnd)
{
    if (c == "" || c.size() < 2) return 0;
    if (isBigEnd) {
        //qDebug() << "Utilities::get16" << c.toHex() << a << b << c[a] << c[b];
        quint16 x = c[0]&0xFF;
        x = static_cast<quint16>((x << 8) | (c[1]&0xFF));
        return x;
    }
    else {
        quint16 x = c[1]&0xFF;
        x = static_cast<quint16>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

QByteArray Utilities::put16(quint16 x, bool isBigEnd)
{
    QByteArray c;
    c.resize(2);
    if (isBigEnd) {
        c[0] = static_cast<char>(x >> 8);
        c[1] = static_cast<char>(x & 0x00FF);
    }
    else {
        c[0] = static_cast<char>(x & 0x00FF);
        c[1] = static_cast<char>(x >> 8);
    }
    return c;
}

quint32 Utilities::get24(QByteArray c, bool isBigEnd)
{
    if (c == "") return 0;
    if (isBigEnd) {
        quint32 x = c[0]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[2]&0xFF));
        return x;
    }
    else {
        quint32 x = c[2]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint32 Utilities::get32(QByteArray c, bool isBigEnd)
{
    if (c == "") return 0;
    if (isBigEnd) {
        quint32 x = c[0]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[3]&0xFF));
        return x;
    }
    else {
        quint32 x = c[3]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

QByteArray Utilities::put32(quint32 x, bool isBigEnd)
{
    QByteArray c;
    c.resize(4);
    if (isBigEnd) {
        c[0] = static_cast<char>(x >> 24);
        c[1] = static_cast<char>(x >> 16);
        c[2] = static_cast<char>(x >> 8);
        c[3] = static_cast<char>(x & 0x000000FF);
    }
    else {
        c[0] = static_cast<char>(x & 0x000000FF);
        c[1] = static_cast<char>(x >> 8);
        c[2] = static_cast<char>(x >> 16);
        c[3] = static_cast<char>(x >> 24);
    }
    return c;
}

quint64 Utilities::get40(QByteArray c, bool isBigEnd)
{
    if (c == "") return 0;
    if (isBigEnd) {
        quint64 x = c[0]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        return x;
    }
    else {
        quint64 x = c[4]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint64 Utilities::get48(QByteArray c, bool isBigEnd)
{
    if (c == "") return 0;
    if (isBigEnd) {
        quint64 x = c[0]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[5]&0xFF));
        return x;
    }
    else {
        quint64 x = c[5]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint64 Utilities::get64(QByteArray c, bool isBigEnd)
{
    if (c == "") return 0;
    if (isBigEnd) {
        quint64 x = c[0]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[5]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[6]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[7]&0xFF));
        return x;
    }
    else {
        quint64 x = c[7]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[6]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[5]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

template<typename T>    // QFile or QBuffer
double Utilities::getReal(T &io, quint32 offset, bool isBigEnd)
{
    /*
    In IFD type 5 = rational unsigned = real/float
    */
        io.seek(offset);
        quint32 a = get32(io.read(4), isBigEnd);
        quint32 b = get32(io.read(4), isBigEnd);
        if (b == 0) return 0;
        return static_cast<double>(a) / b;
}
template double Utilities::getReal<QFile>(QFile&, quint32 offset, bool isBigEnd);
template double Utilities::getReal<QBuffer>(QBuffer&, quint32 offset, bool isBigEnd);

template<typename T>    // QFile or QBuffer
double Utilities::getReal_s(T &io, quint32 offset, bool isBigEnd)
{
    /*
    In IFD type 10 = rational signed = real/float
    */
    io.seek(offset);
    // read first 32 bits and convert to unsigned int
    qint32 a = static_cast<int>(get32(io.read(4), isBigEnd));
    quint32 b = get32(io.read(4), isBigEnd);
    if (b == 0) return 0;
    return static_cast<double>(a) / b;
}
template double Utilities::getReal_s<QFile>(QFile&, quint32 offset, bool isBigEnd);
template double Utilities::getReal_s<QBuffer>(QBuffer&, quint32 offset, bool isBigEnd);



QString Utilities::getCString(QFile &file)
{
    quint32 offset = static_cast<quint32>(file.pos());
    /*
    qDebug() << "Utilities::getCString"
             << "offset =" << offset; //*/
    uint byte = 1;
    int length = -1;
    while (byte) {
        byte = get8(file.read(1));
        length++;
    }
    if (length) {
        file.seek(offset);
        QString s = file.read(length);
        // advance for null termination of string
        file.seek(file.pos() + 1);
        return(s);
    }
    else {
        file.seek(offset + 1);
        return "";
    }
}

template<typename T>    // QFile or QBuffer
QString Utilities::getString(T &io, quint32 offset, quint32 length)
{
/*
    In IFD type 2 = string
*/
    io.seek(offset);
//    return(io.read(length));
    QString s = io.read(length);
    QChar z = '\0';
    if (s.endsWith(z)) s.remove(z);
//    if (s.endsWith('\0')) s.remove('\0');
    return(s);
}
template QString Utilities::getString<QFile>(QFile&, quint32 offset, quint32 length);
template QString Utilities::getString<QBuffer>(QBuffer&, quint32 offset, quint32 length);

QByteArray Utilities::getByteArray(QFile &file, quint32 offset, quint32 length)
{
    file.seek(offset);
    return(file.read(length));
}

QBitArray Utilities::bytes2Bits(QByteArray bytes)
{
    QBitArray bits(bytes.count() * 8);
    for (int i = 0; i < bytes.count(); i++) {
        for (int b = 0; b < 8; b++) {
            bits.setBit(i * 8 + b, bytes.at(i) & (1 << (7 - b)));
        }
    }
    return bits;
}

int Utilities::integer(int x)
{
    return x;
}

quint32 Utilities::swapEndian32(quint32 x)
{
    QVector<quint32> b(4);
    b[0] = (x  & 0x000000FF) >> 0;
    b[1] = (x  & 0x0000FF00) >> 8;
    b[2] = (x  & 0x00FF0000) >> 16;
    b[3] = (x  & 0xFF000000) >> 24;
    b[0] <<= 24;
    b[1] <<= 16;
    b[2] <<= 8;
    b[3] <<= 0;
    return (b[0] | b[1] | b[2] | b[3]);
}

quint64 Utilities::qvariantBytes(QVariant data)
{
    qint64 bytes = 0;
    if (!data.isNull()) {
        switch (data.typeId()) { // Qt 6+ uses typeId(), Qt 5 uses type()
        case QMetaType::QString:
            bytes += data.toString().size() * sizeof(QChar);
            break;
        case QMetaType::QByteArray:
            bytes += data.toByteArray().size();
            break;
        case QMetaType::Int:
        case QMetaType::Bool:
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            bytes += sizeof(data);
            break;
        case QMetaType::QImage:
        {
            QImage img = data.value<QImage>();
            bytes += img.width() * img.height() * (img.depth() / 8);
        }
        break;
        case QMetaType::QIcon:
        {
            QIcon icon = data.value<QIcon>();
            QList<QSize> sizes = icon.availableSizes();
            for (const QSize &size : sizes) {
                int w = size.width();
                int h = size.height();
                bytes += w * h * 4; // Assuming 32-bit (RGBA) storage
            }
        }
        break;
        case QMetaType::QVariantList:
        {
            QVariantList list = data.toList();
            for (const QVariant &v : list) {
                bytes += sizeof(QVariant) + v.toByteArray().size();
            }
        }
        break;
        case QMetaType::QVariantMap:
        {
            QVariantMap map = data.toMap();
            for (auto it = map.begin(); it != map.end(); ++it) {
                bytes += it.key().size() * sizeof(QChar) + it.value().toByteArray().size();
            }
        }
        break;
        default:
            bytes += sizeof(QVariant); // Fallback approximation
        }
    }
    return bytes;
}

//void Utilities::bytes2Bitset32(QByteArray bytes, std::bitset<32> &bits)
//{
//    bits = get32(bytes);
//    for (auto i = 0; i < bytes.count(); i++) {
//        for (auto b = 0; b < 8; b++) {
//            bits[i * 8 + b] = bytes.at(i) & (1 << (7 - b));
//        }
//    }
//}

//uint Utilities::bits2Int(QBitArray bits, int length)
//{
//    uint x = 0;
//    for (int i = 0; i < length; i++) x = (x << 1 | bits.at(i));
//    return x;
//}

//uint Utilities::extractBits(std::bitset<32> &buf, uint n)
//{
//    return (buf >> (32 - n)).to_ulong();
//}

void Utilities::hexFromByteArray(QByteArray &ba, int cols, int start, int end)
{
    QString s;
    int n = 0;
    if (end <= 0) end = ba.length();
    qDebug().noquote();
    qDebug() << "\n";
//    s += "\n";
    for (int i = start; i < end; i++) {
        quint8 x = (0xff & (unsigned int)ba[i]);
//        s += QStringLiteral("%1").arg(x, 2, 16, QLatin1Char('0')).toUpper() + " ";
        qDebug() << QStringLiteral("%1").arg(x, 2, 16, QLatin1Char('0')).toUpper();
//        if (cols > 0 && ++n % cols == 0) s += "\n";
        if (cols > 0 && ++n % cols == 0) qDebug() << " " << n << "\n";
    }
    qDebug() << "\n";
//    return s;
}
