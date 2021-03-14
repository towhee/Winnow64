#include "patterndlg.h"

/*
   Find a repeating pattern in the pixmap (pm) or any selection in pm.  Save this as a tile
   in Embellish QSettings to be used to create a border texture in Embellish.
*/

PatternDlgView::PatternDlgView(QPixmap &pm, QPixmap &tile, QWidget * /*parent*/)
    : pm(pm), tile(tile)
{
    scene = new QGraphicsScene;
    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    scene->addItem(pmItem);
    setScene(scene);
    pmItem->setPixmap(pm);
    zoom = 1;
    matrix.reset();
    matrix.scale(zoom, zoom);
    setMatrix(matrix);

    rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    patternRect = new QGraphicsRectItem;

    imageRect = new QGraphicsRectItem;
    imageRect->setRect(-1, -1, pm.width()+1, pm.height()+1);
    QPen pen;
    pen.setColor(QColor("#708090"));
    pen.setWidth(1);
    imageRect->setPen(pen);
    scene->addItem(imageRect);
    imageRect->show();
}

void PatternDlgView::mousePressEvent(QMouseEvent *event)
{
    setCursor(Qt::CrossCursor);
    origin = event->pos();
    rubberBand->setGeometry(QRect(origin, QSize()));
    rubberBand->show();
}

void PatternDlgView::mouseMoveEvent(QMouseEvent *event)
{
    rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
}

void PatternDlgView::mouseReleaseEvent(QMouseEvent * /*event*/)
{
    setCursor(Qt::ArrowCursor);
    QRect rb = rubberBand->geometry();
    QPoint p0 = mapToScene(rb.topLeft()).toPoint();
    QPoint p1 = mapToScene(rb.bottomRight()).toPoint();
    QRect sampleRect(p0, p1);
    QPixmap sample = pmItem->pixmap().copy(sampleRect);
    findPatternInSample(sample, sampleRect);
}

void PatternDlgView::debugPxArray(QImage &i, int originX, int originY, int size)
{
/*
    Uncomment "Send array to console for debugging" in PatternDlgView::findPatternInSample.
    This function will print the array of pixels to the console.  Copy the console and paste
    into D:\data\My Projects\Winnow Project\Doc\Embellish.xlsx in the tile pixels sheet.  If
    necessary convert text to columns.  You can then analyse the pixels in the array to debug
    the tile pattern.
*/
    qDebug() << __FUNCTION__ << size << i.size();
    QString row;

    for (int y = 0; y < size; y++) {
        row = "";
        for (int x = 0; x < size; x++) {
            QString pRgba = QString::number(i.pixel(originX + x, originY + y), 16) + " ";
            row += pRgba;
        }
        Utilities::log("", row);
        qDebug().noquote() << row;
    }
}

void PatternDlgView::findPatternInSample(QPixmap &sample, QRect &sampleRect)
{
/*
   This function searches the pixmap sample for a repeating pattern.  The x axis is scanned
   for a repeating pattern starting at (0,0) and if found, then the pixmap is searched for
   the next row starting with the same pattern.  If found, then the pixmap tile is created
   with a copy of sample for the repeating area.

   If no repeating area is found then the entire sample is copied to the pixmap tile.
*/
qDebug() << __FUNCTION__;
    rubberBand->hide();
    QPen pen;
    pen.setColor(QColor(Qt::white));
    pen.setWidth(1);
    patternRect->setPen(pen);
    scene->addItem(patternRect);

    QImage i = sample.toImage();
    int w = i.width();
    int h = i.height();
    // tile width and height
    int tw = 0;
    int th = 0;
    /*
    // example using QRgb
    QRgb o = i.pixel(0,0);
    QRgb px;
//    */
    int xMatch;

    /* Send array to console for debugging
    debugPxArray(i, 0, 0, 150);
    return;
//    */

    // cycle through every possible start point to find pattern (v = y coord, u == x coord)
    bool found = false;
    // find a match on the x coordinates
    tw = th = 0;
    found = false;
    // origin or current search is u,v
    for (int x = 1; x < w; ++x) {
        if (i.pixel(x,0) == i.pixel(0,0)) {
            // found a match, check if total match
            xMatch = x;
            for (int x1 = 1, x2 = xMatch + 1; x1 < xMatch; ++x1, ++x2) {
                // not a match
                if (x2 == w || i.pixel(x1,0) != i.pixel(x2,0)) {
                    break;
                }
                if (x1 == xMatch - 1) found = true;
            }
            if (found) {
                tw = x;
                break;
            }
        }
    }

    // a repeating pattern was found on the x axis
    if (found) {
        // find a repeat of the x pattern in another row (y)
        for (int y = 1; y < h; ++y) {
            found = true;
            for (int x = 0; x < tw; x++) {
                if (i.pixel(x,y) != i.pixel(x,0)) {
                        found = false;
                        break;
                }
            }
            if (found) {
                th = y;
                break;
            }
        }
    }

    if (tw && th) {
        msg("Pattern found.  Tile shown in white box.  ");
        int x = sampleRect.x();
        int y = sampleRect.y();
        patternRect->setRect(x, y , tw, th);
        patternRect->show();
        QRect tileRect(x, y , tw, th);
        tile = pm.copy(tileRect);
    }
    else {
        msg("No pattern found.  You can still save the rubberbanded area as a tile, "
            "but it may not create a seamless texture.");
        patternRect->hide();
        rubberBand->show();
        tile = pm.copy(sampleRect);
    }
}

