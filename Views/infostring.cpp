#include "Views/infostring.h"
#include "Main/global.h"

/*
    InfoString is called from MW to display or edit the info text displayed on top of an
    ImageView.

    InfoString provides the token information to build token strings in
    TokenDlg. TokenDlg requires two QStringLists: a token list and token examples list.

    InfoString includes a token parser, required to return the text string to MW.  For example,
    if the selected token string was:

        {MODEL} | {ShutterSpeed} sec at f/{Aperture}

    the function parseTokenString would get the token information from the datamodel and/or
    metadata and return the string (example):

        Nikon D5 | 1/250 sec at f/5.6
*/

#include "Main/mainwindow.h"
extern MW *mw4;
MW *mw4;
#define ep mw4->embelProperties

InfoString::InfoString(QWidget *parent, DataModel *dm, QSettings *setting) :
                       QWidget(parent)
{
    if (G::isLogger) G::log("InfoString::InfoString");
    mw4 = qobject_cast<MW*>(parent);
    this->dm = dm;
    this->setting = setting;
    initTokenList();
    initExampleMap();
    infoTemplates["Default info"] = "{Model} {FocalLength}  {ShutterSpeed} at {Aperture}, ISO {ISO}\n{Title}";

    // connect(this, &InfoString::updateInfo, imageView, &ImageView::updateShootingInfo);
}

void InfoString::usingToken()
{
/*
    Creates a list of items using a token template. This is used to prevent the user
    deleting a token template that is in use.
*/
    usingTokenMap.clear();
    usingTokenMap[loupeInfoTemplate] = "Shooting info template (view menu)";
    usingTokenMap["Default info"] = "Cannot delete default token template";
    setting->beginGroup("Embel/Templates");
    QStringList templates = setting->childGroups();
    setting->endGroup();
    for (int i = 0; i < templates.length(); ++i) {
        QString templateName = templates.at(i);
        QString templateTextPath = "Embel/Templates/" + templateName + "/Texts";
        setting->beginGroup(templateTextPath);
        QStringList texts = setting->childGroups();
        setting->endGroup();
        for (int j = 0; j < texts.length(); ++j) {
            QString path = templateTextPath + "/" + texts.at(j);
            QString sourceKey = path + "/source";
            QString source = setting->value(sourceKey).toString();
            if (sourceKey == "Test") continue;
            QString tokenKey = path + "/metadataTemplate";
            QString token = setting->value(tokenKey).toString();
            usingTokenMap[token] = "Embellish template: " + templateName;
        }
    }
}

QString InfoString::uniqueTokenName(QString name)
{
    setting->beginGroup("InfoTemplates");
    QStringList keys = setting->childKeys();
    setting->endGroup();
    bool nameExists = true;
    QString newName = name;
    int count = 0;
    while (nameExists) {
        if (keys.contains(newName)) newName = name + "_" + QString::number(++count);
        else nameExists = false;
    }
    return newName;
}

void InfoString::add(QMap<QString, QVariant> items)
{
/*
    Add a new tokenKey and tokenString.  This is used when an embellish template is read that
    contains new metadata token templates.
*/
    // get list of existing token keys in QSettings
    setting->beginGroup("InfoTemplates");
        QStringList existingTokenKeys = setting->childKeys();
    setting->endGroup();

    // iterate new items to add
    QMapIterator<QString, QVariant> i(items);
    while (i.hasNext()) {
        i.next();
        QString newKey = i.key();
        QString newValue = setting->value(newKey).toString();
        // check if already exists
        if (existingTokenKeys.contains(newKey)) {
            QString existingValue = setting->value(newKey).toString();
            if (existingValue == newValue) continue;
            newKey = uniqueTokenName(newKey);
        }
        setting->setValue("InfoTemplates/" + newKey, i.value());
        infoTemplates[newKey] = i.value().toString();
    }
}

void InfoString::change(std::function<void ()> updateInfoCallback)
{
    usingToken();
    int index = getCurrentInfoTemplateIndex();
    bool showInLoupeView = true;
    QString title = "Loupe View Info Token Editor";
    LoupeInfoDlg loupeInfoDlg(tokens, exampleMap, infoTemplates, usingTokenMap,
                              index, loupeInfoTemplate, updateInfoCallback, this);
    loupeInfoDlg.exec();

    // save any edits to settings
    setting->beginGroup("InfoTemplates");
        setting->remove("");
        QMapIterator<QString, QString> infoIter(infoTemplates);
        while (infoIter.hasNext()) {
            infoIter.next();
            setting->setValue(infoIter.key(), infoIter.value());
        }
    setting->endGroup();
}

