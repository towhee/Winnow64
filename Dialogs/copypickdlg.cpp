#include "copypickdlg.h"
#include "ui_copypickdlg.h"
#include <QDebug>

/*
Files are copied to a destination based on building a file path consisting of:

      Root Folder                   (rootFolderPath)
    + Path to base folder           (pathToBaseFolder) source pathTemplateString
    + Base Folder Description       (baseFolderDescription)
    + File Name                     (fileBaseName)     source filenameTemplateString
    + File Suffix                   (fileSuffix)

    ie E:/2018/201802/2018-02-08 Rory birthday/2018-02-08_0001.NEF

    rootFolderPath:         ie "E:/" where all the images are located
    pathToBaseFolder:       ie "2018/201802/2018-02-08" from the path template
    baseFolderDescription:  ie "Rory birthday" a description appended to the pathToBaseFolder
    fileBaseName:           ie "2018-02-08_0001" from the filename template
    fileSuffix:             ie ".NEF"

    folderPath:             ie "E:/2018/201802/2018-02-08 Rory birthday/"
                            where current images are to be copied YYYY-MM-DD_Description


    --------------- was this ---------------

    Root Folder + Path to base folder + File Base Name + File Sequence + File Suffix

    Root Folder: where all the images are located ie e:\ or users/pictures
    PathToBaseFolder: ie e:/YYYY/YYMM
    Folder: where current images are to be copied YYYY-MM-DD_Description
    File Base Name: YYYY-MM-DD (fileNameDatePrefix)
    File Sequence: _XXXX
    File Suffix: ie .NEF
*/

CopyPickDlg::CopyPickDlg(QWidget *parent,
                         QFileInfoList &imageList,
                         Metadata *metadata,
                         QString ingestRootFolder,
                         QMap<QString, QString>& pathTemplates,
                         QMap<QString, QString>& filenameTemplates,
                         int& pathTemplateSelected,
                         int& filenameTemplateSelected,
                         bool isAuto) :

                         QDialog(parent),
                         pickList(imageList),
                         metadata(metadata),
                         rootFolderPath(ingestRootFolder),
                         pathTemplatesMap(pathTemplates),
                         filenameTemplatesMap(filenameTemplates),
                         pathTemplateSelected(pathTemplateSelected),
                         filenameTemplateSelected(filenameTemplateSelected),
                         isAuto(isAuto),
                         ui(new Ui::CopyPickDlg)
{
    qDebug() << "CopyPickDlg::CopyPickDlg";
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

    // initialize templates and tokens
    qDebug() << "CopyPickDlg::CopyPickDlg:  "
             << "pathTemplateSelected" << pathTemplateSelected
             << "filenameTemplateSelected" << filenameTemplateSelected;
    initTokenMap();
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

CopyPickDlg::~CopyPickDlg()
{
    delete ui;
}

void CopyPickDlg::accept()
{
    qDebug() << "CopyPickDlg::accept";
    QDir dir(folderPath);
    if (!dir.exists()) {
        if(!dir.mkpath(folderPath)) {
            QMessageBox::critical(this, tr("Error"),
                 "The folder (" + folderPath + ") was not created.");
            QDialog::accept();
            return;
        }
    }
    ui->progressBar->setVisible(true);
    QString prefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());

    for (int i=0; i < pickList.size(); ++i) {
        int progress = (i+1)*100/(pickList.size()+1);
        ui->progressBar->setValue(progress);
        qApp->processEvents();
        QFileInfo fileInfo = pickList.at(i);
        int seqNum = ui->spinBoxStartNumber->value() + i;
        QString sequence = "_" + QString("%1").arg(seqNum, 4 , 10, QChar('0'));
        QString suffix = "." + fileInfo.completeSuffix();
        QString source = fileInfo.absoluteFilePath();
        QString destination = folderPath + "/" + prefix + sequence + suffix;
//        qDebug() << "Copying " << source << " to " << destination;
        QFile::copy(source, destination);
    }
    QDialog::accept();
}

