#include "xmp.h"

/*
Xmp reads and writes xmp tags to the QByteArray buffer xmpBa. The buffer is read from the
source file. If there are no offsets then a sidecar file will be used. If there is no xmp data
in the source file or the file format is not documented then the xmp tags are written to a
sidecar file.

Read/Write to a sidecar:   Xmp xmp(file);
Read/Write to image file:  Xmp xmp(file, offset, nextOffset);

Xmp uses the library rapidxml to parse the xmp text into a DOM rapidxml::xml_document. The
library uses pointers for all operations, so any data passed to rapidxml must be Xmp class
variables to insure their lifespan.

The struct XmpElement is used to describe everything Xmp needs to find, create, get and set
items (ie rating, label, title etc).  The hash of XmpElement called defineElements contains
the definitions for all the items that Winnow uses including: rating, label, createdate,
modifydate, title, rights, creator, lens, lensserialnumber, serialnumber, email, url,
orientation and winnowaddthumb.
*/

/* Example sidecar file that lightroom successfully read/writes:

<x:xmpmeta xmlns:x="adobe:ns:meta/">
    <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
        <rdf:Description
            xmlns:xmp="http://ns.adobe.com/xap/1.0/"
            xmlns:dc="http://purl.org/dc/elements/1.1/"
            xmlns:Iptc4xmpCore="http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/"
            rdf:about="">
            <xmp:ModifyDate>2018-04-16T17:31:55-07:00</xmp:ModifyDate>
            <xmp:Rating>5</xmp:Rating>
            <xmp:Label>Red</xmp:Label>
            <dc:title><rdf:Alt><rdf:li xml:lang="x-default">Test title</rdf:li></rdf:Alt></dc:title>
            <dc:creator><rdf:Seq><rdf:li>rory</rdf:li></rdf:Seq></dc:creator>
            <dc:rights><rdf:Alt><rdf:li xml:lang="x-default">2019 rory</rdf:li></rdf:Alt></dc:rights>
            <Iptc4xmpCore:CreatorContactInfo rdf:parseType='Resource'>
                <Iptc4xmpCore:CiEmailWork>rory@mail.com</Iptc4xmpCore:CiEmailWork>
                <Iptc4xmpCore:CiUrlWork>rory.com</Iptc4xmpCore:CiUrlWork>
            </Iptc4xmpCore:CreatorContactInfo>
        </rdf:Description>
    </rdf:RDF>
</x:xmpmeta>

Example from xmp embedded in D:/Pictures/Coaster/2005-10-11_0082.jpg

<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 5.1.2">
    <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
        <rdf:Description
            xmlns:aux="http://ns.adobe.com/exif/1.0/aux/"
            xmlns:xmp="http://ns.adobe.com/xap/1.0/"
            xmlns:crs="http://ns.adobe.com/camera-raw-settings/1.0/"
            xmlns:xmpRights="http://ns.adobe.com/xap/1.0/rights/"
            xmlns:dc="http://purl.org/dc/elements/1.1/"
            xmlns:lr="http://ns.adobe.com/lightroom/1.0/"
            xmlns:photoshop="http://ns.adobe.com/photoshop/1.0/"
            xmlns:photomechanic="http://ns.camerabits.com/photomechanic/1.0/"
            rdf:about=""
            aux:SerialNumber="5009518"
            aux:Lens="17.0-55.0 mm f/2.8"
            aux:LensInfo="17/1 55/1 28/10 28/10"
            aux:LensID="125"
            aux:ApproximateFocusDistance="119/100"
            aux:ImageNumber="8771"
            xmp:CreatorTool="Adobe Photoshop Lightroom 3.5 (Windows)"
            xmp:ModifyDate="2011-11-15T09:32:10-08:00"
            xmp:MetadataDate="2011-11-15T09:32:10-08:00"
            xmp:CreateDate="2005-10-11T15:24:19.73-07:00"
            xmp:Label="Superior"
            xmp:Rating="0"
            crs:Version="6.5"
            crs:ProcessVersion="5.0"
            crs:WhiteBalance="Custom"
            crs:Temperature="7500"
            crs:Tint="-1"
            crs:Exposure="-1.40"
            crs:Shadows="14"
            crs:Brightness="+88"
            crs:Contrast="+50"
            crs:Saturation="0"
            crs:Sharpness="40"
            crs:LuminanceSmoothing="30"
            crs:ColorNoiseReduction="25"
            crs:ChromaticAberrationR="0"
            crs:ChromaticAberrationB="0"
            crs:VignetteAmount="0"
            crs:ShadowTint="0"
            crs:RedHue="0"
            crs:RedSaturation="0"
            crs:GreenHue="0"
            crs:GreenSaturation="0"
            crs:BlueHue="0"
            crs:BlueSaturation="0"
            crs:FillLight="0"
            crs:Vibrance="0"
            crs:HighlightRecovery="0"
            crs:Clarity="0"
            crs:Defringe="0"
            crs:HueAdjustmentRed="0"
            crs:HueAdjustmentOrange="0"
            crs:HueAdjustmentYellow="0"
            crs:HueAdjustmentGreen="0"
            crs:HueAdjustmentAqua="0"
            crs:HueAdjustmentBlue="0"
            crs:HueAdjustmentPurple="0"
            crs:HueAdjustmentMagenta="0"
            crs:SaturationAdjustmentRed="0"
            crs:SaturationAdjustmentOrange="0"
            crs:SaturationAdjustmentYellow="0"
            crs:SaturationAdjustmentGreen="0"
            crs:SaturationAdjustmentAqua="0"
            crs:SaturationAdjustmentBlue="0"
            crs:SaturationAdjustmentPurple="0"
            crs:SaturationAdjustmentMagenta="0"
            crs:LuminanceAdjustmentRed="0"
            crs:LuminanceAdjustmentOrange="0"
            crs:LuminanceAdjustmentYellow="0"
            crs:LuminanceAdjustmentGreen="0"
            crs:LuminanceAdjustmentAqua="0"
            crs:LuminanceAdjustmentBlue="0"
            crs:LuminanceAdjustmentPurple="0"
            crs:LuminanceAdjustmentMagenta="0"
            crs:SplitToningShadowHue="0"
            crs:SplitToningShadowSaturation="0"
            crs:SplitToningHighlightHue="0"
            crs:SplitToningHighlightSaturation="0"
            crs:SplitToningBalance="0"
            crs:ParametricShadows="0"
            crs:ParametricDarks="0"
            crs:ParametricLights="0"
            crs:ParametricHighlights="0"
            crs:ParametricShadowSplit="25"
            crs:ParametricMidtoneSplit="50"
            crs:ParametricHighlightSplit="75"
            crs:SharpenRadius="+1.0"
            crs:SharpenDetail="25"
            crs:SharpenEdgeMasking="0"
            crs:PostCropVignetteAmount="0"
            crs:GrainAmount="0"
            crs:LensProfileEnable="0"
            crs:LensManualDistortionAmount="0"
            crs:PerspectiveVertical="0"
            crs:PerspectiveHorizontal="0"
            crs:PerspectiveRotate="0.0"
            crs:PerspectiveScale="100"
            crs:ConvertToGrayscale="False"
            crs:ToneCurveName="Medium Contrast"
            crs:CameraProfile="ACR 3.1"
            crs:CameraProfileDigest="A432BB318AD26723D63418A18E177CDF"
            crs:LensProfileSetup="LensDefaults"
            crs:HasSettings="True"
            crs:AlreadyApplied="True"
            xmpRights:Marked="True"
            xmpRights:WebStatement="www.pbase.com/roryhill"
            photoshop:DateCreated="2005-10-11T15:24:19.73-07:00"
            photomechanic:ColorClass="3" photomechanic:Tagged="True"
            photomechanic:Prefs="1:3:0:008771" photomechanic:PMVersion="PM5">
            <crs:ToneCurve>
                <rdf:Seq>
                    <rdf:li>0, 0</rdf:li>
                    <rdf:li>32, 22</rdf:li>
                    <rdf:li>64, 56</rdf:li>
                    <rdf:li>128, 128</rdf:li>
                    <rdf:li>192, 196</rdf:li>
                    <rdf:li>255, 255</rdf:li>
                </rdf:Seq>
            </crs:ToneCurve>
            <dc:subject>
                <rdf:Bag>
                    <rdf:li>Category</rdf:li>
                    <rdf:li>Coaster Xmas 2010</rdf:li>
                    <rdf:li>Favourite</rdf:li>
                    <rdf:li>X</rdf:li>
                    <rdf:li>Category|Coaster Xmas 2010</rdf:li>
                    <rdf:li>Category|Favourite</rdf:li>
                    <rdf:li>Category|X</rdf:li>
                </rdf:Bag>
            </dc:subject>
            <dc:description>
                <rdf:Alt>
                    <rdf:li xml:lang="x-default">[#Beginning of Shooting Data Section]
                        Nikon D2X
                        Focal Length: 45mm
                        Optimize Image:
                        Color Mode: Mode II (Adobe RGB)
                        Long Exposure NR: Off
                        High ISO NR: Off
                        2005/10/11 15:24:19.7
                        Exposure Mode: Aperture Priority
                        White Balance: Auto
                        Tone Comp.: Normal
                        Compressed RAW (12-bit)
                        Metering Mode: Spot
                        AF Mode: AF-C
                        Hue Adjustment: 0°
                        Image Size: Large (4288 x 2848)
                        1/125 sec - F/4.5
                        Flash Sync Mode: Not Attached
                        Saturation: Normal
                        Exposure Comp.: 0 EV
                        Sharpening: Normal
                        Lens: 17-55mm F/2.8 G
                        Sensitivity: ISO 200
                        Auto Flash Comp: 0 EV
                        Image Comment:
                    [#End of Shooting Data Section]</rdf:li>
                </rdf:Alt>
            </dc:description>
            <dc:creator>
                <rdf:Seq>
                    <rdf:li>Rory Hill</rdf:li>
                </rdf:Seq>
            </dc:creator>
            <dc:rights>
                <rdf:Alt>
                    <rdf:li xml:lang="x-default">2007 Rory Hill</rdf:li>
                </rdf:Alt>
            </dc:rights>
            <lr:hierarchicalSubject>
                <rdf:Bag>
                    <rdf:li>Category|Coaster Xmas 2010</rdf:li>
                    <rdf:li>Category|Favourite</rdf:li>
                    <rdf:li>Category|X</rdf:li>
                </rdf:Bag>
            </lr:hierarchicalSubject>
        </rdf:Description>
    </rdf:RDF>
</x:xmpmeta>

*/

