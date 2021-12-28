#include "utilities.h"

bool Utilities::integrityCheck(const QString &path1, const QString &path2)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    int w = preferred.width();
    int h = preferred.height();
    int headerH = static_cast<int>(30 * G::actDevicePixelRatio);
    int screenW = static_cast<int>(G::displayVirtualHorizontalPixels * 0.95);
    int screenH = static_cast<int>(G::displayVirtualVerticalPixels * 0.95 - headerH);
    if (screenW < w) w = screenW;
    if (screenH < h) h = screenH;
    return QSize(w, h);
}

QString Utilities::getDrive(QString path)
{
/*
    Returns the drive portion of a file system path.  The path must use the delimiter /.
    The path must include the drive - the absolute path.
*/
    QString drivePath = "";
#ifdef Q_OS_WIN
    int firstSlash = path.indexOf("/");
    drivePath = path.left(firstSlash+1);
    if (firstSlash == -1) return "";
#endif
#ifdef Q_OS_MAC
    if (path.left(9) == "/Volumes/") {
        drivePath = path.left(path.indexOf("/", 10) /*+ 1*/);
    }
    else {
        if (path.at(0) == "/") drivePath = "/";
    }
#endif
    qDebug() << __FUNCTION__ << path << drivePath;
    return drivePath;
}

void Utilities::uniqueInList(QString &name, const QStringList &list, QString delimiter)
{
/*
    Checks to see if the name already exists in the list.  If so, the delimiter and a
    number are appended to the name, repeating until a unique name is found.

    Usage:  Utilities::uniqueInList(name, list);
*/
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__); 
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

QString Utilities::inputText(QString title, QString description,
                             QStringList doNotUse, QString old)
{
    QString input = old;
    InputDlg d(title, description, input, doNotUse);
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
        QString txt = t + "  " + function + "  " + msg + "\n";
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

QString Utilities::formatMemory(qulonglong bytes, int precision)
{
    qulonglong x = 1024;
    if (bytes == 0) return "0";
    if (bytes < x) return QString::number(bytes) + " bytes";
    if (bytes < x * 1024)
        return QString::number(static_cast<double>(bytes) / x, 'f', precision) + " KB";
    x *= 1024;
    if (bytes < (x * 1024))
        return QString::number(static_cast<double>(bytes) / x, 'f', precision) + " MB";
    x *= 1024;
    if (bytes < (x * 1024))
        return QString::number(static_cast<double>(bytes) / x, 'f', precision) + " GB";
    x *= 1024;
    if (bytes < (x * 1024))
        return QString::number(static_cast<double>(bytes) / x, 'f', precision) + " TB";
    return "More than TB";
}

QString Utilities::enquote(QString &s)
{
    return QChar('\"') + s + QChar('\"');
}

void Utilities::hideCursor()
{
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

QString Utilities::centeredRptHdr(QChar padChar, QString title)
{
    QString hdr;
    QTextStream rpt;
    rpt.setString(&hdr);
    int hdrWidth = 90;
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

// numbers: big endian = "MM" = 0x4D4D   little endian = "II" = 0x4949

int Utilities::get4_1st(QByteArray c)
{
    return static_cast<int>(c[0] >> 4);
}

int Utilities::get4_2nd(QByteArray c)
{
    return static_cast<int>(c[0]&0xF);
}

quint8 Utilities::get8(QByteArray c)
{
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
    if (isBigEnd) {
        // c might be longer than 2 bytes so use last two bytes if big endian
//        c = c.right(2);
//        qDebug() << __FUNCTION__ << c.toHex() << a << b << c[a] << c[b];
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
    uint byte = 1;
    int length = -1;
    while (byte) {
        byte = get8(file.read(1));
        length++;
    }
    if (length) {
        file.seek(offset);
        return(file.read(length));
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
        return(io.read(length));
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
    QDebug debug = qDebug();
    debug.noquote();
    debug << "\n";
//    s += "\n";
    for (int i = start; i < end; i++) {
        quint8 x = (0xff & (unsigned int)ba[i]);
//        s += QStringLiteral("%1").arg(x, 2, 16, QLatin1Char('0')).toUpper() + " ";
        debug << QStringLiteral("%1").arg(x, 2, 16, QLatin1Char('0')).toUpper();
//        if (cols > 0 && ++n % cols == 0) s += "\n";
        if (cols > 0 && ++n % cols == 0) debug << " " << n << "\n";
    }
    debug << "\n";
//    return s;
}
