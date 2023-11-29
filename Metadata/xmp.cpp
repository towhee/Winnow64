#include "xmp.h"

/*

Xmp reads xmp tags from from the source file buffer (xmpBa), which could be from an image
file or an xmp sidecar file. If there are no offsets to the buffer then a sidecar file
will be used.

Each time a new folder is selected and the datamodel is rebuilt the instance is
incremented.  The instance is used to detect clashes between calls to Xmp from a thread
that is out-of-date, working on datamodel information from a prior folder.

Read/Write to a sidecar:   Xmp xmp(file, instance);
Read/Write to image file:  Xmp xmp(file, offset, nextOffset, instance);  // no writing yet

Terms:

    "Element" is a node or attribute in the xmp. If it is an attribute then the parent is
    the node. If it is a node then the parent is the parent node.

    "Item" is a string to identify the associated element ie rating, label... All items
    are converted to lower case.

    "Node" is an element enclosed with < and /> brackets.

    "Attribute" is an element name/value inside a node ie rating = 5. The node is the
    parent.

    "Schema" is the definition for the element ie "xmp", "dc", "aux"...

    "Namespace" is the source of the schema ie xmlns:xmp = "http://ns.adobe.com/xap/1.0/"

Xmp uses the library rapidxml to parse the xmp text into a DOM rapidxml::xml_document.
The library uses pointers for all operations, so any data passed to rapidxml must be Xmp
class variables to insure their lifespan. Two QByteArrayList, a (element name) and v
(element value), are used.

The struct XmpElement is used to describe everything Xmp needs to find, create, get and
set items (ie rating, label, title etc). The hash of XmpElement called defineElements
contains the definitions for all the items that Winnow uses including: rating, label,
createdate, modifydate, title, rights, creator, lens, lensserialnumber, serialnumber,
email, url, orientation and winnowaddthumb.

Xmp::xmlDocElement searches xmlDoc for an item. If found, it returns a XmpElement with
the pointers to it in xmlDoc and its name, parent name and value. The element can be
matched in definedElements for more information.

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

Xmp::Xmp(QFile &file, int instance, QObject *parent) :  QObject(parent)
{
    if (G::isLogger) G::log("Xmp::Xmp");
    this->instance = instance;
    if (file.exists()) {
        filePath = QFileInfo(file).filePath();
        xmpBa = file.readAll();
    }
    initialize();
//    if (err == Xmp::ParseFailed) fix();
}

Xmp::Xmp(QFile &file, uint offset, uint length, int instance, QObject *parent) :  QObject(parent)
{
    if (G::isLogger) G::log("Xmp::Xmp");
    this->instance = instance;
    xmpSegmentOffset = offset;
    if (file.exists()) {
        filePath = QFileInfo(file).filePath();
        file.seek(offset);
        xmpBa = file.read(length);
        xmpmetaStart = xmpBa.indexOf("<x:xmpmeta");
        xmpmetaEnd = xmpBa.indexOf("</x:xmpmeta", xmpmetaStart + 20) + 12;
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
           "\n\t\t\trdf:about=\"\">"
           "\n\t\t</rdf:Description>\n"
           "\t\t<Iptc4xmpCore:CreatorContactInfo rdf:parseType='Resource'>\n"
           "\t\t</Iptc4xmpCore:CreatorContactInfo>\n"
           "\t</rdf:RDF>\n"
           "</x:xmpmeta>";
}

void Xmp::initialize()
{
/*
    Create a rapidXml xmlDoc from xmpBa.  If this fails then quit.  Otherwise, check that
    xmlDoc contains the node rdf:Description with the attribute rdf:about.
*/
    if (G::isLogger) G::log("Xmp::initialize");

    err = Err::NoErr;
    errMsg.resize(Err::Count);
    errMsg[Err::NoErr] = "Successful parse";
    errMsg[Err::ParseFailed] = "XMP parsing failed";
    errMsg[Err::NoRoot] = "No root node";
    errMsg[Err::InvalidRoot] = "Root element not 'x:xmpmeta'";
    errMsg[Err::NoRdf] = "Element 'rdf:RDF' is missing";
    errMsg[Err::NoRdfDescription] = "Element 'rdf:Description' is missing";
    errMsg[Err::NoRdfAbout] = "Element 'rdf:about' is missing";

    if (xmpBa.length() == 0) xmpBa = skeleton();

    // build xmlDoc
    enum {PARSE_FLAGS = rapidxml::parse_non_destructive};
    try {
        xmlDoc.parse<0>(xmpBa.data());    // 0 means default parse flags
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Runtime error was: " << e.what() << std::endl;
        err = Err::ParseFailed;
        return;
    }
    catch (const rapidxml::parse_error& /*e*/) {
//        std::cerr << "Parse error: " << e.what() << "  " << e.where<char>()
//                  << "  " << filePath.toStdString() << std::endl;
        err = Err::ParseFailed;
        return;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception error: " << e.what() << std::endl;
        err = Err::ParseFailed;
        return;
    }
    catch (...) {
        std::cerr << "Unknown error" << std::endl;
        err = Err::ParseFailed;
        return;
    }

    nullXmpElement.node = nullptr;
    nullXmpElement.attr = nullptr;
    nullXmpElement.parent = nullptr;
    nullXmpElement.name = "";
    nullXmpElement.parentName = "";

    // check xmp integrity

    // check first node = x:xmpmeta
    rootNode = xmlDoc.first_node();
    if (rootNode) {
        if (xmlNodeName(rootNode) != "x:xmpmeta") {
            err = Err::InvalidRoot;
        }
    }
    else {
        err = Err::NoRoot;
    }

    // check for node = rdf:RDF
    rapidxml::xml_node<>* n;
    for (n = rootNode->first_node(); n; n = n->next_sibling() ) {
        if (xmlNodeName(n) == "rdf:RDF") {
            rdfNode = n;
        }
        if (rdfNode) break;
    }
    if (!rdfNode) err = Err::NoRdf;

    // check for node = rdf:Description
    if (rdfNode) {
        rdfDescriptionNode = xmlDocElement("rdf:Description", xmlDoc.first_node()).node;
        if (!rdfDescriptionNode) {
             err = Err::NoRdfDescription;
        }
    }

    // check for attribute = rdf:about
    if (rdfDescriptionNode) {
        rdfAbout = xmlDocElement("rdf:about", rdfDescriptionNode).attr;
        if (!rdfAbout) {
            err = Err::NoRdfAbout;
        }
    }

    // okay to use xmp
    isValid = (err == Err::NoErr || err == Err::NoRdfAbout);

    // re-entry to replace invalid xmp with skeleton, then definedElements already populated
    if (definedElements.count()) {
        return;
    }

    XmpElement e;

    // rating
    e.name = "xmp:Rating";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "xmp";
    definedElements["rating"] = e;

    // label
    e.name = "xmp:Label";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "xmp";
    definedElements["label"] = e;

    // create date
    e.name = "xmp:CreateDate";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "xmp";
    definedElements["createdate"] = e;

    // modify date
    e.name = "xmp:ModifyDate";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "xmp";
    definedElements["modifydate"] = e;

    // subject
    e.name = "dc:subject";
    e.parentName = "rdf:Description";
    e.type = ElementType::List;
    e.schema = "dc";
    definedElements["subject"] = e;

    // title
    e.name = "dc:title";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "dc";
    definedElements["title"] = e;

    // copyright
    e.name = "dc:rights";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "dc";
    definedElements["rights"] = e;

    // creator
    e.name = "dc:creator";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "dc";
    definedElements["creator"] = e;

    // email
    e.name = "Iptc4xmpCore:CiEmailWork";
    e.parentName = "Iptc4xmpCore:CreatorContactInfo";
    e.type = ElementType::Node;
    e.schema = "Iptc4xmpCore";
    definedElements["email"] = e;

    // url
    e.name = "Iptc4xmpCore:CiUrlWork";
    e.parentName = "Iptc4xmpCore:CreatorContactInfo";
    e.type = ElementType::Node;
    e.schema = "Iptc4xmpCore";
    definedElements["url"] = e;

    // serial number
    e.name = "aux:SerialNumber";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "aux";
    definedElements["serialnumber"] = e;

    // lens
    e.name = "aux:Lens";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "aux";
    definedElements["lens"] = e;

    // lens serial number
    e.name = "aux:LensSerialNumber";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "aux";
    definedElements["lensserialnumber"] = e;

    // winnow added thumb flag
    e.name = "winnow:WinnowAddThumb";
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;
    e.schema = "winnow";
    definedElements["winnowaddthumb"] = e;

    // schema rdf:Description attributes to add if missing
    e.node = rdfDescriptionNode;
    e.parentName = "rdf:Description";
    e.type = ElementType::Attribute;

    // about
    e.name = "rdf:about";
    e.value = "";
    definedElements["about"] = e;

    // xmp xchema namespace
    e.name = "xmlns:xmp";
    e.value = "http://ns.adobe.com/xap/1.0/";
    definedElements["xmp"] = e;

    // dc schema namespace
    e.name = "xmlns:dc";
    e.value = "http://purl.org/dc/elements/1.1/";
    definedElements["dc"] = e;

    // aus schema namespace
    e.name = "xmlns:aux";
    e.value = "http://ns.adobe.com/exif/1.0/aux/";
    definedElements["aux"] = e;

    // Iptc4xmpCore schema namespace
    e.name = "xmlns:Iptc4xmpCore";
    e.value = "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/";
    definedElements["Iptc4xmpCore"] = e;

    // winnow schema namespace
    e.name = "xmlns:winnow";
    e.value = "http://winnow.ca/1.0/";
    definedElements["winnow"] = e;

    // schema node elements to add if missing

    // rdf node
    e.name = "rdf:RDF";
    e.parentName = "x:xmpmeta";
    e.type = ElementType::Node;
    definedElements["rdf"] = e;

    // rdf description node
    e.name = "rdf:Description";
    e.parentName = "rdf:RDF";
    e.parent = rdfNode;
    e.type = ElementType::Node;
    definedElements["rdfDescription"] = e;
}

