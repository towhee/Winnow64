#include "renamefile.h"
#include "ui_renamefiledlg.h"

RenameFileDlg::RenameFileDlg(QWidget *parent,
                             QString &folderPath,
                             QStringList &selection,
                             QMap<QString,QString> &filenameTemplates,
                             DataModel *dm,
                             Metadata *metadata,
                             ImageCache *imageCache)
    : QDialog(parent),
      ui(new Ui::RenameFiles),
      dm(dm),
      metadata(metadata),
      imageCache(imageCache),
      folderPath(folderPath),
      selection(selection),
      filenameTemplatesMap(filenameTemplates)
{
    ui->setupUi(this);

    QString n = QString::number(selection.count());
    QString title;
    if (n == "1") title = "Rename " + n + " image";
    else title = "Rename " + n + " images";
    setWindowTitle(title);

    // Index list to avoid unique name issues while renaming
    for (int i = 0; i < selection.size(); i++) {
        selectionIndexes.append(dm->proxyIndexFromPath(selection.at(i)));
    }

    // initialize templates and tokens
    initTokenList();
    initExampleMap();

    if (filenameTemplatesMap.count() == 0) {
        filenameTemplatesMap["Original filename"] = "{ORIGINAL FILENAME}";
        filenameTemplatesMap["YYYY-MM-DD_XXXX"] = "{YYYY}-{MM}-{DD}_{XXXX}";
    }
    QMap<QString, QString>::iterator i;
    for (i = filenameTemplatesMap.begin(); i != filenameTemplatesMap.end(); ++i) {
        ui->filenameTemplatesCB->addItem(i.key());
    }
//    ui->filenameTemplatesCB->setCurrentIndex(filenameTemplateSelected);

    updateExample();
    isDebug = true;
}

void RenameFileDlg::debugShowDM(QString title)
{
    qDebug() << title;
    qDebug() << "dm->fPathRow hash:";
    QMap<int,QString> rowMap;
    for (auto i = dm->fPathRow.begin(), end = dm->fPathRow.end(); i != end; ++i)
        qDebug() << i.value() << "\t" << i.key();
        //rowMap.insert(i.value(), i.key());
//    for (int i = 0; i < rowMap.count(); i++)
//        qDebug() << i << "\t" << rowMap[i];
    qDebug() << "Datamodel:";
    for (int i = 0; i < dm->rowCount(); i++) {
        QString path = dm->index(i, G::PathColumn).data(G::PathRole).toString();
        QString name = dm->index(i, G::NameColumn).data().toString();
        qDebug() << i << "\tPath =" << path << "Name =" << name;
    }
//    qDebug() << " ";
}

void RenameFileDlg::appendAllSharingBaseName(QString path)
{
    QFileInfo info(path);
    QString baseName = info.baseName();
    if (baseNamesUsed.contains(baseName)) return;

    // append image file from datamodel selection (must come first)
    filesToRename.append({path, baseName});
    if (isDebug) {
        qDebug() << "RenameFileDlg::appendAllSharingBaseName"
                 << "base =" << baseName
                 << "fileToAppend =" << path
            ;
    }

    // append any auxillary or sidecar files sharing the basename
    for (int i = 0; i < QDir(folderPath).entryInfoList().size(); i++) {
        // ignore if source path
//        qDebug() << QDir(folderPath).entryInfoList().at(i).filePath().toLower() << path.toLower();
        if (QDir(folderPath).entryInfoList().at(i).filePath().toLower() == path.toLower())
            continue;
        QString base = QDir(folderPath).entryInfoList().at(i).baseName();
        if (base == baseName) {
            QString fileToAppend = QDir(folderPath).entryInfoList().at(i).filePath().toLower();
            filesToRename.append({fileToAppend, baseName});
            if (isDebug) {
                qDebug() << "RenameFileDlg::appendAllSharingBaseName"
                         << "base =" << base
                         << "fileToAppend =" << fileToAppend
                            ;
            }
        }
    }
}

