#ifndef XMP_H
#define XMP_H

#include <QObject>
#include <QtCore>
#include <QtWidgets>
//#include <QTextCodec>       // Qt6.2 with Qt5 compatibility remove

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

    enum Err {
        NoErr,
        ParseFailed,
        NoRoot,
        InvalidRoot,
        NoRdf,
        NoRdfDescription,
        NoRdfAbout,
        Count
    } err;
    QVector<QString> errMsg;

    enum ElementType {
        Node,
        Attribute,
        List
    };

    struct XmpElement {
        rapidxml::xml_node<>* node;
        rapidxml::xml_node<>* parent;
        rapidxml::xml_attribute<>* attr;
        QString schema;             // ie xmp, aux, dc...
        QString name;               // used to search ie "xmp:Rating"
        QString parentName;         // req'd if attrName = ""
        ElementType type;           // node or attribute
        QString value;
        QStringList valueList;
        bool operator==(const XmpElement& x) const {
            return (x.name == name && x.parentName == parentName);
        }
        bool exists() const {
            return !(name == "" && parentName == "");
        }
    };

    QString getItem(QByteArray item);
    QStringList getItemList(QByteArray item);
    bool setItem(QByteArray item, QByteArray value);
    void fix();

    QString srcToString();
    bool writeJPG(QByteArray &buffer);
    bool writeSidecar(QFile &sidecarFile);

    QString docToQString();
    QByteArray docToByteArray();
    std::string docToStdString();

    bool isValid = false;

    QHash<QString, XmpElement>definedElements;

private:
    void initialize();
    bool includeSchemaNamespace(QString item);
    void report(XmpElement o);
    static QByteArray skeleton();

    XmpElement xmlDocElement(QString name,
                     rapidxml::xml_node<> *node,
                     rapidxml::xml_node<> *parNode = nullptr);

//    void xmlDocElement(XmpElement &element,
//                     rapidxml::xml_node<> *node,
//                       rapidxml::xml_node<> *parNode = nullptr,
//                       bool iterateList = false);

    void walk(QTextStream &rpt,
              rapidxml::xml_node<>* node,
              int indentSize = 4,
              int indent = 0);

    inline QString xmlNodeName(rapidxml::xml_node<> *node)
    {
        return QString(node->name()).left(static_cast<int>(node->name_size()));
    }

    inline QString xmlNodeValue(rapidxml::xml_node<> *node)
    {
        return QString(node->value()).left(static_cast<int>(node->value_size()));
    }

    inline QString xmlAttributeName(rapidxml::xml_attribute<> *attr)
    {
        return QString(attr->name()).left(static_cast<int>(attr->name_size()));
    }

    inline QString xmlAttributeValue(rapidxml::xml_attribute<> *attr)
    {
        return QString(attr->value()).left(static_cast<int>(attr->value_size()));
    }

    rapidxml::xml_document<> xmlDoc;
    rapidxml::xml_node<> *rootNode = nullptr;
    rapidxml::xml_node<> *rdfNode = nullptr;
    rapidxml::xml_node<> *rdfDescriptionNode = nullptr;
    rapidxml::xml_attribute<> *rdfAbout = nullptr;
    XmpElement nullXmpElement;
    QByteArrayList a;               // attributes/nodes appended to xmlDoc
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
};

#endif // XMP_H
