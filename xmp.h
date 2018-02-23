#ifndef XMP_H
#define XMP_H

#include <QObject>
#include <QtCore>
#include <QtWidgets>
#include <QtXmlPatterns>

class Xmp : public QObject
{
    Q_OBJECT
public:
    Xmp(QFile &file, ulong &offset, ulong &nextOffset, QObject *parent = nullptr);
    QString getItem(QByteArray item);
    bool setItem(QByteArray item, QByteArray value);
    QString metaAsString();
    bool writeJPG(QFile &file, QByteArray &newBuf);
    bool writeSidecar(QFile &file, QByteArray &newBuf);

signals:

public slots:

private:
    void checkSchemas();
    int schemaInsertPos(QByteArray schema);
    void report();

    QByteArray xmpBa;               // the xmpmeta packet
    ulong xmpSegmentOffset;         // file offset to start of xmp segment
    ulong xmpmetaOffset;            // file offset to start of xmpmeta packet
    ulong xmpLength;                // length of xmp segment
    ulong xmpmetaStart;             // offset from start of xmp segment
    ulong xmpmetaEnd;               // offset from start of xmp segment
    ulong xmpPacketEnd;             // offset from start of xmp segment
    ulong xmpmetaRoom;              // xmpPacketEnd - xmpmetaStart

    QString assignmentMethod;       // brackets or equals

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
    QByteArray xmpSchema = " xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"";
    QByteArray dcSchema = " xmlns:dc=\"http://purl.org/dc/elements/1.1/\"";
    QByteArray sidecarSkeleton = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">"
         "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
         "<rdf:Description xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\" "
         "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" rdf:about=\"\">"
         "</rdf:Description></rdf:RDF></x:xmpmeta>";
};

#endif // XMP_H
