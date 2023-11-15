#include "ingestdlg.h"
#include "ui_ingestdlg.h"
//#include "ui_copypickdlg.h"
#include <QDebug>

/*
When this dialog is invoked the files that have been picked are copied to a primary
destination folder, and optionally, to a backup location.  This process is known as
ingestion: selecting and copying images from a camera card to a computer.  If backround
ingesting is selected then the dialog closes and the user can continue working while the
ingest happens in the background in another thread, using all the IngestDlg settings.

The destination folder can be selected/created manually or automatically.  If
automatic, a root folder is selected/created, a path to the destination folder
is defined using tokens, and the destination folder description defined.  The
picked images can be renamed during this process.

Files are copied to a destination based on building a file path consisting of:

      Root Folder                   (rootFolderPath)
    + Path to base folder           (fromRootToBaseFolder) source pathTemplateString
    + Base Folder Description       (baseFolderDescription)
    + File Name                     (fileBaseName)     source filenameTemplateString
    + File Suffix                   (fileSuffix)

    ie E:/2018/201802/2018-02-08 Rory birthday/2018-02-08_0001.NEF

    rootFolderPath:         ie "E:/" where all the images are located
    fromRootToBaseFolder:   ie "2018/201802/2018-02-08" from the path template
    baseFolderDescription:  ie " Rory birthday" a description appended to the pathToBaseFolder
    fileBaseName:           ie "2018-02-08_0001" from the filename template
    fileSuffix:             ie ".NEF"

    folderPath:             ie "E:/2018/201802/2018-02-08 Rory birthday/"
        = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
        = the copy to destination

    The strings fromRootToBaseFolder and the fileBaseName can be tokenized in TokenDlg,
    allowing the user to automate the construction of the entire destination file path.
*/

