#ifndef XMP_H
#define XMP_H

#include <QObject>
#include <QtCore>
#include <QtWidgets>
#include <QTextCodec>       // Qt6.2 with Qt5 compatibility remove
//#include <QtXmlPatterns>  // Qt6.2 with Qt5 compatibility remove

#include "Lib/rapidxml/rapidxml.hpp"
#include "Lib/rapidxml/rapidxml_iterators.hpp"
#include "Lib/rapidxml/rapidxml_print_rgh.hpp"
#include "Lib/rapidxml/rapidxml_utils.hpp"

#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

#include "Main/global.h"
//#include "Metadata/imagemetadata.h"
#include "Utilities/utilities.h"

class Xmp : public QObject
{
    Q_OBJECT
public:
    Xmp(QFile &file, uint offset, uint length, QObject *parent = nullptr);
    Xmp(QFile &file, QObject *parent = nullptr);

    struct XmpObj {
        rapidxml::xml_node<>* node;
        rapidxml::xml_node<>* parent;
        rapidxml::xml_attribute<>* attr;
        QString nodeName;           // req'd ie "rdf:Description"
        QString attrName;           // if "" then node format ie "xmp:Rating"
        QString parentName;         // req'd if attrName = ""
        QString type;               // node or attribute
        QString value;
        bool operator==(const XmpObj& x) const {
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

    void test(XmpObj o);

    bool isItem(QByteArray item);
    QString getItem(QByteArray item);
    bool setItem(QByteArray item, QByteArray value);

    bool getNode(rapidxml::xml_node<>* node, QByteArray nodeName);
    bool getAttribute(QByteArray attrName, QByteArray nodeName = "rdf:Description");
    QString setAttribute(QByteArray attrName, QByteArray nodeName = "rdf:Description");
    QString xmpAsString();
    bool writeJPG(QByteArray &buffer);
    bool readSidecar(QByteArray &buffer);
    bool writeSidecar(QFile &sidecarFile);
    QString diagnostics();

    bool xmlDocContains(QString name, const rapidxml::xml_node<>* node);
    XmpObj xmlDocObj(QString name,
                     rapidxml::xml_node<> *node,
                     rapidxml::xml_node<> *parNode = nullptr);

    QString docToQString();
    QByteArray docToByteArray();
    std::string docToStdString();
    bool isValid = false;

    QHash<QString, QString>xmpHashOld;
    QHash<QString, XmpObj>xmpObjs;

signals:

public slots:

private:
    void initialize();
    void buildHash(rapidxml::xml_node<> *node, rapidxml::xml_node<> *baseNode = nullptr);
    void insertSchemas(QByteArray &item);
    int schemaInsertPos(QByteArray schema);

    void walk(QTextStream &rpt,
              const rapidxml::xml_node<>* node,
              int indentSize = 4,
              int indent = 0);
    void xmlDocRpt(QTextStream &rpt, const rapidxml::xml_node<>* node);

    rapidxml::xml_document<> xmlDoc;
    rapidxml::xml_node<>* xmlNode = nullptr;
    rapidxml::xml_attribute<>* xmlAttribute = nullptr;
    rapidxml::xml_node<>* xmlRdfDescriptionNode = nullptr;
    XmpObj nullXmpObj;
    QByteArrayList a;               // attributes appended to xmlDoc
    QByteArrayList v;               // values appended to xmlDoc

    QByteArray xmpBa;               // the xmpmeta packet
    std::string xmpString;

    ulong xmpSegmentOffset;         // file offset to start of xmp segment
    ulong xmpmetaOffset;            // file offset to start of xmpmeta packet
    ulong xmpLength;                // length of xmp segment
    ulong xmpmetaStart;             // offset from start of xmp segment
    ulong xmpmetaEnd;               // offset from start of xmp segment
    ulong xmpPacketEnd;             // offset from start of xmp segment
    int xmpmetaRoom;                // xmpPacketEnd - xmpmetaStart

    QString folderPath;
    QString baseName;
    QString extension;
    QString filePath;
    QHash<QString,QString> xmpItems;
    QStringList mapSkipNodes = {
        "rdf:li",
        "rdf:Seq",
        "rdf:Alt",
        "rdf:Bag"
    };

    QString assignmentMethod;       // brackets or equals
    QByteArray fileType;            // uppercase suffix
    QHash<QByteArray,QByteArray> schemaHash;

    QHash<QByteArray,QByteArray> startTagHash;

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
    QByteArray sidecarSkeleton = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">"
         "\n\t<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
         "\n\t\t<rdf:Description "
         "\n\t\t\trdf:about=\"\">"
         "\n\t\t</rdf:Description>\n\t</rdf:RDF>\n</x:xmpmeta>";
//    QByteArray sidecarSkeleton = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">"
//                                 "\n\t<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
//                                 "\n\t\t<rdf:Description "
//                                 "\n\t\t\trdf:about=\"\">"
//                                 "\n\t\t</rdf:Description>\n\t</rdf:RDF>\n</x:xmpmeta>";
    QByteArray sidecarExtension = "<photoshop:SidecarForExtension>XXX</photoshop:SidecarForExtension>";
    QByteArray rdfSeq = "<rdf:Seq><rdf:li>XXX</rdf:li></rdf:Seq>";
    QByteArray rdfList = "<TAG><rdf:Alt><rdf:li xml:lang=\"x-default\">XXX</rdf:li></rdf:Alt></TAG>";
};

#endif // XMP_H