void Xmp::fix()
{
/*
    Deal with errors before any updates ie Xmp::setItem
*/
    if (G::isLogger) G::log("Xmp::fix");

    switch (err) {
    case Err::NoErr:
    case Err::Count:
        break;
    case Err::InvalidRoot:
    case Err::ParseFailed:
    case Err::NoRoot:
    case Err::NoRdf:
    case Err::NoRdfDescription:
        qDebug() << "Xmp::fix" << err << filePath;
        xmpBa.clear();
        xmlDoc.clear();
        initialize();
        break;
    case Err::NoRdfAbout:
        QString aboutName = definedElements["about"].name;
        a.append(aboutName.toUtf8());
        v.append("");
        int idx = a.count() - 1;
        rapidxml::xml_attribute<> *attr = xmlDoc.allocate_attribute(a[idx], v[idx]);
        rdfDescriptionNode->append_attribute(attr);
        break;
    } // end switch
}

bool Xmp::includeSchemaNamespace(QString item)
/*
    item is "rating", "label" ...

    Every item added to the xmp document must have a namespace defined as an attribute of
    rdf:Description.  This function looks up the schema for the item, checks to see if the
    namespace already exists, and if not, adds the namespace to rdf:description.  Example
    with namespaces for the schemas xmp, dc, tiff and Iptc4xmpCore added.

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
    if (G::isLogger) G::log("Xmp::includeSchemaNamespace");

    // ie item = modifydate, elementName = xmp:modifydate then schema = xmp
    QString schema = definedElements[item].schema;

    // schema namespace would = xmlns:xmp
    QString schemaNamespace = "xmlns:" + schema;

    // check if schemaNamespace exists in xmlDoc
    if (!xmlDocElement(schemaNamespace, rdfDescriptionNode).exists()) {
        // a and v are QByteArrayList to satisfy rapidxml pointer lifespan requirement
        a.append(schemaNamespace.toUtf8());
        v.append(definedElements[schema].value.toUtf8());
        int idx = a.count() - 1;
        /*
        qDebug() << "Xmp::includeSchemaNamespace" << "Adding schema namespace/value" << a[idx] << v[idx];
        //*/
        rapidxml::xml_attribute<> *attr = xmlDoc.allocate_attribute(a[idx], v[idx]);
        rdfDescriptionNode->insert_attribute(rdfAbout, attr);
    }
    return true;
}

