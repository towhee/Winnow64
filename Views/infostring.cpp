#include "Views/infostring.h"

/* InfoString is called from MW to display or edit the info text displayed on top of an
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

InfoString::InfoString(QWidget *parent, DataModel *dm) :
                       QWidget(parent)
{
    this->dm = dm;
    initTokenList();
    initExampleMap();
    infoTemplates[" Default"] = "{Model} {FocalLength}  {ShutterSpeed} at {Aperture}, ISO {ISO}\n{Title}";
}

void InfoString::editTemplates()
{
    int index = getIndex();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, infoTemplates, index,
          currentInfoTemplate, "Shooting Info in Image View", this);
    tokenDlg->exec();
}

int InfoString::getIndex()
{
/*
The currentInfoTemplate index in the TokenDlg template combo.
*/
    QMap<QString, QString>::iterator i;
    int index = 0;
    for (i = infoTemplates.begin(); i != infoTemplates.end(); ++i) {
       if (i.key() == currentInfoTemplate) return index;
       index++;
    }
    return 0;
}

QString InfoString::getCurrentInfoTemplate(int index)
{
/*
The currentInfoTemplate matching the index in the TokenDlg template combo
*/
    QMap<QString, QString>::iterator i;
    int ii = 0;
    for (i = infoTemplates.begin(); i != infoTemplates.end(); ++i) {
       if (ii == index) return i.key();
       ii++;
    }
    return "";
}

//void InfoString::setCurrentInfoTemplate(QString &currentInfoTemplate)
//{
//    current = currentInfoTemplate;
//}

QString InfoString::getCurrentInfoTemplate()
{
    return currentInfoTemplate;
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
            << "Model"
            << "Lens"
            << "FocalLength"
            << "Creator"
            << "Title"
            << "Copyright"
            << "Email"
            << "Url"
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
    exampleMap["Model"] = "Canon EOS-1D X Mark II";
    exampleMap["Lens"] = "EF600mm f/4 IS II + 1.4x III";
    exampleMap["FocalLength"] = "840mm";
    exampleMap["Creator"] = "Rory Hill";
    exampleMap["Title"] = "Bald Eagle Snatching Fish";
    exampleMap["Copyright"] = "2018 Rory Hill";
    exampleMap["Email"] = "roryhill@something.com";
    exampleMap["Url"] = "roryhill.somewhere.com";
}

bool InfoString::parseToken(QString &tokenString, int pos,
                            QString &token, int &tokenEnd)
{
    if (pos <= 0) return false;
    if (pos >= tokenString.length()) return false;
    QChar ch = tokenString.at(pos);
    if (ch.unicode() == 8233) return false;  // Paragraph Separator
    if (ch == "{") return false;

    // look backwards
    bool foundPossibleTokenStart = false;
    int startPos;
    for (int i = pos; i >= 0; i--) {
        ch = tokenString.at(i);
        if (i < pos && ch == "}") return false;
        if (ch == "{") {
            foundPossibleTokenStart = true;
            startPos = i + 1;
        }
        if (foundPossibleTokenStart) break;
    }

    if (!foundPossibleTokenStart) return false;

    // look forwards
    for (int i = pos; i < tokenString.length(); i++) {
        ch = tokenString.at(i);
        if (ch == "}") {
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
setMetadata must be called first to set metadata variables for the current
image.
*/
    QFileInfo info(fPath);
    m = dm->getMetadata(fPath);
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

QString InfoString::tokenValue(QString &token,
                               QFileInfo &info,
                               QString &fPath,
                               QModelIndex &idx)
{
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
        uint width = m.width;
        uint height = m.height;
        return QString::number((width * height) / 1000000.0, 'f', 1);
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
    return "";
}


// END InfoString
