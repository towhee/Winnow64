#include "ingestdlg.h"
#include "ui_copypickdlg.h"
#include <QDebug>

/*
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
*/

IngestDlg::IngestDlg(QWidget *parent,
                     QFileInfoList &imageList,
                     Metadata *metadata,
                     QString ingestRootFolder,
                     QMap<QString, QString>& pathTemplates,
                     QMap<QString, QString>& filenameTemplates,
                     int& pathTemplateSelected,
                     int& filenameTemplateSelected,
                     bool isAuto) :

                     QDialog(parent),
                     ui(new Ui::IngestDlg),
                     isAuto(isAuto),
                     pickList(imageList),
                     metadata(metadata),
                     pathTemplatesMap(pathTemplates),
                     filenameTemplatesMap(filenameTemplates),
                     pathTemplateSelected(pathTemplateSelected),
                     filenameTemplateSelected(filenameTemplateSelected),
                     rootFolderPath(ingestRootFolder)
{
    ui->setupUi(this);
    ui->pathTemplatesCB->setView(new QListView());      // req'd for setting row height in stylesheet
    ui->filenameTemplatesCB->setView(new QListView());  // req'd for setting row height in stylesheet

    isInitializing = true;

    // stats
    fileCount = pickList.count();
    fileMB = 0;
    foreach(QFileInfo info, pickList) fileMB += (float)info.size()/1000000;
    QString s1 = QString::number(fileCount);
    QString s2 = fileCount == 1 ? " file using " : " files using ";
    QString s3 = QString::number(fileMB, 'f', 1);
    ui->statsLabel->setText(s1 + s2 + s3 + " MB");

    ui->rootFolderLabel->setText(rootFolderPath);
    ui->rootFolderLabel->setToolTip(ui->rootFolderLabel->text());

    // initialize templates and tokens
    initTokenList();
    initExampleMap();
    QMap<QString, QString>::iterator i;
    if (pathTemplatesMap.count() == 0) {
        pathTemplatesMap["YYYY̸YYMM̸YYYY-MM-DD"] = "{YYYY}/{YYYY}{MM}/{YYYY}-{MM}-{DD}";
        pathTemplatesMap["YYYY-MM-DD"] = "{YYYY}-{MM}-{DD}";
    }
    for (i = pathTemplatesMap.begin(); i != pathTemplatesMap.end(); ++i)
        ui->pathTemplatesCB->addItem(i.key());
     ui->pathTemplatesCB->setCurrentIndex(pathTemplateSelected);

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
    isInitializing = false;
    buildFileNameSequence();
}

IngestDlg::~IngestDlg()
{
    delete ui;
}

void IngestDlg::renameIfExists(QString &destination,
                                 QString &fileName,
                                 QString &dotSuffix)
{
    int count = 0;
    bool fileAlreadyExists = true;
    do {
        QFile testFile(destination);
        if (testFile.exists()) {
            QString s = "_" + QString::number(++count);
            destination = folderPath + fileName + s + dotSuffix;
        }
        else fileAlreadyExists = false;
    } while (fileAlreadyExists);
}

void IngestDlg::accept()
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        if(!dir.mkpath(folderPath)) {
            QMessageBox::critical(this, tr("Error"),
                 "The folder (" + folderPath + ") was not created.");
            QDialog::accept();
            return;
        }
    }

    QString key = ui->filenameTemplatesCB->currentText();
    QString tokenString;
    if (filenameTemplatesMap.contains(key))
        tokenString = filenameTemplatesMap[key];
    else {
        // add message explaining failure
        return;
    }

    // copy picked images
    for (int i=0; i < pickList.size(); ++i) {
        int progress = (i+1)*100/(pickList.size()+1);
        ui->progressBar->setValue(progress);
        qApp->processEvents();
        QFileInfo fileInfo = pickList.at(i);
        QString source = fileInfo.absoluteFilePath();
        // seqNum is required by parseTokenString
        seqNum =  ui->spinBoxStartNumber->value() + i;
        QString fileName =  parseTokenString(pickList.at(i), tokenString);
        QString suffix = fileInfo.completeSuffix().toLower();
        QString dotSuffix = "." + suffix;

        // buffer to hold file with edited xmp data
        QByteArray buffer;

        // if there is edited xmp data in an eligible file format
        if (metadata->writeMetadata(source, buffer)) {
            // write a sidecar
            if (metadata->sidecarFormats.contains(suffix)) {
                QString sidecar = folderPath + fileName + ".xmp";
                QFile sidecarFile(sidecar);
                sidecarFile.open(QIODevice::WriteOnly);
                sidecarFile.write(buffer);
                // copy image file
                QString destination = folderPath + fileName + dotSuffix;
                renameIfExists(destination, fileName, dotSuffix);
                QFile::copy(source, destination);            }
            // write inside the source file
            if (metadata->internalXmpFormats.contains(suffix)) {
                QString destination = folderPath + fileName + dotSuffix;
                QFile newFile(destination);
                newFile.open(QIODevice::WriteOnly);
                newFile.write(buffer);
            }
        }
        // no xmp data, just copy source to destination
        else {
            QString destination = folderPath + fileName + dotSuffix;
            renameIfExists(destination, fileName, dotSuffix);
            QFile::copy(source, destination);
        }
    }
    QDialog::accept();
}

