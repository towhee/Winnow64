#include "xmp.h"

Xmp::Xmp(QFile &file, ulong &offset, ulong &nextOffset, QObject *parent) :  QObject(parent)
{
    xmpSegmentOffset = offset;
    ulong xmpLength = nextOffset - offset;
    file.seek(offset);
    xmpBa = file.read(xmpLength);
    xmpmetaStart = xmpBa.indexOf("<x:xmpmeta");
    xmpmetaEnd = xmpBa.indexOf("</x:xmpmeta", xmpmetaStart + 20) + 12;
    xmpPacketEnd = xmpBa.indexOf("<?xpacket end", xmpmetaEnd);
    xmpmetaRoom = xmpPacketEnd - xmpmetaStart;
    xmpmetaOffset = xmpSegmentOffset + xmpmetaStart;

    int xmpmetaLength = xmpmetaEnd - xmpmetaStart;
    xmpBa = xmpBa.mid(xmpmetaStart, xmpmetaLength);
}

void Xmp::checkSchemas()
{
    bool isXmpSchema = true;
    bool isDcSchema = true;
    if (xmpBa.indexOf("xmlns:xmp", xmpmetaStart) == -1) isXmpSchema = false;
    if (xmpBa.indexOf("xmlns:dc", xmpmetaStart) == -1) isDcSchema = false;
    if (isXmpSchema && isDcSchema) return;
    QByteArray rdfDescription = "<rdf:Description";
    int len = rdfDescription.length();
    int pos = xmpBa.indexOf(rdfDescription, xmpmetaStart) + len;
    if (!isXmpSchema) xmpBa.insert(pos, xmpSchema);
    if (!isDcSchema) xmpBa.insert(pos, dcSchema);
}

int Xmp::schemaInsertPos(QByteArray schema)
{
    int pos;
    int schemaStart;
    assignmentMethod = "";

    // search string ie "<dc:"
    QByteArray s1 = "<" + schema + ":";
    QByteArray s2 = " " + schema + ":";

    // find the schema section
    if (schema == "xmp")
        schemaStart = xmpBa.indexOf("xmlns:xmp", xmpmetaStart);
    if (schema == "dc")
        schemaStart = xmpBa.indexOf("xmlns:dc", xmpmetaStart);

    // if already some schema data return pos of first one
    pos = xmpBa.indexOf(s1, schemaStart);
    if (pos > -1) {
        assignmentMethod = "brackets";
        return pos;
    }
    pos = xmpBa.indexOf(s2, schemaStart);
    if (pos > -1) {
        assignmentMethod = "equals";
        return pos;
    }

    // no existing schema data, insert at beginning of schema range
    pos = xmpBa.indexOf(">", schemaStart) + 1;
    assignmentMethod = "brackets";
    return pos;
}

bool Xmp::writeJPG(QFile &file, QByteArray &newBuf)
{
    file.seek(0);
    QByteArray buf = file.readAll();
    newBuf = buf;
    if (xmpBa.length() < xmpmetaRoom) {
        newBuf.replace(xmpmetaOffset, xmpBa.length(), xmpBa);
    }
    // not enough room, insert and update offset next segment
    else {

    }
    return true;
}

bool Xmp::writeSidecar(QFile &file, QByteArray &newBuf)
{
    newBuf = xmpBa;
    return true;
}

void Xmp::set(QFile &file, ulong &offset, ulong &nextOffset, QByteArray &xmpBA)
{
    ulong xmpLength = nextOffset - offset;
    file.seek(offset);
    xmpBa = file.read(xmpLength);
    xmpmetaStart = xmpBa.indexOf("<x:xmpmeta");
    xmpmetaEnd = xmpBa.indexOf("</x:xmpmeta", xmpmetaStart + 20) + 12;

}

QString Xmp::metaAsString()
{
//    int length = xmpmetaEnd - xmpmetaStart;
    return QTextCodec::codecForMib(106)->toUnicode(xmpBa);
//    return QTextCodec::codecForMib(106)->toUnicode(xmpBa.mid(xmpmetaStart, length));
}

