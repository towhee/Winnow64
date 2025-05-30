#include "focuspointtrainer.h"

/*
    Documentation: see FOCUS PREDICTOR at top of mainwindow.cpp
*/

FocusPointTrainer::FocusPointTrainer(QObject *parent)
    : QObject{parent}
{}

void FocusPointTrainer::focus(QString srcPath, float x, float y, QString type, QImage srcImage)
{
    int px = 512;
    QImage trainImage = srcImage.scaled(QSize(px,px),Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // Set to folder with training images (Train_Creature, Train_Subject, Train_Human...)
    QString trainFolderPath = "/Users/roryhill/Documents/Documents - Quark/FocusPointTrainer/Train_Creature";
    QDir dir(trainFolderPath);
    // dir.count() includes two hidden folders (. and ..)
    QString trainFileName = QVariant(dir.count() - 1).toString() + ".jpg";
    QString trainFilePath = trainFolderPath + "/" + trainFileName;

    appendToCSV(type, x, y, srcPath);
    trainImage.save(trainFilePath, "JPG", 100);
}

void FocusPointTrainer::appendToCSV(QString &type, float x, float y, QString &srcPath)
{
    QString cvsFilePath = "/Users/roryhill/Documents/Documents - Quark/FocusPointTrainer/focuspoint.csv";

    QFile file(cvsFilePath);
    bool fileExists = file.exists();

    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);

        // Write header if file is newly created
        if (!fileExists) {
            out << "type,x,y,srcPath\n";
        }

        // Write a new line
        out
            << "\"" << type << "\","
            << x << ","
            << y << ","
            << "\"" << srcPath << "\"\n";

        file.close();
    } else {
        qWarning() << "Failed to open CSV file for writing:" << cvsFilePath;
    }
}
