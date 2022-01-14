#include "xmp.h"

/*
Xmp reads and writes xmp tags to the QByteArray buffer xmpBa. The buffer is read from the
source file. If there are no offsets then a sidecar file will be used. If there is no xmp data
in the source file or the file format is not documented then the xmp tags are written to a
sidecar file.

Read/Write to a sidecar:   Xmp xmp(file);
Read/Write to image file:  Xmp xmp(file, offset, nextOffset);

Xmp uses the library rapidxml to parse the xmp text into a DOM rapidxml::xml_document. The
library uses pointers for all operations, so any variables passed to rapidxml must be Xmp
class variables to insure their lifespan. Also, since the xml nodes are in a tree arrangement,
recursive routines are used to find nodes, which make it difficult to return information.
Class level instantiations of xmlNode and xmlAttribute are used to point to the node and
attribute found.  getNode() and getAttribute define xmlNode and xmlAttribute respectively.

At initialization, the function buildHash() populates xmpHash with all the node and
node/attribute objects in the xmp document.  This makes it easier to determine if a node or
node/attribute exists and where to find an attribute.

Example sidecar file that lightroom successfully read/writes:

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
    }
    initialize();
}

QByteArray Xmp::skeleton()
{
    return "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">"
           "\n\t<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
           "\n\t\t<rdf:Description "
           "\n\t\t\trdf:about=\"\">"
           "\n\t\t</rdf:Description>\n"
           "\t\t<Iptc4xmpCore:CreatorContactInfo rdf:parseType='Resource'>\n"
           "\t\t</Iptc4xmpCore:CreatorContactInfo>\n"
           "\t</rdf:RDF>\n"
           "</x:xmpmeta>";
}

void Xmp::test(XmpObj o)
{
//    QString s = "xmp:Rating";
//    XmpObj o = xmlDocObj(s, xmlDoc.first_node());
    qDebug() << __FUNCTION__
             << "name =" << o.nodeName
             << "attribute =" << o.attrName
             << "parent =" << o.parentName
             << "type =" << o.type
             << "value =" << o.value
             << "exists =" << o.exists()
                ;

}

void Xmp::initialize()
{
    if (G::isLogger) G::log(__FUNCTION__);

    xmpmetaStart = xmpBa.indexOf("<x:xmpmeta");
    xmpmetaEnd = xmpBa.indexOf("</x:xmpmeta", xmpmetaStart + 20) + 12;
    xmpPacketEnd = xmpBa.indexOf("<?xpacket end", xmpmetaEnd);
    xmpmetaRoom = xmpPacketEnd - xmpmetaStart;
    xmpmetaOffset = xmpSegmentOffset + xmpmetaStart;
    int xmpmetaLength = xmpmetaEnd - xmpmetaStart;
    xmpBa = xmpBa.mid(xmpmetaStart, xmpmetaLength);

    if (xmpBa.length() == 0) xmpBa = skeleton();

    enum {PARSE_FLAGS = rapidxml::parse_non_destructive};
//    enum {PARSE_FLAGS =
//            rapidxml::parse_default |
//            rapidxml::parse_no_string_terminators/* |
//            rapidxml::parse_declaration_node |
//            rapidxml::parse_no_data_nodes*/
//         };
    try {
        xmlDoc.parse<0>(xmpBa.data());    // 0 means default parse flags
//        xmlDoc.parse<PARSE_FLAGS>(xmpBa.data());    // 0 means default parse flags
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

    nullXmpObj.nodeName = "";
    nullXmpObj.attrName = "";
    nullXmpObj.parentName = "";

    XmpObj xmpObj;
    xmpObj.nodeName = "rdf:Description"; xmpObj.attrName = "xmp:Rating"; xmpObj.parentName = ""; xmpObj.type = "attribute"; xmpObjs["rating"] = xmpObj;
    xmpObj.nodeName = "rdf:Description"; xmpObj.attrName = "xmp:Label"; xmpObj.parentName = ""; xmpObj.type = "attribute"; xmpObjs["label"] = xmpObj;
    xmpObj.nodeName = "rdf:Description"; xmpObj.attrName = "xmp:CreateDate"; xmpObj.parentName = ""; xmpObj.type = "attribute"; xmpObjs["createdate"] = xmpObj;
    xmpObj.nodeName = "rdf:Description"; xmpObj.attrName = "xmp:ModifyDate"; xmpObj.parentName = ""; xmpObj.type = "attribute"; xmpObjs["modifydate"] = xmpObj;
    xmpObj.nodeName = "rdf:Description"; xmpObj.attrName = "dc:title"; xmpObj.parentName = ""; xmpObj.type = "attribute"; xmpObjs["title"] = xmpObj;
    xmpObj.nodeName = "rdf:Description"; xmpObj.attrName = "xmp:rights"; xmpObj.parentName = ""; xmpObj.type = "attribute"; xmpObjs["rights"] = xmpObj;
    xmpObj.nodeName = "rdf:Description"; xmpObj.attrName = "xmp:creator"; xmpObj.parentName = ""; xmpObj.type = "attribute"; xmpObjs["creator"] = xmpObj;
    xmpObj.nodeName = "Iptc4xmpCore:CiEmailWork"; xmpObj.attrName = ""; xmpObj.parentName = "Iptc4xmpCore:CreatorContactInfo"; xmpObj.type = "node"; xmpObjs["email"] = xmpObj;
    xmpObj.nodeName = "Iptc4xmpCore:CiUrlWork"; xmpObj.attrName = ""; xmpObj.parentName = "Iptc4xmpCore:CreatorContactInfo"; xmpObj.type = "node"; xmpObjs["url"] = xmpObj;


    /*
    xmpSchemaList << "Rating"
                  << "Label"
                  << "ModifyDate"
                  << "CreateDate"
                  << "WinnowAddThumb"
                     ;
    dcSchemaList << "title"
                 << "rights"
                 << "creator"
                    ;
    auxSchemaList << "Lens"
                  << "LensSerialNumber"
                  << "SerialNumber"
                     ;
    iptc4xmpCoreSchemaList << "CiUrlWork"
                           << "CiEmailWork"
                              ;

    useLanguage << "dc:title"
                << "dc:rights"
                   ;
    */
    xmpItems["rating"] = "xmp:Rating";
    xmpItems["label"] = "xmp:Label";
    xmpItems["createdate"] = "xmp:CreateDate";
    xmpItems["modifydate"] = "xmp:ModifyDate";
    xmpItems["title"] = "dc:title";
    xmpItems["rights"] = "dc:rights";
    xmpItems["creator"] = "dc:creator";
    xmpItems["lens"] = "aux:Lens";
    xmpItems["lensserialnumber"] = "aux:LensSerialNumber";
    xmpItems["email"] = "Iptc4xmpCore:CiEmailWork";
    xmpItems["url"] = "Iptc4xmpCore:CiUrlWork";
    xmpItems["orientation"] = "tiff:Orientation";
    xmpItems["winnowaddthumb"] = "xmp:WinnowAddThumb";
    /*
    schemaHash["Rating"] = "xmp";                   // read/write
    schemaHash["Label"] = "xmp";                    // read/write
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
    schemaHash["WinnowAddThumb"] = "xmp";           // read/write
    */
    buildHash(xmlDoc.first_node());
//    qDebug() << __FUNCTION__ << xmpHash;
    isValid = true;
//    std::cout << xmpBa.toStdString() << std::endl << std::flush;
//    std::cout << fromXmlDocument().toStdString() << std::endl << std::flush;


    return;
}

