#include "mov.h"
#include "Main/global.h"
#include <QTimeZone>
#include <QtEndian>
#include <QFile>

bool MOV::isDebug = false;

MOV::MOV()
{
}

bool MOV::readAtomHeader(QFile &file, quint32 &size, char (&type)[4]) {
    if (file.read((char*)&size, 4) != 4) return false;
    if (file.read(type, 4) != 4) return false;
    size = qFromBigEndian(size);  // Convert to host byte order
    return true;
}

void MOV::readAtomTree(QFile &file, quint32 maxOffset, QList<Atom> &atomList) {
    while (file.pos() < maxOffset) {
        quint32 size;
        char type[4];

        // Read atom header
        if (!readAtomHeader(file, size, type)) {
            qWarning() << "Failed to read atom header!";
            break;
        }

        Atom atom;
        atom.type = QString::fromUtf8(type, 4);
        atom.size = size;

        qint64 atomStart = file.pos();
        qint64 atomEnd = atomStart + size - 8;

        // If atom has children, process them recursively
        if (atom.type == "moov" || atom.type == "trak" || atom.type == "mdia" || atom.type == "minf" ||
            atom.type == "stbl" || atom.type == "udta") {
            readAtomTree(file, atomEnd, atom.children);
        } else {
            file.seek(atomEnd);  // Skip to the end of this atom
        }

        atomList.append(atom);
    }
}

void MOV::printAtomTree(const QList<Atom> &atomList, const QString &indent) {
    for (const Atom &atom : atomList) {
        qDebug().noquote() << indent << atom.type << "(size:" << atom.size << ")";
        if (!atom.children.isEmpty()) {
            printAtomTree(atom.children, indent + "  ");
        }
    }
}

void MOV::walkAtomTree(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file" << filePath;
        return;
    }

    QList<Atom> atomTree;
    readAtomTree(file, file.size(), atomTree);  // Read entire atom tree
    printAtomTree(atomTree);  // Print the atom tree
}

quint32 MOV::readUInt32(QFile &file) {
    quint32 value;
    file.read((char*)&value, 4);
    return qFromBigEndian(value);  // Convert to host byte order
}

bool MOV::findAtom(QFile &file, const char *targetAtomType, quint32 &atomSize, qint64 limit) {
    // char type[4];
    // quint32 size;
    // while (readAtomHeader(file, size, type)) {
    //     // qDebug() << "Mov::findAtom  type:" << type;
    //     if (strncmp(type, targetAtomType, 4) == 0) {
    //         atomSize = size;
    //         return true;
    //     }
    //     file.seek(file.pos() + size - 8);  // Skip to next atom
    // }
    // return false;

    char type[4];
    quint32 size;
    qint64 fileSize = (limit > 0 ? limit : file.size());

    while (readAtomHeader(file, size, type)) {
        if (size < 8) {
            // qWarning() << "Invalid atom size" << size;
            return false;
        }

        if (strncmp(type, targetAtomType, 4) == 0) {
            atomSize = size;
            return true;
        }

        qint64 nextPos = file.pos() + (qint64)size - 8;
        if (nextPos > fileSize) {
            qWarning() << "Atom extends beyond EOF, aborting" << type << "size" << size;
            return false;
        }

        if (!file.seek(nextPos)) {
            qWarning() << "Failed to seek to next atom position";
            return false;
        }
    }

    return false;

}

