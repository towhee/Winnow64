#ifndef XMP_H
#define XMP_H

#include <QObject>
#include <QtCore>
#include <QtWidgets>
#include <QTextCodec>       // Qt6.2 with Qt5 compatibility remove
//#include <QtXmlPatterns>  // Qt6.2 with Qt5 compatibility remove

//#include "Lib/rapidxml/rapidxml.hpp"
//#include "Lib/rapidxml/rapidxml_iterators.hpp"
//#include "Lib/rapidxml/rapidxml_print_rgh.hpp"
//#include "Lib/rapidxml/rapidxml_utils.hpp"
#include "Metadata/rapidxml.h"
#include "Metadata/rapidxml_print_rgh.h"

#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

#include "Main/global.h"
#include "Utilities/utilities.h"

class Xmp : public QObject
{
    Q_OBJECT
public:
    Xmp(QFile &file, uint offset, uint length, QObject *parent = nullptr);
    Xmp(QFile &file, QObject *parent = nullptr);

    struct XmpElement {
        rapidxml::xml_node<>* node;
        rapidxml::xml_node<>* parent;
        rapidxml::xml_attribute<>* attr;
        QString schema;             // ie xmp, aux, dc...
        QString elementName;        // used to search ie "xmp:Rating"
        QString nodeName;           // req'd ie "rdf:Description"
        QString attrName;           // if "" then node format ie "xmp:Rating"
        QString parentName;         // req'd if attrName = ""
        QString type;               // node or attribute
        QString value;
        bool operator==(const XmpElement& x) const {
            return (x.nodeName == nodeName
                    && x.attrName == attrName
                    && x.parentName == parentName
                    );
        }
        bool exists() const {
            return !(nodeName == "" && attrName == "" && parentName == "");
        }
    };
    static QByteArray skeleton();

    QString getItem(QByteArray item);
    bool setItem(QByteArray item, QByteArray value);

    QString setAttribute(QByteArray attrName, QByteArray nodeName = "rdf:Description");
    QString xmpAsString();
    bool writeJPG(QByteArray &buffer);
    bool writeSidecar(QFile &sidecarFile);

    QString docToQString();
    QByteArray docToByteArray();
    std::string docToStdString();
    bool isValid = false;

    QHash<QString, XmpElement>definedElements;

private:
    void initialize();
    bool includeSchemaNamespece(QString item);
    void test(XmpElement o);

    XmpElement xmlDocObj(QString name,
                     rapidxml::xml_node<> *node,
                     rapidxml::xml_node<> *parNode = nullptr);

    void walk(QTextStream &rpt,
              const rapidxml::xml_node<>* node,
              int indentSize = 4,
              int indent = 0);

    rapidxml::xml_document<> xmlDoc;
    rapidxml::xml_node<> *rdfDescriptionNode;
    rapidxml::xml_attribute<> *rdfAbout;
    XmpElement nullXmpObj;
    QByteArrayList a;               // attributes appended to xmlDoc
    QByteArrayList v;               // values appended to xmlDoc

    QByteArray xmpBa;               // the xmpmeta packet

    ulong xmpSegmentOffset;         // file offset to start of xmp segment
    ulong xmpmetaOffset;            // file offset to start of xmpmeta packet
    ulong xmpLength;                // length of xmp segment
    ulong xmpmetaStart;             // offset from start of xmp segment
    ulong xmpmetaEnd;               // offset from start of xmp segment
    ulong xmpPacketEnd;             // offset from start of xmp segment
    int xmpmetaRoom;                // xmpPacketEnd - xmpmetaStart

    QString filePath;
    QHash<QString,QString> xmpItems;
    QStringList skipNodes = {
        "rdf:li",
        "rdf:Seq",
        "rdf:Alt",
        "rdf:Bag"
    };

    /*
    QString assignmentMethod;       // brackets or equals
    QByteArray fileType;            // uppercase suffix

    QHash<QByteArray,QByteArray> startTagHash;
    QHash<QString,QByteArray> schemaHash;

    QByteArrayList xmpSchemaList;
    QByteArrayList dcSchemaList;
    QByteArrayList auxSchemaList;
    QByteArrayList iptc4xmpCoreSchemaList;

    QByteArrayList useLanguage;

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
    QByteArray sidecarExtension = "<photoshop:SidecarForExtension>XXX</photoshop:SidecarForExtension>";
    QByteArray rdfSeq = "<rdf:Seq><rdf:li>XXX</rdf:li></rdf:Seq>";
    QByteArray rdfList = "<TAG><rdf:Alt><rdf:li xml:lang=\"x-default\">XXX</rdf:li></rdf:Alt></TAG>";
    */
};

#endif // XMP_H