Xmp::Xmp(QFile &file, QObject *parent) :  QObject(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (file.exists()) {
        filePath = QFileInfo(file).filePath();
        xmpBa = file.readAll();
    }
    initialize();
}

Xmp::Xmp(QFile &file, uint offset, uint length, QObject *parent) :  QObject(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    xmpSegmentOffset = offset;
    if (file.exists()) {
        filePath = QFileInfo(file).filePath();
        file.seek(offset);
        xmpBa = file.read(length);
        xmpPacketEnd = xmpBa.indexOf("<?xpacket end", xmpmetaEnd);
        xmpmetaRoom = xmpPacketEnd - xmpmetaStart;
        xmpmetaOffset = xmpSegmentOffset + xmpmetaStart;
        int xmpmetaLength = xmpmetaEnd - xmpmetaStart;
        xmpBa = xmpBa.mid(xmpmetaStart, xmpmetaLength);
    }
    initialize();
}

QByteArray Xmp::skeleton()
{
    return "<x:xmpmeta"
           "\n\txmlns:x=\"adobe:ns:meta/\">"
           "\n\t<rdf:RDF"
           "\n\t\txmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
           "\n\t\t<rdf:Description"
//           "\n\t\t\txmlns:xmp=\"http://ns.adobe.com/xap/1.0/\""
//           "\n\t\t\txmlns:dc=\"http://purl.org/dc/elements/1.1/\""
//           "\n\t\t\txmlns:tiff=\"http://ns.adobe.com/tiff/1.0/\""
           "\n\t\t\trdf:about=\"\">"
           "\n\t\t</rdf:Description>\n"
           "\t\t<Iptc4xmpCore:CreatorContactInfo rdf:parseType='Resource'>\n"
           "\t\t</Iptc4xmpCore:CreatorContactInfo>\n"
           "\t</rdf:RDF>\n"
           "</x:xmpmeta>";
}