bool Xmp::writeJPG(QByteArray &buffer)
{
/*
    To be completed if decide to write into source image files instead of sidecars
*/
    if (G::isLogger) G::log("Xmp::writeJPG");
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
    if (G::isLogger) G::log("Xmp::writeSidecar");
    sidecarFile.resize(0);
    qint64 bytesWritten = sidecarFile.write(docToQString().toUtf8());
    if (bytesWritten == 0) return false;
    return true;
}

//QString Xmp::getItem(QByteArray item)
//{
//    return getItem1(item).toString();
//}

QString Xmp::getItem(QByteArray item)
{
/*
    Returns the value for the item.  ie getItem("creator") = "Rory"
    items: Rating, Label, title ...
    item case does not matter
*/
    if (G::isLogger) G::log("Xmp::getItem");

    if (!G::dmEmpty && G::instanceClash(instance, "Xmp::getItem")) {
        if (G::isFileLogger) Utilities::log("Xmp::getItem Instance clash", item);
        return "";
    }

    item = item.toLower();
    /*
    qDebug() << "\n" << "Xmp::getItem" << item << definedElements[item].name << definedElements.contains(item);
    //*/
    if (!definedElements.contains(item)) {
        qWarning() << "WARNING" << "Xmp::getItem" << item << "not found in xmpObjs";
        return "";
    }
    XmpElement element = xmlDocElement(definedElements[item].name, xmlDoc.first_node());
     if (!element.exists()) return "";
    // found a simple node or an attribute that has a value
    if (!element.value.isEmpty()) return element.value;
    QStringList propertyElements;
    // check if child node is a property element
    propertyElements << "rdf:Seq" << "rdf:Bag" << "rdf:Alt";
    // try drilling into node found
    rapidxml::xml_node<> *node = element.node->first_node();
    if (node == 0) return "";
    QString nodeName = xmlNodeName(node);

    if (propertyElements.contains(nodeName)) {
        rapidxml::xml_node<> *list = node->first_node();
        if (list == 0) return "";
        nodeName = xmlNodeName(list);
        if (nodeName == "rdf:li") {
            QString val = xmlNodeValue(list);
//            qDebug() << "Xmp::getItem" << item << val << list->first_attribute() << list->last_attribute();
            if (val != "") return val;
            rapidxml::xml_attribute<>* a = list->first_attribute();
            if (a == 0) return "";
            // added 2022-10-11
            rapidxml::xml_attribute<>* b = list->last_attribute();
            if (a == b) return "";
            // end added
            return xmlAttributeName(a);
        }
    }
    return "";
}

