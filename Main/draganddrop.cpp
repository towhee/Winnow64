#include "Main/mainwindow.h"

// DRAG AND DROP ONTO MAIN WINDOW

void MW::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log("MW::dragEnterEvent");
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MW::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (G::isLogger) G::log("MW::dragEnterEvent");
    dragLabel->hide();
}

void MW::dragMoveEvent(QDragMoveEvent *event)
{
    if (G::isLogger) G::log("MW::dragMoveEvent");

    // Cannot show in source folder as this is already the source
    QObject *source = event->source();
    if (source) {
        if (event->source()->objectName() == "Thumbnails") return;
        if (event->source()->objectName() == "Grid") return;
    }

    // show the dragLabel to show images from the source folder
    if (event->mimeData()->hasUrls()) {
        QString text = "Show image in source folder ";
        dragLabel->setText(text);
        dragLabel->adjustSize();
        // Get global cursor position and move label near it
        QPoint globalPos = QCursor::pos();
        int xOffset = -dragLabel->width()/2;
        dragLabel->move(globalPos + QPoint(xOffset, 10)); // Offset slightly from cursor
        dragLabel->show();

        event->accept(); // Accept the event
    }
    else {
        event->ignore();
    }
    // event->acceptProposedAction();
}

void MW::dropEvent(QDropEvent *event)
{
    if (G::isLogger) G::log("MW::dropEvent");
    qDebug() << "MW::dropEvent";

    // Ignore if source is Winnow.  Probably result of an aborted drag.
    if (event->source() == thumbView) {
        thumbView->ignoreDrop = true;
        return;
    }
    if (event->source() == gridView) {
        gridView->ignoreDrop = true;
        return;
    }

    if (event->mimeData()->hasUrls()) {
        QString fPath = event->mimeData()->urls().at(0).toLocalFile();
        handleDrop(fPath);
        dragLabel->hide();
    }
}

void MW::handleDrop(QString fPath)
{
    if (G::isLogger) G::log("MW::handleDrop");
    QFileInfo info(fPath);
    QString incoming = info.dir().absolutePath();
    if (dm->folderList.contains(incoming)) {
        QString ext = info.suffix().toLower();
        if (metadata->supportedFormats.contains(ext)) {
            sel->setCurrentPath(fPath);
        }
    }
    else {
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
        // rgh req'd?  If so, must change as G::currRootFolder has been eliminated
        // selectCurrentViewDir();
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