void CopyPickDlg::updateExistingSequence()
{
    qDebug() << "CopyPickDlg::updateExistingSequence";
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

void CopyPickDlg::on_selectFolderBtn_clicked()
{
    qDebug() << "CopyPickDlg::on_selectFolderBtn_clicked";
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        folderPath = s;
        ui->manualFolderLabel->setText(folderPath);
    }
    buildFileNameSequence();
    updateExistingSequence();
    ui->manualRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

void CopyPickDlg::on_selectRootFolderBtn_clicked()
{
    qDebug() << "CopyPickDlg::on_selectRootFolderBtn_clicked";
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
//    QString rootFolderPath;
    rootFolderPath = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath.length() > 0) {
        ui->rootFolderLabel->setText(rootFolderPath);
    }

    // send to MW where it will be saved in QSettings
    emit updateIngestParameters(rootFolderPath, isAuto);

    pathToBaseFolder = rootFolderPath + "/" + year + month + "/";
//    ui->parentFolderLabel->setText(pathToBaseFolder);

    updateFolderPath();
    updateExistingSequence();
    ui->autoRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

bool CopyPickDlg::isToken(QString tokenString, int pos)
{
    qDebug() << "CopyPickDlg::isToken  tokenString =" << tokenString;
    QChar ch = tokenString.at(pos);
    if (ch.unicode() == 8233) return false;
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
    int n = tokenString.length();
    for (int i = pos; i < tokenString.length(); i++) {
        ch = tokenString.at(i);
        if (ch == "}") {
            for (int j = startPos; j < i; j++) {
                token.append(tokenString.at(j));
            }
//            qDebug() << "tokenMap.contains(token)" << token;
            if (tokenMap.contains(token)) {
                currentToken = token;
                tokenStart = startPos - 1;
                tokenEnd = i + 1;
                return true;
            }
//            qDebug() << "token =" << token;
        }
    }
    return false;
}

QString CopyPickDlg::parseTokenString(QFileInfo info,
                                      QString tokenString)
{
    qDebug() << "CopyPickDlg::parseTokenString";
    QString fPath = info.absoluteFilePath();
    createdDate = metadata->getCreatedDate(fPath);
    QString s;
    int i = 0;
//    for (int x = 0; x < tokenString.length(); x++)
//        qDebug() << "x =" << x << "char =" << tokenString.at(x);
    while (i < tokenString.length()) {
        if (isToken(tokenString, i + 1)) {
            QString tokenResult;
//            qDebug() << "currentToken =" << currentToken;
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
            // sequence from XX to XXXXXXX.  sequnceCount must be pre-assigned.
            if (currentToken.left(2) == "XX") {
                sequenceWidth = currentToken.length();
                tokenResult = QString("%1").arg(sequenceCount, sequenceWidth , 10, QChar('0'));
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

void CopyPickDlg::updateFolderPath()
{
    qDebug() << "CopyPickDlg::updateFolderPath";
    if (ui->autoRadio->isChecked() || isInitializing) {
        baseFolderDescription = (ui->descriptionLineEdit->text().length() > 0)
                ? ui->descriptionLineEdit->text() : "";
        folderPath = pathToBaseFolder + baseFolderDescription + "/";
        ui->folderLabel->setText(folderPath);
    }
    else {
        folderPath = ui->manualFolderLabel->text() + "/";
    }
//    buildFileNameSequence();
}

void CopyPickDlg::buildFileNameSequence()
{
    qDebug() << "CopyPickDlg::buildFileNameSequence";
    if (isInitializing) return;
    // build filename from tokenString
    QString key = ui->filenameTemplatesCB->currentText();
    QString tokenString;
    if (filenameTemplatesMap.contains(key))
        tokenString = filenameTemplatesMap[key];
    // sequenceCount is required by parseTokenString
    sequenceCount =  ui->spinBoxStartNumber->value();
    QString fileName1 =  parseTokenString(pickList.at(0), tokenString);
    sequenceCount++;
    QString fileName2;
    if (fileCount > 1)
        fileName2 = parseTokenString(pickList.at(1), tokenString);
    sequenceCount = sequenceStart + fileCount;
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

void CopyPickDlg::on_descriptionLineEdit_textChanged(const QString& /*arg1*/)
{
    qDebug() << "CopyPickDlg::on_descriptionLineEdit_textChanged";
    updateFolderPath();
    ui->autoRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

void CopyPickDlg::on_spinBoxStartNumber_valueChanged(const QString &arg1)
{
    qDebug() << "CopyPickDlg::on_spinBoxStartNumber_valueChanged";
    arg1.isEmpty();             // suppress compiler warning
    updateFolderPath();
}

int CopyPickDlg::getSequenceStart(const QString &path)
{
    qDebug() << "CopyPickDlg::getSequenceStart";
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
//        qDebug() << fName << fName.indexOf(".", 0) << seq << sequence;
    }
    return sequence;
}

void CopyPickDlg::updateStyleOfFolderLabels()
{
    qDebug() << "CopyPickDlg::updateStyleOfFolderLabels";
    if (ui->autoRadio->isChecked()) {
        ui->folderLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
        ui->manualFolderLabel->setStyleSheet("QLabel{color:rgb(229,229,229);}");
    }
    else {
        ui->folderLabel->setStyleSheet("QLabel{color:rgb(229,229,229);}");
        ui->manualFolderLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
    }
}

void CopyPickDlg::on_autoRadio_toggled(bool checked)
{
    qDebug() << "CopyPickDlg::on_autoRadio_toggled";
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

void CopyPickDlg::initTokenMap()
{
    qDebug() << "CopyPickDlg::initTokenMap";
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
    tokenMap["HOUR"] = "08";
    tokenMap["MINUTE"] = "32";
    tokenMap["SECOND"] = "45";
    tokenMap["TITLE"] = "Hill_Wedding";
    tokenMap["CREATOR"] = "Rory Hill";
    tokenMap["COPYRIGHT"] = "2018 Rory Hill";
    tokenMap["ORIGINAL FILENAME"] = "_C8I0024";
    tokenMap["MAKE"] = "Canon";
    tokenMap["MODEL"] = "Canon EOS-1D X Mark II";
    tokenMap["DIMENSIONS"] = "5472x3648";
    tokenMap["SHUTTER SPEED"] = "1/1000 sec";
    tokenMap["APERTURE"] = "f/5.6";
    tokenMap["ISO"] = "1600";
    tokenMap["FOCAL LENGTH"] = "840 mm";
    tokenMap["XX"] = "01";
    tokenMap["XXX"] = "001";
    tokenMap["XXXX"] = "0001";
    tokenMap["XXXXX"] = "00001";
    tokenMap["XXXXXX"] = "000001";
    tokenMap["XXXXXXX"] = "0000001";
}

void CopyPickDlg::on_pathTemplatesCB_currentIndexChanged(const QString &arg1)
{
    qDebug() << "CopyPickDlg::on_pathTemplatesCB_currentIndexChanged";
    if (arg1 == "") return;
    QString tokenString = pathTemplatesMap[arg1];
    pathToBaseFolder = rootFolderPath + parseTokenString(pickList.at(0), tokenString);
    if (!isInitializing) pathTemplateSelected = ui->pathTemplatesCB->currentIndex();

//    qDebug() << "on_pathTemplatesCB_currentIndexChanged  pathTemplateSelected ="
//             << pathTemplateSelected
//             << "pathTemplatesMap =" << pathTemplatesMap;

    updateFolderPath();
    sequenceStart = getSequenceStart(folderPath);
    updateExistingSequence();

}

void CopyPickDlg::on_filenameTemplatesCB_currentIndexChanged(const QString &arg1)
{
    qDebug() << "CopyPickDlg::on_filenameTemplatesCB_currentIndexChanged";
    if (arg1 == "") return;
    QString tokenString = filenameTemplatesMap[arg1];
    if (!isInitializing) filenameTemplateSelected = ui->filenameTemplatesCB->currentIndex();
//    fileNameBase = parseTokenString(pickList.at(0), filenameTemplatesMap, tokenString);
//    sequenceCount = getSequenceStart(folderPath);
    updateExistingSequence();
//    buildFileNameSequence();
}

void CopyPickDlg::on_pathTemplatesBtn_clicked()
{
    qDebug() << "CopyPickDlg::on_pathTemplatesBtn_clicked";
    // setup TokenDlg
    QString title = "Token Editor - Path from Root to Destination Folder";
    int index = ui->pathTemplatesCB->currentIndex();
    qDebug() << "on_pathTemplatesBtn_clicked  row =" << index;
    QString currentKey = ui->pathTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokenMap, pathTemplatesMap, index,
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

void CopyPickDlg::on_filenameTemplatesBtn_clicked()
{
    qDebug() << "CopyPickDlg::on_filenameTemplatesBtn_clicked";
    // setup TokenDlg
    QString title = "Token Editor - Destination File Name";
    int index = ui->filenameTemplatesCB->currentIndex();
    qDebug() << "filenameTemplatesCB  row =" << index;
    QString currentKey = ui->filenameTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokenMap, filenameTemplatesMap, index,
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

void CopyPickDlg::on_helpBtn_clicked()
{
    QDialog *helpDoc = new QDialog;
    Ui::helpIngest ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void CopyPickDlg::on_cancelBtn_clicked()
{
    reject();
}

void CopyPickDlg::on_okBtn_clicked()
{
    accept();
}