void Xmp::buildHash(rapidxml::xml_node<>* node, rapidxml::xml_node<>* baseNode)
{
    const rapidxml::node_type t = node->type();
    if (t == rapidxml::node_element) {
        QString nodeName = QString(node->name()).left(node->name_size());
        if (nodeName == "rdf:Description") xmlRdfDescriptionNode = node;
        QString attrName = "";
        if (!mapSkipNodes.contains(nodeName)) {
            if (node->value_size()) {
                QString value = QString(node->value()).left(node->value_size());
                xmpHashOld.insert(nodeName + "|", value);
                /*
                qDebug() << __FUNCTION__ << "node:"
                         << "node name ="
                         << QString(node->name()).left(node->name_size())
                         << "node value ="
                         << QString(node->value()).left(node->value_size())
                            ;
                            //*/
            }
             else {
                baseNode = node;
            }
            const rapidxml::xml_attribute<>* a;
            for(a = node->first_attribute(); a; a = a->next_attribute()) {
                attrName = QString(a->name()).left(a->name_size());
                QString value = QString(a->value()).left(a->value_size());
                xmpHashOld.insert(nodeName + "|" + attrName, value);
                /*
                qDebug() << __FUNCTION__ << "attr:"
                         << "node name ="
                         << QString(node->name()).left(node->name_size())
                         << "attr name ="
                         << QString(a->name()).left(a->name_size())
                         << "attr value ="
                         << QString(a->value()).left(a->value_size())
                            ;
                            //*/
            }
        }
        rapidxml::xml_node<>* n;
        for(n = node->first_node(); n; n = n->next_sibling() ) {
            buildHash(n, baseNode);
        }

    }
    if (t == rapidxml::node_data) {
        QString nodeName = QString(baseNode->name()).left(baseNode->name_size());
        QString value = QString(node->value()).left(node->value_size());
        xmpHashOld.insert(nodeName + "|", value);
        /*
        qDebug() << __FUNCTION__ << "data:"
                 << "base node name ="
                 << QString(baseNode->name()).left(baseNode->name_size())
                 << "data value ="
                 << QString(node->value()).left(node->value_size())
                    ;
                    //*/
    }
}