IngestDlg::IngestDlg(QWidget *parent,
                     bool &combineRawJpg,
                     bool &combinedIncludeJpg,
                     bool &autoEjectUsb,
                     bool &integrityCheck,
                     bool &isBackgroundIngest,
                     bool &isBackgroundIngestBeep,
                     bool &ingestIncludeXmpSidecar,
                     bool &isBackup,
                     bool &gotoIngestFolder,
                     int &seqStart,
                     Metadata *metadata,
                     DataModel *dm,
                     QString &ingestRootFolder,
                     QString &ingestRootFolder2,
                     QString &manualFolderPath,
                     QString &manualFolderPath2,
                     QString &folderPath,
                     QString &folderPath2,
                     QString &baseFolderDescription,
                     QMap<QString, QString>& pathTemplates,
                     QMap<QString, QString>& filenameTemplates,
                     int& pathTemplateSelected,
                     int& pathTemplateSelected2,
                     int& filenameTemplateSelected,
                     QStringList& ingestDescriptionCompleter,
                     bool &isAuto,
                     QString css) :

                     QDialog(parent),
                     ui(new Ui::IngestDlg),
                     metadata(metadata),
                     isAuto(isAuto),
                     combineRawJpg(combineRawJpg),
                     combinedIncludeJpg(combinedIncludeJpg),
                     autoEjectUsb(autoEjectUsb),
                     integrityCheck(integrityCheck),
                     isBackgroundIngest(isBackgroundIngest),
                     isBackgroundIngestBeep(isBackgroundIngestBeep),
                     ingestIncludeXmpSidecar(ingestIncludeXmpSidecar),
                     isBackup(isBackup),
                     gotoIngestFolder(gotoIngestFolder),
                     seqStart(seqStart),
                     pathTemplatesMap(pathTemplates),
                     filenameTemplatesMap(filenameTemplates),
                     pathTemplateSelected(pathTemplateSelected),
                     pathTemplateSelected2(pathTemplateSelected2),
                     rootFolderPath(ingestRootFolder),
                     rootFolderPath2(ingestRootFolder2),
                     folderPath(folderPath),
                     folderPath2(folderPath2),
                     baseFolderDescription(baseFolderDescription),
                     ingestDescriptionCompleter(ingestDescriptionCompleter),
                     manualFolderPath(manualFolderPath),
                     manualFolderPath2(manualFolderPath2),
                     filenameTemplateSelected(filenameTemplateSelected)
{
    if (G::isLogger) G::log("IngestDlg::IngestDlg");
    this->dm = dm;
    this->css = css;
    normalText = "QLabel {color:" + G::textColor.name() + ";}";
    redText = "QLabel {color:red;}";

    ui->setupUi(this);
    setStyleSheet(css);


    ui->pathTemplatesCB->setView(new QListView());      // req'd for setting row height in stylesheet
    ui->filenameTemplatesCB->setView(new QListView());  // req'd for setting row height in stylesheet

    isInitializing = true;

    // populate the pick list
    getPicks();

    if (!combineRawJpg) {
        Utilities::setOpacity(ui->combinedIncludeJpgChk, 0.5);
        ui->combinedIncludeJpgChk->setDisabled(true);
    }
    ui->rootFolderLabel->setText(rootFolderPath);
    ui->rootFolderLabel->setToolTip(ui->rootFolderLabel->text());
    ui->rootFolderLabel_2->setText(rootFolderPath2);
    ui->rootFolderLabel_2->setToolTip(ui->rootFolderLabel_2->text());
    ui->manualFolderLabel->setText(manualFolderPath);
    ui->manualFolderLabel->setToolTip(ui->manualFolderLabel->text());
    ui->manualFolderLabel_2->setText(manualFolderPath2);
    ui->manualFolderLabel_2->setToolTip(ui->manualFolderLabel_2->text());

    /* ui->descriptionLineEdit->setStyleSheet not working
    ui->descriptionLineEdit->setStyleSheet(
        "QLineEdit {"
            "margin-left: 15px;"
            "padding-left: 15px;"
        "};"
    );
    //*/
    ui->descriptionLineEdit->setText(baseFolderDescription);

    // initialize templates and tokens
    initTokenList();
    initExampleMap();
    QMap<QString, QString>::iterator i;
    if (pathTemplatesMap.count() == 0) {
        pathTemplatesMap["YYYY̸YYMM̸YYYY-MM-DD"] = "{YYYY}/{YYYY}{MM}/{YYYY}-{MM}-{DD}";
        pathTemplatesMap["YYYY-MM-DD"] = "{YYYY}-{MM}-{DD}";
    }
    for (i = pathTemplatesMap.begin(); i != pathTemplatesMap.end(); ++i) {
        ui->pathTemplatesCB->addItem(i.key());
        ui->pathTemplatesCB_2->addItem(i.key());
    }
    ui->pathTemplatesCB->setCurrentIndex(pathTemplateSelected);
    ui->pathTemplatesCB_2->setCurrentIndex(pathTemplateSelected2);

    if (filenameTemplatesMap.count() == 0) {
        filenameTemplatesMap["YYYY-MM-DD_XXXX"] = "{YYYY}-{MM}-{DD}_{XXXX}";
        filenameTemplatesMap["Original filename"] = "{ORIGINAL FILENAME}";
    }
    for (i = filenameTemplatesMap.begin(); i != filenameTemplatesMap.end(); ++i)
        ui->filenameTemplatesCB->addItem(i.key());
    ui->filenameTemplatesCB->setCurrentIndex(filenameTemplateSelected);

    // isAuto uses tokens, else use manual dest folder choice
    if (isAuto) {
        ui->descriptionLineEdit->setFocus();
        ui->autoRadio->setChecked(true);
    }
    else {
        ui->selectFolderBtn->setFocus();
        ui->manualRadio->setChecked(true);
    }
    if (baseFolderDescription.length() > 0) {
        ui->okBtn->setDefault(true);
        ui->okBtn->setFocus();
    }
    updateEnabledState();

    // assign completer to description
    ingestDescriptionListModel = new QStringListModel(this->ingestDescriptionCompleter);
    QCompleter *completer = new QCompleter(ingestDescriptionListModel, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    ui->descriptionLineEdit->setCompleter(completer);
    ui->descriptionLineEdit_2->setCompleter(completer);

    // initialize autoEjectUsb
    ui->ejectChk->setChecked(autoEjectUsb);
    // initialize integrityCheck
    ui->integrityChk->setChecked(integrityCheck);
    // initialize backgroundIngestCheck
    ui->backgroundIngestChk->setChecked(isBackgroundIngest);
    // initialize backgroundIngestCheckBeep
    ui->beepChk->setChecked(isBackgroundIngestBeep);
    // initialize use backup as well as primary ingest
    ui->backupChk->setChecked(isBackup);
    // initialize open ingest folder after ingest
    ui->openIngestFolderChk->setChecked(gotoIngestFolder);

    ui->progressBar->setVisible(false);
    ui->autoIngestTab->tabBar()->setCurrentIndex(0);
    isInitializing = false;

    updateFolderPaths();

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

IngestDlg::~IngestDlg()
{
    delete ui;
}

void IngestDlg::renameIfExists(QString &destination,
                               QString &baseName,
                               QString dotSuffix)
{
    if (G::isLogger) G::log("IngestDlg::renameIfExists");
    int count = 0;
    bool fileAlreadyExists = true;
    QString newBaseName = baseName + "_";
    do {
        QFile testFile(destination);
        if (testFile.exists()) {
            destination = folderPath + newBaseName + QString::number(++count) + dotSuffix;
            baseName = newBaseName;
        }
        else fileAlreadyExists = false;
    } while (fileAlreadyExists);
}

void IngestDlg::getPicks()
{
/*
    This function generates a list of picked images to ingest from the filtered data. The
    datamodel dm, not the proxy dm->sf, is used, as the user may have sorted the view, and we
    need the original datamodel dm order to efficiently deal with the combined raw/jpg
    scenario.

    The datamodel is sorted by file path. Raw files with the same path precede jpg files with
    duplicate names. Two roles track duplicates: G::DupHideRawRole flags jpg files with
    duplicate raws and G::DupRawIdxRole points to the duplicate raw file from the jpg data
    row. For example:

    Row = 0 "G:/DCIM/100OLYMP/P4020001.ORF" 	DupHideRawRole = true 	DupRawIdxRole = (Invalid)
    Row = 1 "G:/DCIM/100OLYMP/P4020001.JPG" 	DupHideRawRole = false 	DupRawIdxRole = QModelIndex(0,0))
    Row = 2 "G:/DCIM/100OLYMP/P4020002.ORF" 	DupHideRawRole = true 	DupRawIdxRole = (Invalid)
    Row = 3 "G:/DCIM/100OLYMP/P4020002.JPG" 	DupHideRawRole = false 	DupRawIdxRole = QModelIndex(2,0)
*/
    if (G::isLogger) G::log("IngestDlg::getPicks");
//    combinedIncludeJpg = ui->combinedIncludeJpgChk->isChecked();
//    bool inclDupJpg = ui->combinedIncludeJpgChk->isChecked();
    QString fPath;
    pickList.clear();
    for (int row = 0; row < dm->rowCount(); ++row) {
        QModelIndex pickIdx = dm->index(row, G::PickColumn);
        // only picks
        if (pickIdx.data(Qt::EditRole).toString() == "Picked") {
            QModelIndex idx = dm->index(row, 0);
            // only filtered
            if (dm->sf->mapFromSource(idx).isValid()) {
                // if raw+jpg files have been combined
                if (combineRawJpg) {
                    // append the jpg if combineRawJpg and include combined jpgs
                    if (combinedIncludeJpg && idx.data(G::DupIsJpgRole).toBool()) {
                        fPath = idx.data(G::PathRole).toString();
                        QFileInfo fileInfo(fPath);
                        pickList.append(fileInfo);
//                        qDebug() << "IngestDlg::" << "appending" << fPath;
                    }
                    // append combined raw file
                    if (idx.data(G::DupIsJpgRole).toBool()) {
                        idx = qvariant_cast<QModelIndex>(dm->index(row, 0).data(G::DupOtherIdxRole));
                    }
                }
                fPath = idx.data(G::PathRole).toString();
                QFileInfo fileInfo(fPath);
                pickList.append(fileInfo);
//                qDebug() << "IngestDlg::" << "appending" << fPath;
            }
        }
    }

    // seqDate set to the created date of the first pick
    if (pickList.count()) {
        seqDate = pickList.at(0).birthTime().date();
        /*
        qDebug() << "IngestDlg::"
                 << pickList.at(0).absoluteFilePath()
                 << seqDate
                 << G::ingestLastSeqDate
                    ;
                    //*/
    }

    // stats
    fileCount = static_cast<int>(pickList.count());
    fileMB = 0;

    qulonglong bytes = 0;
    foreach(QFileInfo info, pickList) bytes += static_cast<qulonglong>(info.size());
    picksMB = bytes / 1024 / 1024;
    QString s1 = QString::number(fileCount);
    QString s2 = fileCount == 1 ? " file " : " files ";
    QString s3 = Utilities::formatMemory(bytes);
    QString s4 = "";
    if (combinedIncludeJpg) s4 = " including duplicate jpg";
    ui->statsLabel->setText(s1 + s2 + s3 + s4);
    ui->gbsLabel->setText("");
}

void IngestDlg::quitIfNotEnoughSpace()
{
    if (G::isLogger) G::log("IngestDlg::quitIfNotEnoughSpace");

    // check if room on destination drives for images
    bool isFull = picksMB > availableMB;
    bool isFull2 = picksMB > availableMB2 && isBackup;
    if (isFull || isFull2) {
        QString s;
        if (isFull) s = drive + " does ";
        if (isFull2) s = drive2;
        if (isFull && isFull2) s = drive + " and " + drive2 + " do ";
        QString title = "Insufficient drive space";
        QString msg =
            s + "not have sufficient space available to copy the ingesting images.  No images "
            "will be ingested.  Please resolve the disk space issue and try again.";

        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setWindowTitle(title);
        msgBox.setText(msg);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        msgBox.exec();
        // quit ingest dialog
        reject();
    }
}


void IngestDlg::ingest()
{
/*
    The files in pickList are renamed and ingested (copied) from the source folder to the
    destination folder.

    The destination folder path is appended to the ingest folder history (in the file menu).

    The description, if not "" is appended to the description completer list to facilitate
    future ingests.

    Each picked image is copied from the source to the destination.

    The destination file base name is based on the file name template selected. ie
    YYYY-MM-DD_xxxx would rename the source file test.jpg to 2019-01-20_0001.jpg if the seqNum
    = 1. The seqNum is incremented with each image in the pickList. This does not happen if
    the file name template does not include the sequence numbering ie use original file.

    If the destination folder already has a file with the same name then _# is appended to the
    destination base file name.

    If there is edited metadata it is written to a sidecar xmp file.

    Finally the source file is copied to the renamed destination.
*/

    if (G::isLogger) G::log("IngestDlg::ingest");

    QString key = ui->filenameTemplatesCB->currentText();
    QString tokenString  = filenameTemplatesMap[key];

    // copy cycles req'd: 1 if no backup, 2 if backup
    int n;
    isBackup ? n = 2 : n = 1;

    // copy picked images
    ui->progressBar->setVisible(true);
    seqNum =  ui->spinBoxStartNumber->value();
    QElapsedTimer t;
    qint64 bytesWritten = 0;
    t.restart();
    for (int i = 0; i < pickList.size(); ++i) {
        qint64 fileBytesToWrite = 0;
        int progress = (i + 1) * 100 * n / (pickList.size());
        /*
        qDebug() << "IngestDlg::"
                 << "progress =" << progress
                 << "ui->progressBar->minimum() = " << ui->progressBar->minimum()
                 << "ui->progressBar->maximum() = " << ui->progressBar->maximum();
        //*/
        ui->progressBar->setValue(progress);
        emit updateProgress(progress);
        QFileInfo fileInfo = pickList.at(i);
        QString sourcePath = fileInfo.absoluteFilePath();
        QString sourceFolderPath = fileInfo.absoluteDir().absolutePath();
        QString sourceBaseName = fileInfo.baseName();
        QString sourceSidecarPath = sourceFolderPath + "/" + sourceBaseName + ".xmp";

        // seqNum is required by parseTokenString
        // increase sequence unless dup (raw + jpg)
        if (i > 0 && pickList.at(i).baseName() != pickList.at(i-1).baseName())
            seqNum++;

        //qDebug() << "Ingest::ingest  pickList =" << pickList;
        // rename destination file based on the file naming template
        QString destBaseName =  parseTokenString(pickList.at(i), tokenString);
        QString suffix = fileInfo.suffix().toLower();
        QString dotSuffix = "." + suffix;
        QString destFileName = destBaseName + dotSuffix;
        QString sidecarName = destBaseName + ".xmp";

        // check if image already exists at destination folder
        QString destinationPath = folderPath + destFileName;
        QString backupPath = folderPath2 + destFileName;
        QString destSidecarPath = folderPath + sidecarName;
        QString backupSidecarPath = folderPath2 + sidecarName;

        // rename destination and fileName if already exists
        renameIfExists(destinationPath, destBaseName, dotSuffix);

        // rename destination and xmp file if already exists
        renameIfExists(destSidecarPath, destBaseName, ".xmp");

        // set the metadataChangedSourcePath depending on combineRawJpg
        QString metadataChangedSourcePath = sourcePath;
        if (combineRawJpg) {
            // only raw files included if combineRawJpg and there is a rawjpg pair
            int dmRow = dm->rowFromPath(sourcePath.toLower());
            QModelIndex idx = dm->index(dmRow, 0);
            if (idx.isValid()) {
                // check if raw/jpg pair
                if (idx.data(G::DupHideRawRole).toBool()) {
                    QModelIndex jpgIdx = idx.data(G::DupOtherIdxRole).toModelIndex();
                    if (jpgIdx.isValid()) {
                        metadataChangedSourcePath = jpgIdx.data(G::PathRole).toString();
                    }
                }
            }
        }

        // copy source image to destination
        fileBytesToWrite += fileInfo.size();
        bool copyOk = QFile::copy(sourcePath, destinationPath);
        /* for demonstration:
        failedToCopy << sourcePath + " to " + destinationPath;
        integrityFailure << sourcePath + " not same as " + destinationPath;
        // */
        if (!copyOk) {
            qDebug() << "IngestDlg::ingest" << "Failed to copy" << sourcePath << "to" << destinationPath;
            failedToCopy << sourcePath + " to " + destinationPath;
        }
        if (copyOk && integrityCheck) {
            if (!Utilities::integrityCheck(sourcePath, destinationPath)) {
                qDebug() << "IngestDlg::ingest" << "Integrity failure" << sourcePath << "not same as" << destinationPath;
                integrityFailure << sourcePath + " not same as " + destinationPath;
            }
        }

        // copy source image to backup
        if(isBackup) {
            fileBytesToWrite += fileInfo.size();
            bool backupCopyOk = QFile::copy(sourcePath, backupPath);
            if (!backupCopyOk) {
                qDebug() << "IngestDlg::ingest" << "Failed to copy" << sourcePath << "to" << backupPath;
                failedToCopy << sourcePath + " to " + backupPath;
            }
            if (backupCopyOk && integrityCheck) {
                if (!Utilities::integrityCheck(sourcePath, backupPath)) {
                    qDebug() << "IngestDlg::ingest" << "Integrity failure" << sourcePath << "not same as" << backupPath;
                    integrityFailure << sourcePath + " not same as " + backupPath;
                }
            }
        }

        // if sidecar exists copy to destination
        bool sidecarOk = false;
        if (copyOk && QFile(sourceSidecarPath).exists()) {
            sidecarOk = QFile::copy(sourceSidecarPath, destSidecarPath);
            if (!sidecarOk) {
                qWarning() << "WARNING" << "IngestDlg::ingest" << "Failed to copy" << sourceSidecarPath << "to" << destSidecarPath;
                failedToCopy << sourceSidecarPath + " to " + destSidecarPath;
            }

        }

        // check if copied xmp = original xmp
        if (copyOk && integrityCheck) {
            if (!Utilities::integrityCheck(sourceSidecarPath, destSidecarPath)) {
                qWarning() << "WARNING" << "IngestDlg::ingest" << "Integrity failure" << sourceSidecarPath << "not same as" << destSidecarPath;
                integrityFailure << sourceSidecarPath + " not same as " + destSidecarPath;
            }
        }

        // copy sidecar from destination to backup
        if (isBackup && sidecarOk) {
            bool backupSidecarCopyOk = QFile::copy(destSidecarPath, backupSidecarPath);
            if (!backupSidecarCopyOk) {
                qWarning() << "WARNING" << "IngestDlg::ingest" << "Failed to copy" << destSidecarPath << "to" << backupSidecarPath;
                failedToCopy << destSidecarPath + " to " + backupSidecarPath;
            }
            if (copyOk && integrityCheck) {
                if (!Utilities::integrityCheck(destSidecarPath, backupSidecarPath)) {
                    qWarning() << "WARNING" << "IngestDlg::ingest" << "Integrity failure" << destSidecarPath << "not same as" << backupSidecarPath;
                    integrityFailure << destSidecarPath + " not same as " + backupSidecarPath;
                }
            }
        }

        // write to internal xmp (future enhancement?)
        /*
        if (ingestIncludeXmpSidecar && copyOk && metadata->writeMetadata(sourcePath, m, buffer)) {
            if (metadata->internalXmpFormats.contains(suffix)) {
                // write xmp data into image file       rgh needs some work!!!
                QFile newFile(destinationPath);
                fileBytesToCopy += newFile.size();
                newFile.open(QIODevice::WriteOnly);
                qint64 bytesWritten = newFile.write(buffer);
                qDebug() << "IngestDlg::" << bytesWritten << buffer.length();
                newFile.close();
            }
        }
        */

        // stats
        bytesWritten += fileBytesToWrite;
        double MBPerSec = static_cast<double>(bytesWritten) / 1048.576 / static_cast<double>(t.elapsed());
        QString s = QString::number(MBPerSec, 'f', 1) + " MB/sec";
        ui->gbsLabel->setText(s);
    }

    /*
    qDebug() << "IngestDlg::"
             << "bytesCopied =" << bytesCopied
             << "msec =" << t.elapsed()
                ;
                // */

    // update ingest count for Winnow session
    G::ingestCount += pickList.size();
    G::ingestLastSeqDate = seqDate;
    //qDebug() << "IngestDlg::ingest" << seqDate<< G::ingestCount << G::ingestLastSeqDate;

    // show any ingest errors
    if (failedToCopy.length() || integrityFailure.length()) {
        IngestErrors ingestErrors(failedToCopy, integrityFailure);
        ingestErrors.exec();
    }

    emit updateProgress(-1);
    QDialog::accept();
}

bool IngestDlg::parametersOk()
{
    if (G::isLogger) G::log("IngestDlg::parametersOk");
    QString errStr;
    bool err = false;
    //bool backup = ui->backupChk->isChecked();

    if (isAuto) {
        if (rootFolderPath.length() == 0) {
            err = true;
            errStr = "The primary location root folder is undefined";
        }

        if (isBackup) {
            if(rootFolderPath2.length() == 0) {
                err = true;
                errStr += "\nThe backup location root folder is undefined";
            }

            QDir dir2(folderPath2);
            dir2.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            if (dir2.entryInfoList().size() > 0) {
                err = true;
                qDebug() << dir2.entryInfoList().size() << "files";
                errStr += "\nThe automatically selected folder \"" + folderPath2 + "\" already contains "
                          "files.  Backup folders must be empty.";
            }
        }
    }
    else {
        if (manualFolderPath.length() == 0) {
            err = true;
            errStr = "The primary location folder is undefined";
        }

        if (isBackup) {
            if(manualFolderPath2.length() == 0) {
                err = true;
                errStr += "\nThe backup location folder is undefined";
            }

            // check if already files in backup folder
            QDir dir(manualFolderPath2);
            dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            qDebug() << dir.entryInfoList().size() << "files";
            if (dir.entryInfoList().size() > 0) {
                err = true;
                errStr += "\nThe manually selected folder \"" + manualFolderPath2 + "\" already contains "
                         "files.  Backup folders must be empty.";
            }
        }
    }

    if (err) {
        QMessageBox::warning(this, tr("Error"), errStr);
        return false;
    }

    return true;
}

void IngestDlg::updateExistingSequence()
{
/*
    The sequence is a part of the file name to make sure the file name is unique, and defined
    in the file name template with XX... (ie dscn0001.jpg).
*/
    if (G::isLogger) G::log("IngestDlg::updateExistingSequence");
    if (isInitializing) return;

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
        if(ui->spinBoxStartNumber->value() < sequenceNum + 1)
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
    buildFileNameSequence();
}

void IngestDlg::on_selectFolderBtn_clicked()
{
/*
    Manually select the primary folder.  Set up file sequencing.  Show renaming example.
*/
    if (G::isLogger) G::log("IngestDlg::on_selectFolderBtn_clicked");
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString path = ui->manualFolderLabel->text();
    if (path.right(1) == "/") path = path.left(path.length() - 1);
    if (!QDir(path).exists()) path = root;
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Primary Folder"), path,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        if(s.right(1) != "/") s += "/";
        folderPath = s;
        manualFolderPath = folderPath;
        ui->manualFolderLabel->setText(folderPath);
        ui->manualFolderLabel->setToolTip( ui->manualFolderLabel->text());
        updateFolderPaths();
    }
}

void IngestDlg::on_selectFolderBtn_2_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_selectFolderBtn_2_clicked");
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString path = ui->manualFolderLabel_2->text();
    if (path.right(1) == "/") path = path.left(path.length() - 1);
    if (!QDir(path).exists()) path = root;
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Backup Folder"), path,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        if(s.right(1) != "/") s += "/";
        folderPath2 = s;
        manualFolderPath2 = folderPath2;
        ui->manualFolderLabel_2->setText(folderPath2);
        ui->manualFolderLabel_2->setToolTip( ui->manualFolderLabel_2->text());
        updateFolderPaths();
    }
}