QStringList Xmp::getItemList(QByteArray item)
{
    if (G::isLogger) G::log("Xmp::getItemList");

    item = item.toLower();
    QStringList valList;
    /*
    qDebug() << "\n" << "Xmp::getItemList" << item << definedElements[item].name << definedElements.contains(item);
    //*/
    if (!definedElements.contains(item)) {
        qWarning() << "WARNING" << "Xmp::getItemList" << item << "not found in xmpObjs";
        return valList;
    }
    XmpElement element = xmlDocElement(definedElements[item].name, xmlDoc.first_node());
     if (!element.exists()) return valList;
    // found a simple node or an attribute that has a value
    if (!element.value.isEmpty()) return valList;
//    if (!element.value.isEmpty()) return element.value;
    QStringList propertyElements;
    // check if child node is a property element
    propertyElements << "rdf:Seq" << "rdf:Bag" << "rdf:Alt";
    // try drilling into node found
    rapidxml::xml_node<> *node = element.node->first_node();
    if (node == 0) return valList;
    QString nodeName = xmlNodeName(node);

    rapidxml::xml_node<> *n;
    QString val;
    if (propertyElements.contains(nodeName)) {
        for (n = node->first_node(); n; n = n->next_sibling()) {
            if (n == 0) return valList;
            nodeName = xmlNodeName(n);
            if (nodeName == "rdf:li") {
                val = xmlNodeValue(n);
                valList << val;
                if (val != "") continue;
                rapidxml::xml_attribute<>* a = n->first_attribute();
                if (a == 0) continue;
                val = xmlAttributeName(a);
                valList << val;
            }
        }
    }
    return valList;
}