QDateTime MOV::createDate(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file" << filePath;
        return QDateTime();
    }

    // --- Find the moov atom ---
    quint32 moovSize = 0;
    if (!findAtom(file, "moov", moovSize)) {
        qWarning() << "moov atom not found!";
        return QDateTime();
    }

    qint64 moovStart = file.pos();
    qint64 moovEnd   = moovStart + (qint64)moovSize - 8;

    // --- Search for mvhd within moov bounds ---
    quint32 mvhdSize = 0;
    if (!findAtom(file, "mvhd", mvhdSize, moovEnd)) {
        qWarning() << "mvhd atom not found!";
        return QDateTime();
    }

    qint64 mvhdStart = file.pos();

    // --- Read mvhd version to determine field sizes ---
    quint8 version = 0;
    if (file.read((char*)&version, 1) != 1) {
        qWarning() << "Failed to read mvhd version";
        return QDateTime();
    }

    // Skip 3 bytes of flags
    file.seek(file.pos() + 3);

    quint64 creationTime = 0;

    if (version == 0) {
        // 32-bit creation/modification times
        quint32 ctime32 = readUInt32(file);
        creationTime = ctime32;
    }
    else if (version == 1) {
        // 64-bit creation/modification times
        quint64 ctime64 = 0;
        file.read((char*)&ctime64, 8);
        creationTime = qFromBigEndian(ctime64);
    }
    else {
        qWarning() << "Unknown mvhd version" << version;
        return QDateTime();
    }

    // --- Convert to QDateTime ---
    QDateTime baseDate(QDate(1904, 1, 1), QTime(0, 0), QTimeZone::UTC);
    QDateTime createDate = baseDate.addSecs(static_cast<qint64>(creationTime));

    // Debug info
    if (isDebug)
        qDebug() << "MOV::createDate" << filePath << "=>" << createDate.toString(Qt::ISODate);

    return createDate;

}

namespace {
/*  One box header at the device's position: its type, and where its payload starts and
    ends. False at end of data or on a malformed header. */
bool readBox(QIODevice &dev, qint64 limit, QByteArray &type, qint64 &payload, qint64 &end)
{
    const qint64 start = dev.pos();
    if (limit - start < 8) return false;
    const QByteArray h = dev.read(8);
    if (h.size() != 8) return false;
    quint64 size = qFromBigEndian<quint32>(h.constData());
    type = h.mid(4, 4);
    payload = start + 8;
    if (size == 1) {                        // 64-bit largesize follows the type
        const QByteArray big = dev.read(8);
        if (big.size() != 8) return false;
        size = qFromBigEndian<quint64>(big.constData());
        payload = start + 16;
        if (size < 16) return false;
    }
    else if (size == 0) {                   // the box runs to the end
        size = quint64(limit - start);
    }
    else if (size < 8) {
        return false;
    }
    end = start + qint64(size);
    return end <= limit && end > start;
}

/*  Find the first box of this type between the device's position and limit, leaving the
    device at its payload. */
bool findBox(QIODevice &dev, qint64 limit, const char *want, qint64 &payload, qint64 &end)
{
    QByteArray type;
    while (readBox(dev, limit, type, payload, end)) {
        if (type == want) return dev.seek(payload);
        if (!dev.seek(end)) return false;
    }
    return false;
}
}   // namespace

qint64 MOV::durationMs(QIODevice &dev)
{
    if (!dev.isOpen() || !dev.seek(0)) return -1;
    const qint64 size = dev.size();
    qint64 payload = 0, end = 0;
    if (!findBox(dev, size, "moov", payload, end)) return -1;
    if (!findBox(dev, end, "mvhd", payload, end)) return -1;

    /*  mvhd: version (1), flags (3), then v0: ctime, mtime, timescale, duration (4 each);
        v1: ctime, mtime (8 each), timescale (4), duration (8). */
    const QByteArray v = dev.read(4);
    if (v.size() != 4) return -1;
    const int version = quint8(v.at(0));
    quint64 timescale = 0, duration = 0;
    if (version == 0) {
        const QByteArray b = dev.read(16);
        if (b.size() != 16) return -1;
        timescale = qFromBigEndian<quint32>(b.constData() + 8);
        duration = qFromBigEndian<quint32>(b.constData() + 12);
    }
    else if (version == 1) {
        const QByteArray b = dev.read(28);
        if (b.size() != 28) return -1;
        timescale = qFromBigEndian<quint32>(b.constData() + 16);
        duration = qFromBigEndian<quint64>(b.constData() + 20);
    }
    else {
        return -1;
    }
    /*  All-ones is "indeterminate" in ISO 14496-12; a zero timescale is no duration. */
    if (timescale == 0 || duration == 0
        || duration == (version == 0 ? 0xFFFFFFFFull : ~0ull)) return -1;
    return qint64(duration * 1000 / timescale);
}

qint64 MOV::durationMs(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return -1;
    return durationMs(file);
}