void Xmp::insertSchemas(QByteArray &item)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (schema == "Iptc4xmpCore") xmpBa.insert(pos, iptc4xmpCoreSchema);
    if (item == "Orientation") xmpBa.insert(pos, psSchema);
}

int Xmp::schemaInsertPos(QByteArray schema)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (schema == "Iptc4xmpCore")
        schemaStart = xmpBa.indexOf("xmlns:Iptc4xmpCore", xmpmetaStart);

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

bool Xmp::isItem(QByteArray item)
{
    if (G::isLogger) G::log(__FUNCTION__);
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

    if (startPos == -1) return false;
    else return true;
}

bool Xmp::getNode(rapidxml::xml_node<>* node, QByteArray nodeName)
{
/*
    xmlNode is assigned if found.  Make sure to set xmlNode = nullptr before calling.
*/
    if (xmlNode) return true;
    if (G::isLogger) G::log(__FUNCTION__);
    const rapidxml::node_type t = node->type();
    if (t == rapidxml::node_element) {
        QString thisNodeName = QString(node->name()).left(node->name_size());
        if (QString(nodeName) == thisNodeName) {
            xmlNode = node;
            return true;
        }
        // child nodes
        rapidxml::xml_node<>* n;
        for(n = node->first_node(); n; n = n->next_sibling() ) {
            getNode(n, nodeName);
        }
    }
    return false;
}

bool Xmp::getAttribute(QByteArray attrName, QByteArray nodeName)
/*

  */
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (nodeName == "rdf:Description") xmlNode = xmlRdfDescriptionNode;
    else getNode(xmlDoc.first_node(), nodeName);
    xmlAttribute = xmlNode->first_attribute(attrName);
    if (xmlAttribute == 0) { // attribute does not exist
        return false;
    }
    return true;

    /*
    rapidxml::xml_node<>* node = xmlDoc.first_node();
    xmlNode = nullptr;
    getNode(node, nodeName);
//    rapidxml::xml_node<>* node = xmlDoc.first_node()->first_node(testNode.constData());
    if (xmlNode == 0) qWarning() << __FUNCTION__ << "Failed to get node ";
    else qDebug() << __FUNCTION__ << "found node:" << QString(xmlNode->name()).left(xmlNode->name_size());
//    qDebug() << __FUNCTION__ << QString(node->name()).left(node->name_size());
    return QString(xmlNode->value()).left(xmlNode->value_size());
    rapidxml::xml_attribute<>* a = node->first_attribute("xmp:Rating");
    if (a == 0) { // attribute does not exist
        return "";
    }
    qDebug() << __FUNCTION__ << QString(a->value()).left(a->value_size());
    return QString(a->value()).left(a->value_size());
    */
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
    if (G::isLogger) G::log(__FUNCTION__);

    item = item.toLower();
    if (!xmpItems.contains(item)) return "";
    XmpObj obj = xmlDocObj(xmpItems[item], xmlDoc.first_node());
    if (!obj.exists()) return "";
    if (!obj.value.isEmpty()) return obj.value;
    QStringList propertyElements;
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
    /*
    qDebug();
    std::cout << "xmpBa to start:" << std::endl;
    std::cout << xmpBa.toStdString() << std::endl << std::flush;
    QCoreApplication::processEvents();
    qDebug() << __FUNCTION__ << "item =" << item << "value =" << value;
    //*/
    qDebug();
    item = item.toLower();
    qDebug() << __FUNCTION__ << item << value;
    if (!xmpItems.contains(item)) return false;

    // if item exists in xmp remove item so we can replace
    XmpObj obj = xmlDocObj(xmpItems[item], xmlDoc.first_node());
    qDebug() << __FUNCTION__ << "Remove object if exists already:";
    test(obj);
