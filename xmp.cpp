#include "xmp.h"

Xmp::Xmp(QFile &file, ulong &offset, ulong &nextOffset, QObject *parent) :  QObject(parent)
{
    ulong xmpLength = nextOffset - offset;
    file.seek(offset);
    xmpBa = file.read(xmpLength);
    xmpmetaStart = xmpBa.indexOf("<x:xmpmeta");
    xmpmetaEnd = xmpBa.indexOf("</x:xmpmeta", xmpmetaStart + 20) + 12;

    xmpPrefix = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"><rdf:Description rdf:about=\"\" xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">";
    xmpSuffix = "</rdf:Description></rdf:RDF></x:xmpmeta>";
}

void Xmp::set(QFile &file, ulong &offset, ulong &nextOffset, QByteArray &xmpBA)
{
    ulong xmpLength = nextOffset - offset;
    file.seek(offset);
    xmpBa = file.read(xmpLength);
    xmpmetaStart = xmpBa.indexOf("<x:xmpmeta");
    xmpmetaEnd = xmpBa.indexOf("</x:xmpmeta", xmpmetaStart + 20) + 12;

    xmpPrefix = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"><rdf:Description rdf:about=\"\" xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">";
    xmpSuffix = "</rdf:Description></rdf:RDF></x:xmpmeta>";
}

QString Xmp::metaAsString()
{
    int length = xmpmetaEnd - xmpmetaStart;
    return QTextCodec::codecForMib(106)->toUnicode(xmpBa.mid(xmpmetaStart, length));
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
        qDebug() << "Xmp::getItem  item =" << item
                 << "  value =" << value;
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

    // does xmp item exist already
    if (startPos == -1) {
        // not found, create new item
        QByteArray newItem;
        startPos = xmpBa.indexOf("rdf:Description");
        startPos = xmpBa.indexOf(">") + 1;
        if (schema = "xmp") {
            newItem.append("<");
            newItem.append(item);
            newItem.append(">");
            newItem.append(value);
            newItem.append("</");
            newItem.append(item);
            newItem.append(">");
            xmpBa.insert(startPos, newItem);
        }
        if (schema = "dc") {
            newItem.append("<");
            newItem.append(item);
            newItem.append("> <rdf:Alt> <rdf:li xml:lang=\"x-default\">");
            newItem.append(value);
            newItem.append("</rdf:li> </rdf:Alt> </");
            newItem.append(item);
            newItem.append(">");
            xmpBa.insert(startPos, newItem);
        }
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
        return true;
    }
    return false;
}
