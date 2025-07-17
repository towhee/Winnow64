#include "stack.h"

Stack::Stack(QStringList &selection,
             DataModel *dm,
             Metadata *metadata,
             ImageCacheData *icd) :

             selection(selection),
             dm(dm),
             metadata(metadata),
             icd(icd)

{
    if (G::isLogger) G::log("Stack::Stack");
}

void Stack::stop()
{
    if (G::isLogger) G::log("Stack::stop");
    abort = true;
    G::isRunningStackOperation = false;
    G::popup->setProgressVisible(false);
    G::popup->reset();
    G::popup->showPopup("Stack operation has been aborted.");
    qDebug() << "Stack::stop" << abort;
//    qApp->processEvents();
}

QString Stack::mean()
{
    if (G::isLogger) G::log("Stack::mean");
    abort = false;
    G::isRunningStackOperation = true;

    int row = dm->rowFromPath(selection.at(0));
    int w = dm->index(row, G::WidthColumn).data().toInt();
    int h = dm->index(row, G::HeightColumn).data().toInt();

    // check all images in selection have the same dimensions


    // total selection with same width or height
    int n = 0;
    for (int i = 0; i < selection.count(); ++i) {
        row = dm->rowFromPath(selection.at(0));
        int thisW = dm->index(row, G::WidthColumn).data().toInt();
        int thisH = dm->index(row, G::HeightColumn).data().toInt();
        if (thisW == w && thisH == h) n++;
    }
    if (n < 2) {
        G::popup->showPopup("Select more than 1 image to execute a mean stack.", 2000);
        return "";
    }

    QImage image;
    Pixmap *pix = new Pixmap(this, dm, metadata);
    // vector to hold each image
    QVector<QVector<QRgb>> s(h);
    // vector for incrementing mean values
    QVector<QVector<PX>> m(h);
    // initialize vectors
    PX px;
    px.r = 0;
    px.g = 0;
    px.b = 0;
    for (int y = 0; y < h; ++y) {
        s[y].resize(w);         // source = img
        m[y].resize(w);         // mean pixels
        for (int x = 0; x < w; ++ x) {
            m[y][x] = px;
        }
    }

    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(n + 1);
    QString txt = "Generating the average from " + QString::number(n) + " images." +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popup->showPopup(txt, 0, true, 1);

    // incrementally update mean vector for each selected image
    for (int i = 0; i < n; ++i) {
        if (abort) break;
        QString fPath = selection.at(i);
        if (icd->contains(fPath)) {
            image = icd->imCache.value(fPath);
        }
        else {
            pix->load(fPath, image, "Stack::doMean");
        }
        // get image width/height from QImage - might be different than metadata for raw
        if (i == 0) {
            w = image.width();
            h = image.height();
        }
        else {
            if ((image.width() != w) || (image.height() != h)) {
                QString txt = "All images in selection must have the same dimensions.<br>"
                              "Image " + fPath + "<br>"
                              "has diffent dimensions."
                              "<p>Press <font color=\"red\"><b>Esc</b></font> to continue."
                              ;
                G::popup->setProgressVisible(false);
                G::popup->reset();
                 G::popup->showPopup(txt, 0, true, 1);
                return "";
            }
            //if ((image.height() != h)) continue;
        }
        // load image to vector s
        for (int y = 0; y < h; ++y) {
            memcpy(&s[y][0], image.scanLine(y), static_cast<size_t>(image.bytesPerLine()));
        }

        // increment mean vector by s/n
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++ x) {
                QColor rgb(s[y][x]);
                m[y][x].r += (rgb.red() * 1.0 / n);
                m[y][x].g += (rgb.green() * 1.0 / n);
                m[y][x].b += (rgb.blue() * 1.0 / n);
                /*
                if (i==0 && y==0 && x==0) {
                    qDebug() << "Stack::mean"
                             << "rgb =" << rgb
                             << "m[y][x].r =" << m[y][x].r
                             << "m[y][x].g =" << m[y][x].g
                             << "m[y][x].b =" << m[y][x].b
                                ;
                }
                //*/
            }
        }
        G::popup->setProgress(i+1);
        qApp->processEvents();
    }

    QString newFilePath;
    if (!abort) {
        // transfer mean vector to source vector
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++ x) {
                uint r = static_cast<uint>(m[y][x].r);
                uint g = static_cast<uint>(m[y][x].g);
                uint b = static_cast<uint>(m[y][x].b);
                s[y][x] = 0xff000000 | (r << 16) | (g << 8) | b;
            }
        }

        G::popup->setProgress(n + 1);

        // convert source vector to image
        for (int y = 0; y < image.height(); ++y) {
            memcpy(image.scanLine(y), &s[y][0], static_cast<size_t>(image.bytesPerLine()));
        }

        // save
        QFileInfo info(selection.at(0));
        QString base = info.dir().absolutePath() + "/" + info.baseName() + "_MeanStack" +
                       QString::number(n);
        newFilePath = base + ".jpg";
        int count = 0;
        bool fileAlreadyExists = true;
        QString newBase = base + "_";
        do {
            QFile testFile(newFilePath);
            if (testFile.exists()) {
                newFilePath = newBase + QString::number(++count) + ".jpg";
                base = newBase;
            }
            else fileAlreadyExists = false;
        } while (fileAlreadyExists);
        image.save(newFilePath, "JPG", 100);

        // copy metadata from first image to the new stacked image
        QString src = selection.at(0);
        QString dst = newFilePath;
        ExifTool et;
        et.setOverWrite(true);
        // copy all metadata tags from src to dst
        et.copyAllTags(src, dst);
        // copy ICC from src to dst
        et.copyICC(src, dst);
        // add thumbnail to dst
        et.addThumb(src, dst);
        QVariant ret = et.close();
        qDebug() << "Stack::mean" << "et exit code =" << ret;

        G::popup->setProgressVisible(false);
        G::popup->reset();
    }

    delete pix;
    return newFilePath;
}