void Xmp::initialize()
{
    if (G::isLogger) G::log(__FUNCTION__);


    if (xmpBa.length() == 0) xmpBa = skeleton();

    enum {PARSE_FLAGS = rapidxml::parse_non_destructive};
    try {
        xmlDoc.parse<0>(xmpBa.data());    // 0 means default parse flags
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Runtime error was: " << e.what() << std::endl;
        return;
    }
    catch (const rapidxml::parse_error& e) {
        std::cerr << "Parse error was: " << e.what() << "  " << filePath.toStdString() << std::endl;
        return;
    }
    catch (const std::exception& e) {
        std::cerr << "Error was: " << e.what() << std::endl;
        return;
    }
    catch (...) {
        std::cerr << "An unknown error occurred." << std::endl;
        return;
    }

    rdfDescriptionNode = xmlDocObj("rdf:Description", xmlDoc.first_node()).node;
    if (!rdfDescriptionNode) {
        // deal with error
    }

    rdfAbout = xmlDocObj("rdf:about", rdfDescriptionNode).attr;
    if (!rdfAbout) {
        // deal with error
    }

    nullXmpObj.nodeName = "";
    nullXmpObj.attrName = "";
    nullXmpObj.parentName = "";

    XmpElement e;

    e.elementName = "xmp:Rating";
    e.nodeName = "rdf:Description";
    e.attrName = "xmp:Rating";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "xmp";
    definedElements["rating"] = e;

    e.elementName = "xmp:Label";
    e.nodeName = "rdf:Description";
    e.attrName = "xmp:Label";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "xmp";
    definedElements["label"] = e;

    e.elementName = "xmp:CreateDate";
    e.nodeName = "rdf:Description";
    e.attrName = "xmp:CreateDate";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "xmp";
    definedElements["createdate"] = e;

    e.elementName = "xmp:ModifyDate";
    e.nodeName = "rdf:Description";
    e.attrName = "xmp:ModifyDate";
    e.parentName = "";
    e.type = "attribute";
    definedElements["modifydate"] = e;

    e.elementName = "dc:title";
    e.nodeName = "rdf:Description";
    e.attrName = "dc:title";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "dc";
    definedElements["title"] = e;

    e.elementName = "dc:rights";
    e.nodeName = "rdf:Description";
    e.attrName = "dc:rights";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "dc";
    definedElements["rights"] = e;

    e.elementName = "dc:creator";
    e.nodeName = "rdf:Description";
    e.attrName = "dc:creator";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "dc";
    definedElements["creator"] = e;

    e.elementName = "Iptc4xmpCore:CiEmailWork";
    e.nodeName = "Iptc4xmpCore:CiEmailWork";
    e.attrName = "";
    e.parentName = "Iptc4xmpCore:CreatorContactInfo";
    e.type = "node";
    e.schema = "Iptc4xmpCore";
    definedElements["email"] = e;

    e.elementName = "Iptc4xmpCore:CiUrlWork";
    e.nodeName = "Iptc4xmpCore:CiUrlWork";
    e.attrName = "";
    e.parentName = "Iptc4xmpCore:CreatorContactInfo";
    e.type = "node";
    e.schema = "Iptc4xmpCore";
    definedElements["url"] = e;

    e.elementName = "aux:SerialNumber";
    e.nodeName = "rdf:Description";
    e.attrName = "aux:SerialNumber";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "aux";
    definedElements["serialnumber"] = e;

    e.elementName = "aux:LensSerialNumber";
    e.nodeName = "rdf:Description";
    e.attrName = "aux:LensSerialNumber";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "aux";
    definedElements["lensserialnumber"] = e;

    e.elementName = "winnow:WinnowAddThumb";
    e.nodeName = "rdf:Description";
    e.attrName = "winnow:WinnowAddThumb";
    e.parentName = "";
    e.type = "attribute";
    e.schema = "winnow";
    definedElements["winnowaddthumb"] = e;

    e.elementName = "tiff:Orientation";
    e.nodeName = "rdf:Description";         // rgh check this is correct
    e.attrName = "tiff:Orientation";        // rgh check this is correct
    e.parentName = "";
    e.type = "attribute";
    definedElements["orientation"] = e;

    // schema elements
    e.parentName = "";
    e.nodeName = "rdf:Description";
    e.type = "attribute";

    e.elementName = "xmlns:xmp";
    e.attrName = "xmlns:xmp";
    e.value = "http://ns.adobe.com/xap/1.0/";
    definedElements["xmp"] = e;

    e.elementName = "xmlns:xmp";
    e.attrName = "xmlns:xmp";
    e.value = "http://ns.adobe.com/xap/1.0/";
    definedElements["xmp"] = e;

    e.elementName = "xmlns:dc";
    e.attrName = "xmlns:dc";
    e.value = "http://purl.org/dc/elements/1.1/";
    definedElements["dc"] = e;

    e.elementName = "xmlns:aux";
    e.attrName = "xmlns:aux";
    e.value = "http://ns.adobe.com/exif/1.0/aux/";
    definedElements["aux"] = e;

    e.elementName = "xmlns:tif";
    e.attrName = "xmlns:tif";
    e.value = "http://ns.adobe.com/tiff/1.0/";
    definedElements["tif"] = e;

    e.elementName = "xmlns:Iptc4xmpCore";
    e.attrName = "xmlns:Iptc4xmpCore";
    e.value = "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/";
    definedElements["Iptc4xmpCore"] = e;

    e.elementName = "xmlns:winnow";
    e.attrName = "xmlns:winnow";
    e.value = "http://winnow.ca/1.0/";
    definedElements["winnow"] = e;

    isValid = true;
}

