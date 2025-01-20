#include "renamefile.h"
#include "ui_renamefiledlg.h"
#include "Main/global.h"

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

    ui->progressMsg->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->progressBar->setTextVisible(false);
    if (G::useProcessEvents) qApp->processEvents();

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
    //ui->filenameTemplatesCB->setCurrentIndex(filenameTemplateSelected);

    #ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif

    updateExample();

    isDebug = false;
}

void RenameFileDlg::renameFileBase(QString oldBase, QString newBase)
{
    QFileInfoList inf = QDir(folderPath).entryInfoList(QDir::Files);
    for (int i = 0; i < inf.size(); i++) {
        QString existBase = inf.at(i).baseName();
        if (existBase == oldBase) {
            QString oldPath = inf.at(i).filePath();
            QString newPath = inf.at(i).dir().path() + "/" + newBase + "." + inf.at(i).suffix();
            QFile(oldPath).rename(newPath);
            QFile(oldPath).close();
            if (isDebug) qDebug() << "RenameFileDlg::renameFileBase Renamed file oldPath ="
                                  << oldPath << "to newPath =" << newPath;
        }
    }
}

void RenameFileDlg::makeExistingBaseUnique(QString newBase)
{
    QFileInfoList inf = QDir(folderPath).entryInfoList(QDir::Files);
    bool isConflict = false;
    for (int i = 0; i < inf.size(); i++) {
        QString existBase = inf.at(i).baseName();
        if (existBase == newBase) {
            isConflict = true;
            QString uniqueBase;
            int k = 0;
            do {
                uniqueBase = existBase + "-" + QString::number(k);
            } while (baseNames.contains(uniqueBase));

            if (isDebug)
                qDebug() << "\nmakeExistingBaseUnique CONFLICT FOUND"
                         << "existBase =" << existBase
                         << "newBase =" << newBase
                         << "uniqueBase =" << uniqueBase
                            ;
            // update datamodel, imageCache
            QString oldPath = inf.at(i).filePath();
            QString newName = uniqueBase + "." + inf.at(i).suffix();
            QString newPath = inf.at(i).dir().path() + "/" + newName;
            renameDatamodel(oldPath, newPath, newName);
            if (isDebug) {
                qDebug() << "\nmakeExistingBaseUnique renamed in datamodel ="
                         << "oldPath ="  << oldPath
                         << "newPath ="  << newPath
                         << "newName ="  << newName
                    ;
                diagDatamodel();
            }
            // add new uniqueBase to baseNames
            baseNames.append(uniqueBase);
            // delete existBase from baseNames
            baseNames.removeOne(existBase);
            if (isDebug) {
                qDebug() << "\nmakeExistingBaseUnique appended uniqueBase ="
                         << uniqueBase << "to baseNames and removed"
                         << existBase << "from baseNames"
                            ;
                diagBaseNames();
            }
            // rename existing files with name conflict
            renameFileBase(existBase, uniqueBase);
            if (isDebug) {
                qDebug() << "\nmakeExistingBaseUnique renamed file(s) with existBase ="
                         << existBase << "to file with uniqueBase =" << uniqueBase;
                diagFiles();
            }
            // update filesToRename
            renameAllSharingBaseName(existBase, uniqueBase);
            if (isDebug) {
                qDebug() << "\nmakeExistingBaseUnique in filesToRename renamed all existBase ="
                         << existBase << "to uniqueBase =" << uniqueBase;
                diagFilesToRename();
            }
            return;
        }
    }
    if (!isConflict && isDebug) qDebug() << "makeExistingBaseUnique No base name conflict";
}

void RenameFileDlg::renameDatamodel(QString oldPath, QString newPath, QString newName)
{
    int row = dm->fPathRow[oldPath];
    QString rowStr = QString::number(row);
    //            qDebug() << oldPath << "found.  Datamodel row =" << row;

    dm->fPathRow.remove(oldPath);
    dm->fPathRow[newPath] = row;

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

    // update imageCache
    imageCache->rename(oldPath, newPath);
    if (isDebug) qDebug() << "In ImageCache renamed oldPath =" << oldPath
                 << "to newPath =" << newPath;
}

void RenameFileDlg::renameAllSharingBaseName(QString oldBase, QString newBase)
{
    int pathCol = 0;    // filesToRename.at(i).at(pathCol)
    int baseCol = 1;    // filesToRename.at(i).at(baseCol)
    int doneCol = 2;    // filesToRename.at(i).at(doneCol)
    for (int i = 0; i < filesToRename.size(); i++) {
        if (filesToRename.at(i).at(baseCol) == oldBase) {
            QString oldPath = filesToRename.at(i).at(pathCol);
            QString dirPath = QFileInfo(oldPath).dir().path();
            QString ext = QFileInfo(oldPath).suffix();
            QString newPath = dirPath +"/" + newBase + "." + ext;
            filesToRename[i][pathCol] = newPath;
            filesToRename[i][baseCol] = newBase;
            if (isDebug) {
                qDebug() << "RenameFileDlg::renameAllSharingBaseName in filesToRename"
                         << "renamed oldPath =" << oldPath << "to newPath =" << newPath << "and"
                         << "renamed oldBase =" << oldBase << "to newBase =" << newBase;
            }
        }
    }
}

