#include "renamefile.h"
#include "Utilities/fileops.h"
#include "ui_renamefiledlg.h"
#include "Main/global.h"
#include "Utilities/tokenfilename.h"
#include "Utilities/htmlwindow.h"
#include <cerrno>
#include <cstring>
#ifdef Q_OS_MAC
#include <sys/stat.h>
#endif

bool RenameFileDlg::isFileLocked(const QString &path)
{
/*
    Returns true if the file has the macOS user-immutable (uchg) or
    system-immutable (schg) flag set — i.e. it is "Locked" in Finder's
    Get Info panel. rename() and unlink() both fail with EPERM on such
    files. On non-mac platforms this is a no-op.
*/
#ifdef Q_OS_MAC
    struct stat st;
    if (::stat(path.toUtf8().constData(), &st) != 0) return false;
    return (st.st_flags & (UF_IMMUTABLE | SF_IMMUTABLE)) != 0;
#else
    Q_UNUSED(path);
    return false;
#endif
}

QStringList RenameFileDlg::lockedFilesInSelection(const QString &folderPath,
                                                  const QStringList &selection)
{
/*
    Returns display names of any file in folderPath that (a) shares a base name
    with any file in selection and (b) is locked in Finder. Sidecars count —
    rename() would fail on them mid-operation just as it would on the image.
*/
    QSet<QString> bases;
    for (const QString &p : selection) bases.insert(QFileInfo(p).baseName());

    QStringList locked;
    const QFileInfoList inf = QDir(folderPath).entryInfoList(QDir::Files);
    for (const QFileInfo &fi : inf) {
        if (!bases.contains(fi.baseName())) continue;
        if (isFileLocked(fi.filePath())) locked << fi.fileName();
    }
    return locked;
}

// Format a list of file names for display, capping long lists with a summary
// line so the dialog doesn't become unreadably tall.
static QString formatNameList(const QStringList &names, int max = 20)
{
    if (names.size() <= max) return names.join("<br>");
    QStringList head = names.mid(0, max);
    return head.join("<br>") + "<br>+ " +
           QString::number(names.size() - max) + " more";
}

QString RenameFileDlg::lockedFilesMsg(const QStringList &names)
{
    return
        "The following file(s) are <b>locked in Finder</b> and cannot be renamed:"
        "<br><br><b>" + formatNameList(names) + "</b><br><br>"
        "To unlock, select the file in Finder, press <b>Cmd+I</b>, "
        "and uncheck <b>Locked</b>. Then try again.";
}