void IngestDlg::on_selectRootFolderBtn_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_selectRootFolderBtn_clicked");
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    rootFolderPath = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder for Primary Ingest Location"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath.length() > 0) {
        if(rootFolderPath.right(1) != "/") rootFolderPath += "/";
        ui->rootFolderLabel->setText(rootFolderPath);
        ui->rootFolderLabel->setToolTip(ui->rootFolderLabel->text());
    }

    updateFolderPaths();
}

void IngestDlg::on_selectRootFolderBtn_2_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_selectRootFolderBtn_2_clicked");
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    rootFolderPath2 = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder for Backup Location"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath2.length() > 0) {
        if(rootFolderPath2.right(1) != "/") rootFolderPath2 += "/";
        ui->rootFolderLabel_2->setText(rootFolderPath2);
        ui->rootFolderLabel_2->setToolTip(ui->rootFolderLabel_2->text());
    }

    updateFolderPaths();
}

bool IngestDlg::isToken(QString tokenString, int pos)
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
//    int n = tokenString.length();
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

QString IngestDlg::parseTokenString(QFileInfo info, QString tokenString)
{
    if (G::isLogger) G::log("IngestDlg::parseTokenString");
    QString fPath = info.absoluteFilePath();
    if (fPath == "") return "";
    ImageMetadata m = dm->imMetadata(fPath);
    createdDate = m.createdDate;
//    qDebug() << "IngestDlg::" << fPath << createdDate;
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

void IngestDlg::updateFolderPaths()
{
    if (G::isLogger) G::log("IngestDlg::updateFolderPaths");
    QStorageInfo d;

    // Auto folders
    if (isAuto) {
        baseFolderDescription = (ui->descriptionLineEdit->text().length() > 0)
                ? ui->descriptionLineEdit->text() : "";
        folderPath = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/";
        /*
        qDebug() << "IngestDlg::"
                 << "\nrootFolderPath        =" << rootFolderPath
                 << "\nfromRootToBaseFolder  =" << fromRootToBaseFolder
                 << "\nbaseFolderDescription =" << baseFolderDescription
                 << "\nfolderPath            =" << folderPath
                    ;
                 // */
        ui->folderLabel->setText(folderPath);
        ui->folderLabel->setToolTip(ui->folderLabel->text());
        drivePath = Utilities::getDrive(rootFolderPath);
//        qDebug() << "IngestDlg::" << "pathDrive =" << drivePath;
        d.setPath(drivePath);
        autoDriveAvailable = d.isValid();
        autoDriveAvailable ? ui->folderLabel->setStyleSheet(normalText) : ui->folderLabel->setStyleSheet(redText);

        baseFolderDescription2 = (ui->descriptionLineEdit_2->text().length() > 0)
                ? ui->descriptionLineEdit_2->text() : "";
        folderPath2 = rootFolderPath2 + fromRootToBaseFolder2 + baseFolderDescription2 + "/";
        ui->folderLabel_2->setText(folderPath2);
        ui->folderLabel_2->setToolTip(ui->folderLabel_2->text());
        drive2Path = Utilities::getDrive(rootFolderPath2);
//        qDebug() << "IngestDlg::" << "pathDrive2 =" << drive2Path;
        d.setPath(drive2Path);
        autoDrive2Available = d.isValid();
        autoDrive2Available ? ui->folderLabel_2->setStyleSheet(normalText) : ui->folderLabel_2->setStyleSheet(redText);

        // reset formating or disabling does not work
        ui->manualFolderLabel->setStyleSheet(G::css);
        ui->manualFolderLabel_2->setStyleSheet(G::css);

        invalidDrive = (!autoDriveAvailable || !autoDrive2Available);
    }
    // Manual folders
    else {
        folderPath = ui->manualFolderLabel->text() + "/";
        ui->manualFolderLabel->setToolTip(ui->manualFolderLabel->text());
        drivePath = Utilities::getDrive(folderPath);
        d.setPath(drivePath);
        manualDriveAvailable = d.isValid();
        manualDriveAvailable ? ui->manualFolderLabel->setStyleSheet(normalText) : ui->manualFolderLabel->setStyleSheet(redText);

        folderPath2 = ui->manualFolderLabel_2->text() + "/";
        ui->manualFolderLabel_2->setToolTip(ui->manualFolderLabel_2->text());
        drive2Path = Utilities::getDrive(folderPath2);
        d.setPath(drive2Path);
        manualDrive2Available = d.isValid();
        manualDrive2Available ? ui->manualFolderLabel_2->setStyleSheet(normalText) : ui->manualFolderLabel_2->setStyleSheet(redText);

        // reset formating or disabling does not work
        ui->folderLabel->setStyleSheet(G::css);
        ui->folderLabel_2->setStyleSheet(G::css);

        invalidDrive = (!manualDriveAvailable || !manualDrive2Available);
    }

    getAvailableStorageMB();
    updateExistingSequence();
    seqStart += getSequenceStart(folderPath);
    buildFileNameSequence();
    updateExistingSequence();
    updateEnabledState();
}

void IngestDlg::buildFileNameSequence()
{
/*
    This function displays what the ingested file paths will be for the 1st, 2nd and last
    file. If the primary location tab is selected the paths to the primary location are shown.
    If the backup location tab is chosen then the backup location paths are shown.
*/
    if (G::isLogger) G::log("IngestDlg::buildFileNameSequence");
    if (isInitializing) return;
    // build filename from tokenString
    QString key = ui->filenameTemplatesCB->currentText();
    QString tokenString;
    if (filenameTemplatesMap.contains(key))
        tokenString = filenameTemplatesMap[key];
    else return;

    // seqNum is required by parseTokenString
    seqStart =  ui->spinBoxStartNumber->value();
    seqNum = seqStart;
    QString ext1 = "." + pickList.at(0).suffix().toUpper();
    QString fileName1 =  parseTokenString(pickList.at(0), tokenString) + ext1;

    QString dirPath;
    // use auto
    if (ui->autoRadio->isChecked()) {
        // folderpath based on primary or backup tab selected
        if(ui->autoIngestTab->tabBar()->currentIndex() == 0) dirPath = folderPath;
        else dirPath = folderPath2;
    }
    else {
        dirPath = folderPath;
    }

    if (fileCount == 1) {
        ui->folderPathLabel->setText(dirPath + fileName1);
        ui->folderPathLabel->setToolTip(dirPath + fileName1);
        ui->folderPathLabel_2->setText("");
        ui->folderPathLabel_2->setToolTip("");
        ui->folderPathLabel_3->setText("");
        ui->folderPathLabel_4->setText("");
        ui->folderPathLabel_4->setToolTip("");
        return;
    }

    seqNum++;
    QString ext2 = "." + pickList.at(1).suffix().toUpper();
    QString fileName2 = parseTokenString(pickList.at(1), tokenString) + ext2;
    if (fileCount == 2) {
        ui->folderPathLabel->setText(dirPath + fileName1);
        ui->folderPathLabel->setToolTip(dirPath + fileName1);
        ui->folderPathLabel_2->setText(dirPath + fileName2);
        ui->folderPathLabel_2->setToolTip(dirPath + fileName2);
        ui->folderPathLabel_3->setText("");
        ui->folderPathLabel_4->setText("");
        ui->folderPathLabel_4->setToolTip("");
        return;
    }

    seqNum = seqStart + fileCount - 1;
    QString extN = "." + pickList.at(fileCount - 1).suffix().toUpper();
    QString fileNameN = parseTokenString(pickList.at(fileCount - 1), tokenString) + extN;
    if (fileCount > 2) {
        ui->folderPathLabel->setText(dirPath + fileName1);
        ui->folderPathLabel->setToolTip(dirPath + fileName1);
        ui->folderPathLabel_2->setText(dirPath + fileName2);
        ui->folderPathLabel_2->setToolTip(dirPath + fileName2);
        ui->folderPathLabel_3->setText("...");
        ui->folderPathLabel_4->setText(dirPath + fileNameN);
        ui->folderPathLabel_4->setToolTip(dirPath + fileNameN);
    }
}

void IngestDlg::on_descriptionLineEdit_textChanged(const QString& arg1)
{
    if (G::isLogger) G::log("IngestDlg::on_descriptionLineEdit_textChanged");
    static QString prevText = "";
    // copy primary description to backup description unless it is already different
    QString desc2 = ui->descriptionLineEdit_2->text();
    if (desc2  == prevText) ui->descriptionLineEdit_2->setText(arg1);
    prevText = arg1;
    updateFolderPaths();
    // add description to completer list (move to after dialog closes)
//    QString desc = ui->descriptionLineEdit->text();
//    if (desc.length() > 0) {
//        if (!ingestDescriptionCompleter.contains(desc))
//            ingestDescriptionCompleter << desc;
//    }
}

void IngestDlg::on_descriptionLineEdit_2_textChanged(const QString/* &arg1*/)
{
    if (G::isLogger) G::log("IngestDlg::on_descriptionLineEdit_2_textChanged");
    // add description to completer list (move to after dialog closes)
//    QString desc = ui->descriptionLineEdit->text();
//    if (desc.length() > 0) {
//        if (!ingestDescriptionCompleter.contains(desc))
//            ingestDescriptionCompleter << desc;
//    }
    updateFolderPaths();
}

void IngestDlg::on_spinBoxStartNumber_textChanged(const QString /* &arg1 */)
{
    if (G::isLogger) G::log("IngestDlg::on_spinBoxStartNumber_textChanged");
    buildFileNameSequence();
}

int IngestDlg::getSequenceStart(const QString &path)
{
    if (G::isLogger) G::log("IngestDlg::getSequenceStart");
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

    // check if previous ingest to another folder for the same date
    if (seqDate == G::ingestLastSeqDate) {
        if (sequence < G::ingestCount) {
            sequence = G::ingestCount + 1;
        }
    }

    return sequence;
}

void IngestDlg::getAvailableStorageMB()
{
/*
    Return how many MB available on the drive selected that contains path.  If the drive is
    unavailable change its font to red.

    Each time the destination folder parameters are changed for either auto generation or
    manual, updateFolderPaths() is called, where class variables folderPath, folderPath2,
    pathDrive, pathDrive2, autoDriveAvailable and autoDriveAvailable2 are updated and
    this function is called.
*/
    if (G::isLogger) G::log("IngestDlg::getAvailableStorageMB");

    QStorageInfo info(drivePath);
    /*
    qDebug() << "IngestDlg::"
             << "drivePath =" << drivePath
             << "folderPath =" << folderPath
             << "info.name() =" << info.name()
             << "info.rootPath() =" << info.rootPath()
             << "info.displayName()) =" << info.displayName()
             << "info.device() =" << info.device()
             << "info.fileSystemType() =" << info.fileSystemType()
             << "info.isValid() =" << info.isValid()
             << "\ninfo.mountedVolumes() =" << info.mountedVolumes()
                ;
                //*/

    if (info.isValid() && info.rootPath() != "") {
        #ifdef Q_OS_WIN
            if (info.rootPath() == info.displayName()) drive = drivePath;
            else drive = drivePath + " (" + info.displayName() + ")";
        #endif
        #ifdef Q_OS_MAC
            drive = info.displayName();
        #endif
        qint64 bytes = info.bytesAvailable();
        availableMB = bytes / 1024 / 1024;
        QString s = drive + " " + Utilities::formatMemory(bytes);
        if (picksMB > availableMB) ui->drive->setStyleSheet(redText);
        else ui->drive->setStyleSheet(normalText);
        ui->drive->setText(s);
    }
    else {
        ui->drive->setStyleSheet(redText);
        ui->drive->setText("Primary drive " + drivePath + " is unavailable.");
    }

    // backup drive
    info.setPath(drive2Path);
    /*
    qDebug() << "IngestDlg::"
             << "drive2Path =" << drive2Path
             << "folderPath2 =" << folderPath2
             << "info.name() =" << info.name()
             << "info.rootPath() =" << info.rootPath()
             << "info.displayName()) =" << info.displayName()
             << "info.device() =" << info.device()
             << "info.fileSystemType() =" << info.fileSystemType()
             << "info.isValid() =" << info.isValid()
             << "\ninfo.mountedVolumes() =" << info.mountedVolumes()
                ;
                //*/
    if (info.isValid() && info.rootPath() != "") {
        #ifdef Q_OS_WIN
            if (info.rootPath() == info.displayName()) drive2 = drive2Path;
            else drive2 = drive2Path + " (" + info.displayName() + ")";
        #endif
        #ifdef Q_OS_MAC
            drive2 = info.displayName() + ":";
        #endif
        qint64 bytes = info.bytesAvailable();
        availableMB2 = bytes / 1024 / 1024;
        QString s = drive2 + " " + Utilities::formatMemory(bytes);
        if (picksMB > availableMB2) ui->drive_2->setStyleSheet(redText);
        else ui->drive_2->setStyleSheet(normalText);
        ui->drive_2->setText(s);
    }
    else {
        ui->drive_2->setStyleSheet(redText);
        ui->drive_2->setText("Backup drive " + drive2Path + " is unavailable.");
    }
    ui->drive_2->setVisible(isBackup);
}

void IngestDlg::updateEnabledState()
{
    if (G::isLogger) G::log("IngestDlg::updateEnabledState");
    if (isAuto) {
        // auto widgets
        ui->autoIngestTab->tabBar()->setStyleSheet(G::css);
        ui->selectRootFolderBtn->setEnabled(true);
        ui->selectRootFolderBtn_2->setEnabled(true);
        ui->templateLabel1->setEnabled(true);
        ui->templateLabel1_2->setEnabled(true);
        ui->rootFolderLabel->setEnabled(true);
        ui->rootFolderLabel_2->setEnabled(true);
        ui->pathTemplatesCB->setEnabled(true);
        ui->pathTemplatesCB_2->setEnabled(true);
        ui->pathTemplatesBtn->setEnabled(true);
        ui->pathTemplatesBtn_2->setEnabled(true);
        ui->folderDescription->setEnabled(true);
        ui->folderDescription_2->setEnabled(true);
        ui->descriptionLineEdit->setEnabled(true);
        ui->descriptionLineEdit_2->setEnabled(true);
        ui->editDescriptionListBtn->setEnabled(true);
        ui->editDescriptionListBtn_2->setEnabled(true);
        ui->destinationFolderLabel->setEnabled(true);
        ui->destinationFolderLabel_2->setEnabled(true);
        ui->folderLabel->setEnabled(true);
        ui->folderLabel_2->setEnabled(true);

        // manual widgets
        ui->selectFolderBtn->setEnabled(false);
        ui->selectFolderBtn_2->setEnabled(false);
        ui->manualFolderLabel->setEnabled(false);
        ui->manualFolderLabel_2->setEnabled(false);
    }
    else {
        // auto widgets
        ui->autoIngestTab->tabBar()->setStyleSheet("QTabBar::tab {color:" + G::disabledColor.name() + ";}");
        ui->selectRootFolderBtn->setEnabled(false);
        ui->selectRootFolderBtn_2->setEnabled(false);
        ui->templateLabel1->setEnabled(false);
        ui->templateLabel1_2->setEnabled(false);
        ui->rootFolderLabel->setEnabled(false);
        ui->rootFolderLabel_2->setEnabled(false);
        ui->pathTemplatesCB->setEnabled(false);
        ui->pathTemplatesCB_2->setEnabled(false);
        ui->pathTemplatesBtn->setEnabled(false);
        ui->pathTemplatesBtn_2->setEnabled(false);
        ui->folderDescription->setEnabled(false);
        ui->folderDescription_2->setEnabled(false);
        ui->descriptionLineEdit->setEnabled(false);
        ui->descriptionLineEdit_2->setEnabled(false);
        ui->editDescriptionListBtn->setEnabled(false);
        ui->editDescriptionListBtn_2->setEnabled(false);
        ui->destinationFolderLabel->setEnabled(false);
        ui->destinationFolderLabel_2->setEnabled(false);
        ui->folderLabel->setEnabled(false);
        ui->folderLabel_2->setEnabled(false);

        // manual widgets
        ui->selectFolderBtn->setEnabled(true);
        ui->selectFolderBtn_2->setEnabled(true);
        ui->manualFolderLabel->setEnabled(true);
        ui->manualFolderLabel_2->setEnabled(true);
    }

    // check boxes
    ui->beepChk->setEnabled(isBackgroundIngest);
}

void IngestDlg::on_autoRadio_toggled(bool checked)
{
    if (G::isLogger) G::log("IngestDlg::on_autoRadio_toggled");
    isAuto = checked;
    updateFolderPaths();
}

void IngestDlg::initTokenList()
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

void IngestDlg::initExampleMap()
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

void IngestDlg::on_pathTemplatesCB_currentTextChanged(const QString &arg1)
{
    if (G::isLogger) G::log("IngestDlg::on_pathTemplatesCB_currentTextChanged");
    QString tokenString = pathTemplatesMap[arg1];
    fromRootToBaseFolder = parseTokenString(pickList.at(0), tokenString);
//    if (fromRootToBaseFolder.length() > 0) fromRootToBaseFolder += "/";
    if (!isInitializing) pathTemplateSelected = ui->pathTemplatesCB->currentIndex();
    updateFolderPaths();
    seqStart += getSequenceStart(folderPath);
    updateExistingSequence();
}

void IngestDlg::on_pathTemplatesCB_2_currentTextChanged(const QString &arg1)
{
    if (G::isLogger) G::log("IngestDlg::on_pathTemplatesCB_2_currentTextChanged");
    if (arg1 == "") return;
    QString tokenString = pathTemplatesMap[arg1];
    fromRootToBaseFolder2 = parseTokenString(pickList.at(0), tokenString);
    if (!isInitializing) pathTemplateSelected2 = ui->pathTemplatesCB_2->currentIndex();
    updateFolderPaths();
}

void IngestDlg::on_filenameTemplatesCB_currentTextChanged(const QString &arg1)
{
    if (G::isLogger) G::log("IngestDlg::on_filenameTemplatesCB_currentTextChanged");
    if (arg1 == "") return;
    QString tokenString = filenameTemplatesMap[arg1];
    if (!isInitializing) filenameTemplateSelected = ui->filenameTemplatesCB->currentIndex();
    updateExistingSequence();
}

void IngestDlg::on_pathTemplatesBtn_clicked()
{
/*

*/
    if (G::isLogger) G::log("IngestDlg::on_pathTemplatesBtn_clicked");
    qDebug() << "IngestDlg::on_pathTemplatesBtn_clicked";
    // setup TokenDlg
    QMap<QString,QString> usingTokenMap;
    QString title = "Token Editor - Path from Root to Destination Folder";
    int index = ui->pathTemplatesCB->currentIndex();
    QString currentKey = ui->pathTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, pathTemplatesMap, usingTokenMap,
                                      index, currentKey, title, this);

    tokenDlg->exec();

    // rebuild template list and set to same item as TokenDlg for user continuity
    ui->pathTemplatesCB->clear();
    // for some reason ui->pathTemplatesCB->clear() prepends blank item to pathTemplatesMap
    pathTemplatesMap.remove("");
    QMap<QString, QString>::iterator i;
    int row = 0;
    for (i = pathTemplatesMap.begin(); i != pathTemplatesMap.end(); ++i) {
        QVariant iKey = i.key();
        if (i.key() == "") continue;
        ui->pathTemplatesCB->addItem(i.key());
        if (i.key() == currentKey) index = row;
        row++;
    }
    int len = ui->pathTemplatesCB->count();
    ui->pathTemplatesCB->setCurrentIndex(index);
    on_pathTemplatesCB_currentTextChanged(currentKey);
}