bool Xmp::includeSchemaNamespece(QString item)
/*
    item is "rating", "label" ...

    Every item added to the xmp document must have a namespace defined as an attribute of
    rdf:Description.  This function looks up the schema for the item, checks to see if the
    namespace already exists, and if not, adds the namespace to rdf:description.  Example
    with namespaces for the schems xmp, dc, tiff and Iptc4xmpCore added.

    <rdf:RDF
        xmlns:rdf = "http://www.w3.org/1999/02/22-rdf-syntax-ns#">
        <rdf:Description
            xmlns:xmp = "http://ns.adobe.com/xap/1.0/"
            xmlns:dc = "http://purl.org/dc/elements/1.1/"
            xmlns:tiff = "http://ns.adobe.com/tiff/1.0/"
            xmlns:Iptc4xmpCore = "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/"
            rdf:about = ""
            ...
*/
{
    if (G::isLogger) G::log(__FUNCTION__);
    // ie item = modifydate, elementName = xmp:modifydate then schema = xmp
    QString schema = definedElements[item].schema;
    qDebug() << __FUNCTION__ << "item =" << item << "schema =" << schema;
    // schema namespace would = xmlns:xmp
    QString schemaNamespace = "xmlns:" + schema;
    // check if schemaNamespace exists in xmlDoc
    if (!xmlDocObj(schemaNamespace, rdfDescriptionNode).exists()) {
        qDebug() << __FUNCTION__ << "schema" << schema << "does not exist.  Adding schema.";
        a.append(schemaNamespace.toUtf8());
        v.append(definedElements[schema].value.toUtf8());
        int idx = a.count() - 1;
        qDebug() << __FUNCTION__ << "Adding schema namespace/value" << a[idx] << v[idx];
        rapidxml::xml_attribute<> *attr = xmlDoc.allocate_attribute(a[idx], v[idx]);
//        rdfDescriptionNode->append_attribute(attr);
        rdfDescriptionNode->insert_attribute(rdfAbout, attr);

//        if (!setItem(schema, definedElements[schema].value)) {
//            qWarning() << __FUNCTION__ << "failed to setItem for" << schema;
//            return false;
//        }
    }
    return true;
}