int InfoString::getCurrentInfoTemplateIndex()
{
/*
    The loupeInfoTemplate index in the TokenDlg template combo. The loupeInfoTemplate is
    used to show an information overlay on the images (shooting info by default)
*/
    QMap<QString, QString>::iterator i;
    int index = 0;
    for (i = infoTemplates.begin(); i != infoTemplates.end(); ++i) {
       if (i.key() == loupeInfoTemplate) return index;
       index++;
    }
    return 0;
}

QString InfoString::getCurrentInfoTemplate()
{
    return loupeInfoTemplate;
}

void InfoString::initTokenList()
{
    tokens  << "Path"
            << "Filename"
            << "Type"
            << "Pick"
            << "Rating"
            << "Label"
            << "SizeBytes"
            << "MPix"
            << "CreateDate"
            << "YYYY"
            << "YY"
            << "MONTH"
            << "Month"
            << "MON"
            << "Mon"
            << "MM"
            << "DAY"
            << "Day"
            << "DDD"
            << "Ddd"
            << "DD"
            << "HOUR"
            << "MINUTE"
            << "SECOND"
            << "MILLISECOND"
            << "ModifiedDate"
            << "Dimensions"
            << "Width"
            << "Height"
            << "Rotation"
            << "Orientation"
            << "ShootingInfo"
            << "Aperture"
            << "ShutterSpeed"
            << "ISO"
            << "Exposure Compensation"
            << "Model"
            << "Lens"
            << "FocalLength"
            << "Creator"
            << "Title"
            << "Copyright"
            << "Email"
            << "Url"
            << "Err"
               ;
}

void InfoString::initExampleMap()
{
    exampleMap["Path"] = "users/rory/Pictures/2018/2018-02/2018-02-18_Spring in Paris/018-02-18_0046.cr2";
    exampleMap["Filename"] = "2018-02-18_0046.cr2";
    exampleMap["Type"] = "CR2";
    exampleMap["Pick"] = "True";
    exampleMap["Rating"] = "4";
    exampleMap["Label"] = "Red";
    exampleMap["SizeBytes"] = "3,163,237";
    exampleMap["MPix"] = "5.18";
    exampleMap["CreateDate"] = "2018-01-07 14:24:36";
    exampleMap["YYYY"] = "2018";
    exampleMap["YY"] = "18";
    exampleMap["MONTH"] = "JANUARY";
    exampleMap["Month"] = "January";
    exampleMap["MON"] = "JAN";
    exampleMap["Mon"] = "Jan";
    exampleMap["MM"] = "01";
    exampleMap["DAY"] = "WEDNESDAY";
    exampleMap["Day"] = "Wednesday";
    exampleMap["DDD"] = "WED";
    exampleMap["Ddd"] = "Wed";
    exampleMap["DD"] = "07";
    exampleMap["HOUR"] = "14";
    exampleMap["MINUTE"] = "24";
    exampleMap["SECOND"] = "36";
    exampleMap["MILLISECOND"] = "167";
    exampleMap["ModifiedDate"] = "2018-03-14 07:55:12";
    exampleMap["Dimensions"] = "5472x3648";
    exampleMap["Width"] = "5472";
    exampleMap["Height"] = "3648";
    exampleMap["Rotation"] = "0";
    exampleMap["Orientation"] = "0";
    exampleMap["ShootingInfo"] = "Canon EOS-1D X Mark II 840mm 1/250 sec at f/5.6, ISO 400";
    exampleMap["Aperture"] = "f/5.6";
    exampleMap["ShutterSpeed"] = "1/250";
    exampleMap["ISO"] = "400";
    exampleMap["Exposure Compensation"] = "-1";
    exampleMap["Model"] = "Canon EOS-1D X Mark II";
    exampleMap["Lens"] = "EF600mm f/4 IS II + 1.4x III";
    exampleMap["FocalLength"] = "840mm";
    exampleMap["Creator"] = "Rory Hill";
    exampleMap["Title"] = "Bald Eagle Snatching Fish";
    exampleMap["Copyright"] = "2018 Rory Hill";
    exampleMap["Email"] = "roryhill@something.com";
    exampleMap["Url"] = "roryhill.somewhere.com";
    exampleMap["Err"] = "Winnow failed to something.";
}

bool InfoString::parseToken(QString &tokenString, int pos,
                            QString &token, int &tokenEnd)
{
    if (pos <= 0) return false;
    if (pos >= tokenString.length()) return false;
    QChar ch = tokenString.at(pos);
    if (ch.unicode() == 8233) return false;  // Paragraph Separator
    if (ch == '{') return false;

    // look backwards
    bool foundPossibleTokenStart = false;
    int startPos = 0;
    for (int i = pos; i >= 0; i--) {
        ch = tokenString.at(i);
        if (i < pos && ch == '}') return false;
        if (ch == '{') {
            foundPossibleTokenStart = true;
            startPos = i + 1;
        }
        if (foundPossibleTokenStart) break;
    }

    if (!foundPossibleTokenStart) return false;

    // look forwards
    for (int i = pos; i < tokenString.length(); i++) {
        ch = tokenString.at(i);
        if (ch == '}') {
            for (int j = startPos; j < i; j++) {
                token.append(tokenString.at(j));
            }
            if (exampleMap.contains(token)) {
//                tokenStart = startPos - 1;
                tokenEnd = i + 1;
                return true;
            }
        }
    }
    return false;
}