//    return false;
    if (obj.exists()) {
        if (obj.type == "attribute") obj.node->remove_attribute(obj.attr);
        if (obj.type == "node") obj.parent->remove_node(obj.node);
    }

    // get default XmpObj for item
    obj = xmpObjs[item];
    qDebug() << __FUNCTION__ << "Object to append (obj):";
    test(obj);

    // item and value lifetime must be same as xmpDoc, so save in QByteArrayList
    if (obj.type == "attribute") {
        XmpObj parObj = xmlDocObj(obj.nodeName, xmlDoc.first_node());
        if (!parObj.exists()) return false; // create parent node if missing!!
        a.append(obj.attrName.toUtf8());
        v.append(value);
        // get just appended list index
        int idx = a.count() - 1;
        // append attribute and value
        rapidxml::xml_attribute<> *attr = xmlDoc.allocate_attribute(a[idx], v[idx]);
        parObj.node->append_attribute(attr);
//        xmlRdfDescriptionNode->append_attribute(attr);
    }
    if (obj.type == "node") {
        XmpObj parObj = xmlDocObj(obj.parentName, xmlDoc.first_node());
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

Xmp::XmpObj Xmp::xmlDocObj(QString name,
                           rapidxml::xml_node<> *node,
                           rapidxml::xml_node<> *parNode)
{
    XmpObj obj = nullXmpObj;
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

bool Xmp::xmlDocContains(QString name, const rapidxml::xml_node<> *node)
{
    QString nodeName = QString(node->name()).left(node->name_size());
    if (nodeName == name) return true;
    // node attributes
    const rapidxml::xml_attribute<>* a;
    for(a = node->first_attribute(); a; a = a->next_attribute()) {
        QString attrName = QString(a->name()).left(a->name_size());
        if (attrName == name) return true;
    }
    // child nodes
    rapidxml::xml_node<>* n;
    for(n = node->first_node(); n; n = n->next_sibling() ) {
        if (xmlDocContains(name, n)) return true;
    }
    return false;
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

    QString rptString = "";

//    /*
    // format text using my walk function
//    QTextStream rpt;
//    rpt.setString(&rptString);
//    walk(rpt, xmlDoc.first_node(), 3);
    //*/

//    /*
    // format text using rapidxml::print
    std::string s;
    rapidxml::print(std::back_inserter(s), xmlDoc);
    std::cout << s << std::endl << std::flush;
    rptString = QString::fromStdString(s);
    //*/

    return rptString;
}

void Xmp::xmlDocRpt(QTextStream &rpt, const rapidxml::xml_node<> *node)
{

}

QString Xmp::diagnostics()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "XMP Diagnostics");
    rpt << "\n";
    rpt << "\n" << "xmpSegmentOffset = " << G::s((qulonglong)xmpSegmentOffset);
    rpt << "\n" << "xmpmetaOffset = " << G::s((qulonglong)xmpmetaOffset);
    rpt << "\n" << "xmpLength = " << G::s((qulonglong)xmpLength);
    rpt << "\n" << "xmpmetaStart = " << G::s((qulonglong)xmpmetaStart);
    rpt << "\n" << "xmpmetaEnd = " << G::s((qulonglong)xmpmetaEnd);
    rpt << "\n" << "xmpPacketEnd = " << G::s((qulonglong)xmpPacketEnd);
    rpt << "\n" << "xmpmetaRoom = " << G::s((qulonglong)xmpmetaRoom);
    rpt << "\n" << "assignmentMethod = " << G::s(assignmentMethod);
    rpt << "\n\n" ;
    return reportString;
}