void IngestDlg::on_pathTemplatesBtn_2_clicked()
{
/*
    Performs same function as button on primary tab - just here for convenience and to
    make the ui more intuitive
*/
    if (G::isLogger) G::log("IngestDlg::on_pathTemplatesBtn_2_clicked");
    on_pathTemplatesBtn_clicked();
}

void IngestDlg::on_filenameTemplatesBtn_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_filenameTemplatesBtn_clicked");
    // setup TokenDlg
    // title is also used to filter warnings, so if you change it here also change
    // it in TokenDlg::updateUniqueFileNameWarning
    QString title = "Token Editor - Destination File Name";
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

void IngestDlg::on_editDescriptionListBtn_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_editDescriptionListBtn_clicked");
    EditListDlg listDlg(ingestDescriptionListModel, "Description completer list");
    listDlg.exec();
    ingestDescriptionCompleter = ingestDescriptionListModel->stringList();
}

void IngestDlg::on_editDescriptionListBtn_2_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_editDescriptionListBtn_2_clicked");
    on_editDescriptionListBtn_clicked();
}

void IngestDlg::on_combinedIncludeJpgChk_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_combinedIncludeJpgChk_clicked");
    combinedIncludeJpg = ui->combinedIncludeJpgChk->isChecked();
    getPicks();
}