void RenameFileDlg::appendAllSharingBaseName(QString path)
{
/*
    Append all paths with a file name containing the same base name as in path
    to filesToRename.
*/
    QFileInfo info(path);
    QString baseName = info.baseName();
    if (baseNamesUsed.contains(baseName)) return;

    // append image file from datamodel selection (must come first)
    filesToRename.append({path, baseName, "false"});
    if (isDebug) {
        qDebug() << "RenameFileDlg::appendAllSharingBaseName in filesToRename appending"
                 << "base =" << baseName
                 << "\tfileToAppend =" << path
            ;
    }

    // append any auxillary or sidecar files sharing the basename
    QFileInfoList fInfo = QDir(folderPath).entryInfoList(QDir::Files);
    for (int i = 0; i < fInfo.size(); i++) {
        // ignore if source path
//        qDebug() << QDir(folderPath).entryInfoList().at(i).filePath().toLower() << path.toLower();
        if (fInfo.at(i).filePath() == path)
            continue;
        QString base = fInfo.at(i).baseName();
        if (base == baseName) {
            QString fileToAppend = fInfo.at(i).filePath();
            filesToRename.append({fileToAppend, baseName, "false"});
            if (isDebug) {
                qDebug() << "RenameFileDlg::appendAllSharingBaseName in filesToRename appending"
                         << "base =" << base
                         << "\tfileToAppend =" << fileToAppend
                            ;
            }
        }
    }
}

void RenameFileDlg::resolveNameConflicts()
{

}