RenameFileDlg::RenameFileDlg(QWidget *parent,
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
                             selection(selection),
                             filenameTemplatesMap(filenameTemplates)
{
    ui->setupUi(this);

    /* The selection may span folders (Library / Catalog mode, recursive folders).
       Every name-conflict, sidecar and sequence scan is per folder, so rename()
       runs once per folder; folderPath starts at the first, which is all the
       single-file manual rename needs. */
    for (const QString &path : std::as_const(selection)) {
        const QString folder = QFileInfo(path).path();
        if (!folders.contains(folder)) folders << folder;
    }
    if (!folders.isEmpty()) folderPath = folders.first();

    QString n = QString::number(selection.count());
    QString title;
    if (n == "1") title = "Rename " + n + " image";
    else title = "Rename " + n + " images";
    setWindowTitle(title);

    ui->progressMsg->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->progressBar->setTextVisible(false);
    if (G::useProcessEvents) qApp->processEvents();


    // Simple rename is only meaningful for a single-file selection. For a
    // multi-selection, hide both checkboxes and the simple body — only the
    // template path applies.
    // The dialog has no top-level layout, so sizeHint() underestimates the
    // geometry-rect height (Qt then opens shorter than the .ui declares).
    // Pin the height explicitly in both branches.
    const int fullHeight = 483;
    const int shift = 168;  // templateGroupBox y=178 → 10 when simple is hidden
    if (selection.count() == 1) {
        QFileInfo info(selection.at(0));
        ui->manualRenameEdit->setText(info.baseName());

        /* "Only rename the selected file" is off by default: the whole base-name group
           (x.tif, x.jpg, x.arw, x.xmp) renames together, as it always has. In a
           combined raw+JPG view the pair is ONE thumbnail, so renaming one half would
           leave the hidden other half behind under the old name -- disable, and say
           why. Explicitly disabled, it stays off when the group box is re-enabled. */
        ui->onlySelectedChk->setChecked(false);
        const int row = dm->fPathRow.value(selection.at(0), -1);
        const bool isCombinedPair = G::combineRawJpg && row >= 0
            && dm->index(row, G::PathColumn).data(G::DupOtherIdxRole).isValid();
        if (isCombinedPair) {
            ui->onlySelectedChk->setEnabled(false);
            ui->onlySelectedChk->setText("Only rename the selected file: off while "
                                         "Raw+JPG are combined");
            ui->onlySelectedChk->setToolTip(
                "This image is a combined raw+JPG pair, shown as one thumbnail.\n"
                "Renaming one half would leave the hidden other half under the old "
                "name.\nTurn off Combine Raw+Jpg to rename one file alone.");
        }
        // Default: template active, simple grayed out.
        ui->simpleRenameChk->setChecked(false);
        ui->templateRenameChk->setChecked(true);
        ui->manualRenameGroupBox->setEnabled(false);
        ui->templateGroupBox->setEnabled(true);
        ui->filenameTemplatesBtn->setEnabled(true);

        // Mutual exclusion: clicking either checkbox forces it on, the other
        // off, and toggles the enabled state of both bodies.
        connect(ui->simpleRenameChk, &QCheckBox::clicked, this, [this]() {
            ui->simpleRenameChk->setChecked(true);
            ui->templateRenameChk->setChecked(false);
            ui->manualRenameGroupBox->setEnabled(true);
            ui->templateGroupBox->setEnabled(false);
            ui->filenameTemplatesBtn->setEnabled(false);
            ui->manualRenameEdit->setFocus();
        });
        connect(ui->templateRenameChk, &QCheckBox::clicked, this, [this]() {
            ui->simpleRenameChk->setChecked(false);
            ui->templateRenameChk->setChecked(true);
            ui->manualRenameGroupBox->setEnabled(false);
            ui->templateGroupBox->setEnabled(true);
            ui->filenameTemplatesBtn->setEnabled(true);
        });

        setFixedHeight(fullHeight);
    } else {
        ui->simpleRenameChk->setVisible(false);
        ui->manualRenameGroupBox->setVisible(false);
        ui->templateRenameChk->setVisible(false);

        auto moveUp = [shift](QWidget *w) {
            w->move(w->x(), w->y() - shift);
        };
        moveUp(ui->templateGroupBox);
        moveUp(ui->progressMsg);
        moveUp(ui->progressBar);
        moveUp(ui->layoutWidget);

        setFixedHeight(fullHeight - shift);
    }

    ui->helpBtn->setStyleSheet("background-color: " + G::helpColor.name() + ";");

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

/*
    BASE NAME = up to the FIRST dot (QFileInfo::baseName), so every rebuilt name keeps
    completeSuffix -- everything after it. With suffix() a full-name sidecar
    DSC_1.JPG.xmp came out as <new>.xmp: the raw's sidecar name in a raw+JPEG pair.
*/
void RenameFileDlg::renameFileBase(QString oldBase, QString newBase)
{
    QFileInfoList inf = QDir(folderPath).entryInfoList(QDir::Files);
    for (int i = 0; i < inf.size(); i++) {
        QString existBase = inf.at(i).baseName();
        if (existBase == oldBase) {
            QString oldPath = inf.at(i).filePath();
            QString newPath = inf.at(i).dir().path() + "/" + newBase + "." + inf.at(i).completeSuffix();
            QFile(oldPath).rename(newPath);
            FileOps::onMoved(oldPath, newPath);
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
                uniqueBase = existBase + "-" + QString::number(k++);
            } while (baseNames.contains(uniqueBase));

            if (isDebug)
                qDebug() << "\nmakeExistingBaseUnique CONFLICT FOUND"
                         << "existBase =" << existBase
                         << "newBase =" << newBase
                         << "uniqueBase =" << uniqueBase
                            ;
            // update datamodel, imageCache
            QString oldPath = inf.at(i).filePath();
            QString newName = uniqueBase + "." + inf.at(i).completeSuffix();
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
    if (pathIdx.isValid()) dm->setData(pathIdx, newPath, G::KeyRole);
    if (nameIdx.isValid()) dm->setData(nameIdx, newName);

    // update imageCache
    imageCache->rename(oldPath, newPath);
    if (isDebug) qDebug() << "In ImageCache renamed oldPath =" << oldPath
                 << "to newPath =" << newPath;

    /*  The image's VERSIONS are keyed by its path (Utilities/versionkey.h). Their records
        rode along in the renamed sidecar and their loupe previews in FileOps::onMoved;
        the rows and the ImageCache entries are re-keyed here. */
    for (const auto &kv : dm->rekeyVersions(oldPath, newPath))
        imageCache->rename(kv.first, kv.second);
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
            QString ext = QFileInfo(oldPath).completeSuffix();
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
    Selection (grouped by folder)   selection
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
    seqNum  = ui->spinBoxStartNumber->value();

    /* Group the selection by folder up front, before anything is renamed: a renamed
       row can re-sort in the proxy, so nothing below reads paths back from the model.
       The sequence number runs on across folders, so with an {XX..} token every
       renamed image gets a distinct number however many folders it came from. */
    QMap<QString, QStringList> pathsInFolder;
    for (const QString &path : std::as_const(selection))
        pathsInFolder[QFileInfo(path).path()] << path;

    ui->progressMsg->setVisible(true);
    ui->progressBar->setVisible(true);
    QStringList sidecars;
    int progress = 0;
    for (const QString &folder : std::as_const(folders)) {
        folderPath = folder;
        renameInFolder(pathsInFolder.value(folder), sidecars, progress);
    }

    // update current image
    dm->currentKey = dm->currentSfIdx.data(G::KeyRole).toString();

    if (isDebug) {
        qDebug() << "Renaming completed";
    }

    if (!sidecars.isEmpty()) {
        ui->progressMsg->setVisible(false);
        ui->progressBar->setVisible(false);
        QMessageBox::information(this, "Sidecar files renamed",
            "In addition to the selected file(s), the following file(s) sharing "
            "the same base name were also renamed:<br><br><b>" +
            formatNameList(sidecars) + "</b>");
    }
}

void RenameFileDlg::renameInFolder(const QStringList &paths, QStringList &sidecars,
                                   int &progress)
{
/*
    rename() for the selected images in ONE folder (folderPath). Appends the names of
    the non-selected files sharing a base name (sidecars, raw+JPEG partners) to
    sidecars, and continues seqNum from where the previous folder left it.
*/
    QString tokenString = filenameTemplatesMap[ui->filenameTemplatesCB->currentText()];
    int pathCol = 0;    // filesToRename.at(i).at(pathCol)
    int baseCol = 1;    // filesToRename.at(i).at(baseCol)
    int doneCol = 2;    // filesToRename.at(i).at(doneCol)

    baseNames.clear();
    baseNamesUsed.clear();

    // populate base names in folder
    QFileInfoList inf = QDir(folderPath).entryInfoList(QDir::Files);

    QString folderTxt;
    if (folders.size() > 1)
        folderTxt = " (folder " + QString::number(folders.indexOf(folderPath) + 1)
                    + " of " + QString::number(folders.size()) + ")";
    progress = 0;
    ui->progressBar->setMaximum(inf.size());
    ui->progressMsg->setText("Step 1 of 3: Preparing" + folderTxt + "...");
    for (int i = 0; i < inf.size(); i++) {
        const QString base = inf.at(i).baseName();
        if (!baseNames.contains(base)) baseNames.append(base);
        ui->progressBar->setValue(++progress);
        if (G::useProcessEvents) qApp->processEvents();
    }


    // Build list of all files to rename
    progress = 0;
    ui->progressBar->setMaximum(paths.size());
    ui->progressMsg->setText("Step 2 of 3: Checking for name conflicts" + folderTxt + "...");
    if (isDebug)  qDebug() << "FILE LIST TO RENAME:";
    filesToRename.clear();
    QSet<QString> selectionPaths;
    for (const QString &path : paths) {
        selectionPaths.insert(path);
        appendAllSharingBaseName(path);
        ui->progressBar->setValue(++progress);
        if (G::useProcessEvents) qApp->processEvents();
    }

    // Sidecars = files in filesToRename that weren't part of the user selection.
    // Capture their original names now, before the rename loop mutates paths.
    for (int i = 0; i < filesToRename.size(); i++) {
        const QString &p = filesToRename.at(i).at(0);
        if (!selectionPaths.contains(p)) sidecars << QFileInfo(p).fileName();
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
        QString newPath = info.dir().path() + "/d78sn34_" + QString::number(i) + "." + info.completeSuffix();

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
        FileOps::onMoved(oldPath, uniquePath);

        // update datamodel
        QModelIndex idx = dm->proxyIndexFromKey(oldPath);
        // might be a sidecar file not in datamodel
        if (idx.isValid()) {
            int row = idx.row();
            QModelIndex pathIdx = dm->sf->index(row, G::PathColumn);
            QModelIndex nameIdx = dm->sf->index(row, G::NameColumn);
            QString newName = Utilities::getFileName(uniquePath);
            dm->sf->setData(pathIdx, uniquePath, G::KeyRole);
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
    QString txt = "Step 3 of 3: Renaming " + QString::number(filesToRename.size())
                  + " files" + folderTxt + "...";
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

        QString newName = newBase + "." + info.completeSuffix();
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
        /* Keep the cached develop preview with the file. The sidecar (and with it the
           thumbnail preview) is carried by the basename scan above. */
        FileOps::onMoved(oldPath, newPath);
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
        FileOps::onMoved(oldPath, newPath);
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
        if (G::useProcessEvents) qApp->processEvents();
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

    /* One sequence runs across every folder in the selection, so start it past the
       highest existing number in ANY of them. */
    QDir dir(folderPath);
    if (dir.exists()) {
        int sequenceNum = 0;
        for (const QString &folder : std::as_const(folders))
            sequenceNum = qMax(sequenceNum, getSequenceStart(folder));
        if (ui->spinBoxStartNumber->value() < sequenceNum + 1)
            ui->spinBoxStartNumber->setValue(sequenceNum + 1);
        const QString where = folders.size() > 1
            ? QString::number(folders.size()) + " folders exist" : QString("Folder exists");
        if (sequenceNum > 0)
            ui->existingSequenceLabel->setText(where + " and last image sequence found = "
                                               + QString::number(sequenceNum));
        else
            ui->existingSequenceLabel->setText(where + " but no sequenced images found");
    }
    else {
        ui->spinBoxStartNumber->setValue(1);
        ui->existingSequenceLabel->setText("");
    }
    seqNum = ui->spinBoxStartNumber->value();
}

void RenameFileDlg::updateExample()
{
    if (selection.isEmpty()) {
        ui->exampleLbl->setText(QString());
        return;
    }
    QFileInfo info(selection.at(0));
    QString tokenString = filenameTemplatesMap[ui->filenameTemplatesCB->currentText()];
    ui->exampleLbl->setText(parseTokenString(info, tokenString));
}

bool RenameFileDlg::isTemplateNoOp()
{
/*
    Returns true if applying the currently selected template to every file in
    the selection would produce a base name identical to the file's existing
    base name — i.e. clicking OK would rename nothing.
*/
    QString tokenString = filenameTemplatesMap.value(ui->filenameTemplatesCB->currentText());
    if (tokenString.isEmpty()) return false;
    if (selection.isEmpty()) return false;
    seqNum = ui->spinBoxStartNumber->value();
    for (const QString &path : std::as_const(selection)) {
        QFileInfo info(path);
        if (parseTokenString(info, tokenString) != info.baseName()) return false;
    }
    return true;
}

QRegularExpression RenameFileDlg::templateAsRegex(const QString &tokenString)
{
/*
    Convert a template like "image{XXXX}" or "{YYYY}-{MM}-{DD}_{XXXX}" into an
    anchored regex (^image\d{4}$, etc.) used to test whether a file's existing
    base name already follows the template's structure. Free-form tokens
    (TITLE, ORIGINAL FILENAME, etc.) collapse to ".+?" since their content is
    arbitrary; date/sequence/numeric tokens collapse to character-class
    patterns that match exactly what parseTokenString would emit.
*/
    static const QMap<QString, QString> tokenPattern = {
        {"ORIGINAL FILENAME", ".+?"},
        {"YYYY", "\\d{4}"},
        {"YY", "\\d{2}"},
        {"MONTH", "(?:JANUARY|FEBRUARY|MARCH|APRIL|MAY|JUNE|JULY|AUGUST|SEPTEMBER|OCTOBER|NOVEMBER|DECEMBER)"},
        {"Month", "(?:January|February|March|April|May|June|July|August|September|October|November|December)"},
        {"MON", "(?:JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC)"},
        {"Mon", "(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)"},
        {"MM", "\\d{2}"},
        {"DAY", "(?:MONDAY|TUESDAY|WEDNESDAY|THURSDAY|FRIDAY|SATURDAY|SUNDAY)"},
        {"Day", "(?:Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday)"},
        {"DDD", "(?:MON|TUE|WED|THU|FRI|SAT|SUN)"},
        {"Ddd", "(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun)"},
        {"DD", "\\d{2}"},
        {"HOUR", "\\d{2}"},
        {"MINUTE", "\\d{2}"},
        {"SECOND", "\\d{2}"},
        {"MILLISECOND", "\\d{3}"},
        {"TITLE", ".+?"},
        {"CREATOR", ".+?"},
        {"COPYRIGHT", ".+?"},
        {"MAKE", ".+?"},
        {"MODEL", ".+?"},
        {"DIMENSIONS", "\\d+x\\d+"},
        {"SHUTTER SPEED", ".+?"},
        {"APERTURE", "f/[\\d.]+"},
        {"ISO", "\\d+"},
        {"FOCAL LENGTH", "\\d+\\s*mm"},
        {"XX", "\\d{2}"},
        {"XXX", "\\d{3}"},
        {"XXXX", "\\d{4}"},
        {"XXXXX", "\\d{5}"},
        {"XXXXXX", "\\d{6}"},
        {"XXXXXXX", "\\d{7}"},
    };

    QString pattern;
    int i = 0;
    while (i < tokenString.length()) {
        if (tokenString.at(i) == '{') {
            int end = tokenString.indexOf('}', i + 1);
            if (end > i) {
                QString token = tokenString.mid(i + 1, end - i - 1);
                if (tokenPattern.contains(token)) {
                    pattern += tokenPattern.value(token);
                    i = end + 1;
                    continue;
                }
            }
        }
        pattern += QRegularExpression::escape(QString(tokenString.at(i)));
        i++;
    }
    return QRegularExpression("^" + pattern + "$");
}

bool RenameFileDlg::allFilesAlreadyConform()
{
/*
    Returns true if every selected file's base name already matches the
    selected template's structure. The motivating case: folder contains
    image1.jpg, image2.jpg, image3.jpg and the user picks a template like
    "image{XX}". Renaming would only shift sequence numbers, not impose a new
    convention — likely not what the user intended.
*/
    if (selection.isEmpty()) return false;
    QString tokenString = filenameTemplatesMap.value(ui->filenameTemplatesCB->currentText());
    if (tokenString.isEmpty()) return false;
    QRegularExpression rx = templateAsRegex(tokenString);
    if (!rx.isValid()) return false;
    for (const QString &path : std::as_const(selection)) {
        if (!rx.match(QFileInfo(path).baseName()).hasMatch()) return false;
    }
    return true;
}

bool RenameFileDlg::renameSingleManual(const QString &newBase)
{
/*
    Manual rename path for a single-file selection. Renames the selected file and
    any sidecars sharing its basename to newBase, preserving each extension.
    Rejects the operation if newBase collides with an existing file that is NOT
    part of the oldBase group (which is itself being renamed).
*/
    if (selection.isEmpty()) return false;

    QFileInfo selInfo(selection.at(0));
    QString oldBase = selInfo.baseName();
    if (newBase == oldBase) return true;     // nothing to do

    QFileInfoList inf = QDir(folderPath).entryInfoList(QDir::Files);

    // Conflict check: any existing file with baseName == newBase is a duplicate
    // unless its baseName also equals oldBase (impossible) — i.e. any hit is a
    // real conflict.
    for (const QFileInfo &fi : inf) {
        if (fi.baseName() == newBase) {
            QString msg = "A file named <b>" + fi.fileName() +
                          "</b> already exists.<br><br>"
                          "Choose a different name.";
            QMessageBox::warning(this, "Name conflict", msg);
            return false;
        }
    }

    // Rename the selected file plus every sidecar sharing oldBase, on disk and
    // in the datamodel.
    const QString selectedFileName = selInfo.fileName();
    QStringList sidecars;
    for (const QFileInfo &fi : inf) {
        if (fi.baseName() != oldBase) continue;
        QString oldPath = fi.filePath();
        QString newName = newBase + "." + fi.completeSuffix();
        QString newPath = fi.dir().path() + "/" + newName;
        errno = 0;
        if (std::rename(oldPath.toUtf8().constData(), newPath.toUtf8().constData()) != 0) {
            QMessageBox::warning(this, "Rename failed",
                "Could not rename <b>" + fi.fileName() + "</b>:<br><br>" +
                QString::fromLocal8Bit(std::strerror(errno)));
            return false;
        }
        FileOps::onMoved(oldPath, newPath);
        if (dm->fPathRow.contains(oldPath)) {
            renameDatamodel(oldPath, newPath, newName);
        }
        if (fi.fileName() != selectedFileName) sidecars << fi.fileName();
    }

    dm->currentKey = dm->currentSfIdx.data(G::KeyRole).toString();

    if (!sidecars.isEmpty()) {
        QMessageBox::information(this, "Sidecar files renamed",
            "In addition to the selected file, the following file(s) sharing "
            "the same base name were also renamed:<br><br><b>" +
            formatNameList(sidecars) + "</b>");
    }

    return true;
}

bool RenameFileDlg::renameOnlySelected(const QString &newBase)
{
/*
    Simple rename with "Only rename the selected file": the image and the sidecars it
    OWNS, not every file sharing its base name. Renaming x.tif leaves x.jpg, x.arw and
    the raw's x.xmp where they are.

    Ownership is FileOps::companions -- the one rule move, copy, ingest and trash
    already use (see "Sidecar Naming" in notes/Documentation.txt): raw and HEIC own
    x.xmp; JPEG, TIFF, PNG and DNG own x.tif.xmp, and x.xmp only when no other image
    shares the base name. Each companion's new name comes from FileOps::companionDest,
    so x.tif.xmp becomes y.tif.xmp and a raw's x.xmp becomes y.xmp.

    Not FileOps::moveFile: it deletes an existing destination first, and in a
    case-only rename (x.tif -> X.tif) on a case-insensitive volume the destination
    IS the source.
*/
    if (selection.isEmpty()) return false;
    const QString src = selection.at(0);
    const QFileInfo si(src);
    if (newBase == si.baseName()) return true;     // nothing to do
    const QString dst = si.dir().absoluteFilePath(newBase + "." + si.completeSuffix());

    /* A tif/jpg still reading an old base-name x.xmp takes it to its full-name x.tif.xmp
       first -- moved when the image is alone, copied when another image may read it.
       That is what its next edit would do anyway. Without it the rename would carry
       x.xmp to y.xmp, where a y.arw would claim it. */
    FileOps::prepareSidecarForWrite(src);
    const QStringList comps = FileOps::companions(src);

    // refuse if any destination exists (a case-only change is not a clash)
    auto clash = [](const QString &from, const QString &to) {
        return QFileInfo::exists(to) && from.compare(to, Qt::CaseInsensitive) != 0;
    };
    QStringList clashes;
    if (clash(src, dst)) clashes << QFileInfo(dst).fileName();
    for (const QString &c : comps) {
        const QString d = FileOps::companionDest(c, src, dst);
        if (clash(c, d)) clashes << QFileInfo(d).fileName();
    }
    if (!clashes.isEmpty()) {
        QMessageBox::warning(this, "Name conflict",
            "These files already exist:<br><br><b>" + formatNameList(clashes) +
            "</b><br><br>Choose a different name.");
        return false;
    }

    errno = 0;
    if (std::rename(src.toUtf8().constData(), dst.toUtf8().constData()) != 0) {
        QMessageBox::warning(this, "Rename failed",
            "Could not rename <b>" + si.fileName() + "</b>:<br><br>" +
            QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    QStringList failed;
    for (const QString &c : comps) {
        const QString d = FileOps::companionDest(c, src, dst);
        if (std::rename(c.toUtf8().constData(), d.toUtf8().constData()) != 0)
            failed << QFileInfo(c).fileName();
    }
    FileOps::onMoved(src, dst);
    if (dm->fPathRow.contains(src)) renameDatamodel(src, dst, QFileInfo(dst).fileName());
    dm->currentKey = dm->currentSfIdx.data(G::KeyRole).toString();

    if (!failed.isEmpty()) {
        QMessageBox::warning(this, "Sidecar not renamed",
            "The image was renamed, but these sidecar file(s) could not be:<br><br><b>" +
            formatNameList(failed) + "</b><br><br>They still have the old name, so the "
            "renamed image will not see the settings they hold.");
    }
    return true;
}

void RenameFileDlg::on_helpBtn_clicked()
{
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Renaming Images",
                   ":/Docs/renamehelp.html",
                   QSize(800, 700), r, this);
}

void RenameFileDlg::on_okBtn_clicked()
{
    // Manual rename is only chosen when a single file is selected AND the
    // "Enter a new file name" checkbox is ticked. Otherwise use the template.
    if (selection.count() == 1 && ui->simpleRenameChk->isChecked()) {
        QString typed = ui->manualRenameEdit->text().trimmed();
        if (typed.isEmpty()) {
            QMessageBox::warning(this, "Empty name",
                                 "Please enter a new file name.");
            return;  // stay open so the user can fix it
        }
        QFileInfo info(selection.at(0));
        if (typed == info.baseName()) {
            accept();  // no-op: user didn't change the name
            return;
        }
        const bool ok = ui->onlySelectedChk->isChecked() ? renameOnlySelected(typed)
                                                         : renameSingleManual(typed);
        if (!ok) return;  // conflict, stay open
        accept();
        return;
    }
    if (isTemplateNoOp()) {
        G::popup->showPopup("Selected template will not change the file name(s).", 2000);
        return;  // stay open so the user can pick a different template
    }
    if (allFilesAlreadyConform()) {
        QString msg = QString(
            "All %1 selected file(s) already follow the selected template's pattern.<br><br>"
            "Renaming will assign new sequence numbers, which may not be what you want.<br><br>"
            "Continue?").arg(selection.count());
        auto choice = QMessageBox::question(this, "Files already match template", msg,
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::No);
        if (choice != QMessageBox::Yes) return;  // stay open
    }
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
    The token editor lists tokens in this order. Both the order and the token set come
    from the shared table (Utilities/tokenfilename.h), so Ingest, Rename and Export offer
    exactly the same tokens.
*/
    if (G::isLogger) G::log("RenameFileDlg::initTokenList");
    tokens = TokenFileName::tokens();
}

void RenameFileDlg::initExampleMap()
{
/*
    Example values shown beside each token in the editor -- from the shared table, so an
    example cannot drift from what parse() actually produces.
*/
    if (G::isLogger) G::log("RenameFileDlg::initExampleMap");
    exampleMap = TokenFileName::exampleMap();
}

QString RenameFileDlg::parseTokenString(QFileInfo info, QString tokenString)
{
/*
    Delegates to the shared token parser (Utilities/tokenfilename.h). The token language
    is common to Ingest, Rename and Export -- this used to be a private copy in each, so
    a token added in one place did not appear in the others.
*/
    if (G::isLogger) G::log("RenameFileDlg::parseTokenString");
    const QString fPath = info.absoluteFilePath();
    if (fPath == "") return "";
    const ImageMetadata m = dm->imMetadata(fPath);
    return TokenFileName::parse(m, info, tokenString, seqNum);
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
        QString path = dm->index(i, G::PathColumn).data(G::KeyRole).toString();
        QString name = dm->index(i, G::NameColumn).data().toString();
        qDebug() << i << "\tPath =" << path << "Name =" << name;
    }
}