void IngestDlg::on_ejectChk_stateChanged(int /*arg1*/)
{
    if (G::isLogger) G::log("IngestDlg::on_ejectChk_stateChanged");
    autoEjectUsb = ui->ejectChk->isChecked();
}

void IngestDlg::on_integrityChk_stateChanged(int /*arg1*/)
{
    if (G::isLogger) G::log("IngestDlg::on_integrityChk_stateChanged");
    integrityCheck = ui->integrityChk->isChecked();
}

void IngestDlg::on_backgroundIngestChk_stateChanged(int /*arg1*/)
{
    if (G::isLogger) G::log("IngestDlg::on_backgroundIngestChk_stateChanged");
    isBackgroundIngest = ui->backgroundIngestChk->isChecked();
    ui->openIngestFolderChk->setEnabled(!isBackgroundIngest);
    ui->ejectChk->setEnabled(!isBackgroundIngest);
    ui->beepChk->setEnabled(isBackgroundIngest);
    if (isBackgroundIngest) {
        ui->ejectChk->setChecked(false);
    }
}

void IngestDlg::on_beepChk_stateChanged(int /*arg1*/)
{
    if (G::isLogger) G::log("IngestDlg::on_beepChk_stateChanged");
    isBackgroundIngestBeep = ui->beepChk->isChecked();
}