QString InfoString::parseTokenString(QString &tokenString,
                                     QString &fPath,
                                     QModelIndex &idx)
{
/*
    Use when datamodel is available. setMetadata must be called first to set metadata
    variables for the current image.
*/
    QFileInfo info(fPath);
    QString ext = info.suffix().toLower();
    if (mw4->metadata->noMetadataFormats.contains(ext)) return "";
    m = dm->imMetadata(fPath);
    QString s;
    int tokenEnd;
    int i = 0;
    while (i < tokenString.length()) {
        QString token;
        if (parseToken(tokenString, i + 1, token, tokenEnd)) {
            s.append(tokenValue(token, info, fPath, idx));
            i = tokenEnd;
        }
        else {
            s.append(tokenString.at(i));
            i++;
        }
    }
    return s;
}

QString InfoString::parseTokenString(QString &tokenString,
                                     QString &fPath)
{
/*
   Use when datamodel is NOT available.  For example EmbelExport::exportFile, which can be
   called from startup arguments, and will not have the datamodel loaded for the file.
*/
    QFileInfo info(fPath);
    Metadata metadata;
    metadata.loadImageMetadata(info, dm->instance, true, true, false, false,
                               "InfoString::parseTokenString", /*isRemote*/true);
    //if (G::isFileLogger) Utilities::log("InfoString::parseTokenString  title =", metadata.m.title);
    QString s;
    int tokenEnd;
    int i = 0;
    while (i < tokenString.length()) {
        QString token;
        if (parseToken(tokenString, i + 1, token, tokenEnd)) {
            s.append(tokenValue(token, info, fPath, metadata.m));
            i = tokenEnd;
        }
        else {
            s.append(tokenString.at(i));
            i++;
        }
    }
    return s;
}

QString InfoString::tokenValue(QString &token,
                               QFileInfo &info,
                               QString &fPath,
                               QModelIndex &idx)
{
/*
    Finds the token in the datamodel and returns the datamodel value.
*/
    if (token == "Path")
        return fPath;
    if (token == "Filename")
        return info.fileName();
    if (token == "Type")
        return info.suffix().toUpper();
    if (token == "Pick") {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        return pickIdx.data(Qt::EditRole).toString();
    }
    if (token == "Rating") {
        QModelIndex ratingIdx = dm->sf->index(idx.row(), G::RatingColumn);
        return ratingIdx.data(Qt::EditRole).toString();
    }
    if (token == "Label") {
        QModelIndex labelIdx = dm->sf->index(idx.row(), G::LabelColumn);
        return labelIdx.data(Qt::EditRole).toString();
    }
    if (token == "SizeBytes") {
        QFileInfo info(fPath);
        return QLocale(QLocale::English).toString(info.size());
    }
    if (token == "MPix") {
        return QString::number((m.width * m.height) / 1000000.0, 'f', 1);
    }
    if (token == "CreateDate")
        return m.createdDate.toString("yyyy-MM-dd hh:mm:ss");
    if (token == "YYYY")
        return m.createdDate.date().toString("yyyy");
    if (token == "YY")
        return m.createdDate.date().toString("yy");
    if (token == "MONTH")
        return m.createdDate.date().toString("MMMM").toUpper();
    if (token == "Month")
        return m.createdDate.date().toString("MMMM");
    if (token == "MON")
        return m.createdDate.date().toString("MMM").toUpper();
    if (token == "Mon")
        return m.createdDate.date().toString("MMM");
    if (token == "MM")
        return m.createdDate.date().toString("MM");
    if (token == "DAY")
        return m.createdDate.date().toString("dddd").toUpper();
    if (token == "Day")
        return m.createdDate.date().toString("dddd");
    if (token == "DDD")
        return m.createdDate.date().toString("ddd").toUpper();
    if (token == "Ddd")
        return m.createdDate.date().toString("ddd");
    if (token == "DD")
        return m.createdDate.date().toString("dd");
    if (token == "HOUR")
        return m.createdDate.time().toString("hh");
    if (token == "MINUTE")
        return m.createdDate.time().toString("mm");
    if (token == "SECOND")
        return m.createdDate.time().toString("ss");
    if (token == "MILLISECOND")
        return m.createdDate.time().toString("zzz");
    if (token == "ModifiedDate")
        return info.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    if (token == "Dimensions")
        return m.dimensions;
    if (token == "Width")
        return QString::number(m.width);
    if (token == "Height")
        return QString::number(m.height);
    if (token == "Rotation")
        return QString::number(m.rotationDegrees);
    if (token == "Orientation")
        return QString::number(m.orientation);
    if (token == "ShootingInfo")
        return m.shootingInfo.trimmed();
    if (token == "Aperture")
        return m.aperture.trimmed();
    if (token == "ShutterSpeed")
        return m.exposureTime.trimmed();
    if (token == "ISO")
        return m.ISO.trimmed();
    if (token == "Exposure Compensation")
        return m.exposureCompensation.trimmed();
    if (token == "Model")
        return m.model.trimmed();
    if (token == "Lens")
        return m.lens.trimmed();
    if (token == "FocalLength")
        return m.focalLength.trimmed();
    if (token == "Creator")
        return m.creator.trimmed();
    if (token == "Title")
        return m.title.trimmed();
    if (token == "Copyright")
        return m.copyright.trimmed();
    if (token == "Email")
        return m.email.trimmed();
    if (token == "Url")
        return m.url.trimmed();
    return "";
}

