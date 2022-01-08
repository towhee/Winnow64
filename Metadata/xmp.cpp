#include "xmp.h"

/*
Xmp reads and writes xmp tags to the QByteArray buffer. The buffer is read from the source
file. If there are no offsets then a sidecar file will be used. If there is no xmp data in the
source file or the file format is not documented then the xmp tags are written to a sidecar
file.

Read/Write to a sidecar:   Xmp xmp(file);
Read/Write to image file:  Xmp xmp(file, offset, nextOffset);

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
           "\n\t\t</rdf:Description>\n\t</rdf:RDF>\n</x:xmpmeta>";
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

    if (xmpBa.length() == 0) xmpBa.append(sidecarSkeleton);

//    enum {PARSE_FLAGS = rapidxml::parse_non_destructive};
    enum {PARSE_FLAGS = rapidxml::parse_default};
    try {
        xmlDoc.parse<PARSE_FLAGS>(xmpBa.data());    // 0 means default parse flags
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

    lookupHash["rating_attr"] = "rdf:Description|xmp:Rating";
    lookupHash["rating_node"] = "Rating|";
    lookupHash["label_attr"] = "rdf:Description|xmp:Label";
    lookupHash["label_node"] = "Label|";
    lookupHash["createdate_attr"] = "rdf:Description|xmp:CreateDate";
    lookupHash["createdate_node"] = "CreateDate|";
    lookupHash["modifydate_attr"] = "rdf:Description|xmp:ModifyDate";
    lookupHash["modifydate_node"] = "ModifyDate|";
    lookupHash["title_attr"] = "rdf:Description|dc:title";
    lookupHash["title_node"] = "dc:title|";
    lookupHash["rights_attr"] = "rdf:Description|dc:rights";
    lookupHash["rights_node"] = "dc:rights|";
    lookupHash["creator_attr"] = "rdf:Description|dc:creator";
    lookupHash["creator_node"] = "dc:creator|";
    lookupHash["lens_attr"] = "rdf:Description|aux:Lens";
    lookupHash["lens_node"] = "aux:Lens|";
    lookupHash["lensserialnumber_attr"] = "rdf:Description|aux:LensSerialNumber";
    lookupHash["lensserialnumber_node"] = "aux:LensSerialNumber|";
    lookupHash["email_attr"] = "rdf:Description|Iptc4xmpCore:CiEmailWork";
    lookupHash["email_node"] = "Iptc4xmpCore:CiEmailWork|";
    lookupHash["url_attr"] = "rdf:Description|Iptc4xmpCore:CiUrlWork";
    lookupHash["url_node"] = "Iptc4xmpCore:CiUrlWork|";
    lookupHash["orientation_attr"] = "rdf:Description|tiff:Orientation";
    lookupHash["orientation_node"] = "tiff:Orientation|";
    lookupHash["winnowaddthumb_attr"] = "rdf:Description|xmp:WinnowAddThumb";
    lookupHash["winnowaddthumb_node"] = "xmp:WinnowAddThumb|";

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

    buildHash(xmlDoc.first_node());
//    qDebug() << __FUNCTION__ << xmpHash;
    isValid = true;
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
                xmpHash.insert(nodeName + "|", value);
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
                xmpHash.insert(nodeName + "|" + attrName, value);
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
        xmpHash.insert(nodeName + "|", value);
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

bool Xmp::writeSidecar()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QFile sidecarFile(filePath);
    qint64 fileBytesToWrite = sidecarFile.size();
    sidecarFile.open(QIODevice::WriteOnly);
    qint64 bytesWritten = sidecarFile.write(xmpBa);
//    if (bytesWritten == 0) failedToCopy << sidecarPath;
    sidecarFile.close();
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

QString Xmp::attribute(QByteArray attrName, QByteArray nodeName)
/*

  */
{
    if (G::isLogger) G::log(__FUNCTION__);
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

    QByteArray lookup = item.toLower().append("_attr");
    if (lookupHash.contains(lookup)) {  // try attribute
        QString key = lookupHash[lookup];
        if (xmpHash.contains(key)) {
            return xmpHash[key];
        }
        else {  // try node
            lookup = item.toLower().append("_node");
            if (lookupHash.contains(lookup)) {  // try attribute
                QString key = lookupHash[lookup];
                if (xmpHash.contains(key)) {
                    return xmpHash[key];
                }
            }
        }
    }
    return "";

    // OLD CODE...
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
    int endPos = 0;
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
        // use QTextDocument to convert xmp escape coded characters ie &#39; = apostrophe
        QTextDocument d;
        d.setHtml(xmpBa.mid(startPos, endPos - startPos));
        return d.toPlainText();

        // this works, but fails with html escape codes
//        return QTextCodec::codecForMib(106)->toUnicode(result);
    }
    else {
        return "";
        // use hand graphic to denote "not found" vs found but = "" (blank)
//        return "🖐";
    }
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
    if (G::isLogger) G::log(__FUNCTION__);

    QByteArray lookup = item.toLower().append("_attr");
    qDebug() << __FUNCTION__ << "lookup =" << lookup;
    if (lookupHash.contains(lookup)) {  // try attribute
        QString key = lookupHash[lookup];
        qDebug() << __FUNCTION__ << "key =" << key;
        QStringList xmlObj =  key.split("|");
        if (xmlObj.count() < 2) return false;
        // item and value lifetime must be same as xmpDoc, so save in QByteArrayList
        a.append(xmlObj.at(1).toUtf8());
        v.append(value);
        qDebug() << __FUNCTION__ << xmlObj;
        if (xmpHash.contains(key)) {    // edit existing value

        }
        else {  // append value to xmlRdfDescriptionNode
            int i = a.count() - 1;
            qDebug() << __FUNCTION__ << "Append attribute =" << a[i] << "value =" << v[i];
            rapidxml::xml_attribute<> *attr = xmlDoc.allocate_attribute(a[i], v[i]);
            xmlRdfDescriptionNode->append_attribute(attr);
        }
        qDebug() << __FUNCTION__ << xmpBa;
        qDebug() << __FUNCTION__ << "Report:";
        std::cout << report().toStdString() << std::endl << std::flush;
    }
    return true;

    // OLD CODE ...
    // ie schema = "Rating"
    QByteArray schema = schemaHash[item];

    // ie "xmp:Rating"
    QByteArray tag = schema;
    tag.append(":");
    tag.append(item);

    // make sure schema exists in xmp
    insertSchemas(item);

    /*
    qDebug() << "Xmp::setItem  item =" << item
             << "schema =" << schema
             << "tag =" << tag
             << "xmpBa =" << xmpBa
                ;
             //*/

    // search for item in case it already exists in xmp side car file
    int startPos = xmpBa.indexOf(tag, xmpmetaStart);

    // does item exist already
    if (startPos == -1) {
        // not found, create new item
        QByteArray newItem;
        newItem.clear();
        startPos = schemaInsertPos(schema);     // determines assignmentMethod
        if (schema == "xmp" || schema == "tiff" || schema == "aux") {
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
        if (schema == "dc") {
            /* ie
             ie <dc:title><rdf:Alt><rdf:li xml:lang="x-default">This is the title</rdf:li></rdf:Alt></dc:title>
             ie <dc:creator><rdf:Seq><rdf:li>Rory Hill</rdf:li></rdf:Seq></dc:creator>
            //*/
            newItem.append("\n\t\t\t<");
            newItem.append(tag);
            if (useLanguage.contains(tag)) {
                newItem.append("><rdf:Alt><rdf:li xml:lang=\"x-default\">");
                newItem.append(value);
                newItem.append("</rdf:li></rdf:Alt></");
            }
            else {
                newItem.append("><rdf:Seq><rdf:li>");
                newItem.append(value);
                newItem.append("</rdf:li></rdf:Seq></");
            }
            newItem.append(tag);
            newItem.append(">");
            xmpBa.insert(startPos, newItem);
        }
        if (schema == "Iptc4xmpCore") {
            /* ie
             <Iptc4xmpCore:CreatorContactInfo rdf:parseType='Resource'>
                  <Iptc4xmpCore:CiEmailWork>hillrg@mail.com</Iptc4xmpCore:CiEmailWork>  // 1st item
                  <Iptc4xmpCore:CiUrlWork>hill.com</Iptc4xmpCore:CiEmailWork>           // 2nd item
              </Iptc4xmpCore:CreatorContactInfo>
            //*/

            // does a Iptc4xmpCore:CreatorContactInfo item already exist?
            int pos = xmpBa.indexOf("<Iptc4xmpCore:Ci", startPos);
            // no, this is the 1st item: build header and add item
            if (pos == -1) {
                newItem.append("\n\t\t\t<Iptc4xmpCore:CreatorContactInfo rdf:parseType='Resource'>");
                newItem.append("\n\t\t\t\t<");
                newItem.append(tag);
                newItem.append(">");
                newItem.append(value);
                newItem.append("</");
                newItem.append(tag);
                newItem.append(">");
                newItem.append("\n\t\t\t<Iptc4xmpCore:CreatorContactInfo>");
            }
            // 2nd item, already header: just insert 2nd item above 1st item
            else {
                startPos = pos;
                newItem.append("<");
                newItem.append(tag);
                newItem.append(">");
                newItem.append(value);
                newItem.append("</");
                newItem.append(tag);
                newItem.append(">\n\t\t\t\t");
            }
            xmpBa.insert(startPos, newItem);
        }
        // item created
        return true;
    }

    // item exists, replace item
    int endPos;
    bool foundItem = false;
    startPos += tag.length();

    if (schema == "dc") {
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
            else return false;
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

void Xmp::walk(QTextStream &rpt, const rapidxml::xml_node<>* node, int indentSize, int indentCount)
{
    QString nodeName;
    QString attrName;
    QString attrVal;
    // indent for this node
    QString indentNodeStr = QString(indentCount*indentSize, ' ');
    QString indentElementStr = QString((indentCount+1)*indentSize, ' ');
    rpt << indentNodeStr;

    const rapidxml::node_type t = node->type();
    if (t == rapidxml::node_element) {
        // start node
        nodeName = QString(node->name()).left(node->name_size());
        rpt << "<" << nodeName << "\n";

        // node attributes
        const rapidxml::xml_attribute<>* a;
        for(a = node->first_attribute(); a; a = a->next_attribute()) {
            attrName = QString(a->name()).left(a->name_size());
            attrVal = QString(a->value()).left(a->value_size());
            rpt << indentElementStr << attrName << " = \"" << attrVal << "\"";
            if (a == node->last_attribute()) rpt << ">";
            rpt << "\n";
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
        rpt << QString(node->value()).left(node->value_size()) << "\n";
        return;
    }

    rpt << "Node type:" << t;
}

QString Xmp::report()
{
/*
    Not being used.
*/
    if (G::isLogger) G::log(__FUNCTION__);

//    using namespace rapidxml;
//    rapidxml::xml_document<> xmlDoc;    // character type defaults to char
//    enum {PARSE_FLAGS = rapidxml::parse_non_destructive};
//    xmlDoc.parse<PARSE_FLAGS>(xmpBa.data());    // 0 means default parse flags
    QTextStream rpt;
    QString rptString = "";
    rpt.setString(&rptString);
    walk(rpt, xmlDoc.first_node(), 3);
//    std::cout << std::flush;
    return rptString;

    /*
    std::cout << "Name of my first node is: " << xmlDoc.first_node()->name() << "\n" << std::flush;
    rapidxml::xml_node<> *node = xmlDoc.first_node();
    std::cout << "Node foobar has value " << node->value() << "\n" << std::flush;
    for (rapidxml::xml_attribute<> *attr = node->first_attribute();
         attr; attr = attr->next_attribute())
    {
        std::cout << "Node has attribute " << attr->name() << " ";
        std::cout << "with value " << attr->value() << "\n" << std::flush;
    }
    qDebug() << __FUNCTION__ << "RAPIDXML:";

//    rapidxml::print(std::cout, doc, 0);   // 0 means default printing flags
//*/
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
