#include "copypickdlg.h"
#include "ui_copypickdlg.h"
#include <QDebug>

CopyPickDlg::CopyPickDlg(QWidget *parent, QFileInfoList &imageList,
                         Metadata *metadata) :
    QDialog(parent),
    ui(new Ui::CopyPickDlg)
{
    ui->setupUi(this);
//    this->setStyleSheet("CopyPickDlgName"); // not working

    pickList = imageList;
    this->metadata = metadata;
//    ui->progressBar->setVisible(false);

//    ui->comboBox->addItem("Test 1");
//    ui->comboBox->addItem("Test 2");

    fileNameDatePrefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
    QString dateTime = metadata->getDateTime(pickList.at(0).absoluteFilePath());
    QString year = dateTime.left(4);
    QString month = dateTime.mid(5,2);
    folderPath = "E:/" + year + "/" + year + month + "/" + fileNameDatePrefix;
//    folderPath = "/users/roryhill/pictures/" + fileNameDatePrefix;
    updateFolderPath();
    ui->descriptionLineEdit->setFocus();
    getSequenceStart("");
}

CopyPickDlg::~CopyPickDlg()
{
    delete ui;
}

void CopyPickDlg::accept()
{
    QString destPath = ui->folderPathLabel->text();
    QDir dir;
    dir.mkdir(destPath);
    ui->progressBar->setVisible(true);
    QString prefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
    for (int i=0; i < pickList.size(); ++i) {
        int progress = (i+1)*100/(pickList.size()+1);
        ui->progressBar->setValue(progress);
        qApp->processEvents();
        qDebug() << "progressBar->value()" << ui->progressBar->value();
        QFileInfo fileInfo = pickList.at(i);
        QString sequence = "_" + QString("%1").arg(i+1, 4 ,10, QChar('0'));
        QString suffix = "." + fileInfo.completeSuffix();
        QString source = fileInfo.absoluteFilePath();
        QString destination = destPath + "/" + prefix + sequence + suffix;
        QFile::copy(source, destination);
    }
    QDialog::accept();
}

void CopyPickDlg::on_selectFolderBtn_clicked()
{
//    QString root = "E:/2016";
    QString root = "/users/roryhill/pictures";
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Month Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
//    qDebug() << "Folderpath: " << s;
    if (s.length() > 0) {
        folderPath = s + "/" + metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
        updateFolderPath();
    }
}

void CopyPickDlg::updateFolderPath()
{
    folderDescription = ui->descriptionLineEdit->text().length()>0
            ? "_" + ui->descriptionLineEdit->text() : "";
    ui->folderPathLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
    ui->folderPathLabel->setText(folderPath + folderDescription);
}

void CopyPickDlg::on_descriptionLineEdit_textChanged(const QString &arg1)
{
    updateFolderPath();
}

int CopyPickDlg::getSequenceStart(const QString &path)
{
    QDir dir;
    dir.setPath("/users/roryhill/Pictures/2048JPG");
//    dir.setPath(folderPath);
//    if (!dir.exists()) return 0;
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
        qDebug() << fName << fName.indexOf(".", 0) << seq << sequence;
    }
    return sequence;
}