void IngestDlg::on_backupChk_stateChanged(int arg1)
{
    if (G::isLogger) G::log("IngestDlg::on_backupChk_stateChanged");
    isBackup = arg1;
    getAvailableStorageMB();
}

void IngestDlg::on_helpBtn_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_helpBtn_clicked");

    QDialog *dlg = new QDialog;
    Ui::IngestAutoPath *ui = new Ui::IngestAutoPath;
    ui->setupUi(dlg);
    ui->textBrowser->setOpenExternalLinks(true);
    dlg->setWindowTitle("Ingest Automatic Path Help");
    dlg->setStyleSheet(G::css);
    dlg->exec();

//    QFile f(":/Docs/ingestautopath.html");
//    f.open(QIODevice::ReadOnly);
//    QDialog *dlg = new QDialog;
//    QTextBrowser *text = new QTextBrowser;
////    QString style = "background-color: rgb(55,55,55); font-size:" + G::fontSize + "px;";
////    text->setStyleSheet(style);
//    text->setMinimumWidth(600);
//    text->setMinimumHeight(600);
//    text->setContentsMargins(9,9,9,9);
//    text->setHtml(f.readAll());
//    text->setStyleSheet(css);
//    dlg->setLayout(new QHBoxLayout);
//    dlg->layout()->setContentsMargins(0,0,0,0);
//    dlg->layout()->addWidget(text);
//    dlg->exec();
}