void IngestDlg::updateExistingSequence()
{
    QDir dir(folderPath);
    if(dir.exists()) {
        int sequenceNum = getSequenceStart(folderPath);
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
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        folderPath = s;
        ui->manualFolderLabel->setText(folderPath);
        ui->manualFolderLabel->setToolTip( ui->manualFolderLabel->text());
    }
    buildFileNameSequence();
    updateExistingSequence();
    ui->manualRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

void IngestDlg::on_selectRootFolderBtn_clicked()
{
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    rootFolderPath = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath.length() > 0) {
        ui->rootFolderLabel->setText(rootFolderPath);
        ui->rootFolderLabel->setToolTip(ui->rootFolderLabel->text());
    }

    // send to MW where it will be saved in QSettings
    emit updateIngestParameters(rootFolderPath, isAuto);

    updateFolderPath();
    updateExistingSequence();
    ui->autoRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

bool IngestDlg::isToken(QString tokenString, int pos)
{
    QChar ch = tokenString.at(pos);
    if (ch.unicode() == 8233) return false;  // Paragraph Separator
    if (ch == "{") return false;
    if (pos == 0) return false;

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
    QString token;
//    int n = tokenString.length();
    for (int i = pos; i < tokenString.length(); i++) {
        ch = tokenString.at(i);
        if (ch == "}") {
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

QString IngestDlg::parseTokenString(QFileInfo info,
                                      QString tokenString)
{
    QString fPath = info.absoluteFilePath();
    createdDate = metadata->getCreatedDate(fPath);
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
                tokenResult = createdDate.date().toString("hh");
            if (currentToken == "MINUTE")
                tokenResult = createdDate.date().toString("mm");
            if (currentToken == "SECOND")
                tokenResult = createdDate.date().toString("ss");
            if (currentToken == "TITLE")
                tokenResult = metadata->getTitle(fPath);
            if (currentToken == "CREATOR")
                tokenResult = metadata->getCreator(fPath);
            if (currentToken == "COPYRIGHT")
                tokenResult = metadata->getCopyright(fPath);
            if (currentToken == "ORIGINAL FILENAME")
                tokenResult = info.baseName();
            if (currentToken == "MAKE")
                tokenResult = metadata->getMake(fPath);
            if (currentToken == "MODEL")
                tokenResult = metadata->getModel(fPath);
            if (currentToken == "DIMENSIONS")
                tokenResult = metadata->getDimensions(fPath);
            if (currentToken == "SHUTTER SPEED")
                tokenResult = metadata->getExposureTime(fPath);
            if (currentToken == "APERTURE")
                tokenResult = metadata->getAperture(fPath);
            if (currentToken == "ISO")
                tokenResult = metadata->getISO(fPath);
            if (currentToken == "FOCAL LENGTH")
                tokenResult = metadata->getFocalLength(fPath);
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

void IngestDlg::updateFolderPath()
{
    if (ui->autoRadio->isChecked() || isInitializing) {
        baseFolderDescription = (ui->descriptionLineEdit->text().length() > 0)
                ? ui->descriptionLineEdit->text() : "";
        folderPath = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/";
        ui->folderLabel->setText(folderPath);
        ui->folderLabel->setToolTip(ui->folderLabel->text());
    }
    else {
        folderPath = ui->manualFolderLabel->text() + "/";
    }
    updateExistingSequence();
}

void IngestDlg::buildFileNameSequence()
{
    if (isInitializing) return;
    // build filename from tokenString
    QString key = ui->filenameTemplatesCB->currentText();
    QString tokenString;
    if (filenameTemplatesMap.contains(key))
        tokenString = filenameTemplatesMap[key];
    // seqNum is required by parseTokenString
    seqStart =  ui->spinBoxStartNumber->value();
    seqNum = seqStart;
    QString fileName1 =  parseTokenString(pickList.at(0), tokenString);
    seqNum++;
    QString fileName2;
    if (fileCount > 1)
        fileName2 = parseTokenString(pickList.at(1), tokenString);
    seqNum = seqStart + fileCount - 1;
    QString fileNameN = parseTokenString(pickList.at(fileCount - 1), tokenString);
    ui->folderPathLabel->setText(folderPath + fileName1);
    ui->folderPathLabel->setToolTip(folderPath + fileName1);
    if(fileCount > 1) {
        ui->folderPathLabel_2->setText(folderPath + fileName2);
        ui->folderPathLabel_2->setToolTip(folderPath + fileName2);
        ui->folderPathLabel_4->setText(folderPath + fileNameN);
        ui->folderPathLabel_4->setToolTip(folderPath + fileNameN);
    }
    else {
        ui->folderPathLabel_2->setText("");
        ui->folderPathLabel_2->setToolTip("");
        ui->folderPathLabel_4->setText(folderPath + fileName1);
        ui->folderPathLabel_4->setToolTip(folderPath + fileName1);
    }
}

void IngestDlg::on_descriptionLineEdit_textChanged(const QString& /*arg1*/)
{
    updateFolderPath();
    ui->autoRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

void IngestDlg::on_spinBoxStartNumber_valueChanged(const QString /* &arg1 */)
{
    updateFolderPath();
}

int IngestDlg::getSequenceStart(const QString &path)
{
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

void IngestDlg::updateStyleOfFolderLabels()
{
    if (ui->autoRadio->isChecked()) {
        ui->folderLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
        ui->manualFolderLabel->setStyleSheet("QLabel{color:rgb(229,229,229);}");
    }
    else {
        ui->folderLabel->setStyleSheet("QLabel{color:rgb(229,229,229);}");
        ui->manualFolderLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
    }
}

void IngestDlg::on_autoRadio_toggled(bool checked)
{
    if (checked) {      // auto
        isAuto = true;
        if (ui->folderLabel->text().length() > 0) {
            updateFolderPath();
            updateStyleOfFolderLabels();
            getSequenceStart(folderPath);
            updateExistingSequence();
        }
    }
    else {              // manual
        isAuto = false;
        if (ui->manualFolderLabel->text().length() > 0) {
            folderPath = ui->manualFolderLabel->text();
            buildFileNameSequence();
            updateExistingSequence();
            updateStyleOfFolderLabels();
        }
    }
    emit updateIngestParameters(rootFolderPath, isAuto);
}

void IngestDlg::initTokenList()
{
/*
The list of tokens in the token editor will appear in this order.
*/
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

void IngestDlg::on_pathTemplatesCB_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "") return;
    QString tokenString = pathTemplatesMap[arg1];
    fromRootToBaseFolder = parseTokenString(pickList.at(0), tokenString);
    if (!isInitializing) pathTemplateSelected = ui->pathTemplatesCB->currentIndex();
    updateFolderPath();
    seqStart = getSequenceStart(folderPath);
    updateExistingSequence();
}

void IngestDlg::on_filenameTemplatesCB_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "") return;
    QString tokenString = filenameTemplatesMap[arg1];
    if (!isInitializing) filenameTemplateSelected = ui->filenameTemplatesCB->currentIndex();
    updateExistingSequence();
}

void IngestDlg::on_pathTemplatesBtn_clicked()
{
    // setup TokenDlg
    QString title = "Token Editor - Path from Root to Destination Folder";
    int index = ui->pathTemplatesCB->currentIndex();
    qDebug() << "on_pathTemplatesBtn_clicked  row =" << index;
    QString currentKey = ui->pathTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, pathTemplatesMap, index,
                                      currentKey, title, this);
    tokenDlg->exec();

    // rebuild template list and set to same item as TokenDlg for user continuity
    ui->pathTemplatesCB->clear();
    QMap<QString, QString>::iterator i;
    int row = 0;
    for (i = pathTemplatesMap.begin(); i != pathTemplatesMap.end(); ++i) {
        ui->pathTemplatesCB->addItem(i.key());
        if (i.key() == currentKey) index = row;
        row++;
    }
    ui->pathTemplatesCB->setCurrentIndex(index);
    on_pathTemplatesCB_currentIndexChanged(currentKey);
}

void IngestDlg::on_filenameTemplatesBtn_clicked()
{
    // setup TokenDlg
    QString title = "Token Editor - Destination File Name";
    int index = ui->filenameTemplatesCB->currentIndex();
    QString currentKey = ui->filenameTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, filenameTemplatesMap, index,
                                      currentKey, title, this);
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
    on_filenameTemplatesCB_currentIndexChanged(currentKey);
}

void IngestDlg::on_helpBtn_clicked()
{
    QWidget *helpDoc = new QWidget;
    Ui::helpIngest ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void IngestDlg::on_cancelBtn_clicked()
{
    reject();
}

void IngestDlg::on_okBtn_clicked()
{
    accept();
}