QString Xmp::getItem(QByteArray item)
{
/*
items: Rating, Label, title
item case is important: title is legal, Title is illegal

xmp schema can have two formats:
    xmp:Rating="2"                  // from LR, Photoshop
    <xmp:Rating>0</xmp:Rating>      // from camera

dc schema:
    <dc:title> <rdf:Alt> <rdf:li xml:lang="x-default">Cormorant in California</rdf:li> </rdf:Alt> </dc:title>
*/
    QString schema;
    if (item == "Rating" || item == "Label") {
        item.prepend("xmp:");
        schema = "xmp";
    }
    if (item == "title") {
        item.prepend("dc:");
        schema = "dc";
    }
    int startPos = xmpBa.indexOf(item, xmpmetaStart);
    if (startPos == -1) return "";

    // item exists, check if schema is xmp or dc
    int endPos;
    bool foundItem = false;
    startPos += item.length();

    if (schema == "xmp") {
        if (xmpBa.at(startPos) == 0x3D) {                     // = '=' or QString::QString("=")
            if (xmpBa.at(++startPos) == 0x22) {               // = '"'
                endPos = xmpBa.indexOf("\"", ++startPos);
                foundItem = true;
            }
            else return "";
        }

        if (xmpBa.at(startPos) == 0x3E) {                     // = '>'
            endPos = xmpBa.indexOf("<", ++startPos);
            foundItem = true;
        }
    }

    if (schema == "dc") {
        QByteArray temp = xmpBa.mid(startPos, 100);
        startPos = xmpBa.indexOf("rdf:li", startPos);
        temp = xmpBa.mid(startPos, 100);
        startPos = xmpBa.indexOf(">", startPos) + 1;
        temp = xmpBa.mid(startPos, 100);
        endPos = xmpBa.indexOf("<", startPos);
        foundItem = true;
    }

    if (foundItem) {
        QByteArray result = xmpBa.mid(startPos, endPos - startPos);
        QString value = QTextCodec::codecForMib(106)->toUnicode(result);
//        qDebug() << "Xmp::getItem  item =" << item
//                 << "  value =" << value;
        return value;
    }
    return "";
}

bool Xmp::setItem(QByteArray item, QByteArray value)
{
/*
items: Rating, Label, title
item case is important: title is legal, Title is illegal

xmp schema can have two formats:
    xmp:Rating="2"                  // from LR, Photoshop
    <xmp:Rating>0</xmp:Rating>      // from camera

dc schema:
    <dc:title> <rdf:Alt> <rdf:li xml:lang="x-default">Cormorant in California</rdf:li> </rdf:Alt> </dc:title>
*/
    report();
    QByteArray schema;
    if (item == "Rating" || item == "Label") {
        item.prepend("xmp:");
        schema = "xmp";
    }
    if (item == "title") {
        item.prepend("dc:");
        schema = "dc";
    }

    // make sure schema exists in xmp
    checkSchemas();

    int startPos = xmpBa.indexOf(item, xmpmetaStart);

    // does xmp item exist already
    if (startPos == -1) {
        // not found, create new item
        QByteArray newItem;
        startPos = schemaInsertPos(schema);     //
        if (schema == "xmp") {
            if (assignmentMethod == "brackets") {
                // ie <xmp:Rating>3</xmp:Rating>
                newItem.append("<");
                newItem.append(item);
                newItem.append(">");
                newItem.append(value);
                newItem.append("</");
                newItem.append(item);
                newItem.append(">");
                xmpBa.insert(startPos, newItem);
            }
            if (assignmentMethod == "equals") {
                // ie xmp:Rating="3"
                newItem.append(item);
                newItem.append("=\"");
                newItem.append(value);
                newItem.append("\" ");
                xmpBa.insert(startPos, newItem);
            }
        }
        if (schema == "dc") {
            // ie <dc:title><rdf:Alt><rdf:li xml:lang="x-default">title</rdf:li></rdf:Alt></dc:title>
            newItem.append("<");
            newItem.append(item);
            newItem.append("> <rdf:Alt> <rdf:li xml:lang=\"x-default\">");
            newItem.append(value);
            newItem.append("</rdf:li> </rdf:Alt> </");
            newItem.append(item);
            newItem.append(">");
            qDebug() << "\nBefore set title:\n"
                     << metaAsString();
            xmpBa.insert(startPos, newItem);
            qDebug() << "\nAfter set title:\n"
                     << metaAsString();
        }
//        report();
        return true;
    }

    // item exists, replace item
    int endPos;
    bool foundItem = false;
    startPos += item.length();

    if (schema == "xmp") {
        if (xmpBa.at(startPos) == 0x3D) {                     // = '=' or QString::QString("=")
            if (xmpBa.at(++startPos) == 0x22) {               // = '"'
                endPos = xmpBa.indexOf("\"", ++startPos);
                foundItem = true;
            }
            else return "";
        }

        if (xmpBa.at(startPos) == 0x3E) {                     // = '>'
            endPos = xmpBa.indexOf("<", ++startPos);
            foundItem = true;
        }
    }

    if (schema == "dc") {
        QByteArray temp = xmpBa.mid(startPos, 100);
        startPos = xmpBa.indexOf("rdf:li", startPos);
        temp = xmpBa.mid(startPos, 100);
        startPos = xmpBa.indexOf(">", startPos) + 1;
        temp = xmpBa.mid(startPos, 100);
        endPos = xmpBa.indexOf("<", startPos);
        foundItem = true;
    }

    if (foundItem) {
        xmpBa.replace(startPos, endPos - startPos, value);
        report();
        return true;
    }
    return false;
}

void Xmp::report()
{

    QXmlQuery query;
    query.setQuery(metaAsString());

    // Set up the output device
    QByteArray outArray;
    QBuffer buffer(&outArray);
    buffer.open(QIODevice::ReadWrite);

    // format xmp
    QXmlFormatter formatter(query, &buffer);
    query.evaluateTo(&formatter);

    QString xmpString = QTextCodec::codecForMib(106)->toUnicode(outArray);

    QMessageBox msg;
    msg.setText(metaAsString());
//    msg.setText(xmpString);
    msg.exec();
}