PatternDlg::PatternDlg(QWidget *parent, QPixmap &pm)
    : QDialog(parent),
      pm(pm)
{
    QSize r = Utilities::fitScreen(QSize(800, 750));
    resize(r.width(), r.height());
    int imageWindowH;

    pm.height() > 600 ? imageWindowH = 600 : imageWindowH = pm.height();
    r.height() < 750 ? imageWindowH = r.height() - 150 : imageWindowH = 600;

    setWindowTitle("Tile Extractor");
    setMouseTracking(true);

    saveBtn->setStyleSheet("QPushButton {min-width: 120px;}");
    exitBtn->setStyleSheet("QPushButton {min-width: 120px;}");

    layout = new QVBoxLayout;

    // graphicview at top
    v = new PatternDlgView(pm, tile);
    vLayout = new QHBoxLayout;
    vLayout->addWidget(v);
    vFrame = new QFrame;
//    vFrame->setMinimumHeight(h);
//    vFrame->setMaximumHeight(300);
//    vFrame->setMaximumHeight(imageWindowH);
    vFrame->setLayout(vLayout);

    // message area
    msg = new QLabel;
    msg->setWordWrap(true);
    msg->setAlignment(Qt::AlignTop);
    QString msgText = "INSTRUCTIONS:  Mouse click and drag to make a selection.  " //  \n\n"
            "To extract a repeating pattern, the selection must be within the "
            "pattern area and be double the size of a tile.  To select a texture "
            "with no repeating pattern just make a selection and save.  Note that this "
            "may not create a seamless texture.";
    msg->setText(msgText);
    msg->setToolTip(msgText);
    msg->setMinimumHeight(100);
    msg->setMaximumHeight(100);
    msgLayout = new QVBoxLayout;
    msgLayout->addWidget(msg);
    msgLayout->addStretch();
    msgFrame = new QFrame;
    msgFrame->setLineWidth(1);
    msgFrame->setLayout(msgLayout);

    // buttons at bottom
    btnLayout = new QHBoxLayout;
    btnLayout->setAlignment(Qt::AlignRight);
    btnFrame = new QFrame;
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(exitBtn);
    btnFrame->setLayout(btnLayout);

    layout->addWidget(vFrame);
    layout->addWidget(msgFrame);
    layout->addWidget(btnFrame);

    setLayout(layout);

    connect(v, &PatternDlgView::msg, this, &PatternDlg::updateMsg);
    connect(exitBtn, &QPushButton::clicked, this, &PatternDlg::quit);
    connect(saveBtn, &QPushButton::clicked, this, &PatternDlg::save);
}

PatternDlg::~PatternDlg()
{
}

void PatternDlg::updateMsg(QString txt)
{
    msg->setText(txt);
}

void PatternDlg::save()
{
    qDebug() << __FUNCTION__ << "tile.size() =" << tile.size();
    QStringList doNotUse;
    QString name = Utilities::inputText("Save tile", "Enter tile name", doNotUse);
    emit saveTile(name, &tile);
}

void PatternDlg::quit()
{
    accept();
}
