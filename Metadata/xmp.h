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

class Xmp : public QObject
{
    Q_OBJECT
public:
    Xmp(QFile &file, uint offset, uint length, int instance, QObject *parent = nullptr);
    Xmp(QFile &file, int instance, QObject *parent = nullptr);

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
    bool setItemList(QByteArray item, const QStringList &values);
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

    /*
        rapidxml stores POINTERS, never copies, so every node name and value handed to
        allocate_node/allocate_attribute must outlive xmlDoc. a and v are the
        class-lifetime stores that satisfy that. These two append and hand back the
        stable pointer, so the lifetime rule is written down once instead of at every
        call site.

        The pointer survives a and v reallocating around it: QByteArray owns a heap
        block and has no small-string buffer, so moving the QByteArray object during a
        QList regrow does not move the bytes constData() points at.
    */
    const char *keepName(const QByteArray &name);
    const char *keepValue(const QByteArray &value);

    /* Remove an element from xmlDoc in either of the two forms xmlDocElement can
       report, doing nothing if it was not found. */
    void removeItem(const XmpElement &element);

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
    /*  Backing store for every name and value handed to rapidxml, which stores POINTERS
        and never copies -- see keepName/keepValue, which are the ONLY things that may
        append here.

        THEY USED TO BE READ AS A PAIR: every writer appended one name and one value and
        then indexed BOTH with "a.count() - 1", an unwritten invariant that the two grow
        in lockstep. setItemList broke it the moment it landed, because one property
        write appends three names (the property, rdf:Bag, rdf:li) and N values -- so the
        NEXT writer's a.count()-1 indexed v out of range and the app aborted on a QList
        assert. It survived the first tests only because their name and value counts
        happened to be equal. Nothing indexes these by position any more. */
    QByteArrayList a;               // names (nodes/attributes) appended to xmlDoc
    QByteArrayList v;               // values appended to xmlDoc

    QByteArray xmpBa;               // the xmpmeta packet

    ulong xmpSegmentOffset;         // file offset to start of xmp segment
    ulong xmpmetaOffset;            // file offset to start of xmpmeta packet
    ulong xmpLength;                // length of xmp segment
    ulong xmpmetaStart;             // offset from start of xmp segment
    ulong xmpmetaEnd;               // offset from start of xmp segment
    ulong xmpPacketEnd;             // offset from start of xmp segment
    int xmpmetaRoom;                // xmpPacketEnd - xmpmetaStart

    int instance;
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