bool Xmp::writeJPG(QByteArray &buffer)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (xmpBa.length() < xmpmetaRoom) {
        buffer.replace(xmpmetaOffset, xmpBa.length(), xmpBa);
    }
    // not enough room, insert and update offset next segment
    else {

    }
    return true;
}

bool Xmp::writeSidecar(QFile &sidecarFile)
{
    if (G::isLogger) G::log(__FUNCTION__);
    sidecarFile.resize(0);
    qint64 bytesWritten = sidecarFile.write(docToQString().toUtf8());
    if (bytesWritten == 0) return false;
    return true;
}

QString Xmp::xmpAsString()
{
/*
    Returns unicode string from QByteArray xmpBa
*/
    if (G::isLogger) G::log(__FUNCTION__);
    return QTextCodec::codecForMib(106)->toUnicode(xmpBa);
}

QString Xmp::getItem(QByteArray item)
{
/*
    Returns the value for the item.  ie getItem("creator") = "Rory"
    items: Rating, Label, title ...
    item case does not matter
*/
    if (G::isLogger) G::log(__FUNCTION__);

    item = item.toLower();
//    qDebug() << __FUNCTION__ << item << definedElements[item].nodeName << definedElements.contains(item);
    if (!definedElements.contains(item)) {
        qDebug() << __FUNCTION__ << item << "not found in xmpObjs";
        return "";
    }
    XmpElement obj = xmlDocObj(definedElements[item].elementName, xmlDoc.first_node());
    /*
    qDebug() << __FUNCTION__ << "obj from xmlDoc:";
    //*/
//    test(obj);
    if (!obj.exists()) return "";
    //found a simple node or an attribute that has a value
    if (!obj.value.isEmpty()) return obj.value;
    QStringList propertyElements;
    // check if child node is a preperty element
    propertyElements << "rdf:Seq" << "rdf:Bag" << "rdf:Alt";
    // try drilling into node found
    rapidxml::xml_node<> *node = obj.node->first_node();
    if (node == 0) return "";
    QString nodeName = QString(node->name()).left(node->name_size());
    if (propertyElements.contains(nodeName)) {
        rapidxml::xml_node<> *list = node->first_node();
        if (list == 0) return "";
        nodeName = QString(list->name()).left(list->name_size());
        if (nodeName == "rdf:li") {
            QString val = QString(list->value()).left(list->value_size());
            if (val != "") return val;
            rapidxml::xml_attribute<>* a = list->first_attribute();
            if (a == 0) return "";
            return QString(a->name()).left(a->name_size());
        }
    }
    return "";
}

