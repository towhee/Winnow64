#ifndef XMP_H
#define XMP_H

#include <QObject>
#include <QtCore>
#include <QtWidgets>
#include <QtXmlPatterns>
#include "Main/global.h"

class Xmp : public QObject
{
    Q_OBJECT
public:
    Xmp(QFile &file, ulong &offset, ulong &nextOffset, bool useSidecar = false,
        QObject *parent = nullptr);
    QString getItem(QByteArray item);
    bool setItem(QByteArray item, QByteArray value);
    QString metaAsString();
    bool writeJPG(QByteArray &buffer);
    bool writeSidecar(QByteArray &buffer);

signals:

public slots:

private:
    void insertSchemas(QByteArray &item);
    int schemaInsertPos(QByteArray schema);
    void report();

    QByteArray xmpBa;               // the xmpmeta packet
    ulong xmpSegmentOffset;         // file offset to start of xmp segment
    ulong xmpmetaOffset;            // file offset to start of xmpmeta packet
    ulong xmpLength;                // length of xmp segment
    ulong xmpmetaStart;             // offset from start of xmp segment
    ulong xmpmetaEnd;               // offset from start of xmp segment
    ulong xmpPacketEnd;             // offset from start of xmp segment
    int xmpmetaRoom;                // xmpPacketEnd - xmpmetaStart

    QString assignmentMethod;       // brackets or equals
    QByteArray fileType;            // uppercase suffix
    QHash<QByteArray,QByteArray> schemaHash;
    QHash<QByteArray,QByteArray> startTagHash;

    QByteArrayList xmpSchemaList;
    QByteArrayList dcSchemaList;
    QByteArrayList auxSchemaList;
    QByteArrayList iptc4xmpCoreSchemaList;

    QByteArray xmpmetaPrefix = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">";
    QByteArray xmpmetaSuffix = "</x:xmpmeta>";
    QByteArray rdfPrefix = "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">";
    QByteArray rdfSuffix = "</rdf:RDF>";
    QByteArray rdfDescriptionPrefix = "<rdf:Description";  // + schemas
    QByteArray rdfDescriptionSuffix = "</rdf:Description>";
    QByteArray xmpSchema = "\n\t\t\txmlns:xmp=\"http://ns.adobe.com/xap/1.0/\" ";
    QByteArray dcSchema = "\n\t\t\txmlns:dc=\"http://purl.org/dc/elements/1.1/\" ";
    QByteArray tifSchema = "\n\t\t\txmlns:tiff=\"http://ns.adobe.com/tiff/1.0/\" ";
    QByteArray iptc4xmpCoreSchema = "\n\t\t\txmlns:Iptc4xmpCore=\"http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/\" ";
    QByteArray psSchema = "\n\t\t\txmlns:photoshop=\"http://ns.adobe.com/photoshop/1.0/\" ";
    QByteArray sidecarSkeleton = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">"
         "\n\t<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
         "\n\t\t<rdf:Description "
         "\n\t\t\trdf:about=\"\">"
         "\n\t\t</rdf:Description>\n\t</rdf:RDF>\n</x:xmpmeta>";
    QByteArray sidecarExtension = "<photoshop:SidecarForExtension>XXX</photoshop:SidecarForExtension>";
    QByteArray rdfSeq = "<rdf:Seq><rdf:li>XXX</rdf:li></rdf:Seq>";
    QByteArray rdfList = "<TAG><rdf:Alt><rdf:li xml:lang=\"x-default\">XXX</rdf:li></rdf:Alt></TAG>";
};

#endif // XMP_H