void RenameFileDlg::rename()
{
/*
    Renames the base name for all the selected images.  Note that there could be multiple
    images with the same base name but only one might be selected.  For example,: image.nef,
    image.jpg and image.xmp.  Also, the renamed file could match an existing, but not
    selected, file.

    Structures:
    Selection                   selectionIndexes
    All files in folder         allFilesList
    All files for base name     baseNamesList
    All base names renamed      baseNamesUsedList
    All files to rename         filesToRenameList

    First iterate through the selection:
    • create a unique basename to prevent duplicating an existing file
*/
    QString tokenString = filenameTemplatesMap[ui->filenameTemplatesCB->currentText()];
    seqNum  = ui->spinBoxStartNumber->value();
    int pathCol = 0;    // filesToRename.at(i).at(pathCol)
    int baseCol = 1;    // filesToRename.at(i).at(baseCol)

    // Build list of all files to rename
    if (isDebug)  qDebug() << "FILE LIST TO RENAME:";
    filesToRename.clear();
    for (int i = 0; i < selectionIndexes.size(); i++) {
        int row = selectionIndexes.at(i).row();
        QString path = dm->sf->data(dm->sf->index(row, G::PathColumn), G::PathRole).toString();
        appendAllSharingBaseName(path);
    }

    /*
    // Assign unique names to filesToRename
    if (isDebug) qDebug() << "ASSIGN UNIQUE NAMES:";
    if (isDebug) debugShowDM("Before assign unique base names");
//    int iBase = 0;
//    QString prevOldPath = "";
    for (int i = 0; i < filesToRename.size(); i++) {
        QString oldPath = filesToRename.at(i).at(pathCol);
        QFileInfo info(oldPath);
        QString newPath = info.dir().path() + "/d78sn34_" + QString::number(i) + "." + info.suffix();

        if (oldPath.toLower() == newPath.toLower()) {
            if (isDebug) qDebug() << "RenameFileDlg::rename" << i << oldPath << newPath << "Nothing to do here";
            continue;
        }
        // if newPath already exists then get unique path using _x
        QString uniquePath = newPath;
        if (QFile(newPath).exists()) {
            Utilities::uniqueFilePath(uniquePath, "_");
        }

        // temp unique rename file
        QFile(oldPath).rename(uniquePath);
        QFile(oldPath).close();

        // update datamodel
        QModelIndex idx = dm->proxyIndexFromPath(oldPath);
        // might be a sidecar file not in datamodel
        if (idx.isValid()) {
            int row = idx.row();
            QModelIndex pathIdx = dm->sf->index(row, G::PathColumn);
            QModelIndex nameIdx = dm->sf->index(row, G::NameColumn);
            QString newName = Utilities::getFileName(uniquePath);
            dm->sf->setData(pathIdx, uniquePath, G::PathRole);
            dm->sf->setData(nameIdx, newName);
            dm->fPathRow.remove(oldPath);
            dm->fPathRow[uniquePath] = row;
            // update imageCache
            imageCache->rename(oldPath, uniquePath);
        }
        // update filesToRename
        filesToRename[i][pathCol] = uniquePath;

        if (isDebug)
            qDebug() << "RenameFileDlg::rename unique:"
                     << "oldPath =" << oldPath
                     << "uniquePath =" << uniquePath
                ;
    }
    if (isDebug) debugShowDM("After assign unique base names");
    */

    // Rename files using selected template
    if (isDebug) qDebug() << "\nRENAME FILES:";
    QString prevBaseName = "";
//    QString prevBaseName = filesToRename.at(0).at(baseCol);
    QString newBase = "";

    for (int i = 0; i < filesToRename.size(); i++) {
        QString iStr = QString::number(i);

        QString oldPath = filesToRename.at(i).at(pathCol);
        QString baseName = filesToRename.at(i).at(baseCol);
        QFileInfo info(oldPath);

        // is file an image in datamodel
        bool inDatamodel = dm->fPathRow.contains(oldPath.toLower());
        bool baseNameChange = prevBaseName != baseName;

        if (inDatamodel) {
            newBase = parseTokenString(info, tokenString);
        }

        if (isDebug) {
            qDebug()
                 << "\nFILE =" << i
                 << "oldPath =" << oldPath
                 << "baseName =" << baseName
                 << "prevBaseName =" << prevBaseName
                 << "newBase =" << newBase
                 << "baseNameChange =" << baseNameChange
                 << "inDatamodel =" << inDatamodel
                 << "seqNum =" << seqNum
                   ;
        }

        QString newName = newBase + "." + info.suffix();
        QString newPath = info.dir().path() + "/" + newName;

//        if (isDebug) debugShowDM(iStr + " File to rename = " + oldPath);

        if (isDebug) {
            qDebug() << "Renaming file"
                     << "oldPath =" << oldPath << "to"
                     << "newPath =" << newPath
                ;
        }

        // File rename oldPath to newPath
        QFile(oldPath).rename(newPath);
        QFile(oldPath).close();
        /*
        // Update datamodel
//        if (isDebug) debugShowDM("Check if dm->fPathRow contains " + oldPath);

//        bool found = false;
//        for (auto i = dm->fPathRow.begin(), end = dm->fPathRow.end(); i != end; ++i) {
//            if (i.key() == oldPath) found = true;
//            if (isDebug) qDebug()
//                    << "i.key() =" << i.key()
//                    << "oldPath =" << oldPath
//                    << "i.value() =" << i.value()
//                    << "found =" << found;
//        }

//        if (isDebug) qDebug() << "found =" << found << oldPath;  */

        // update datamodel
        if (inDatamodel) {
            int row = dm->fPathRow[oldPath.toLower()];
            QString rowStr = QString::number(row);
//            qDebug() << oldPath << "found.  Datamodel row =" << row;

            dm->fPathRow.remove(oldPath.toLower());
            dm->fPathRow[newPath.toLower()] = row;

//            if (isDebug) debugShowDM("Removed " + oldPath + " Added " + newPath + " row = " + rowStr);

            if (isDebug) {
                qDebug() << "RenameFileDlg::rename updating datamodel"
                         << "newPath =" << newPath
                         << "row =" << row
                    ;
            }
            QModelIndex pathIdx = dm->index(row, G::PathColumn);
            QModelIndex nameIdx = dm->index(row, G::NameColumn);
            if (pathIdx.isValid()) dm->setData(pathIdx, newPath, G::PathRole);
            if (nameIdx.isValid()) dm->setData(nameIdx, newName);

//            debugShowDM("After DM update for row = " + rowStr);

            // update imageCache
            imageCache->rename(oldPath, newPath);
//            debugShowDM("After DM update for row = " + rowStr);
        }

        if (baseNameChange) {
            seqNum++;
            prevBaseName = baseName;
        }
    }

    // update current image
    dm->currentFilePath = dm->currentSfIdx.data(G::PathRole).toString();
}