bool Xmp::setItem(QByteArray item, QByteArray value)
{
/*
    This function updates the xmp value for the item.

    The nodes and attributes contained in xmlDoc are summarized in xmpHash to expedite lookups.
    The format of xmpHash is (node|attribute, value);  The node|attribute string is used to
    identify the node or node attribute the value resides within.

    The lookupHash is used to convert items (rating, label etc) into the node|attribute format
    required by xmpHash.

    Each item is added to an attribute list (a) and a value list (v).  This is necessary as
    xmlDoc only contains pointers, so the data resides in the attributes and values, which must
    have lifespans equal to the Xmp class.

    If an item already exists, it is removed and the new item and value are appended as an
    attribute to the node <rdf:Description>. This converts an items that were in the node
    format to the attribute format.  For example:
        Node format:      <xmp:Rating>5</xmp:Rating>
        Attribute format: xmp:Rating = "5"
*/
    if (G::isLogger) G::log(__FUNCTION__);
    item = item.toLower();
    qDebug() << __FUNCTION__ << item << value;
    if (!definedElements.contains(item)) {
        qWarning() << __FUNCTION__ << "failed for" << item;
        return false;
    }

    // check the item schema namespace has been defined in rdf:description
    if (!includeSchemaNamespece(item)) return false;

    // if item exists in xmp remove item so we can replace
    XmpElement obj = xmlDocObj(definedElements[item].elementName, xmlDoc.first_node());
    qDebug() << __FUNCTION__ << "Remove object if exists already:";
    test(obj);
    if (obj.exists()) {
        if (obj.type == "attribute") obj.node->remove_attribute(obj.attr);
        if (obj.type == "node") obj.parent->remove_node(obj.node);
        qDebug() << __FUNCTION__ << "Removed obj.type" << obj.type << item;
    }
    else qDebug() << __FUNCTION__ << item << "was not found for removal";

    // get default XmpObj for item
    obj = definedElements[item];
    qDebug() << __FUNCTION__ << "Object to append (obj):";
    test(obj);

    // item and value lifetime must be same as xmpDoc, so save in QByteArrayList
    if (obj.type == "attribute" && value != "") {
        qDebug() << __FUNCTION__ << "Appending attribute" << obj.attrName;
        XmpElement parObj = xmlDocObj(obj.nodeName, xmlDoc.first_node());
        qDebug() << __FUNCTION__ << "Attribute node object (parObj):";
        test(parObj);
        if (!parObj.exists()) return false; // create parent node if missing!!
        a.append(obj.attrName.toUtf8());
        v.append(value);
        // get just appended list index
        int idx = a.count() - 1;
        // append attribute and value
        rapidxml::xml_attribute<> *attr = xmlDoc.allocate_attribute(a[idx], v[idx]);
        parObj.node->append_attribute(attr);
    }
    if (obj.type == "node") {
        qDebug() << __FUNCTION__ << "Appending node" << obj.nodeName << "to node" << obj.parentName;
        XmpElement parObj = xmlDocObj(obj.parentName, xmlDoc.first_node());
        qDebug() << __FUNCTION__ << "Node parent object (parObj):";
        test(parObj);
        if (!parObj.exists()) return false;  // create parent node if missing!!
        a.append(obj.nodeName.toUtf8());
        v.append(value);
        // get just appended list index
        int idx = a.count() - 1;
        // append node and value
        qDebug() << __FUNCTION__ << "Node name/value" << a[idx] << v[idx];
        rapidxml::xml_node<> *node = xmlDoc.allocate_node(rapidxml::node_element, a[idx], v[idx]);
        parObj.node->append_node(node);
    }

        /* debugging
        std::string s;
        rapidxml::print(std::back_inserter(s), xmlDoc);
        xmpBa = QByteArray::fromStdString(s);
        std::cout << "xmpBa from print xmlDoc:" << std::endl;
        std::cout << xmpBa.toStdString() << std::endl << std::flush;
        QCoreApplication::processEvents();

        std::cout << s << std::endl << std::flush;
        std::cout << xmpBa.toStdString() << std::endl << std::flush;
        //*/
    return true;
}

