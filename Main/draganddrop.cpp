#include "Main/mainwindow.h"

// DRAG AND DROP

void MW::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log("MW::dragEnterEvent");
    event->acceptProposedAction();
}

void MW::dropEvent(QDropEvent *event)
{
    if (G::isLogger) G::log("MW::dropEvent");
    qDebug() << "MW::dropEvent";
    if (event->mimeData()->hasUrls()) {
        QString fPath = event->mimeData()->urls().at(0).toLocalFile();
        // prevent drop onto folder already active in Winnow
//        if (QFileInfo(fPath).dir() == G::currRootFolder) {
//            return;
//        }
        handleDrop(fPath);
    }
}

void MW::handleDrop(QString fPath)
{
    if (G::isLogger) G::log("MW::handleDrop");
    QFileInfo info(fPath);
    QDir incoming = info.dir();
    if (incoming == currRootDir) {
        QString fileType = info.suffix().toLower();
        if (metadata->supportedFormats.contains(fileType)) {
            dm->select(dragDropFilePath);
        }
    }
    else {
        qDebug() << "MW::handleDrop" << fPath;
        folderAndFileSelectionChange(fPath, "handleDropOnCentralView");
    }

}

void MW::dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath)
{
/*

*/
    if (G::isLogger) G::log("MW::dropOp");
    qDebug() << "MW::dropOp";
    QApplication::restoreOverrideCursor();
    copyOp = (keyMods == Qt::ControlModifier);
    QMessageBox msgBox;
    QString destDir;

    if (QObject::sender() == fsTree) {
        destDir = getSelectedPath();
    } else if (QObject::sender() == bookmarks) {
        if (bookmarks->currentItem()) {
            destDir = bookmarks->currentItem()->toolTip(0);
        } else {
            addBookmark(cpMvDirPath);
            return;
        }
    } else {
        // Unknown sender
        return;
    }

    if (!isValidPath(destDir)) {
        msgBox.critical(this, tr("Error"), tr("Can not move or copy images to this folder."));
        selectCurrentViewDir();
        return;
    }

    if (destDir == G::currRootFolder) {
        msgBox.critical(this, tr("Error"), tr("Destination folder is same as source."));
        return;
    }

    if (dirOp) {
        QString dirOnly =
            cpMvDirPath.right(cpMvDirPath.size() - cpMvDirPath.lastIndexOf(QDir::separator()) - 1);

        QString question = tr("Move \"%1\" to \"%2\"?").arg(dirOnly).arg(destDir);
        int ret = QMessageBox::question(this, tr("Move folder"), question,
                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if (ret == QMessageBox::Yes) {
            QFile dir(cpMvDirPath);
            bool ok = dir.rename(destDir + QDir::separator() + dirOnly);
            if (!ok) {
                QMessageBox msgBox;
                msgBox.critical(this, tr("Error"), tr("Failed to move folder."));
            }
            setStatus(tr("Folder moved"));
        }
    }
//    else {
//        CpMvDialog *cpMvdialog = new CpMvDialog(this);
//        GData::copyCutIdxList = thumbView->selectionModel()->selectedIndexes();
//        cpMvdialog->exec(thumbView, destDir, false);

//        if (!GData::copyOp) {
//            int row = cpMvdialog->latestRow;
//            if (thumbView->thumbViewModel->rowCount()) {
//                if (row >= thumbView->thumbViewModel->rowCount()) {
//                    row = thumbView->thumbViewModel->rowCount() -1 ;
//                }

//                thumbView->setCurrentRow(row);
//                thumbView->selectThumbByRow(row);
//            }
//        }

//        QString state = QString((GData::copyOp? tr("Copied") : tr("Moved")) + " " +
//                            tr("%n image(s)", "", cpMvdialog->nfiles));
//        setStatus(state);
//        delete(cpMvdialog);
//    }

//    thumbView->loadVisibleThumbs();
}
