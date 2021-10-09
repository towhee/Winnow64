#include "stack.h"

Stack::Stack(QModelIndexList &selection,
             DataModel *dm,
             Metadata *metadata,
             ImageCacheData *icd) :

             selection(selection),
             dm(dm),
             metadata(metadata),
             icd(icd)

{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__;
}

void Stack::getPicks()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString fPath;
    pickList.clear();
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
        // only picks
        if (pickIdx.data(Qt::EditRole).toString() == "true") {
            QModelIndex idx = dm->sf->index(row, 0);
            fPath = idx.data(G::PathRole).toString();
            pickList.append(fPath);
        }
    }
}

void Stack::mean()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__;
    int row = dm->rowFromPath(selection.at(0).data(G::PathRole).toString());
    int w = dm->index(row, G::WidthColumn).data().toInt();
    int h = dm->index(row, G::HeightColumn).data().toInt();

    // total selection with same width or height
    int n = 0;
    for (int i = 0; i < selection.count(); ++i) {
        row = dm->rowFromPath(selection.at(0).data(G::PathRole).toString());
        int thisW = dm->index(row, G::WidthColumn).data().toInt();
        int thisH = dm->index(row, G::HeightColumn).data().toInt();
        if (thisW == w && thisH == h) n++;
    }
    if (n < 2) return;
    qDebug() << __FUNCTION__ << "n =" << n;

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

    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(n + 2);
    QString txt = "Crunching the mean from  " + QString::number(n) + " images." +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->showPopup(txt, 0, true, 1);

    qDebug() << __FUNCTION__ << "Initialize done";
    // incrementally update mean vector for each selected image
    for (int i = 0; i < n; ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        if (icd->imCache.contains(fPath)) icd->imCache.find(fPath, image);
        else pix->load(fPath, image, "Stack::doMean");
        // get image width/height from QImage - might be different than metadata for raw
        if (i == 0) {
            w = image.width();
            h = image.height();
        }
        else {
            if (image.width() != w) continue;
            if (image.height() != h) continue;
        }
        // load image to vector s
        for (int y = 0; y < h; ++y) {
            memcpy(&s[y][0], image.scanLine(y), static_cast<size_t>(image.bytesPerLine()));
        }
        qDebug() << __FUNCTION__ << "s[y][0] =" << s[0][0];
        // increment mean vector by s/n
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++ x) {
                QColor rgb(s[y][x]);
                m[y][x].r += (rgb.red() * 1.0 / n);
                m[y][x].g += (rgb.green() * 1.0 / n);
                m[y][x].b += (rgb.blue() * 1.0 / n);
                if (i==0 && y==0 && x==0) {
                    qDebug() << __FUNCTION__
                             << "rgb =" << rgb
                             << "m[y][x].r =" << m[y][x].r
                             << "m[y][x].g =" << m[y][x].g
                             << "m[y][x].b =" << m[y][x].b
                                ;
                }
            }
        }
        G::popUp->setProgress(i+1);
        qApp->processEvents();
        qDebug() << __FUNCTION__ << "Image" << i << "done";
    }

    qDebug() << __FUNCTION__ << "transfer mean vector to source vector";
    // transfer mean vector to source vector
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++ x) {
            uint r = static_cast<uint>(m[y][x].r);
            uint g = static_cast<uint>(m[y][x].g);
            uint b = static_cast<uint>(m[y][x].b);
            s[y][x] = 0xff000000 | (r << 16) | (g << 8) | b;
        }
    }

    G::popUp->setProgress(n + 1);
    qApp->processEvents();

    qDebug() << __FUNCTION__ << "convert source vector to image";
    // convert source vector to image
    for (int y = 0; y < image.height(); ++y) {
        memcpy(image.scanLine(y), &s[y][0], static_cast<size_t>(image.bytesPerLine()));
    }

    G::popUp->setProgress(n + 2);
    qApp->processEvents();

    qDebug() << __FUNCTION__ << "save";
    // save
    QFileInfo info(selection.at(0).data(G::PathRole).toString());
    QString base = info.dir().absolutePath() + "/" + info.baseName() + "_MeanStack";
    QString newFilePath = base + ".jpg";
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

    G::popUp->setProgressVisible(false);
    G::popUp->hide();

    delete pix;
}
