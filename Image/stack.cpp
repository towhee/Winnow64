#include "stack.h"

Stack::Stack(Method method, DataModel *dm, Metadata *metadata, ImageCacheData *icd)
{
    if (G::isLogger) G::log(__FUNCTION__);
    this->dm = dm;
    this->metadata = metadata;
    this->icd = icd;
    getPicks();
    if (!pickList.length()) return;
    doMean();
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

void Stack::doMean()
{
    if (G::isLogger) G::log(__FUNCTION__);
    int n = pickList.length();
    int row = dm->rowFromPath(pickList.at(0));
    int w = dm->index(row, G::WidthColumn).data().toInt();
    int h = dm->index(row, G::HeightColumn).data().toInt();
    QImage image;
    Pixmap *pix = new Pixmap(this, dm, metadata);
    // vector to hold each image
    QVector<QVector<QRgb>> s(h);
    QVector<QVector<PX>> m(h);
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

    for (int i = 0; i < pickList.length(); ++i) {
        QString fPath = pickList.at(i);
        if (icd->imCache.contains(fPath)) icd->imCache.find(fPath, image);
        else pix->load(fPath, image, "Stack::doMean");
        if (image.width() != w) continue;
        if (image.height() != h) continue;
        // load image to vector s
        for (int y = 0; y < h; ++y) {
            memcpy(&s[y][0], image.scanLine(y), static_cast<size_t>(image.bytesPerLine()));
        }
        // increment mean vector by s/n
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++ x) {
                QColor rgb(s[y][x]);
                m[y][x].r += rgb.red() * 1.0 / n;
                m[y][x].g += rgb.green() * 1.0 / n;
                m[y][x].b += rgb.blue() * 1.0 / n;
            }
        }
    }

    // transfer mean vector to source vector
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++ x) {
            uint r = static_cast<uint>(m[y][x].r);
            uint g = static_cast<uint>(m[y][x].g);
            uint b = static_cast<uint>(m[y][x].b);
            s[y][x] = 0xff000000 | (r<<16) | (g<<8) | b;
        }
    }

    // convert source vector to image
    for (int y = 0; y < image.height(); ++y) {
        memcpy(image.scanLine(y), &s[y][0], static_cast<size_t>(image.bytesPerLine()));
    }


    delete pix;
}