int RenameFileDlg::getSequenceStart(const QString &path)
{
    if (G::isLogger) G::log("RenameFile::getSequenceStart");
    QDir dir(path);
    if (!dir.exists()) return 0;

    // filter on images only
    QStringList fileFilters;
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters.append("*." + str);
    dir.setNameFilters(fileFilters);
    dir.setFilter(QDir::Files);

    QStringList numbers;
    numbers << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" <<"9";
    QString seq;    // existing number in file
    QString ch;     // one character
    int sequence = 0;
    bool foundNumber;
    for (int f = 0; f < dir.entryList().size(); ++f) {
        seq = "";
        QString fName = dir.entryList().at(f);
        int period = fName.indexOf(".", 0);
        if (period < 1) continue;
        foundNumber = false;
        for (int i = period; i > 0; i--) {
            ch = fName.mid(i, 1);
            if (numbers.contains(ch)) {
                foundNumber = true;
                seq.insert(0, ch);
            }
            else {
                if (foundNumber) {
                    if (seq.toInt() > sequence) sequence = seq.toInt();
                    break;
                }
            }
        }
    }

    return sequence;
}

void RenameFileDlg::updateExistingSequence()
{
/*
    The sequence is a part of the file name to make sure the file name is unique, and defined
    in the file name template with XX... (ie dscn0001.jpg).
*/
    if (G::isLogger) G::log("IngestDlg::updateExistingSequence");
//    if (isInitializing) return;

    QString tokenKey = ui->filenameTemplatesCB->currentText();
    if(tokenKey.length() == 0) return;

    QString tokenString = filenameTemplatesMap[tokenKey];

    // if not a sequence in the token string then disable and return
    if (!tokenString.contains("XX")) {
        Utilities::setOpacity(ui->startSeqLabel, 0.5);
        Utilities::setOpacity(ui->spinBoxStartNumber, 0.5);
        ui->spinBoxStartNumber->setDisabled(true);
        ui->existingSequenceLabel->setVisible(false);
        return;
    }

    // enable sequencing
    Utilities::setOpacity(ui->startSeqLabel, 1.0);
    Utilities::setOpacity(ui->spinBoxStartNumber, 1.0);
    ui->spinBoxStartNumber->setDisabled(false);
    ui->existingSequenceLabel->setVisible(true);

    QDir dir(folderPath);
    if (dir.exists()) {
        int sequenceNum = getSequenceStart(folderPath);
        if (ui->spinBoxStartNumber->value() < sequenceNum + 1)
            ui->spinBoxStartNumber->setValue(sequenceNum + 1);
        if (sequenceNum > 0)
            ui->existingSequenceLabel->setText("Folder exists and last image sequence found = "
                                               + QString::number(sequenceNum));
        else
            ui->existingSequenceLabel->setText("Folder exists but no sequenced images found");
    }
    else {
        ui->spinBoxStartNumber->setValue(1);
        ui->existingSequenceLabel->setText("");
    }
    seqNum = ui->spinBoxStartNumber->value();
}