Xmp::XmpElement Xmp::xmlDocObj(QString name,
                           rapidxml::xml_node<> *node,
                           rapidxml::xml_node<> *parNode)
{
/*
    Return an XmlElement struct for the element name, starting from *node.  Info includes:
    - *node
    - *parent
    - *attribute
    - nodeName
    - attrName
    - type
    - value
*/
    XmpElement obj = nullXmpObj;
    QString nodeName = QString(node->name()).left(node->name_size());
    QString parName = "";
    if (parNode) parName = QString(parNode->name()).left(parNode->name_size());
    if (nodeName == name) {
        obj.node = node;
        obj.parent = parNode;
        obj.attr = nullptr;
        obj.nodeName = name;
        obj.type = "node";
        obj.value = QString(node->value()).left(node->value_size());
        return obj;
    }
    // node attributes
    rapidxml::xml_attribute<>* a;
    for(a = node->first_attribute(); a; a = a->next_attribute()) {
        QString attrName = QString(a->name()).left(a->name_size());
        if (attrName == name) {
            obj.node = node;
            obj.parent = parNode;
            obj.attr = a;
            obj.nodeName = nodeName;
            obj.attrName = attrName;
            obj.parentName = parName;
            obj.type = "attribute";
            obj.value = QString(a->value()).left(a->value_size());
            return obj;
        }
    }
    // child nodes
    rapidxml::xml_node<>* n;
    for(n = node->first_node(); n; n = n->next_sibling() ) {
        obj = xmlDocObj(name, n, node);
        if (!(obj == nullXmpObj)) return obj;
    }
    return obj;
}

