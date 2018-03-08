#include "Views/infostring.h"

InfoString::InfoString(QWidget *parent, Metadata *metadata, DataModel *dm) :
                       QWidget(parent)
{
    this->dm = dm;
    m = metadata;
    initTokenMap();
    infoTemplates[" Default"] = "{Model} {FocalLength}  {ShutterSpeed} at f/{Aperture}, ISO {ISO}\n{Title}";
    // "{Model} {FocalLength}  {ShutterSpeed} sec at f/{Aperture}, ISO {ISO}\n{Title}"
}

void InfoString::editTemplates()
{
    int index = getIndex();
    TokenDlg *tokenDlg = new TokenDlg(tokenMap, infoTemplates, index,
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
    return 0;
}

//void InfoString::setCurrentInfoTemplate(QString &currentInfoTemplate)
//{
//    current = currentInfoTemplate;
//}

QString InfoString::getCurrentInfoTemplate()
{
    return currentInfoTemplate;
}

void InfoString::initTokenMap()
{
//    tokenMap["⏎"] = "";
    tokenMap["Path"] = "users/rory/Pictures/2018/2018-02/2018-02-18_Spring in Paris/018-02-18_0046.cr2";
    tokenMap["Filename"] = "2018-02-18_0046.cr2";
    tokenMap["Type"] = "CR2";
    tokenMap["Pick"] = "True";
    tokenMap["Rating"] = "4";
    tokenMap["Label"] = "Red";
    tokenMap["SizeBytes"] = "3,163,237";
//    tokenMap["SizeMB"] = "3,163,237";
    tokenMap["MPix"] = "5.18";
    tokenMap["CreateDate"] = "2018-01-07 14:24:36";
    tokenMap["YYYY"] = "2018";
    tokenMap["YY"] = "18";
    tokenMap["MONTH"] = "JANUARY";
    tokenMap["Month"] = "January";
    tokenMap["MON"] = "JAN";
    tokenMap["Mon"] = "Jan";
    tokenMap["MM"] = "01";
    tokenMap["DAY"] = "WEDNESDAY";
    tokenMap["Day"] = "Wednesday";
    tokenMap["DDD"] = "WED";
    tokenMap["Ddd"] = "Wed";
    tokenMap["DD"] = "07";
    tokenMap["HOUR"] = "14";
    tokenMap["MINUTE"] = "24";
    tokenMap["SECOND"] = "36";
    tokenMap["ModifiedDate"] = "2018-03-14 07:55:12";
    tokenMap["Dimensions"] = "5472x3648";
    tokenMap["Width"] = "5472";
    tokenMap["Height"] = "3648";
    tokenMap["Rotation"] = "0";
    tokenMap["Orientation"] = "0";
    tokenMap["ShootingInfo"] = "Canon EOS-1D X Mark II 840mm 1/250 sec at f/5.6, ISO 400";
    tokenMap["Aperture"] = "5.6";
    tokenMap["ShutterSpeed"] = "1/250";
    tokenMap["ISO"] = "400";
    tokenMap["Model"] = "Canon EOS-1D X Mark II";
    tokenMap["Lens"] = "EF600mm f/4 IS II + 1.4x III";
    tokenMap["FocalLength"] = "840mm";
    tokenMap["Creator"] = "Rory Hill";
    tokenMap["Title"] = "Bald Eagle Snatching Fish";
    tokenMap["Copyright"] = "2018 Rory Hill";
    tokenMap["Email"] = "roryhill@something.com";
    tokenMap["Url"] = "roryhill.somewhere.com";
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
            if (tokenMap.contains(token)) {
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
    if (token == "⏎")
        return "\n";
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
        uint width = m->width;
        uint height = m->height;
        return QString::number((width * height) / 1000000.0, 'f', 1);
    }
    if (token == "CreateDate")
        return m->createdDate.toString("yyyy-MM-dd hh:mm:ss");
    if (token == "YYYY")
        return m->createdDate.date().toString("yyyy");
    if (token == "YY")
        return m->createdDate.date().toString("yy");
    if (token == "MONTH")
        return m->createdDate.date().toString("MMMM").toUpper();
    if (token == "Month")
        return m->createdDate.date().toString("MMMM");
    if (token == "MON")
        return m->createdDate.date().toString("MMM").toUpper();
    if (token == "Mon")
        return m->createdDate.date().toString("MMM");
    if (token == "MM")
        return m->createdDate.date().toString("MM");
    if (token == "DAY")
        return m->createdDate.date().toString("dddd").toUpper();
    if (token == "Day")
        return m->createdDate.date().toString("dddd");
    if (token == "DDD")
        return m->createdDate.date().toString("ddd").toUpper();
    if (token == "Ddd")
        return m->createdDate.date().toString("ddd");
    if (token == "DD")
        return m->createdDate.date().toString("dd");
    if (token == "HOUR")
        return m->createdDate.date().toString("hh");
    if (token == "MINUTE")
        return m->createdDate.date().toString("mm");
    if (token == "SECOND")
        return m->createdDate.date().toString("ss");
    if (token == "ModifiedDate")
        return info.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    if (token == "Dimensions")
        return m->dimensions;
    if (token == "Width")
        return QString::number(m->width);
    if (token == "Height")
        return QString::number(m->height);
    if (token == "Rotation")
        return QString::number(m->rotationDegrees);
    if (token == "Orientation")
        return QString::number(m->orientation);
    if (token == "ShootingInfo")
        return m->shootingInfo;
    if (token == "Aperture")
        return m->aperture;
    if (token == "ShutterSpeed")
        return m->exposureTime;
    if (token == "ISO")
        return m->ISO;
    if (token == "Model")
        return m->model;
    if (token == "Lens")
        return m->lens;
    if (token == "FocalLength")
        return m->focalLength;
    if (token == "Creator")
        return m->creator;
    if (token == "Title")
        return m->title;
    if (token == "Copyright")
        return m->copyright;
    if (token == "Email")
        return m->email;
    if (token == "Url")
        return m->url;
    return "";
}


// END InfoString