void RenameFileDlg::updateExample()
{
    QFileInfo info(selection.at(0));
    QString tokenString = filenameTemplatesMap[ui->filenameTemplatesCB->currentText()];
    ui->exampleLbl->setText(parseTokenString(info, tokenString));
}

void RenameFileDlg::on_okBtn_clicked()
{
    rename();
    accept();
}

void RenameFileDlg::on_filenameTemplatesBtn_clicked()
{
/*
    Invoke the template token editor to edit an existing template or create a new one.
*/
    if (G::isLogger) G::log("RenameFileDlg::on_filenameTemplatesBtn_clicked");
    // setup TokenDlg
    // title is also used to filter warnings, so if you change it here also change
    // it in TokenDlg::updateUniqueFileNameWarning
    QString title = "Token Editor - Rename images";
    QMap<QString,QString> usingTokenMap;    // dummy
    int index = ui->filenameTemplatesCB->currentIndex();
    QString currentKey = ui->filenameTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, filenameTemplatesMap, usingTokenMap,
                                      index, currentKey, title, this);
    tokenDlg->exec();

    // rebuild template list and set to same item as TokenDlg for user continuity
    ui->filenameTemplatesCB->clear();
    QMap<QString, QString>::iterator i;
    int row = 0;
    for (i = filenameTemplatesMap.begin(); i != filenameTemplatesMap.end(); ++i) {
        ui->filenameTemplatesCB->addItem(i.key());
        if (i.key() == currentKey) index = row;
        row++;
    }
    ui->filenameTemplatesCB->setCurrentIndex(index);
    on_filenameTemplatesCB_currentTextChanged(currentKey);
}

void RenameFileDlg::on_filenameTemplatesCB_currentTextChanged(const QString &arg1)
{
    if (G::isLogger) G::log("RenameFileDlg::on_filenameTemplatesCB_currentTextChanged");
    if (arg1 == "") return;
    QString tokenString = filenameTemplatesMap[arg1];
//    if (!isInitializing) filenameTemplateSelected = ui->filenameTemplatesCB->currentIndex();
    updateExistingSequence();
    updateExample();
}

void RenameFileDlg::on_spinBoxStartNumber_textChanged(const QString /* &arg1 */)
{
    if (G::isLogger) G::log("IngestDlg::on_spinBoxStartNumber_textChanged");
    seqNum  = ui->spinBoxStartNumber->value();
    updateExample();
}

void RenameFileDlg::initTokenList()
{
/*
The list of tokens in the token editor will appear in this order.
*/
    if (G::isLogger) G::log("IngestDlg::initTokenList");
    tokens  << "ORIGINAL FILENAME"
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
            << "TITLE"
            << "CREATOR"
            << "COPYRIGHT"
            << "MAKE"
            << "MODEL"
            << "DIMENSIONS"
            << "SHUTTER SPEED"
            << "APERTURE"
            << "ISO"
            << "FOCAL LENGTH"
            << "XX"
            << "XXX"
            << "XXXX"
            << "XXXXX"
            << "XXXXXX"
            << "XXXXXXX"
               ;
}

void RenameFileDlg::initExampleMap()
{
    if (G::isLogger) G::log("IngestDlg::initExampleMap");
    exampleMap["ORIGINAL FILENAME"] = "_C8I0024";
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
    exampleMap["HOUR"] = "08";
    exampleMap["MINUTE"] = "32";
    exampleMap["SECOND"] = "45";
    exampleMap["MILLISECOND"] = "167";
    exampleMap["TITLE"] = "Hill_Wedding";
    exampleMap["CREATOR"] = "Rory Hill";
    exampleMap["COPYRIGHT"] = "2018 Rory Hill";
    exampleMap["MAKE"] = "Canon";
    exampleMap["MODEL"] = "Canon EOS-1D X Mark II";
    exampleMap["DIMENSIONS"] = "5472x3648";
    exampleMap["SHUTTER SPEED"] = "1/1000 sec";
    exampleMap["APERTURE"] = "f/5.6";
    exampleMap["ISO"] = "1600";
    exampleMap["FOCAL LENGTH"] = "840 mm";
    exampleMap["XX"] = "01";
    exampleMap["XXX"] = "001";
    exampleMap["XXXX"] = "0001";
    exampleMap["XXXXX"] = "00001";
    exampleMap["XXXXXX"] = "000001";
    exampleMap["XXXXXXX"] = "0000001";
}