QString InfoString::tokenValue(QString &token,
                               QFileInfo &info,
                               QString &fPath,
                               ImageMetadata &m)
{
/*
Finds the token in the datamodel and returns the datamodel value.
*/
    QString msg = "Token = " + token + "fPath = " + fPath;
    if (token == "Path")
        return fPath;
    if (token == "Filename")
        return info.fileName();
    if (token == "Type")
        return info.suffix().toUpper();
    if (token == "Pick") {
        return "N/A";
    }
    if (token == "Rating") {
        return "N/A";
    }
    if (token == "Label") {
        return "N/A";
    }
    if (token == "SizeBytes") {
        QFileInfo info(fPath);
        return QLocale(QLocale::English).toString(info.size());
    }
    if (token == "MPix") {
        return QString::number((m.width * m.height) / 1000000.0, 'f', 1);
    }
    if (token == "CreateDate")
        return m.createdDate.toString("yyyy-MM-dd hh:mm:ss");
    if (token == "YYYY")
        return m.createdDate.date().toString("yyyy");
    if (token == "YY")
        return m.createdDate.date().toString("yy");
    if (token == "MONTH")
        return m.createdDate.date().toString("MMMM").toUpper();
    if (token == "Month")
        return m.createdDate.date().toString("MMMM");
    if (token == "MON")
        return m.createdDate.date().toString("MMM").toUpper();
    if (token == "Mon")
        return m.createdDate.date().toString("MMM");
    if (token == "MM")
        return m.createdDate.date().toString("MM");
    if (token == "DAY")
        return m.createdDate.date().toString("dddd").toUpper();
    if (token == "Day")
        return m.createdDate.date().toString("dddd");
    if (token == "DDD")
        return m.createdDate.date().toString("ddd").toUpper();
    if (token == "Ddd")
        return m.createdDate.date().toString("ddd");
    if (token == "DD")
        return m.createdDate.date().toString("dd");
    if (token == "HOUR")
        return m.createdDate.time().toString("hh");
    if (token == "MINUTE")
        return m.createdDate.time().toString("mm");
    if (token == "SECOND")
        return m.createdDate.time().toString("ss");
    if (token == "MILLISECOND")
        return m.createdDate.time().toString("zzz");
    if (token == "ModifiedDate")
        return info.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    if (token == "Dimensions")
        return m.dimensions;
    if (token == "Width")
        return QString::number(m.width);
    if (token == "Height")
        return QString::number(m.height);
    if (token == "Rotation")
        return QString::number(m.rotationDegrees);
    if (token == "Orientation")
        return QString::number(m.orientation);
    if (token == "ShootingInfo")
        return m.shootingInfo;
    if (token == "Aperture")
        return m.aperture;
    if (token == "ShutterSpeed")
        return m.exposureTime;
    if (token == "ISO")
        return m.ISO;
    if (token == "Exposure Compensation")
        return m.exposureCompensation;
    if (token == "Model")
        return m.model;
    if (token == "Lens")
        return m.lens;
    if (token == "FocalLength")
        return m.focalLength;
    if (token == "Creator")
        return m.creator;
    if (token == "Title")
        return m.title;
    if (token == "Copyright")
        return m.copyright;
    if (token == "Email")
        return m.email;
    if (token == "Url")
        return m.url;
    // if (token == "Err") {
    //     QString err = "";
    //     for (const auto& s : G::err[m.fPath]) err += s + "\n";
    //     return err;
    // }
    return "";
}
// END InfoString