void IngestDlg::on_cancelBtn_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_cancelBtn_clicked");
    //test();
    //return;
    reject();
}

void IngestDlg::on_okBtn_clicked()
{
    if (G::isLogger) G::log("IngestDlg::on_okBtn_clicked");
    // check parameters
    if (!parametersOk()) return;

    // add description to completer list (move to after dialog closes)
    QString desc = ui->descriptionLineEdit->text();
    if (desc.length() > 0) {
        if (!ingestDescriptionCompleter.contains(desc))
            ingestDescriptionCompleter << desc;
    }

    quitIfNotEnoughSpace();

    bool backup = ui->backupChk->isChecked();

    // make sure the destination folder exists
    QDir dir(folderPath);
    if (!dir.exists()) {
        if(!dir.mkpath(folderPath)) {
            QMessageBox::critical(nullptr, tr("Error"),
                 "The folder \"" + folderPath + "\" was not created.");
            return;
        }
    }

    // make sure the backup folder has been created
    if (backup) {
        QDir dir2(folderPath2);
        if (!dir2.mkpath(folderPath2)) {
            QMessageBox::warning(nullptr, tr("Error"),
                 "The folder \"" + folderPath2 + "\" was not created.");
            return;
        }
    }

    // make sure there is a file name template selected
    QString key = ui->filenameTemplatesCB->currentText();
    if (!filenameTemplatesMap.contains(key)) {
        QMessageBox::warning(nullptr, tr("Error"),
             "A file name template must be selected.");
        return;
    }

    if (ui->backgroundIngestChk->isChecked()) {
        QDialog::accept();
    }
    else {
        ingest();
    }
}