bool RenameFileDlg::isToken(QString tokenString, int pos)
{
    if (G::isLogger) G::log("IngestDlg::isToken");
    if (pos >= tokenString.length()) return false;
//    qDebug() << "IngestDlg::isToken  tokenString =" << tokenString << pos;
    QChar ch = tokenString.at(pos);
    if (ch.unicode() == 8233) return false;     // Paragraph Separator
    if (ch == '{') return false;                // qt6.2 changed " to '
    if (pos == 0) return false;

    // look backwards
    bool foundPossibleTokenStart = false;
    int startPos = 0;
    for (int i = pos; i >= 0; i--) {
        ch = tokenString.at(i);
        if (i < pos && ch == '}') return false; // qt6.2 changed " to '
        if (ch == '{') {                        // qt6.2 changed " to '
            foundPossibleTokenStart = true;
            startPos = i + 1;
        }
        if (foundPossibleTokenStart) break;
    }

    if (!foundPossibleTokenStart) return false;

    // look forwards
    QString token;
    for (int i = pos; i < tokenString.length(); i++) {
        ch = tokenString.at(i);
        if (ch == '}') {                        // qt6.2 changed " to '
            for (int j = startPos; j < i; j++) {
                token.append(tokenString.at(j));
            }
            if (exampleMap.contains(token)) {
                currentToken = token;
                tokenStart = startPos - 1;
                tokenEnd = i + 1;
                return true;
            }
        }
    }
    return false;
}

QString RenameFileDlg::parseTokenString(QFileInfo info, QString tokenString)
{
    if (G::isLogger) G::log("IngestDlg::parseTokenString");
    QString fPath = info.absoluteFilePath();
    if (fPath == "") return "";
    ImageMetadata m = dm->imMetadata(fPath);

    //return "Test_000" + QString::number(seqNum);

    createdDate = m.createdDate;
    QString s;
    int i = 0;

    while (i < tokenString.length()) {
        if (isToken(tokenString, i + 1)) {
            QString tokenResult;
            // get metadata related to token
            if (currentToken == "YYYY")
                tokenResult = createdDate.date().toString("yyyy");
            if (currentToken == "YY")
                tokenResult = createdDate.date().toString("yy");
            if (currentToken == "MONTH")
                tokenResult = createdDate.date().toString("MMMM").toUpper();
            if (currentToken == "Month")
                tokenResult = createdDate.date().toString("MMMM");
            if (currentToken == "MON")
                tokenResult = createdDate.date().toString("MMM").toUpper();
            if (currentToken == "Mon")
                tokenResult = createdDate.date().toString("MMM");
            if (currentToken == "MM")
                tokenResult = createdDate.date().toString("MM");
            if (currentToken == "DAY")
                tokenResult = createdDate.date().toString("dddd").toUpper();
            if (currentToken == "Day")
                tokenResult = createdDate.date().toString("dddd");
            if (currentToken == "DDD")
                tokenResult = createdDate.date().toString("ddd").toUpper();
            if (currentToken == "Ddd")
                tokenResult = createdDate.date().toString("ddd");
            if (currentToken == "DD")
                tokenResult = createdDate.date().toString("dd");
            if (currentToken == "HOUR")
                tokenResult = createdDate.time().toString("hh");
            if (currentToken == "MINUTE")
                tokenResult = createdDate.time().toString("mm");
            if (currentToken == "SECOND")
                tokenResult = createdDate.time().toString("ss");
            if (currentToken == "MILLISECOND")
                tokenResult = createdDate.time().toString("zzz");
            if (currentToken == "TITLE")
                tokenResult = m.title;
            if (currentToken == "CREATOR")
                tokenResult = m.creator;
            if (currentToken == "COPYRIGHT")
                tokenResult = m.copyright;
            if (currentToken == "ORIGINAL FILENAME")
                tokenResult = info.baseName();
            if (currentToken == "MAKE")
                tokenResult = m.make;
            if (currentToken == "MODEL")
                tokenResult = m.model;
            if (currentToken == "DIMENSIONS")
                tokenResult = m.dimensions;
            if (currentToken == "SHUTTER SPEED")
                tokenResult = m.exposureTime;
            if (currentToken == "APERTURE")
                tokenResult = m.aperture;
            if (currentToken == "ISO")
                tokenResult = m.ISO;
            if (currentToken == "FOCAL LENGTH")
                tokenResult = m.focalLength;
            // sequence from XX to XXXXXXX.  seqNum must be pre-assigned.
            if (currentToken.left(2) == "XX") {
                seqWidth = currentToken.length();
                tokenResult = QString("%1").arg(seqNum, seqWidth , 10, QChar('0'));
            }
            s.append(tokenResult);
            i = tokenEnd;
        }
        else {
            s.append(tokenString.at(i));
            i++;
        }
    }
    return s;
}
