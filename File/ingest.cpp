#include "ingest.h"

Ingest::Ingest(QWidget *parent,
               bool &combineRawJpg,
               bool &combinedIncludeJpg,
               bool &integrityCheck,
               bool &ingestIncludeXmpSidecar,
               bool &isBackup,
               int &seqStart,
               Metadata *metadata,
               DataModel *dm,
               QString &folderPath,
               QString &folderPath2,
               QMap<QString, QString>&filenameTemplates,
               int &filenameTemplateSelected) :

               combineRawJpg(combineRawJpg),
               combinedIncludeJpg(combinedIncludeJpg),
               integrityCheck(integrityCheck),
               ingestIncludeXmpSidecar(ingestIncludeXmpSidecar),
               isBackup(isBackup),
               seqStart(seqStart),
               filenameTemplatesMap(filenameTemplates),
               filenameTemplateSelected(filenameTemplateSelected)
{
    this->dm = dm;
    this->metadata = metadata;
    this->folderPath = folderPath;      // cannot concatenate if referenced
    this->folderPath2 = folderPath2;    // cannot concatenate if referenced
    seqNum = seqStart;
    initExampleMap();
    getPicks();
}

void Ingest::stop()
{
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
}

void Ingest::commence()
{
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    start(NormalPriority);
}

void Ingest::initExampleMap()
{
    if (G::isLogger) G::log("Ingest::initExampleMap");
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

bool Ingest::isToken(QString tokenString, int pos)
{
    if (G::isLogger) G::log("Ingest::isToken");
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

QString Ingest::parseTokenString(QFileInfo info, QString tokenString)
{
    if (G::isLogger) G::log("Ingest::parseTokenString");
    QString fPath = info.absoluteFilePath();
    ImageMetadata m = dm->imMetadata(fPath);
    QDateTime createdDate = m.createdDate;
//    qDebug() << "Ingest::parseTokenString" << fPath << createdDate;
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

void Ingest::getPicks()
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
    if (G::isLogger) G::log("Ingest::getPicks");
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
//                        qDebug() << "Ingest::getPicks" << "appending" << fPath;
                    }
                    // append combined raw file
                    if (idx.data(G::DupIsJpgRole).toBool()) {
                        idx = qvariant_cast<QModelIndex>(dm->index(row, 0).data(G::DupOtherIdxRole));
                    }
                }
                fPath = idx.data(G::PathRole).toString();
                QFileInfo fileInfo(fPath);
                pickList.append(fileInfo);
//                qDebug() << "Ingest::getPicks" << "appending" << fPath;
            }
        }
    }
}