void Xmp::walk(QTextStream &rpt, const rapidxml::xml_node<>* node, int indentSize, int indentCount)
{
    QString nodeName;
    QString attrName;
    QString attrVal;
    // indent for this node
    QString indentNodeStr = QString(indentCount*indentSize, ' ');
    QString indentElementStr = QString((indentCount+1)*indentSize, ' ');

    const rapidxml::node_type t = node->type();
    if (t == rapidxml::node_element) {
        // start node
        nodeName = QString(node->name()).left(node->name_size());
        rpt << indentNodeStr << "<" << nodeName;

        // node attributes
        if (node->first_attribute()) {
            const rapidxml::xml_attribute<>* a;
            rpt << "\n";
            for (a = node->first_attribute(); a; a = a->next_attribute()) {
                attrName = QString(a->name()).left(a->name_size());
                attrVal = QString(a->value()).left(a->value_size());
                rpt << indentElementStr << attrName << " = \"" << attrVal << "\"";
                if (a == node->last_attribute()) rpt << ">";
                rpt << "\n";
            }
        }
        else {
            rpt << ">";
            indentNodeStr = "";
        }

        // child nodes
        rapidxml::xml_node<>* n;
        for(n = node->first_node(); n; n = n->next_sibling() ) {
            walk(rpt, n, indentSize, indentCount+1);
        }

        // end node while unwinding recursion
        rpt << indentNodeStr << "</" << nodeName << ">\n";
        return;
    }

    if (t == rapidxml::node_data) {
        rpt << QString(node->value()).left(node->value_size());
        return;
    }
}

QString Xmp::docToQString()
{
/*
    Returns a formatted xml text string from the DOM document rapidxml::xml_document (xmlDoc).
*/
    if (G::isLogger) G::log(__FUNCTION__);

    /* format text using my walk function
    QString rptString = "";
    QTextStream rpt;
    rpt.setString(&rptString);
    walk(rpt, xmlDoc.first_node(), 3);
    //*/

    // format text using rapidxml::print
    std::string s;
    rapidxml::print(std::back_inserter(s), xmlDoc);
    return QString::fromStdString(s);
}

QByteArray Xmp::docToByteArray()
{
    return docToQString().toUtf8();
}

std::string Xmp::docToStdString()
{
    std::string s;
    rapidxml::print(std::back_inserter(s), xmlDoc);
    return s;
}

void Xmp::test(XmpElement o)
{
    qDebug() << __FUNCTION__
             << "name =" << o.nodeName
             << "attribute =" << o.attrName
             << "parent =" << o.parentName
             << "type =" << o.type
             << "value =" << o.value
             << "exists =" << o.exists()
                ;
}

