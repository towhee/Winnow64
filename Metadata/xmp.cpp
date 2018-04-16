#include "xmp.h"

/*
Xmp reads and writes xmp tags to a QByteArray buffer.  The buffer is read from
the image file, based on supplied ofsets.  If there is no xmp data in the image
file or the file format is not documented then the xmp tags are written to a
sidecar buffer.
*/

Xmp::Xmp(QFile &file, ulong &offset, ulong &nextOffset, bool useSidecar,
         QObject *parent) :  QObject(parent)
{
    // no file xmp data, use the sidecar skeleton
    if (useSidecar) {
        xmpBa = sidecarSkeleton;
        xmpmetaStart = 0;
        xmpmetaEnd = xmpBa.length();
    }
    else {
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
    QFileInfo info(file);
    fileType.append(info.suffix().toUpper().toLatin1());
    xmpSchemaList << "Rating" << "Label" << "ModifyDate" << "CreateDate";
    dcSchemaList << "title" << "rights" << "creator";
    auxSchemaList << "Lens" << "LensSerialNumber" << "SerialNumber";
    iptc4xmpCoreSchemaList << "CiUrlWork" << "CiEmailWork";

    schemaHash["Rating"] = "xmp";                   // read/write
    schemaHash["Label"] = "xmp";                    // read/write
    schemaHash["ModifyDate"] = "xmp";               // write only
    schemaHash["CreateDate"] = "xmp";               // read only
    schemaHash["ModifyDate"] = "xmp";               // write only
    schemaHash["title"] = "dc";                     // read/write
    schemaHash["rights"] = "dc";                    // read/write
    schemaHash["creator"] = "dc";                   // read/write
    schemaHash["Lens"] = "aux";                     // read only
    schemaHash["LensSerialNumber"] = "aux";         // read only
    schemaHash["SerialNumber"] = "aux";             // read only
    schemaHash["CiEmailWork"] = "Iptc4xmpCore";     // read/write
    schemaHash["CiUrlWork"] = "Iptc4xmpCore";       // read/write
    schemaHash["Orientation"] = "tiff";             // write (sidecars only)
}

void Xmp::insertSchemas(QByteArray &item)
{
    QByteArray schema = schemaHash[item];
    QByteArray search = schema;
    search.prepend("xmlns:");

    // already exist
    if (xmpBa.indexOf(search, xmpmetaStart) != -1) return;

    QByteArray rdfDescription = "<rdf:Description ";
    int len = rdfDescription.length();
    int pos = xmpBa.indexOf(rdfDescription, xmpmetaStart) + len;
    if (schema == "xmp") xmpBa.insert(pos, xmpSchema);
    if (schema == "dc") xmpBa.insert(pos, dcSchema);
    if (schema == "tiff") xmpBa.insert(pos, tifSchema);
    if (item == "Orientation") xmpBa.insert(pos, psSchema);
}

int Xmp::schemaInsertPos(QByteArray schema)
{
    int pos;
    int schemaStart;
    assignmentMethod = "";

    // search string ie "<dc:"
    QByteArray s1 = "\n\t\t\t<" + schema + ":";
    QByteArray s2 = "\n\t\t\t " + schema + ":";
    QByteArray s3 = "<" + schema + ":";
    QByteArray s4 = " " + schema + ":";

    // find the schema section
    if (schema == "xmp")
        schemaStart = xmpBa.indexOf("xmlns:xmp", xmpmetaStart);
    if (schema == "dc")
        schemaStart = xmpBa.indexOf("xmlns:dc", xmpmetaStart);
    if (schema == "tiff")
        schemaStart = xmpBa.indexOf("xmlns:tiff", xmpmetaStart);

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
    pos = xmpBa.indexOf(s3, schemaStart);
    if (pos > -1) {
        assignmentMethod = "brackets";
        return pos;
    }
    pos = xmpBa.indexOf(s4, schemaStart);
    if (pos > -1) {
        assignmentMethod = "equals";
        return pos;
    }

    // no existing schema data, insert at beginning of schema range
    pos = xmpBa.indexOf(">", schemaStart) + 1;
    assignmentMethod = "brackets";
    return pos;
}

bool Xmp::writeJPG(QByteArray &buffer)
{
    if (xmpBa.length() < xmpmetaRoom) {
        buffer.replace(xmpmetaOffset, xmpBa.length(), xmpBa);
    }
    // not enough room, insert and update offset next segment
    else {

    }
    return true;
}

bool Xmp::writeSidecar(QByteArray &buffer)
{
    buffer = xmpBa;

//    qDebug() << G::t.restart() << "\t" << "Xmp::writeSidecar: " << buffer;

    return true;
}

QString Xmp::metaAsString()
{
    return QTextCodec::codecForMib(106)->toUnicode(xmpBa);
}

QString Xmp::getItem(QByteArray item)
{
/*
items: Rating, Label, title ...
item case is important: title is legal, Title is illegal

schemas: xmp, dc, aux ...

xmp schema can have two formats:
    xmp:Rating="2"                  // from LR, Photoshop
    <xmp:Rating>0</xmp:Rating>      // from camera

dc schema:
    <dc:title> <rdf:Alt> <rdf:li xml:lang="x-default">Cormorant in California</rdf:li> </rdf:Alt> </dc:title>

xmp:ModifyDate="2017-12-21T16:51:02-08:00"
*/
    int startPos;
    QByteArray searchItem;
    QByteArray schema = schemaHash[item];

    QByteArray tag = schema;
    tag.append(":");
    tag.append(item);

    searchItem = tag;
    searchItem.append("=\"");
    startPos = xmpBa.indexOf(searchItem, xmpmetaStart);
    if (startPos == -1) {
        searchItem = tag;
        searchItem.append(">");
        startPos = xmpBa.indexOf(searchItem, xmpmetaStart);
    }

    if (startPos == -1) return "";

    // tag exists, check if schema is xmp or dc
    int endPos;
    bool foundItem = false;
    startPos += searchItem.length() - 1;
    QByteArray temp = xmpBa.mid(startPos, 100);

    if (schema == "dc") {
        startPos = xmpBa.indexOf("rdf:li", startPos);
        startPos = xmpBa.indexOf(">", startPos) + 1;
        endPos = xmpBa.indexOf("<", startPos);
        foundItem = true;
    }
    else {
        if (xmpBa.at(startPos) == 0x22) {                   // = '"'
            endPos = xmpBa.indexOf("\"", ++startPos);
            foundItem = true;
        }
        if (xmpBa.at(startPos) == 0x3E) {                   // = '>'
            endPos = xmpBa.indexOf("<", ++startPos);
            foundItem = true;
        }
    }

    if (foundItem) {
        QByteArray result = xmpBa.mid(startPos, endPos - startPos);
        QString value = QTextCodec::codecForMib(106)->toUnicode(result);
        return value;
    }
    return "";
}

bool Xmp::setItem(QByteArray item, QByteArray value)
{
/*
items: Rating, Label, title, Orientation ...
item case is important: title is legal, Title is illegal

xmp schema can have two formats:
    xmp:Rating="2"                  // from LR, Photoshop
    <xmp:Rating>0</xmp:Rating>      // from camera

dc schema for title:
    <dc:title> <rdf:Alt> <rdf:li xml:lang="x-default">Cormorant in California</rdf:li> </rdf:Alt> </dc:title>
*/
    // ie schema = "Rating"
    QByteArray schema = schemaHash[item];

    // ie "xmp:Rating"
    QByteArray tag = schema;
    tag.append(":");
    tag.append(item);

    // make sure schema exists in xmp
    insertSchemas(item);

//    qDebug() << G::t.restart() << "\t" << "Xmp::setItem  item =" << item
//             << "schema =" << schema
//             << "tag =" << tag;

    int startPos = xmpBa.indexOf(tag, xmpmetaStart);

    // does item exist already
    if (startPos == -1) {
        // not found, create new item
        QByteArray newItem;
        newItem.clear();
        startPos = schemaInsertPos(schema);     // determines assignmentMethod
        if (schema == "xmp" || schema == "tiff" || schema == "Iptc4xmpCore" || schema == "aux") {
            if (assignmentMethod == "brackets") {
                // ie <xmp:Rating>3</xmp:Rating>
                newItem = "\n\t\t\t<";
                newItem.append(tag);
                newItem.append(">");
                newItem.append(value);
                newItem.append("</");
                newItem.append(tag);
                newItem.append(">");
                xmpBa.insert(startPos, newItem);
            }
            if (assignmentMethod == "equals") {
                // ie xmp:Rating="3"
                newItem.append("\n\t\t\t");
                newItem.append(tag);
                newItem.append("=\"");
                newItem.append(value);
                newItem.append("\" ");
                xmpBa.insert(startPos, newItem);
            }
            // add sidecar extension if writing orientation
            if (item == "Orientation") {
                newItem.clear();
                newItem.append("\n\t\t\t");
                newItem.append(sidecarExtension);
                newItem.replace("XXX", fileType);
                xmpBa.insert(startPos, newItem);
            }
        }
        if (item == "title" || item == "rights") {
            // ie <dc:title><rdf:Alt><rdf:li xml:lang="x-default">This is the title</rdf:li></rdf:Alt></dc:title>
            newItem.append("\n\t\t\t<");
            newItem.append(tag);
            newItem.append("><rdf:Alt><rdf:li xml:lang=\"x-default\">");
            newItem.append(value);
            newItem.append("</rdf:li></rdf:Alt></");
            newItem.append(tag);
            newItem.append(">");
//            qDebug() << G::t.restart() << "\t" << "\nBefore set title:\n"
//                     << metaAsString();
            xmpBa.insert(startPos, newItem);
//            qDebug() << G::t.restart() << "\t" << "\nAfter set title:\n"
//                     << metaAsString();
        }
        if (item == "creator") {
            // ie <dc:creator><rdf:Seq><rdf:li>Rory Hill</rdf:li></rdf:Seq></dc:creator>
            newItem.append("\n\t\t\t<");
            newItem.append(tag);
            newItem.append("><rdf:Seq><rdf:li>");
            newItem.append(value);
            newItem.append("</rdf:li></rdf:Seq></");
            newItem.append(tag);
            newItem.append(">");
            xmpBa.insert(startPos, newItem);
        }
        // item created
        return true;
    }

    // item exists, replace item
    int endPos;
    bool foundItem = false;
    startPos += tag.length();

    if (item == "title" || item == "rights" || item == "creator") {
        QByteArray temp = xmpBa.mid(startPos, 100);
        startPos = xmpBa.indexOf("rdf:li", startPos);
        temp = xmpBa.mid(startPos, 100);
        startPos = xmpBa.indexOf(">", startPos) + 1;
        temp = xmpBa.mid(startPos, 100);
        endPos = xmpBa.indexOf("<", startPos);
        foundItem = true;
    }
    else {
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

    if (foundItem) {
        xmpBa.replace(startPos, endPos - startPos, value);
//        report();
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