bool Xmp::setItem(QByteArray item, QByteArray value)
{
/*
    This function updates the xmp value for the item in xmlDoc. Items include "rating",
    "label", "url" ... Items are the keys to the hash definedElements.

    Check Xmp::err before calling this function.  If Xmp::err then call Xmp::fix(), then call
    this function.  See Metadata::writeXmp example.

    Finding and defining an element is done by Xmp::xmlDocElement, which searches xmlDoc for
    the item and creates an XmpElement with all the element data.

    Each item is added to an attribute list (a) and a value list (v).  This is necessary as
    xmlDoc only contains pointers, so the data resides in the attributes and values, which must
    have lifespans equal to the Xmp class.

    If an item already exists, it is removed and the new item and value are appended as an
    attribute to the node <rdf:Description>. This will convert any items that were in the node
    format to the attribute format.  For example:
        Node format:        <xmp:Rating>5</xmp:Rating>
        Attribute format:   xmp:Rating = "5"
*/
    if (G::isLogger) G::log("Xmp::setItem");

    // confirm valid xmp. This should be checked before calling this function.
    if (err) return false;

    // check valid item.
    item = item.toLower();
    if (!definedElements.contains(item)) {
        qWarning() << "WARNING" << "Xmp::setItem" << "failed for" << item;
        return false;
    }

    // check the item schema namespace has been defined in rdf:description
    if (!includeSchemaNamespace(item)) return false;

    // if item exists in xmp remove item so we can replace
    XmpElement element = xmlDocElement(definedElements[item].name, xmlDoc.first_node());
    if (element.exists()) {
        if (element.type == ElementType::Attribute)
            element.node->remove_attribute(element.attr);
        if (element.type == ElementType::Node)
            element.parent->remove_node(element.node);
    }

    // get default XmpObj for item
    element = definedElements[item];

    // item and value lifetime must be same as xmpDoc, so save in QByteArrayList a and v
    if (element.type == ElementType::Attribute && value != "") {
        XmpElement parElement = xmlDocElement(element.parentName, xmlDoc.first_node());
        if (!parElement.exists()) return false; // create parent node if missing!!
        // class lifetime variables
        a.append(element.name.toUtf8());
        v.append(value);
        // get last index for the append list
        int idx = a.count() - 1;
        // append attribute and value
        rapidxml::xml_attribute<> *attr = xmlDoc.allocate_attribute(a[idx], v[idx]);
        parElement.node->append_attribute(attr);
    }
    if (element.type == ElementType::Node) {
        XmpElement parElement = xmlDocElement(element.parentName, xmlDoc.first_node());
        if (!parElement.exists()) return false;  // create parent node if missing!!
        // class lifetime variables
        a.append(element.name.toUtf8());
        v.append(value);
        // get just appended list index
        int idx = a.count() - 1;
        // append node and value
        rapidxml::xml_node<> *node = xmlDoc.allocate_node(rapidxml::node_element, a[idx], v[idx]);
        parElement.node->append_node(node);
    }
    return true;
}

