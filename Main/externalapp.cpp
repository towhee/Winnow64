#include "Main/mainwindow.h"

void MW::externalAppManager()
{
/*
   This function opens a dialog that allows the user to add and delete external
   executables that can be passed image files. externalApps is a QList that holds string
   pairs: the program name and the path to the external program executable. This list is
   passed as a reference to the appdlg, which modifies it and then after the dialog is
   closed the appActions are rebuilt.
*/
    if (G::isLogger) G::log("MW::externalAppManager");
    Appdlg *appdlg = new Appdlg(externalApps, this);

    if(appdlg->exec()) {
        /*
        Menus cannot be added/deleted at runtime in MacOS so 10 menu items are created
        in the MW constructor and then edited here based on changes made in appDlg.
        */
        settings->beginGroup("ExternalApps");
        settings->remove("");
        for (int i = 0; i < 10; ++i) {
            if (i < externalApps.length()) {
                QString shortcut = "Alt+" + xAppShortcut[i];
                appActions.at(i)->setShortcut(QKeySequence(shortcut));
                appActions.at(i)->setText(externalApps.at(i).name);
                appActions.at(i)->setVisible(true);
                qDebug() << "MW::externalAppManager" << i
                         << externalApps.at(i).name
                         << externalApps.at(i).path;
                // save to settings
                QString sortPrefix = xAppShortcut[i];
                if (sortPrefix == "0") sortPrefix = "X";
                settings->setValue(sortPrefix + externalApps.at(i).name, externalApps.at(i).path);
            }
            else {
                appActions.at(i)->setVisible(false);
                appActions.at(i)->setText("");
            }
        }
        settings->endGroup();
        
        settings->beginGroup("ExternalAppArgs");
        settings->remove("");
        for (int i = 0; i < 10; ++i) {
            if (i < externalApps.length()) {
                // save to settings
                QString sortPrefix = xAppShortcut[i];
                if (sortPrefix == "0") sortPrefix = "X";
//                qDebug() << "MW::externalAppManager" << i
//                         << externalApps.at(i).name
//                         << externalApps.at(i).args;
                settings->setValue(sortPrefix + externalApps.at(i).name, externalApps.at(i).args);
            }
        }
        settings->endGroup();
    }
}

void MW::cleanupSender()
{
/*
   When the process responsible for running the external program is finished a signal is
   received here to delete the process.
*/
    if (G::isLogger) G::log("MW::cleanupSender");
    delete QObject::sender();
}

void MW::externalAppError(QProcess::ProcessError /*err*/)
{
    if (G::isLogger) G::log("MW::externalAppError");
    QMessageBox msgBox;
    msgBox.critical(this, tr("Error"), tr("Failed to start external application."));
}

void MW::runExternalApp()
{
    if (G::isLogger) G::log("MW::runExternalApp");

    QString appPath = "";
    QString appName = (static_cast<QAction*>(sender()))->text();
    qDebug() << "MW::runExternalApp" << appName;

    // append any app command arguments (before add file paths)
    QStringList arguments;
    QStringList files;
    for (int i = 0; i < externalApps.length(); ++i) {
        if (externalApps.at(i).name == appName) {
            appPath = externalApps.at(i).path;
            if (externalApps.at(i).args.length() > 0)
                arguments << externalApps.at(i).args.split(" ");
            break;
        }
    }
    if (appPath == "") return;      // add err handling
    QFileInfo appInfo;              // qt6.2
    appInfo.setFile(appPath);       // qt6.2
    QString appExecutable = appInfo.fileName();

    // get list of selected or picked image files to send to external app
//    if (!dm->getSelection(arguments)) return;
    dm->getSelection(files);
    int nFiles = files.size();

    if (nFiles < 1) {
        G::popUp->showPopup("No images have been selected", 2000);
        return;
    }

    if (nFiles > 5) {
        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setText(QString::number(nFiles) + " files will be processed.");
        msgBox.setInformativeText("Do you want continue?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setIcon(QMessageBox::Warning);
        QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                    "QPushButton:default {background-color: rgb(68,95,118);}";
        msgBox.setStyleSheet(s);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        int ret = msgBox.exec();
        resetFocus();
        if (ret == QMessageBox::Cancel) return;
    }

    arguments << files;

    QString folderPath;
    QFileInfo fInfo;                        // qt6.2
    fInfo.setFile(arguments.at(0));         // qt6.2
    folderPath = fInfo.dir().absolutePath() + "/";

    #ifdef Q_OS_WIN
    arguments.replaceInStrings("/", "\\");
    #endif

    QProcess *process = new QProcess();
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(cleanupSender()));
    connect(process, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(externalAppError(QProcess::ProcessError)));

    // Photoshop exception on macOS
    #ifdef Q_OS_MAC
    if (appName.contains("Photoshop")) {
        // based on this working in terminal
        // open "/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg" -a "Adobe Photoshop CS6"

        // Build the file path argument string
        QString fileArgs;
        for (const QString& filePath : arguments) {
            fileArgs += QString("\"%1\" ").arg(filePath);
        }

        // Construct the full command as a single string
        QString command = QString("open %1 -a \"%2\"").arg(fileArgs.trimmed(), appName);

        // Run the command using the shell
        process->start("bash", QStringList() << "-c" << command);

        return;
    }
    #endif

    /*
    qDebug() << "MW::runExternalApp"
             << "\nfolderPath =" << folderPath
             << "\nappPath =" << appPath
             << "\narguments =" << arguments
                ;  //*/

    process->setArguments(arguments);
    process->setProgram(appPath);
    process->setWorkingDirectory(folderPath);
    process->start();
}