void IngestDlg::on_autoIngestTab_currentChanged(int /*index*/)
{
    if (G::isLogger) G::log("IngestDlg::on_autoIngestTab_currentChanged");
    buildFileNameSequence();
}

void IngestDlg::on_openIngestFolderChk_stateChanged(int arg1)
{
    if (G::isLogger) G::log("IngestDlgon_openIngestFolderChk_stateChanged");
    gotoIngestFolder = arg1;
}

void IngestDlg::fontSize()
{
/*
    The font size is determined by the assigned size G::fontsize in combination with the
    display screen viewing percentage.  This function is called by the resize and move
    events in case the user drags the dialog from one screen to another with different
    settings.
*/
    if (G::isLogger) G::log("IngestDlg::fontSize");
    QScreen *screen = qApp->screenAt(geometry().center());
    if (screen == nullptr) return;
    qreal screenScaling = screen->logicalDotsPerInch() / 96;
    int fontPtSize = static_cast<int>(G::strFontSize.toInt() / screenScaling);
    int fontPxSize = static_cast<int>(fontPtSize * G::ptToPx);
    QString titleShift = QString::number(fontPxSize * screenScaling / 2);

    QString fs = "QWidget {font-size: " + G::strFontSize + "pt;}";
//    QString fs = "QWidget {font-size: " + QString::number(fontPtSize) + "pt;}";
    /*
    qDebug() << "IngestDlg::"
             << "G::fontSize" << G::fontSize
             << "screenScaling" << screenScaling
             << "adjustedFontPtSize" << adjustedFontPtSize
             << "fs" << fs;  */

    QString titlePlacement =
         "QGroupBox {margin-top: " + titleShift + "px;}"
         "QGroupBox::title {top: -"  + titleShift + "px;}";

    setStyleSheet(fs + titlePlacement);
    ui->helpBtn->setStyleSheet(fs);
    ui->autoRadio->setStyleSheet(fs);

    ui->autoIngestTab->setStyleSheet(fs);
    ui->descriptionLineEdit->setStyleSheet(fs);
    ui->descriptionLineEdit_2->setStyleSheet(fs);
    ui->destinationFolderLabel->setStyleSheet(fs);
    ui->destinationFolderLabel_2->setStyleSheet(fs);
    ui->folderDescription->setStyleSheet(fs);
    ui->folderDescription_2->setStyleSheet(fs);
    ui->folderLabel->setStyleSheet(fs);
    autoDriveAvailable ? ui->folderLabel->setStyleSheet(normalText) : ui->folderLabel->setStyleSheet(redText);
    ui->folderLabel_2->setStyleSheet(fs);
    autoDrive2Available ? ui->folderLabel_2->setStyleSheet(normalText) : ui->folderLabel_2->setStyleSheet(redText);
    ui->pathTemplatesCB->setStyleSheet(fs);
    ui->pathTemplatesCB_2->setStyleSheet(fs);
    ui->pathTemplatesBtn->setStyleSheet(fs);
    ui->pathTemplatesBtn_2->setStyleSheet(fs);
    ui->rootFolderLabel->setStyleSheet(fs);
    ui->rootFolderLabel_2->setStyleSheet(fs);
    ui->selectRootFolderBtn->setStyleSheet(fs);
    ui->selectRootFolderBtn_2->setStyleSheet(fs);
    ui->templateLabel1->setStyleSheet(fs);
    ui->templateLabel1_2->setStyleSheet(fs);

    ui->selectFolderBtn->setStyleSheet(fs);
    ui->selectFolderBtn_2->setStyleSheet(fs);

    ui->filenameGroupBox->setStyleSheet(fs);
    ui->templateLabel2->setStyleSheet(fs);
    ui->existingSequenceLabel->setStyleSheet(fs);
    ui->filenameTemplatesBtn->setStyleSheet(fs);
    ui->filenameTemplatesCB->setStyleSheet(fs);
    ui->spinBoxStartNumber->setStyleSheet(fs);
    ui->startSeqLabel->setStyleSheet(fs);

    ui->seqGroupBox->setStyleSheet(fs);
    ui->folderPathLabel->setStyleSheet(fs);
    ui->folderPathLabel_2->setStyleSheet(fs);
    ui->folderPathLabel_3->setStyleSheet(fs);
    ui->folderPathLabel_4->setStyleSheet(fs);

    ui->cancelBtn->setStyleSheet(fs);
    ui->okBtn->setStyleSheet(fs);

    adjustSize();
}

void IngestDlg::showEvent(QShowEvent *event)
{
    if (G::isLogger) G::log("IngestDlg::showEvent");
    QDialog::showEvent(event);
    ui->autoIngestTab->setStyleSheet(
        "QWidget {border-color:" + G::tabWidgetBorderColor.name() + ";border-radius: 0px;}"
        "QPushButton, QComboBox, QLineEdit {border-color:" + G::borderColor.name() + ";}"
    );
    updateEnabledState();
}

void IngestDlg::keyPressEvent(QKeyEvent *event)
{
/*
    Intercept keystrokes and shortcut click ingest button if "q" or "Q" pressed.  This
    improves ergonomics when the user opens IngestDlg with a "Q" from MW and immediately
    wants to ingest without changing any of the dialog settings.
*/
    if (G::isLogger) G::log("IngestDlg::keyPressEvent");
    QDialog::keyPressEvent(event);
    if (event->key() == Qt::Key_Q) on_okBtn_clicked();
}

void IngestDlg::test()
{
    if (G::isLogger) G::log("IngestDlg::test");
    //ui->folderLabel->setStyleSheet("QLabel {color:" + G::disabledColor.name() + ";}");
    QString key = ui->filenameTemplatesCB->currentText();
    qDebug() << "IngestDlg::test"
             << "filenameTemplatesMap.contains(key) =" << filenameTemplatesMap.contains(key)
             << "key =" << ui->filenameTemplatesCB->currentText()
             << "filenameTemplatesMap =" << filenameTemplatesMap
                ;
}