void Ingest::renameIfExists(QString &destination,
                               QString &baseName,
                               QString dotSuffix)
{
    if (G::isLogger) G::log("Ingest::renameIfExists");
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

void Ingest::run()
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

    // rgh to do: replace int filenameTemplateSelected with QString filenameTemplateSelected
    QMapIterator<QString,QString> it(filenameTemplatesMap);
    int i = 0;
    QString tokenString = "";
    while (it.hasNext()) {
        it.next();
        if (i == filenameTemplateSelected) {
//            qDebug() << "Ingest::run" << "it.value()" << it.value();
            tokenString = it.value();
            break;
        }
        i++;
    }
    if (tokenString == "") {
        // add failure message
        return;
    }

    // copy cycles req'd: 1 if no backup, 2 if backup
    int n;
    isBackup ? n = 2 : n = 1;

    // list of files not copied

    QStringList failedToCopy;
    QStringList integrityFailure;

    // copy picked images
    for (int i = 0; i < pickList.size(); ++i) {
        if (abort) break;
        int progress = (i + 1) * 100 * n / (pickList.size());
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

        // rename destination file based on the file naming template
        QString destBaseName =  parseTokenString(pickList.at(i), tokenString);
        QString suffix = fileInfo.suffix().toLower();
        QString dotSuffix = "." + suffix;
        QString destFileName = destBaseName + dotSuffix;
        QString sidecarName = destBaseName + ".xmp";

        // check if image already exists at destination folder
        /* folderPath cannot be referenced - this causes a memory error */
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
            int dmRow = dm->rowFromPath(sourcePath);
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
        bool copyOk = QFile::copy(sourcePath, destinationPath);

        /* for demonstration:
        failedToCopy << sourcePath + " to " + destinationPath;
        integrityFailure << sourcePath + " not same as " + destinationPath;
        // */
        if (!copyOk) {
            qDebug() << "Ingest::run" << "Failed to copy" << sourcePath << "to" << destinationPath;
            failedToCopy << sourcePath + " to " + destinationPath;
        }
        if (copyOk && integrityCheck) {
            if (!Utilities::integrityCheck(sourcePath, destinationPath)) {
                qDebug() << "Ingest::run" << "Integrity failure" << sourcePath << "not same as" << destinationPath;
                integrityFailure << sourcePath + " not same as " + destinationPath;
            }
        }

        // copy source image to backup
        if(isBackup) {
            bool backupCopyOk = QFile::copy(sourcePath, backupPath);
            if (!backupCopyOk) {
                qDebug() << "Ingest::run" << "Failed to copy" << sourcePath << "to" << backupPath;
                failedToCopy << sourcePath + " to " + backupPath;
            }
            if (backupCopyOk && integrityCheck) {
                if (!Utilities::integrityCheck(sourcePath, backupPath)) {
                    qDebug() << "Ingest::run" << "Integrity failure" << sourcePath << "not same as" << backupPath;
                    integrityFailure << sourcePath + " not same as " + backupPath;
                }
            }
        }

        // if sidecar exists copy to destination
        bool sidecarOk = false;
        if (copyOk && QFile(sourceSidecarPath).exists()) {
            sidecarOk = QFile::copy(sourceSidecarPath, destSidecarPath);
            if (!sidecarOk) {
                QString msg = "Failer to copy " + sourceSidecarPath + " to " + destSidecarPath + ".";
                G::issue("Warning", msg, "Ingest::ingest");
                failedToCopy << sourceSidecarPath + " to " + destSidecarPath;
            }

        }

        // check if copied xmp = original xmp
        if (copyOk && integrityCheck) {
            if (!Utilities::integrityCheck(sourceSidecarPath, destSidecarPath)) {
                QString msg = "Integrity failure, " + sourceSidecarPath + " not same as " + destSidecarPath + ".";
                G::issue("Warning", msg, "Ingest::ingest");
                integrityFailure << sourceSidecarPath + " not same as " + destSidecarPath;
            }
        }

        // copy sidecar from destination to backup
        if (isBackup && sidecarOk) {
            bool backupSidecarCopyOk = QFile::copy(destSidecarPath, backupSidecarPath);
            if (!backupSidecarCopyOk) {
                QString msg = "Failed to copy " + destSidecarPath + " to " + backupSidecarPath + ".";
                G::issue("Warning", msg, "Ingest::ingest");
                failedToCopy << destSidecarPath + " to " + backupSidecarPath;
            }
            if (copyOk && integrityCheck) {
                if (!Utilities::integrityCheck(destSidecarPath, backupSidecarPath)) {
                    QString msg = "Integrity failure, " + destSidecarPath + " not same as " + backupSidecarPath + ".";
                    G::issue("Warning", msg, "Ingest::ingest");
                    integrityFailure << destSidecarPath + " not same as " + backupSidecarPath;
                }
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
                qDebug() << "Ingest::run" << bytesWritten << buffer.length();
                newFile.close();
            }
        }
        */

    // update ingest count for Winnow session
    G::ingestCount += pickList.size();
    G::ingestLastSeqDate = seqDate;
    //qDebug() << "Ingest::run" << seqDate << G::ingestCount << G::ingestLastSeqDate;

    // show any ingest errors
    if (failedToCopy.length() || integrityFailure.length()) {
        // Cannot show QDialog in non-gui thread, so message info to main thread
        // Note: difference from IngestDlg::ingest
        emit rptIngestErrors(failedToCopy, integrityFailure);
    }

    if (abort) G::popUp->showPopup("Background ingest terminated", 2000);

    emit updateProgress(-1);
    emit ingestFinished();
}