void RenameFileDlg::rename()
{
/*
    Renames the base name for all the selected images.  Note that there could be multiple
    images with the same base name but only one might be selected.  For example,: image.nef,
    image.jpg and image.xmp.  Also, the renamed file could match an existing, but not
    selected, file.

    Structures:
    Selection                       selectionIndexes
    All files in folder             allFilesList
    All files for base name list    baseNames
    All base names renamed list     baseNamesUsed
    All files to rename list        filesToRename[path,basename]

    Example:  Rename to templete "Test_XXXX" ie "Test_0003", "Test_0004" ...

    All files in folder.
        "2022-06-12_0004.jpg"   rename to "Test_0003.jpg"
        "2022-06-12_0004.xmp"   rename to "Test_0003.xmp"
        "2022-06-12_0005.jpg"   rename to "Test_0004.jpg"  *1
        "2022-06-13_0014.jpg"   rename to "Test_0005.jpg"
        "2022-06-14_0001.jpg"   rename to "Test_0006.jpg"
        "2022-06-14_0001.xmp"   rename to "Test_0006.xmp"
        "IMG_5853.heic"         rename to "Test_0007.jpg"  *2
        "Test_0004.jpg"         rename to "Test_0008.jpg"
        "Test_0004.txt"         rename to "Test_0008.txt"
        "Test_0007.jpg"         rename to "Test_0098.jpg"
        "Test_0007.txt"         rename to "Test_0098.txt"
        "Test_0007.xmp"         rename to "Test_0098.xmp"

        *1 "Test_0004.jpg" already exists
        *2 "Test_0007.jpg" already exists

    First iterate through the selection to create filesToRename list.
         Base Name           File Name
         "2022-06-12_0004" 	 "2022-06-12_0004.jpg"
         "2022-06-12_0004" 	 "2022-06-12_0004.xmp"
         "2022-06-12_0005" 	 "2022-06-12_0005.jpg"
         "2022-06-13_0014" 	 "2022-06-13_0014.jpg"
         "2022-06-14_0001" 	 "2022-06-14_0001.jpg"
         "2022-06-14_0001" 	 "2022-06-14_0001.xmp"
         "IMG_5853"          "IMG_5853.heic"
         "Test_0004"         "Test_0004.jpg"
         "Test_0004"     	 "Test_0004.txt"
         "Test_0007"         "Test_0007.jpg"
         "Test_0007"         "Test_0007.txt"
         "Test_0007"         "Test_0007.xmp"

*/
    QString tokenString = filenameTemplatesMap[ui->filenameTemplatesCB->currentText()];
    seqNum  = ui->spinBoxStartNumber->value();
    int pathCol = 0;    // filesToRename.at(i).at(pathCol)
    int baseCol = 1;    // filesToRename.at(i).at(baseCol)
    int doneCol = 2;    // filesToRename.at(i).at(doneCol)

    // populate base names in folder
    QFileInfoList inf = QDir(folderPath).entryInfoList(QDir::Files);

    ui->progressMsg->setVisible(true);
    ui->progressBar->setVisible(true);
    int progress = 0;
    ui->progressBar->setMaximum(inf.size());
    ui->progressMsg->setText("Step 1 of 3: Preparing...");
    for (int i = 0; i < inf.size(); i++) {
        const QString base = inf.at(i).baseName();
        if (!baseNames.contains(base)) baseNames.append(base);
        ui->progressBar->setValue(++progress);
        qApp->processEvents();
    }


    // Build list of all files to rename
    progress = 0;
    ui->progressBar->setMaximum(selectionIndexes.size());
    ui->progressMsg->setText("Step 2 of 3: Checking for name conflicts...");
    if (isDebug)  qDebug() << "FILE LIST TO RENAME:";
    filesToRename.clear();
    for (int i = 0; i < selectionIndexes.size(); i++) {
        int row = selectionIndexes.at(i).row();
        QString path = dm->sf->data(dm->sf->index(row, G::PathColumn), G::PathRole).toString();
        appendAllSharingBaseName(path);
        ui->progressBar->setValue(++progress);
        qApp->processEvents();
    }

    if (isDebug) diagFiles();
    if (isDebug) diagFilesToRename();
    if (isDebug) diagBaseNames();
    if (isDebug) diagDatamodel();

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
    QString newBase = "";

    progress = 0;
    ui->progressBar->setMaximum(filesToRename.size());
    QString txt = "Step 2 of 3: Renaming " + QString::number(filesToRename.size()) + " files...";
    ui->progressMsg->setText(txt);

    for (int i = 0; i < filesToRename.size(); i++) {
        QString iStr = QString::number(i);

        QString oldPath = filesToRename.at(i).at(pathCol);
        QString baseName = filesToRename.at(i).at(baseCol);
        QFileInfo info(oldPath);

        // is file an image in datamodel
        bool inDatamodel = dm->fPathRow.contains(oldPath);
        bool baseNameChange = prevBaseName != baseName;

        if (inDatamodel) {
            newBase = parseTokenString(info, tokenString);
        }

        QString newName = newBase + "." + info.suffix();
        QString newPath = info.dir().path() + "/" + newName;

        if (isDebug) {
            qDebug()
                 << "\nFILE =" << i << "*************************************************************"
                 << "\noldPath =" << oldPath
                 << "baseName =" << baseName
                 << "prevBaseName =" << prevBaseName
                 << "\nnewBase =" << newBase
                 << "newName =" << newName
                 << "newPath =" << newPath
                 << "\nbaseNameChange =" << baseNameChange
                 << "inDatamodel =" << inDatamodel
                 << "seqNum =" << seqNum
                   ;
        }

        // if newPath already exists then eliminate conflict by renaming the existing
        // files with baseName == newBase, updating baseNames and filesToRename
        if (baseNameChange) {
            makeExistingBaseUnique(newBase);
        }
        /*
        if (baseNameChange) {
            if (isDebug) qDebug() << "Checking if" << newPath << "exists";
            if (QFile(newPath).exists()) {
                makeExistingBaseUnique(newBase);
            }
            else {
                if (isDebug) {
                    qDebug() << "No name conflicts";
                    diagBaseNames();
                    diagFiles();
                    diagFilesToRename();
                }
            }
        }
        else {
            if (isDebug) {
                diagBaseNames();
                diagFiles();
                diagFilesToRename();
            }
        }
        //*/
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
        if (dm->fPathRow.contains(oldPath)) {
            renameDatamodel(oldPath, newPath, newName);
        }

        // update filesToRename list to done = true
        filesToRename[i][doneCol] = "true";

        if (baseNameChange) {
            seqNum++;
            prevBaseName = baseName;
        }

        ui->progressBar->setValue(++progress);
        qApp->processEvents();
    }

//    ui->

    // update current image
    dm->currentFilePath = dm->currentSfIdx.data(G::PathRole).toString();

    if (isDebug) {
        qDebug() << "Renaming completed";
    }
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

void RenameFileDlg::diagFiles() {
    qDebug() << "\nFiles in folder:" << folderPath;
    QFileInfoList fInfo = QDir(folderPath).entryInfoList(QDir::Files);
    for (int i = 0; i < fInfo.size(); i++) {
        qDebug() << fInfo.at(i).fileName();
    }
}

void RenameFileDlg::diagFilesToRename() {
    int pathCol = 0;    // filesToRename.at(i).at(pathCol)
    int baseCol = 1;    // filesToRename.at(i).at(baseCol)
    int doneCol = 2;    // filesToRename.at(i).at(doneCol)
    qDebug() << "\nfilesToRename:";
    for (int i = 0; i < filesToRename.size(); i++) {
        qDebug() << i
                 << "base =" << filesToRename.at(i).at(baseCol)
                 << "\tdone =" << filesToRename.at(i).at(doneCol)
                 << "\tpath =" << filesToRename.at(i).at(pathCol)
                    ;
    }
}

void RenameFileDlg::diagBaseNames() {
    qDebug() << "\nbaseNames:";
    for (int i = 0; i < baseNames.size(); i++) {
        qDebug() << baseNames.at(i);
    }
}

void RenameFileDlg::diagBaseNamesUsed() {
    qDebug() << "\nbaseNamesUsed:";
    for (int i = 0; i < baseNamesUsed.size(); i++) {
        qDebug() << baseNamesUsed.at(i);
    }
}

void RenameFileDlg::diagDatamodel()
{
    qDebug() << "\ndm->fPathRow hash:";
    //    QMap<int,QString> rowMap;
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
}


