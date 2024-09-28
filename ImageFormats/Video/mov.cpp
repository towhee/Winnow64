#include "mov.h"

MOV::MOV()
{
    isDebug = true;
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

bool MOV::findAtom(QFile &file, const char *targetAtomType, quint32 &atomSize) {
    char type[4];
    quint32 size;
    while (readAtomHeader(file, size, type)) {
        qDebug() << "Mov::findAtom  type:" << type;
        if (strncmp(type, targetAtomType, 4) == 0) {
            atomSize = size;
            return true;
        }
        file.seek(file.pos() + size - 8);  // Skip to next atom
    }
    return false;
}

QDateTime MOV::createDate(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file" << filePath;
        return QDateTime();
    }

    quint32 moovSize;

    if (!findAtom(file, "moov", moovSize)) {
        qWarning() << "moov atom not found!";
        return QDateTime();
    }

    // Look for mvhd atom inside moov
    quint32 mvhdSize;
    if (!findAtom(file, "mvhd", mvhdSize)) {
        qWarning() << "mvhd atom not found!";
        return QDateTime();
    }

    // Seek to creation time (offset 12 bytes from start of mvhd atom)
    file.seek(file.pos() + 4);
    // file.seek(file.pos() + 12);
    quint32 creationTime = readUInt32(file);

    // Convert to QDateTime
    QDateTime baseDate(QDate(1904, 1, 1), QTime(0, 0), Qt::UTC);
    QDateTime createDate = baseDate.addSecs(creationTime);

    qDebug() << "Mov::extractCreateDate" << createDate;

    return createDate;
}
