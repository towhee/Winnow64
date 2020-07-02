#include "patterndlg.h"

PatternDlg::PatternDlg(QWidget *parent, QPixmap &pm, QPixmap &tile, QString &name)
    : QDialog(parent),
      pm(pm),
      tile(tile),
      name(name)
{
    qDebug() << __FUNCTION__ << pm.size();
    resize(800, 600);
    setWindowTitle("Tile Extractor");

    saveBtn->setMinimumSize(100, 25);
    patternBtn->setMinimumSize(100, 25);
    plusBtn->setMinimumSize(100, 25);
    minusBtn->setMinimumSize(100, 25);
    zoomText = new QLabel("100%");

    // image at top
    layout = new QVBoxLayout;
    v = new QGraphicsView;
    scene = new QGraphicsScene;
    QGraphicsPixmapItem *pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    scene->addItem(pmItem);
    v->setScene(scene);
    pmItem->setPixmap(pm);

    // buttons at bottom
    btnLayout = new QHBoxLayout;
    btnLayout->setAlignment(Qt::AlignRight);
    btnFrame = new QFrame;
    btnLayout->addWidget(plusBtn);
    btnLayout->addWidget(minusBtn);
    btnLayout->addWidget(zoomText);
    btnLayout->addStretch();
    btnLayout->addWidget(patternBtn);
    btnLayout->addWidget(saveBtn);
    btnFrame->setLayout(btnLayout);

    layout->addWidget(v);
    layout->addWidget(btnFrame);
    layout->setGeometry(QRect(-11, -11, 800, 600));

    setLayout(layout);
    zoom = 1;

    imageRect = new QGraphicsRectItem;
    imageRect->setRect(-1, -1, pm.width(), pm.height());
    QPen pen;
    pen.setColor(QColor("#708090"));
    pen.setWidth(1);
    imageRect->setPen(pen);
    scene->addItem(imageRect);
    imageRect->show();


    connect(plusBtn, &QPushButton::clicked, this, &PatternDlg::zoomIn);
    connect(minusBtn, &QPushButton::clicked, this, &PatternDlg::zoomOut);
    connect(patternBtn, &QPushButton::clicked, this, &PatternDlg::findPattern);
    connect(saveBtn, &QPushButton::clicked, this, &PatternDlg::save);

    findPattern();

//    delete pmItem;
//    delete btnLayout;
}

PatternDlg::~PatternDlg()
{
//    delete saveBtn;
//    delete patternBtn;
//    delete plusBtn;
//    delete minusBtn;
//    delete zoomText;
//    delete v;
//    delete scene;
//    delete imageRect;
//    delete layout;
//    delete btnLayout;
//    delete btnFrame;
}

void PatternDlg::save()
{
    qDebug() << __FUNCTION__;
    bool ok;
    QInputDialog input;
    input.setFixedWidth(500);
    name = input.getText(this, tr("Tile extractor"),
                  tr("Enter tile name"), QLineEdit::Normal,
                  "", &ok);
    if (ok) {
        // quit dialog
        accept();
    }
}

void PatternDlg::findPattern()
{

    patternRect = new QGraphicsRectItem;
    QPen pen;
    pen.setColor(QColor(Qt::white));
    pen.setWidth(1);
    patternRect->setPen(pen);
    scene->addItem(patternRect);

    QDebug debug = qDebug();
    debug.noquote();

    QImage i = pm.toImage();
    int w = i.width();
    int h = i.height();
    // tile width and height
    int tw = 0;
    int ty = 0;
    /*
    // example using QRgb
    QRgb o = i.pixel(0,0);
    QRgb px;
//    */
    int xMatch;
    QString s, sx, sy, s0, s1, s2;
    s0 = "    0:";
    s0 += QString::number(i.pixel(0,0)).rightJustified(10);

    // find a match on the x coordinates
    /*
    debug << s0 << "\n";
//    */
    bool found = false;
    for (int x = 1; x < w; ++x) {
        /*
        sx = QString::number(x).rightJustified(4) + ":";
        sx += QString::number(i.pixel(x,0)).rightJustified(10);
        */
        if (i.pixel(x,0) == i.pixel(0,0)) {
            // found a match, check if total match
            /*
            debug << sx << "==" << s0 <<"Match found - check next\n";
//            */
            xMatch = x;
            for (int x1 = 1, x2 = xMatch + 1; x1 < xMatch; ++x1, ++x2) {
                /*
                s1 = QString::number(x1).rightJustified(4) + ":";
                s1 += QString::number(i.pixel(x1,0)).rightJustified(10);
                s2 = QString::number(x2).rightJustified(4) + ":";
                s2 += QString::number(i.pixel(x2,0)).rightJustified(10);
//                */
                // not a match
                if (x2 == w || i.pixel(x1,0) != i.pixel(x2,0)) {
                    /*
                    debug << sx << "   " << s1 << "!=" << s2 << "Match not found, continue trying\n";
//                    */
                    break;
                }
                /*
                debug << sx << "   " << s1 << "==" << s2 << "Match found so far\n";
                */
                if (x1 == xMatch - 1) found = true;
            }
            if (found) {
                /*
                debug << "match found at" << x << "Tile x = 0 to " << x - 1 << "\n\n";
                */
                tw = x;
                break;
            }
        }
        /*
        debug << sx << "!=" << s0 << "\n";
//        */
    }

    // find a match on the y coordinates
    int yMatch;
    debug << s0 << "\n";
    found = false;
    for (int y = 1; y < h; ++y) {
        /*
        sy = QString::number(y).rightJustified(4) + ":";
        sy += QString::number(i.pixel(0,y)).rightJustified(10);
//        */
        if (i.pixel(0,y) == i.pixel(0,0)) {
            // found a match, check if total match
            /*
            debug << sy << "==" << s0 <<"Match found - check next\n";
//            */
            yMatch = y;
            for (int y1 = 1, y2 = yMatch + 1; y1 < yMatch; ++y1, ++y2) {
                /*
                s1 = QString::number(y1).rightJustified(4) + ":";
                s1 += QString::number(i.pixel(0,y1)).rightJustified(10);
                s2 = QString::number(y2).rightJustified(4) + ":";
                s2 += QString::number(i.pixel(0,y2)).rightJustified(10);
//                */
                // not a match
                if (y2 == h || i.pixel(0,y1) != i.pixel(0,y2)) {
                    /*
                    debug << sy << "   " << s1 << "!=" << s2 << "Match not found, continue trying\n";
//                    */
                    break;
                }
                /*
                debug << sy << "   " << s1 << "==" << s2 << "Match found so far\n";
//                */
                if (y1 == yMatch - 1) found = true;
            }
            if (found) {
                /*
                debug << "match found at" << y << "Tile y = 0 to " << y - 1 << "\n\n";
//                */
                ty = y;
                break;
            }
        }
        /*
        debug << sy << "!=" << s0 << "\n";
//        */
    }

    if (tw && ty) {
        patternRect->setRect(-1,-1,tw,ty);
        patternRect->show();
        QRect tileRect(0, 0, tw, ty);
        tile = pm.copy(tileRect);
    }

}

void PatternDlg::zoomIn()
{
    matrix.reset();
    zoom *= 2;
    matrix.scale(zoom, zoom);
    v->setMatrix(matrix);
    zoomText->setText(QString::number(zoom*100) + "%");
}

void PatternDlg::zoomOut()
{
    matrix.reset();
    zoom /= 2;
    matrix.scale(zoom, zoom);
    v->setMatrix(matrix);
    zoomText->setText(QString::number(zoom*100) + "%");
}