Xmp::XmpElement Xmp::xmlDocElement(QString name,
                                   rapidxml::xml_node<> *node,
                                   rapidxml::xml_node<> *parNode)
{
/*
    Return an XmlElement struct for the element name, starting from *node in xmlDoc
    and recursing until found.

    Note that parNode is not used for intial function call, only when recursing.

    Element info includes:
    - *node
    - *parent
    - *attribute
    - name
    - parentName
    - type (node vs attribute)
    - value
    - valueList (if contains list ie subject = list of key words)

    Input:

        "name" is the xmp element name ie "dc:title".

        "node" is the node to start search under ie choose "x:xmpmeta" (root node) to search
        entire document.
*/
    XmpElement element = nullXmpElement;
    QString nodeName = xmlNodeName(node);
    QString parName = "";
    if (parNode) {
        parName = xmlNodeName(parNode);
    }
    /*
    qDebug() << "Xmp::xmlDocElement"
             << "Node name =     " << nodeName
             << "Node parent name =" << parName
             << "Node value =" << xmlNodeValue(node)
//             << "element.name =" << element.name
                ;
    //*/

    if (nodeName == name) {
        element.node = node;
        element.parent = parNode;
        element.attr = nullptr;
        element.name = name;
        element.type = ElementType::Node;
        element.value = xmlNodeValue(node);
        return element;
    }
    // node attributes
    rapidxml::xml_attribute<>* a;
    for (a = node->first_attribute(); a; a = a->next_attribute()) {
        QString attrName = xmlAttributeName(a);
        /*
        qDebug() << "Xmp::xmlDocElement"
                 << "Attribute name =" << attrName
                 << "element.name =" << element.name
                    ;
                    //*/
        if (attrName == name) {
            element.node = node;
            element.parent = parNode;
            element.attr = a;
            element.name = attrName;
            element.parentName = nodeName;
            element.type = ElementType::Attribute;
            element.value = xmlAttributeValue(a);
            return element;
        }
    }
    // recurse child nodes
    rapidxml::xml_node<>* n;
    for (n = node->first_node(); n; n = n->next_sibling() ) {
        element = xmlDocElement(name, n, node);
        if (!(element == nullXmpElement)) return element;
    }
    return element;
}

void Xmp::walk(QTextStream &rpt, rapidxml::xml_node<>* node, int indentSize, int indentCount)
{
/*
    Format "rpt" from xmlDoc.

    Replaced by
        rapidxml::print(std::back_inserter(s), xmlDoc);
    in Xmp::docToQString()
*/
    QString nodeName;
    QString attrName;
    QString attrVal;
    // indent for this node
    QString indentNodeStr = QString(indentCount*indentSize, ' ');
    QString indentElementStr = QString((indentCount+1)*indentSize, ' ');

    const rapidxml::node_type t = node->type();
    if (t == rapidxml::node_element) {
        // start node
        nodeName = xmlNodeName(node);
        rpt << indentNodeStr << "<" << nodeName;

        // node attributes
        if (node->first_attribute()) {
            rapidxml::xml_attribute<>* a;
            rpt << "\n";
            for (a = node->first_attribute(); a; a = a->next_attribute()) {
                attrName = xmlAttributeName(a);
                attrVal = xmlAttributeValue(a);
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
        for (n = node->first_node(); n; n = n->next_sibling() ) {
            walk(rpt, n, indentSize, indentCount+1);
        }

        // end node while unwinding recursion
        rpt << indentNodeStr << "</" << nodeName << ">\n";
        return;
    }

    if (t == rapidxml::node_data) {
        rpt << xmlNodeValue(node);
        return;
    }
}

QString Xmp::srcToString()
{
/*
    Returns unicode string from QByteArray xmpBa
*/
    if (G::isLogger) G::log("Xmp::srcToString");
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    return QTextCodec::codecForMib(106)->toUnicode(xmpBa);  // deprecated in Qt 6
#else
    return QString::fromUtf8(xmpBa);
#endif
}

QString Xmp::docToQString()
{
/*
    Returns a formatted xml text string from the DOM document rapidxml::xml_document (xmlDoc).
*/
    if (G::isLogger) G::log("Xmp::docToQString");

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

void Xmp::report(XmpElement o)
{
/*
    for debugging
*/
    QString elType;
    o.type == ElementType::Node ? elType = "node" : elType = "attribute";
    qDebug() << "Xmp::report"
             << "\n  name =" << o.name
             << "\n  parent =" << o.parentName
             << "\n  type =" << elType
             << "\n  exists =" << o.exists()
             << "\n  value =" << o.value
             << "\n  value list =" << o.valueList
                ;
}

