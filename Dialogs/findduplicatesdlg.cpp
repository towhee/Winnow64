#include "Dialogs/findduplicatesdlg.h"
#include "ui_findduplicatesdlg.h"
#include "Main/global.h"
#include "Utilities/htmlwindow.h"
#include "Effects/effects.h"
#include "ui_metadatareport.h"
#include <QElapsedTimer>
#include <QEventLoop>

/*******************************************************************************************/

/*
    DragToList: the include / exclude folder lists.

    Every item is an editable folder path with a checkbox: checked = include the
    folder's whole subfolder tree, unchecked = that folder only. Paths get in four ways: dropped (FSTree,
    Finder, Explorer), the Add buttons, editing an item (double-click, F2 / Return),
    or appending one (double-click empty space, or the context menu). The editor
    autocompletes folder paths.

    An edited path is normalized (~ expanded, native separators, no trailing slash) and
    shown in red when the folder does not exist. When the editor closes, empty and
    duplicate rows are removed, so an abandoned append leaves nothing behind.
    pathsChanged() fires whenever the list content changes by drop or edit.
*/

namespace {
class PathEditDelegate : public QStyledItemDelegate
{
/*
    The default line edit, plus a completer over the folders in the file system.
*/
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            QFileSystemModel *fsModel = new QFileSystemModel(lineEdit);
            fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
            fsModel->setRootPath(QString());
            QCompleter *completer = new QCompleter(fsModel, lineEdit);
            #ifdef Q_OS_WIN
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            #endif
            lineEdit->setCompleter(completer);
            lineEdit->setPlaceholderText("Type a folder path");
        }
        return editor;
    }
};
} // namespace

DragToList::DragToList(QWidget *parent) :
    QListWidget(parent)
{
    setAcceptDrops(true);
    setDragEnabled(false);
    setDragDropMode(QAbstractItemView::DropOnly);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    setItemDelegate(new PathEditDelegate(this));
    setToolTip("Drag folders here, or double-click empty space to type a path.\n"
               "Check a folder to include its subfolders too.\n"
               "Double-click a path to edit it. Delete removes the selected path.\n"
               "Right-click for more.");
    connect(this, &QListWidget::itemChanged, this, &DragToList::onItemChanged);
}

QString DragToList::normalizePath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty()) return path;
    if (path == "~" || path.startsWith("~/")) path = QDir::homePath() + path.mid(1);
    return QDir::cleanPath(QDir::fromNativeSeparators(path));  // also drops a trailing /
}

QListWidgetItem *DragToList::newPathItem(const QString &path)
{
    // editable path + "include subfolders" checkbox (off: that folder only)
    QListWidgetItem *item = new QListWidgetItem(path);
    item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    return item;
}

bool DragToList::includesSubfolders(int row) const
{
    QListWidgetItem *it = item(row);
    return it && it->checkState() == Qt::Checked;
}

void DragToList::markValidity(QListWidgetItem *item)
{
    // red text + tooltip for a folder that does not exist (a typo, or an unmounted drive)
    QSignalBlocker blocker(this);
    if (QFileInfo(item->text()).isDir()) {
        item->setData(Qt::ForegroundRole, QVariant());
        item->setToolTip(item->text());
    }
    else {
        item->setForeground(QColor("#E57373"));
        item->setToolTip("Folder not found: " + item->text());
    }
}

bool DragToList::addPath(const QString &path)
{
/*
    Append path as an editable item. Returns false if it is empty or already listed.
*/
    QString p = normalizePath(path);
    if (p.isEmpty() || !findItems(p, Qt::MatchExactly).isEmpty()) return false;
    QListWidgetItem *item = newPathItem(p);
    {
        QSignalBlocker blocker(this);
        addItem(item);
    }
    markValidity(item);
    return true;
}

void DragToList::appendNew()
{
    // an empty row with its editor open; removed in closeEditor if left empty
    QListWidgetItem *item = newPathItem(QString());
    {
        QSignalBlocker blocker(this);
        addItem(item);
    }
    setCurrentItem(item);
    scrollToItem(item);
    editItem(item);
}

void DragToList::onItemChanged(QListWidgetItem *item)
{
    // an edit was committed, or the subfolders checkbox toggled
    QString p = normalizePath(item->text());
    if (p != item->text()) {
        QSignalBlocker blocker(this);
        item->setText(p);
    }
    markValidity(item);
    emit pathsChanged();
}

bool DragToList::removeEmptyAndDuplicates()
{
    bool removed = false;
    for (int i = count() - 1; i >= 0; i--) {
        // walk from the end so the FIRST occurrence of a path is the one kept
        QString p = item(i)->text();
        bool isDup = false;
        for (int j = 0; j < i; j++) {
            if (item(j)->text() == p) { isDup = true; break; }
        }
        if (p.isEmpty() || isDup) {
            delete takeItem(i);
            removed = true;
        }
    }
    return removed;
}

void DragToList::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    // after the editor is gone it is safe to remove rows (commit or escape)
    QListWidget::closeEditor(editor, hint);
    if (removeEmptyAndDuplicates()) emit pathsChanged();
}

void DragToList::keyPressEvent(QKeyEvent *event)
{
    // Delete / Backspace removes the selected paths (the editor, when open, gets keys first)
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        const QList<QListWidgetItem*> items = selectedItems();
        if (!items.isEmpty()) {
            qDeleteAll(items);
            emit pathsChanged();
        }
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}

void DragToList::mouseDoubleClickEvent(QMouseEvent *event)
{
    // double-click on a path edits it (edit trigger); on empty space appends a new one
    if (!itemAt(event->pos())) {
        appendNew();
        event->accept();
        return;
    }
    QListWidget::mouseDoubleClickEvent(event);
}

void DragToList::contextMenuEvent(QContextMenuEvent *event)
{
    QListWidgetItem *item = itemAt(event->pos());
    QMenu menu(this);
    QAction *addAction = menu.addAction("Add folder path");
    QAction *editAction = menu.addAction("Edit path");
    QAction *removeAction = menu.addAction("Remove path");
    menu.addSeparator();
    QAction *subAction = menu.addAction("Include subfolders");
    subAction->setCheckable(true);
    subAction->setChecked(item && item->checkState() == Qt::Checked);
    editAction->setEnabled(item);
    removeAction->setEnabled(item);
    subAction->setEnabled(item);
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == addAction) appendNew();
    else if (chosen == subAction) {
        // itemChanged -> onItemChanged -> pathsChanged
        item->setCheckState(subAction->isChecked() ? Qt::Checked : Qt::Unchecked);
    }
    else if (chosen == editAction) editItem(item);
    else if (chosen == removeAction) {
        delete item;
        emit pathsChanged();
    }
}

void DragToList::showEvent(QShowEvent *event)
{
/*
    These actions must be run after the dialog constructor is finished.
*/
    setStyleSheet(
        "color: yellow;"
        "background-image: url(:/images/dragfoldershere.png)"
        );
    event->accept();
}

void DragToList::dragEnterEvent(QDragEnterEvent *event)
{
/*
    Folders arrive from FSTree (InternalMove, so it proposes a MoveAction) or from the
    OS file manager. Always take them as a Copy: accepting a Move tells the source view
    to remove what was dragged.
*/
    if (event->mimeData()->hasUrls()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else event->ignore();
}

void DragToList::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else event->ignore();
}

void DragToList::dropEvent(QDropEvent *event)
{
    // qDebug() << "DragToList::dropEvent" << event;
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    const QList<QUrl> urls = event->mimeData()->urls();
    bool added = false;
    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        if (!QFileInfo(path).isDir()) continue;
        if (addPath(path)) added = true;    // skips folders already listed
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    if (added) emit pathsChanged();
}

/*******************************************************************************************

Purpose

    For the images in the folder(s) currently selected in Winnow, look for matching images
    in the target foldewr(s).  A typical use case is selecting the downloads folder in
    Winnow, and then set all recent folders in the photo library as the target, and then
    search for matches to see if you already have to download images in your library.

Comparison terms:

    - A or a refers to the items in dm->sf datamodel
    - B or b refers to the items in ui->cmpToFolders
    - cmpToFolders is the list of all folders containing B images
    - bList is the list of all B images
    - matches is a hash indexed on A with the matching images from B
    - delta is the average difference in pixels A vs B

*/

FindDuplicatesDlg::FindDuplicatesDlg(QWidget *parent, DataModel *dm) :
    QDialog(parent),
    ui(new Ui::FindDuplicatesDlg),
    dm(dm)
{
    /* Modeless, independent window so folders can be dragged in from FSTree while it
       is open. Parented to MW so it stays above it (a parentless window drops behind
       MW as soon as the drag starts). */
    setWindowFlag(Qt::Window);
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);

    metadata = new Metadata(this);
    frameDecoder = new FrameDecoder;
    pixmap = new Pixmap(this, dm, metadata);
    connect(frameDecoder, &FrameDecoder::frameImage, this, &FindDuplicatesDlg::setImageFromVideoFrame);
    // add disconnect in destructor...

    ui->setupUi(this);
    // Group-box title accents are dialog-specific: cyan for sections, white for
    // the A/B image compare boxes. Layered on top of G::css via the cascade.
    setStyleSheet(G::css +
        "QGroupBox#targetBox::title,"
        "QGroupBox#criteriaBox::title,"
        "QGroupBox#compareImagesBox::title,"
        "QGroupBox#candidateListBox::title { color: #6CC1E8; }"
        "QGroupBox#AImageBox::title,"
        "QGroupBox#BImageBox::title { color: white; }"
    );

    ui->helpBtn->setStyleSheet("background-color: " + G::helpColor.name() + ";");

    // candidate images tableview
    ui->tv->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tv->setEnabled(false);
    ui->tv->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->tv->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
            "border-color:black;"
            "border-width:0px;"
            "border:none;"
            //"text-align: left;"
            //"outline-color:red;"
        "}");
    ui->tv->verticalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "border:none;"
        "}");

    // Image comparison
    QImage prevIm(":/images/prev.png");
    QImage nextIm(":/images/next.png");
    ui->prevToolBtn->setIcon(QIcon(QPixmap::fromImage(prevIm.scaled(16,16))));
    ui->nextToolBtn->setIcon(QIcon(QPixmap::fromImage(nextIm.scaled(16,16))));
    ui->sameFileTypeCB->setChecked(true);
    ui->sameCreationDateCB->setChecked(true);
    ui->sameAspectCB->setChecked(false);
    ui->sameDurationCB->setChecked(true);
    // set enabled states
    on_samePixelsCB_clicked();

    // a changed target list makes any results stale (not mid-run: clear() empties bItems)
    connect(ui->includeSubfolders, &DragToList::pathsChanged, this, [this]() {
        if (!isRunning) clear();
    });
    connect(ui->excludeSubfolders, &DragToList::pathsChanged, this, [this]() {
        if (!isRunning) clear();
    });

    clear();

    setupModel();

    #ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif

    abort = false;
    isDebug = false;
}

FindDuplicatesDlg::~FindDuplicatesDlg()
{
    delete ui;
    if (frameDecoder) delete frameDecoder;
    // pixmap is parented to this dialog and deleted by Qt
}

int FindDuplicatesDlg::candidatesInDataModel() const
{
/*
    How many candidates are still in the datamodel. Winnow may have moved to another
    folder since the window opened (pressing a folder in FSTree to drag it loads it),
    in which case the duplicate flags have nowhere to go.
*/
    int n = 0;
    for (const A &item : aItems) {
        if (dm->proxyRowFromPath(item.path) >= 0) n++;
    }
    return n;
}

void FindDuplicatesDlg::updateApplyState()
{
/*
    Grey "Update duplicates" with the reason when none of the candidates are loaded in
    Winnow any more.
*/
    if (isRunning) return;
    bool ok = candidatesInDataModel() > 0;
    ui->updateDupsAndQuitBtn->setEnabled(ok);
    QString reason = "Candidate folder is no longer open in Winnow. "
                     "Reopen it to update duplicates.";
    ui->updateDupsAndQuitBtn->setToolTip(ok ? "" : reason);
    if (!ok) ui->progressLbl->setText(reason);
    else if (ui->progressLbl->text() == reason) ui->progressLbl->setText("");
}

void FindDuplicatesDlg::changeEvent(QEvent *event)
{
    // re-check on return to the window: the datamodel may have changed meanwhile
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        updateApplyState();
    }
    QDialog::changeEvent(event);
}

void FindDuplicatesDlg::closeEvent(QCloseEvent *event)
{
/*
    The search loops pump the event loop, so the window can be closed mid-run. Deleting
    it then would pull the dialog out from under the running loop: abort instead, and
    close when on_compareBtn_clicked unwinds.
*/
    if (isRunning) {
        abort = true;
        closePending = true;
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

bool FindDuplicatesDlg::candidatesHaveVideo() const
{
/*
    True if any candidate (selected image) is a video.
*/
    for (int a = 0; a < aItems.count(); a++) {
        if (aItems.at(a).video) return true;
    }
    return false;
}

void FindDuplicatesDlg::setupModel()
{
    /* Only selected images are candidates; if nothing is selected, fall back to all
       rows so the dialog still has something to work with. Snapshot everything the
       comparison needs (see struct A). */
    QList<int> sfRows;
    for (int sfRow = 0; sfRow < dm->sf->rowCount(); sfRow++) {
        if (dm->isSelected(sfRow)) sfRows << sfRow;
    }
    if (sfRows.isEmpty()) {
        for (int sfRow = 0; sfRow < dm->sf->rowCount(); sfRow++) sfRows << sfRow;
    }
    aItems.clear();
    for (int sfRow : std::as_const(sfRows)) {
        A item;
        item.path = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
        item.name = dm->sf->index(sfRow, G::NameColumn).data().toString();
        item.created = dm->sf->index(sfRow, G::CreatedColumn).data().toString();
        item.aspect = dm->sf->index(sfRow, G::AspectRatioColumn).data().toDouble();
        item.video = dm->sf->index(sfRow, G::VideoColumn).data().toBool();
        item.duration = dm->sf->index(sfRow, G::DurationColumn).data().toString();
        QIcon icon = dm->sf->index(sfRow, 0).data(Qt::DecorationRole).value<QIcon>();
        item.icon = icon.pixmap(icon.actualSize(QSize(256, 256))).toImage();
        aItems << item;
    }

    model.setRowCount(aItems.count());
    model.setColumnCount(5);
    // optional way to set header alignment
    QStandardItem *iconItem = new QStandardItem("Icon");
    iconItem->setData(Qt::AlignHCenter, Qt::TextAlignmentRole);
    // model.setHorizontalHeaderItem(0, dupItem);
    model.setHorizontalHeaderItem(0, new QStandardItem("Dup"));
    model.setHorizontalHeaderItem(1, new QStandardItem(" #"));
    model.setHorizontalHeaderItem(2, new QStandardItem("Delta"));
    model.setHorizontalHeaderItem(3, iconItem);
    // model.setHorizontalHeaderItem(3, new QStandardItem("Icon"));
    model.setHorizontalHeaderItem(4, new QStandardItem("  File Name"));

    // populate model
    for (int a = 0; a < aItems.count(); a++) {
        QVariant dupCount = 0;
        //model.setData(model.index(a,0), 0);
        // add checkbox
        model.itemFromIndex(model.index(a,0))->setCheckable(true);
        // add pixmap
        QPixmap pm = QPixmap::fromImage(aItems.at(a).icon).scaled(48, 48, Qt::KeepAspectRatio);
        model.setData(model.index(a,3), pm, Qt::DecorationRole);
        // add file name
        QString fName = aItems.at(a).name;
        model.setData(model.index(a,4), fName);
    }

    // format tableview
    ui->tv->setModel(&model);
    QFontMetrics fm(ui->tv->fontMetrics());
    int w0 = fm.boundingRect("Dup-").width();
    int w1 = fm.boundingRect("=99=").width();
    int w2 = fm.boundingRect("=Delta=").width();
    int w3 = 55;    // icon width = 48px
    ui->tv->setColumnWidth(0, w0);
    ui->tv->setColumnWidth(1, w1);
    ui->tv->setColumnWidth(2, w2);
    ui->tv->setColumnWidth(3, w3);
    ui->tv->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    // format

    isDebug = false;
}

void FindDuplicatesDlg::getPreview(QString fPath, QImage &image, QString source)
{
/*
    Get the preview image for display in the comparison view: either the candidate or
    target.  If it is a video, then get the first video frame.

    The getPreview and showPreview are separate functions because video previews are
    obtained from FrameDecoder, which does not return the QImage, rather it siganls when
    it has the first frame
*/
    QFileInfo fileInfo(fPath);
    ImageMetadata *m;
    int row = dm->proxyRowFromPath(fPath);
    metadata->loadImageMetadata(fileInfo, row, dm->instance, true, true, false, true, "FindDuplicatesDlg::preview");
    m = &metadata->m;

    if (m->video) {
        // first video frame arrives async via FrameDecoder → setImageFromVideoFrame
        frameDecoder->clear();
        frameDecoder->addToQueue(fPath, 0, source, -1, dm->instance);
    }
    else {
        frameDecoder->clear();
        bool colorManage = true;    // previews are shown to the user
        pixmap->loadIndependent(fPath, image, 0, source, colorManage);
    }
}

void FindDuplicatesDlg::showPreview(QString path, QImage image, QString source)
{
    /*
    Show the preview image for display in the comparison view: either the candidate or
    target.  If it is a video, then get the first video frame.

    The getPreview and showPreview are separate functions because video previews are
    obtained from FrameDecoder, which does not return the QImage, rather it siganls when
    it has the first frame
*/
    // previewLongSide = ui->candidateLbl->width();
    // if (image.width() > previewLongSide)
    //     previewSize = QSize(ui->candidateLbl->width(), ui->candidateLbl->height());
    // else
    //     previewSize = QSize(image.width(), image.width());

    if (source == "FindDupCandidate") {
        pA = QPixmap::fromImage(image);
        fit(pA, ui->candidateLbl);
        ui->candidateLbl->setToolTip(path);
    }
    if (source == "FindDupMatch") {
        pB = QPixmap::fromImage(image);
        fit(pB, ui->matchLbl);
        ui->matchLbl->setToolTip(path);
    }
}

void FindDuplicatesDlg::setImageFromVideoFrame(QString path, QImage image, QString source)
{
/*
    Thumbnails and previews for videos are obtained from FrameDecoder, which signals here
    with the first frame image.
*/
    int w = image.width();
    int h = image.height();
    // qDebug() << path << "We did it!" << w << h << image << source;
    if (source == "BItemThumbnail") {
        bool foundItem = false;
        int b;
        for (b = 0; b < bItems.size(); b++) {
            if (bItems.at(b).fPath == path) {
                foundItem = true;
                break;
            }
        }
        if (foundItem) {
            bItems[b].im = normalizeBThumb(image);
            // qDebug() << path << "We found it!" << bItems[b].im;
        }
        // one queued video frame has been delivered (found or not)
        if (pendingVideoFrames > 0) pendingVideoFrames--;
    }
    if (source == "FindDupCandidate") {
        showPreview(path, image, source);
    }
    if (source == "FindDupMatch") {
        showPreview(path, image, source);
    }
}

double FindDuplicatesDlg::compareImagesHues(QImage &imA, QImage &imB)
{
/*
    Not being used.  Alternative to comparing RGB pixels.
*/
    Effects effect;
    QVector<int> huesA(360, 0);
    QVector<int> huesB(360, 0);
    quint64 pixelsA = imA.width() * imA.height();
    quint64 pixelsB = imB.width() * imB.height();
    double deltaHue = 0;
    /*
    qDebug().noquote()
             << "FindDuplicatesDlg::visCmpImagesHues"
             << "A: w =" << imA.width() << "h =" << imA.height() << "pixelsA =" << pixelsA
             << "B: w =" << imB.width() << "h =" << imB.height() << "pixelsB =" << pixelsA
        ; //*/

    effect.hueCount(imA, huesA);
    effect.hueCount(imB, huesB);

    for (int hue = 0; hue < 360; hue++) {
        double diff = qAbs(huesA.at(hue) - huesB.at(hue));
        deltaHue += diff;
        /*
        qDebug().noquote()
                 << "FindDuplicatesDlg::visCmpImagesHues"
                 << QString::number(hue).leftJustified(4)
                 << "A =" << QString::number(huesA.at(hue)).leftJustified(4)
                 << "B =" << QString::number(huesB.at(hue)).leftJustified(4)
                 << "diff =" << diff
                 << "deltaHue =" << deltaHue
            ; //*/
    }

    // normalized deltaHue
    deltaHue = deltaHue / pixelsA * 100;
    /*
    qDebug().noquote()
        << "FindDuplicatesDlg::visCmpImagesHues"
        << "normalized deltaHue =" << deltaHue / pixelsA * 100
        ; //*/

    return deltaHue;
}

QImage FindDuplicatesDlg::normalizeBThumb(const QImage &im) const
{
/*
    Scale a thumbnail to the candidate decoration icon size (256px long side,
    aspect preserved) and convert to RGB32. Both the candidate (A) and target (B)
    thumbnails are run through this so they share dimensions for matching-aspect
    images, which is what makes the per-pixel compareRGB meaningful. 256 matches
    the decoration role extraction QSize(256, 256) used for the A image.
*/
    if (im.isNull()) return im;
    return im.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation)
             .convertToFormat(QImage::Format_RGB32);
}

int FindDuplicatesDlg::compareRGB(QImage &imA, QImage &imB)
{
/*
    Compare the RGB for each pixel in imA / imB.  The difference (deltaRGB) can be
    0 - 255.  All the deltaRGBs are summed and then devided by the number of deltaRGBs
    to arrive at an overall deltaRGB.

    If deltaRGB = 0 then there is a perfect match between the images.
*/
    int w = qMin(imA.width(),  imB.width());
    int h = qMin(imA.height(), imB.height());

    if (w == 0 || h == 0) {
        QString msg = "Image width and/or height = 0.";
        G::issue("Warning", msg, "FindDuplicatesDlg::compareRGB");
        return 255;
    }

    /* Read pixels straight from the scanlines. QImage::pixelColor() allocates a
       QColor and bounds-checks for every pixel; called over every candidate x
       target combination it was the main cause of the long main-thread stall.
       Convert once to a known 32-bit layout so the bytes can be read as QRgb.
       The numeric result is identical to the previous pixelColor() version. */
    const QImage a = imA.format() == QImage::Format_RGB32
                         ? imA : imA.convertToFormat(QImage::Format_RGB32);
    const QImage b = imB.format() == QImage::Format_RGB32
                         ? imB : imB.convertToFormat(QImage::Format_RGB32);

    quint64 deltaRGB = 0;
    for (int y = 0; y < h; y++) {
        const QRgb *lineA = reinterpret_cast<const QRgb*>(a.constScanLine(y));
        const QRgb *lineB = reinterpret_cast<const QRgb*>(b.constScanLine(y));
        for (int x = 0; x < w; x++) {
            QRgb p1 = lineA[x];
            QRgb p2 = lineB[x];
            deltaRGB += qAbs(qRed(p1)   - qRed(p2));
            deltaRGB += qAbs(qGreen(p1) - qGreen(p2));
            deltaRGB += qAbs(qBlue(p1)  - qBlue(p2));
        }
    }
    int deltaAverage = deltaRGB / (1.0 * w * h * 3 * 256) * 100;
    return deltaAverage;
}

void FindDuplicatesDlg::pixelCompare()
{
/*
    Iterate through all combinations of candidate images (imA) and target images (imB),
    comparing RGB to get a deltaPixels for each combination.

    If there is a match (dletaPixels < deltaThreshold) then add the imB to the
    matches hash.
*/
    initializeResultsVector();
    quint64 totIterations = aItems.count() * bItems.count();
    ui->progressLbl->setText("Searching for duplicates in " + QString::number(totIterations) + " combinations");
    // iterate filtered datamodel
    int counter = 0;
    int lastPct = -1;   // last progress percent painted (UI-update throttle)
    for (int a = 0; a < aItems.count(); a++) {
        QString aPath = aItems.at(a).path;
        QString aFName = aItems.at(a).name;

        // candidate thumbnail, normalized to the same size/format as the targets
        QImage imA = normalizeBThumb(aItems.at(a).icon);

        // compare candidate to each thumbnail in bList
        for (int b = 0; b < bItems.size(); b++) {
            if (abort) {
                ui->progressLbl->setText("Search aborted");
                clear();
                return;
            }
            // do not report the candidate as a duplicate of itself
            if (sameFilePath(a, b)) {
                results[a][b].match = false;
                counter++;
                continue;
            }

            // getMetadataBItems has loaded thumbnails into bItems
            QImage imB = bItems.at(b).im;
            QString bPath = bItems.at(b).fPath;
            int deltaPixels;

            // compare every pixel
            if (imB.width() > 0 && imB.height() > 0) {
                deltaPixels = compareRGB(imA, imB);
            }
            else {
                deltaPixels = 255;  // worst delta possible;
            }

            // save for reporting / debugging
            results[a][b].deltaPixels = deltaPixels;
            results[a][b].match = false;    // default

            if (isDebug)
            {
            qDebug() << "FindDuplicatesDlg::pixelCompare"
                     << "a =" << a
                     << "b =" << b
                     << "deltaPixels =" << deltaPixels
                     << "ui->deltaThreshold->value() =" << ui->deltaThreshold->value()
                     << bItems.at(b).fPath
                ;
            }

            /*
            // preview (for debugging)
            // candidate image
            getPreview(aPath, imA, "FindDupCandidate");
            showPreview(imA, aPath, "FindDupCandidate");
            // best match image
            getPreview(bPath, imB, "FindDupMatch");
            showPreview(imB, bPath, "FindDupMatch");
            */

            if (deltaPixels <= ui->deltaThreshold->value()) {
                qDebug() << "FindDuplicatesDlg::pixelCompare"
                         << "a =" << a
                         << "b =" << b
                         << "deltaPixels =" << deltaPixels
                         << "ui->deltaThreshold->value() =" << ui->deltaThreshold->value()
                         << bItems.at(b).fPath
                    ;
                Matches mItem;
                mItem.deltaPixels = deltaPixels;
                mItem.path = bItems.at(b).fPath;
                matches[a] << mItem;
                results[a][b].match = true;
            }

            counter++;
            /* Throttle UI updates to once per percent so the dialog stays
               responsive (progress bar + abort button) without flooding the
               event loop. Pumped unconditionally: G::useProcessEvents is globally
               false, which is why the comparison froze the GUI (beachball). */
            int pct = totIterations ? 1.0 * counter / totIterations * 100 : 0;
            if (pct != lastPct) {
                ui->progressBar->setValue(pct);
                qApp->processEvents();
                lastPct = pct;
            }
        }
    }

    // sort deltas
    struct
    {
        bool operator()(Matches a, Matches b) const {return a.deltaPixels < b.deltaPixels;}
    }
    lessDelta;
    for(auto& vec : matches) {
        std::sort(vec.begin(), vec.end(), lessDelta);
    }

    // update A list candidate list (tableview tv in dialog using model for indexes)
    for (int a = 0; a < model.rowCount(); a++) {
        // are there any duplicates in target images
        if (matches[a].count() == 0) continue;
        // update model
        QModelIndex idxDelta = model.index(a, MC::Delta);
        int deltaPixels = matches[a].at(0).deltaPixels;
        if (deltaPixels <= ui->deltaThreshold->value()) {
            model.setData(idxDelta, deltaPixels);
            QModelIndex idxChkBox = model.index(a, MC::CheckBox);
            model.itemFromIndex(idxChkBox)->setCheckState(Qt::Checked);
            model.setData(model.index(a, MC::Count), matches[a].count());
        }
    }
}

QString FindDuplicatesDlg::currentMatchString(int a, int b)
{
/*
    Reports ie "3 of 7 matches" under the target image preview.
                b of tot

    matches[a].at(b)
        a = match index = candidate row selected
        b = nth match for candidate a
*/
    if (!matches.contains(a)) {
        return "No matches";
    }
    QString tot = QString::number(matches[a].count());
    return QString::number(b+1) + " of " + tot + " matches";
}

void FindDuplicatesDlg::clear()
{
    abort = false;
    frameDecoder->clear();
    pendingVideoFrames = 0;
    ui->progressBar->setValue(0);
    matches.clear();
    results.clear();
    bItems.clear();
    for (int i = 0; i < model.rowCount(); i++) {
        model.itemFromIndex(model.index(i, MC::CheckBox))->setCheckState(Qt::Unchecked);
        model.setData(model.index(i, MC::Count), 0);
        model.setData(model.index(i, MC::Delta), "");
    }
    ui->prevToolBtn->setVisible(false);
    ui->nextToolBtn->setVisible(false);
    ui->deltaTxt->setVisible(false);
    ui->deltaLbl->setText("");
    ui->currentLbl->setText("");
    ui->currentLbl->setVisible(false);
    ui->candidateFilenameLbl->setText("");
    ui->candidateFilenameLbl->setVisible(false);
    ui->candidateLbl->setPixmap(QPixmap());
    ui->targetPathLbl->setText("");
    ui->targetPathLbl->setVisible(false);
    ui->matchLbl->setPixmap(QPixmap());
    ui->progressLbl->setText("");    //ui->currentLbl->setText("");
    ui->tv->setEnabled(false);
}

void FindDuplicatesDlg::showImageComparisonStuff(int a, int b, QString bPath)
{
    if (!matches.contains(a)) return;
    if (ui->samePixelsCB->isChecked()) {
        ui->deltaLbl->setVisible(true);
        ui->deltaTxt->setVisible(true);
        ui->deltaLbl->setText(QString::number(matches[a].at(0).deltaPixels));
    }
    else {
        ui->deltaLbl->setVisible(false);
        ui->deltaTxt->setVisible(false);
    }
    ui->prevToolBtn->setVisible(true);
    ui->nextToolBtn->setVisible(true);
    ui->currentLbl->setVisible(true);
    ui->currentLbl->setText(currentMatchString(a, b));
    ui->currentLbl->setToolTip(currentMatchString(a, b));
    ui->targetPathLbl->setVisible(true);
    ui->targetPathLbl->setText(bPath);
    ui->targetPathLbl->setToolTip(bPath);
    ui->candidateLbl->setText("");
    ui->matchLbl->setText("");
}

void FindDuplicatesDlg::initializeResultsVector()
{
/*
    The results vector is used to save all the comparisons between the A candidate
    images and the B target images for reporting / debugging. It must be reset before
    a search for matches is run.
*/
    results.clear();
    results.resize(aItems.count());
    for (auto& vec : results) {
        vec.resize(bItems.count());
    }
}

void FindDuplicatesDlg::on_clrFoldersBtn_clicked()
{
/*
    Removes any include and exclude folders from the target folder lists.
*/
    ui->includeSubfolders->clear();
    ui->excludeSubfolders->clear();
    clear();
}

QStringList FindDuplicatesDlg::chooseFolders(const QString &title)
{
/*
    Opens a folder chooser that allows the user to select one or more folders. The
    native macOS and Windows directory pickers only permit a single selection, so the
    Qt dialog is used with DontUseNativeDialog and multi-selection enabled on its
    internal item views.
*/
    QFileDialog dlg(this, title);
    dlg.setFileMode(QFileDialog::Directory);
    dlg.setOption(QFileDialog::ShowDirsOnly, true);
    dlg.setOption(QFileDialog::DontResolveSymlinks, true);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);

    /* enable multiple folder selection on the dialog's internal views */
    if (QListView *listView = dlg.findChild<QListView*>("listView")) {
        listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    if (QTreeView *treeView = dlg.findChild<QTreeView*>("treeView")) {
        treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    QStringList folders;
    if (dlg.exec() == QDialog::Accepted) {
        folders = dlg.selectedFiles();
    }
    return folders;
}

void FindDuplicatesDlg::addFolders(DragToList *list, const QString &title)
{
/*
    Prompts the user to select one or more folders and appends any that are not already
    present to the supplied target folder list (include or exclude).
*/
    QStringList folders = chooseFolders(title);
    if (folders.isEmpty()) return;
    foreach (const QString &folder, folders) {
        list->addPath(folder);      // skips folders already listed
    }
    clear();
}

void FindDuplicatesDlg::on_addIncludeBtn_clicked()
{
/*
    Adds one or more folders to the list of target folders to search for duplicates.
*/
    addFolders(ui->includeSubfolders, tr("Select folder(s) to include"));
}

void FindDuplicatesDlg::on_addExcludeBtn_clicked()
{
/*
    Adds one or more folders to the list of folders to exclude from the duplicate search.
*/
    addFolders(ui->excludeSubfolders, tr("Select folder(s) to exclude"));
}

void FindDuplicatesDlg::getMetadataBItems()
{
/*
    The B list bItems (target folder(s) images) of images is used to look for duplicates
    of the candidate A list images in the datamodel. The list is built in buildBList().

    The list bItems contains all the information required for comparison:
        - file name
        - type
        - createdDate
        - aspect
        - duration
        - im (QImage thumbnail for pixel comparison)

    This function populates bItems.
*/
    int totIterations = bItems.count();

    // populate bItems;
    QString s = "Reading metadata for " + QString::number(totIterations) + " source files";
    ui->progressLbl->setText(s);
    int counter = 0;
    for (int b = 0; b < bItems.count(); b++ ) {
        if (abort) {
            ui->progressLbl->setText("Search aborted");
            clear();
            return;
        }

        counter++;
        QString s = "Reading target images: " + QString::number(counter) + " of "
                    + QString::number(totIterations);
        int pctProgress = 1.0 * counter / totIterations * 100;
        ui->progressLbl->setText(s);
        ui->progressBar->setValue(pctProgress);
        /* Pump the event loop so the dialog stays responsive while reading target
           metadata. G::useProcessEvents is globally false, so this was a no-op and
           the GUI froze (beachball). */
        qApp->processEvents();

        QString fPath = bItems.at(b).fPath;

        // get the thumbnail (used to compare to A thumbnail in datamodel)
        QImage image;
        if (ui->samePixelsCB->isChecked()) {
            QString ext = QFileInfo(fPath).suffix().toLower();
            if (metadata->videoFormats.contains(ext)) {
                // first video frame arrives async via FrameDecoder → setImageFromVideoFrame,
                // which stores it into bItems[b].im. Track it so we can wait for
                // all frames before comparing.
                frameDecoder->addToQueue(fPath, G::maxIconSize, "BItemThumbnail", -1, dm->instance);
                pendingVideoFrames++;
            }
            else {
                pixmap->loadIndependent(fPath, image, G::maxIconSize, "BItemThumbnail");
                bItems[b].im = normalizeBThumb(image);
            }
        }

        // get metadata info for the B file to calc aspect
        QFileInfo fInfo(fPath);
        bool loadMeta = true;
        int row = dm->proxyRowFromPath(fPath);

        if (!metadata->loadImageMetadata(fInfo, row, dm->instance, true, true, false, true, "FindDuplicatesDlg::buildBItemsList")) {
            loadMeta = false;
        }
        ImageMetadata *m = &metadata->m;

        // file name
        bItems[b].name = QFileInfo(fPath).fileName().toLower();

        // type
        bItems[b].type = QFileInfo(fPath).suffix().toLower();


        if (bItems[b].type == "heic") {
            int x = 0;
        }

        // create date
        /*
        qDebug() << "FindDuplicatesDlg::getMetadataBItems"
                 << "b =" << b
                 << "m->createdDate =" << m->createdDate;
        */
        if (m->createdDate.isValid()) {
            bItems[b].createdDate = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");
        }
        else {
            bItems[b].createdDate = fInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        }

        // duration
        if (ui->sameDurationCB->isChecked()) {
            if (metadata->videoFormats.contains(bItems.at(b).type)) {
                QString s = metadata->readExifToolTag(fPath, "duration#");
                quint64 duration = static_cast<quint64>(s.toDouble());
                //duration /= 1000;
                QTime durationTime((duration / 3600) % 60, (duration / 60) % 60,
                                   duration % 60, (duration * 1000) % 1000);
                QString format = "mm:ss";
                if (duration > 3600) format = "hh:mm:ss";
                bItems[b].duration = durationTime.toString(format);
           }
           else bItems[b].duration = "00:00";
        }

        if (ui->sameAspectCB->isChecked()) {
            double aspect;
            if (m->width && m->height) {
                if (m->orientation == 6 || m->orientation == 8) aspect = m->height * 1.0 / m->width;
                else aspect = m->width * 1.0 / m->height;
            }
            else {
                if (!image.isNull()) aspect = image.width() * 1.0 / image.height();
                else aspect = 0;
            }
            bItems[b].aspect = QString::number(aspect,'f', 2);
        }
    }

    /* Video thumbnails are delivered asynchronously by FrameDecoder, so wait for
       the queued first frames before returning (otherwise pixelCompare runs with
       null target images and videos never match). FrameDecoder delivers via
       queued signals, so pump the event loop; a timeout guards against a decode
       that never completes. */
    if (pendingVideoFrames > 0) {
        ui->progressLbl->setText("Decoding video frames for " +
                                 QString::number(pendingVideoFrames) + " videos");
        QElapsedTimer timer;
        timer.start();
        while (pendingVideoFrames > 0 && !abort && timer.elapsed() < 30000) {
            qApp->processEvents(QEventLoop::AllEvents, 50);
        }
    }
}

void FindDuplicatesDlg::buildBList()
{
/*
    Build a B list bItems (target folder(s) images) of images to look for duplicates of
    the candidate A list images in the datamodel.

    The list bItems contains all the information required for comparison:
        - type
        - createdDate
        - aspect
        - duration
        - im (QImage thumbnail for pixel comparison)

    This function iterates through all the images defined by the include/exclude folders
    and appends a bItem for each image.  The bItem fields are populated in
    getMetadataBItems().
*/
    // build include folders list bFolderPaths
    ui->progressLbl->setText("Building list of image folders");
    QStringList bFolderPaths;
    for (int cF = 0; cF < ui->includeSubfolders->count(); cF++) {
        QString root = ui->includeSubfolders->item(cF)->text();
        if (!root.isEmpty() && root[root.length()-1] == '/') {
            root.chop(1);
        }
        if (!QFileInfo(root).isDir()) continue;     // typed path that does not exist
        bFolderPaths << root;
        if (ui->includeSubfolders->includesSubfolders(cF)) {
            QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                bFolderPaths << it.filePath();
            }
        }
    }
    /*
    qDebug() << "bFolderPaths list:";
    foreach (QString s, bFolderPaths) qDebug() << "" << s;
    //*/

    // build exclude folders list bExcludeFolderPaths
    QStringList bExcludeFolderPaths;
    for (int cF = 0; cF < ui->excludeSubfolders->count(); cF++) {
        QString root = ui->excludeSubfolders->item(cF)->text();
        if (!root.isEmpty() && root[root.length()-1] == '/') {
            root.chop(1);
        }
        bExcludeFolderPaths << root;
        if (ui->excludeSubfolders->includesSubfolders(cF)) {
            QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                bExcludeFolderPaths << it.filePath();
            }
        }
    }
    if (isDebug) {
        qDebug() << "bExcludeFolderPaths list:";
        foreach (QString s, bExcludeFolderPaths) qDebug() << "" << s;
    }

    // remove exclude folders from bFolderPaths
    foreach (const QString &s, bExcludeFolderPaths) {
        bFolderPaths.removeAll(s);
    }
    if (isDebug) {
        qDebug() << "bFolderPaths list after exclusion:";
        foreach (QString s, bFolderPaths) qDebug() << "" << s;
    }

    // populate bItems fPath
    QDir *dir = new QDir;
    QStringList *fileFilters = new QStringList;
    foreach (const QString &str, metadata->supportedFormats) {
        fileFilters->append("*." + str);
    }
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    /* If no candidate is a video, exclude videos from the targets too. */
    bool includeVideos = candidatesHaveVideo();

    // build list of image files in bItems
    QStringList bFiles;
    foreach (QString dPath, bFolderPaths) {
        dir->setPath(dPath);
        const QFileInfoList entries = dir->entryInfoList();
        for (int i = 0; i < entries.size(); i++) {
            if (!includeVideos) {
                QString ext = entries.at(i).suffix().toLower();
                if (metadata->videoFormats.contains(ext)) continue;
            }
            B bItem;
            bItem.fPath = entries.at(i).filePath();
            //qDebug() << bItem.fPath;
            bItems << bItem;
        }
    }
}

bool FindDuplicatesDlg::sameFilePath(int a, int b)
{
/*
    Compare candidate / target image file path and return result.  This is used
    to prevent including the candidate file as a duplicate result.
*/
    QString pathA = aItems.at(a).path.toLower();
    QString pathB = bItems.at(b).fPath.toLower();
    bool isSame = (pathA == pathB);
    if (isDebug)
        qDebug() << "FilePath     a =" << a << pathA << "b =" << b << pathB << "isSame" << isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameFileName(int a, int b)
{
    /*
    Compare candidate / target image file name and return result
*/
    QString nameA = aItems.at(a).name.toLower();
    QString nameB = bItems.at(b).name;
    bool isSame = (nameA == nameB);
    if (isDebug)
        qDebug() << "FileName     a =" << a << nameA << "b =" << b << nameB << "isSame" << isSame;
    results[a][b].sameName = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameFileType(int a, int b)
{
/*
    Compare candidate / target image file type and return result
*/
    QString pathA = aItems.at(a).path;
    QString extA = QFileInfo(pathA).suffix().toLower();
    QString pathB = bItems.at(b).fPath;
    QString extB = QFileInfo(pathB).suffix().toLower();
    bool isSame = (extA == extB);
    if (isDebug)
        qDebug() << "FileType     a =" << a << extA << "b =" << b << extB << "isSame" << isSame;
    results[a][b].sameType = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameCreationDate(int a, int b)
{
/*
    Compare candidate / target image creation date and return result
*/
    QString dateA = aItems.at(a).created;
    QString dateB = bItems.at(b).createdDate;
    //QString dateB = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");
    bool isSame = (dateA == dateB);
    if (isDebug)
    qDebug() << "CreationDate a =" << a << dateA << "b =" << b << dateB << "isSame" << isSame;
    results[a][b].sameCreationDate = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameAspect(int a, int b)
{
/*
    Compare candidate / target image aspect and return result
*/
    // A datamodel
    double aspect = aItems.at(a).aspect;
    QString aspectA = QString::number(aspect,'f', 2);
    // B target
    QString aspectB = bItems.at(b).aspect;
    bool isSame = (aspectA == aspectB);
    if (isDebug)
    qDebug() << "sameAspect   a =" << a << aspectA << "b =" << b << aspectB << "isSame" << isSame;
    results[a][b].sameAspect = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameDuration(int a, int b)
{
/*
    Compare candidate / target image duration and return result
    Only relevent for videos.
*/
    // A datamodel
    bool isSame;
    QString durationA;
    QString durationB;
    bool isVideo = aItems.at(a).video;
    if (isVideo) {
        durationA = aItems.at(a).duration;
        if (durationA.length() == 0) durationA = "00:00";
        // B collection
        durationB = bItems.at(b).duration;
        isSame = (durationA == durationB);
    }
    else {
        durationA = "00:00";
        durationB = "00:00";
        isSame = true;
    }
    if (isDebug)
        qDebug() << "sameDuration   a =" << a << durationA << "b =" << b << durationB << "isSame" << isSame;
    results[a][b].sameDuration = isSame;
    return isSame;
}

void FindDuplicatesDlg::findMatches()
{
/*
    Iterates all candidate/target combinations, comparing the criteria that is checked.
    If they all match then the model item is checked then:
        - the item path is appended to the matches hash
        - results match set to true
    Note that pixel comparison is not done here.  See pixelCompare().
*/
    initializeResultsVector();
    int aCount =  aItems.count();
    int bCount =  bItems.count();
    for (int a = 0; a < aCount; a++) {
        matchCount = 0;
        int pctProgress = 1.0 * (a+1) / aCount * 100;
        ui->progressBar->setValue(pctProgress);
        /* Pump the event loop (per candidate) to keep the dialog responsive;
           G::useProcessEvents is globally false. */
        qApp->processEvents();
        for (int b = 0; b < bCount; b++) {
            QString s = "a = " + QString::number(a+1) + " of " + QString::number(aCount) + "   " +
                        "b = " + QString::number(b+1) + " of " + QString::number(bCount);
            ui->progressLbl->setText(s);
            results[a][b].match = false;

            // same file path = candidate image file = compare to itself
            if (sameFilePath(a, b)) continue;

            // same file name
            if (ui->sameFileNameCB->isChecked()) {
                if (!sameFileName(a, b)) continue;
            }
            // same file type
            if (ui->sameFileTypeCB->isChecked()) {
                if (!sameFileType(a, b)) continue;
            }
            // same creation date
            if (ui->sameCreationDateCB->isChecked()) {
                if (!sameCreationDate(a, b)) continue;
            }
            // same aspect
            if (ui->sameAspectCB->isChecked()) {
                if (!sameAspect(a, b)) continue;
            }
            // same duration (video)
            if (ui->sameDurationCB->isChecked()) {
                if (!sameDuration(a, b)) continue;
            }
            // item match
            Matches mItem;
            mItem.path = bItems.at(b).fPath;
            matches[a] << mItem;
            model.itemFromIndex(model.index(a, MC::CheckBox))->setCheckState(Qt::Checked);
            matchCount++;
            results[a][b].match = true;
            if (isDebug) reportFindMatch(a, b);
            /*
            qDebug() //<< "FindDuplicatesDlg::findMatches"
                     << QString::number(a).leftJustified(5)
                     << bItems.at(b).fPath
                ; //*/
            //break;
        }
        model.setData(model.index(a,MC::Count), matchCount);
    }
}

void FindDuplicatesDlg::buildResults()
{
/*
    Used for reporting / debugging
    The results vector matrix: results[a][b] is an R item.
        a = index for datamodel items (candidates)
        b = index for bItems (targets)

        R parameters:
            bool sameType;
            bool sameCreationDate;
            bool sameAspect;
            bool sameDuration;
            double deltaPixels;
            bool match;

    Examples:   Is datamodel item a type the same as bItems type
                    results[a][b].sameType
                The deltaPixels match for image a / b
                    results[a][b].deltaPixels
*/
    // initialize result vector [A indexs][BItems]
    initializeResultsVector();

    // compare criteria
    if (isDebug)
    qDebug() << "\nFindDuplicatesDlg::buildResults\n";

    for (int a = 0; a < aItems.count(); a++) {
        for (int b = 0; b < bItems.count(); b++) {
            if (isDebug)
            qDebug() << "FindDuplicatesDlg::buildResults  A ="
                     <<  aItems.at(a).name
                     <<  "B =" << bItems.at(b).fPath
                ;
            // same file name
            if (ui->sameFileNameCB->isChecked()) {
                results[a][b].sameName = sameFileName(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameName" << results[a][b].sameName;
            }

            // same file type
            if (ui->sameFileTypeCB->isChecked()) {
                results[a][b].sameType = sameFileType(a, b);
                if (isDebug)
                    qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameType" << results[a][b].sameType;
            }

            // same creation date
            if (ui->sameCreationDateCB->isChecked()) {
                results[a][b].sameCreationDate = sameCreationDate(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameCreationDate" << results[a][b].sameCreationDate;
            }

            // same aspect
            if (ui->sameAspectCB->isChecked()) {
                results[a][b].sameAspect = sameAspect(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameAspect" << results[a][b].sameAspect;
            }
            // same duration (video)
            if (ui->sameDurationCB->isChecked()) {
                results[a][b].sameDuration = sameDuration(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameDuration" << results[a][b].sameDuration;
            }
            if (isDebug)
            qDebug() << "\n";
        }
    }
}

void::FindDuplicatesDlg::reportResults()
{
/*
    The dialog report button executes this function, listing all the candicate/target
    matches, with the comparison results.  If alt/option pressed then all A/B
    combinations are reported, up to 100,000.
*/
    Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
    bool isModifier = false;
    if (modifiers & Qt::AltModifier) isModifier = true;
    // check for too many combinations
    if (isModifier) {
        quint32 tot = aItems.count() * bItems.count();
        QLocale locale(QLocale::English, QLocale::UnitedStates);
        // Format the number using the locale-specific rules
        //QString formattedNumber = locale.toString(tot);
        if (tot > 100000) {
            QString msg = "The report length (candidates * targets) = " +
                          locale.toString(tot) +
                          ". This exceeds the maximum allowed (100,000).";
            G::popup->showPopup(msg, 4000);
            return;
        }
    }
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    QString s = " ";
    int aDigits = QString::number(aItems.count()).length() + 1;
    int bDigits = QString::number(bItems.count()).length() + 1;
    // longest A filename string length
    int maxFileNameLenA = 0;
    for (int a = 0; a < aItems.count(); a++) {
        QString pathA = aItems.at(a).name;
        if (pathA.length() > maxFileNameLenA) maxFileNameLenA = pathA.length();
    }
    // longest B path string length
    int pathLenB = 0;
    for (int b = 0; b < bItems.count(); b++) {
        QString pathB = bItems.at(b).fPath;
        if (pathB.length() > pathLenB) pathLenB = pathB.length();
    }
    // report each combination
    for (int a = 0; a < aItems.count(); a++) {
        QString fileNameA = aItems.at(a).name.leftJustified(maxFileNameLenA);
        QString pathA = aItems.at(a).path;
        QString typeA = QFileInfo(pathA).suffix().toLower();
        QString dateA = aItems.at(a).created;
        QString aspectA = QString::number(aItems.at(a).aspect,'f', 2);
        QString durationA = aItems.at(a).duration;
        if (durationA.length() == 0) durationA = "00:00";
        for (int b = 0; b < bItems.count(); b++) {
            // show only matches
            if (!isModifier && !results[a][b].match) continue;
            // show all
            rpt << "  a " + QString::number(a).leftJustified(aDigits);
            rpt << "b " + QString::number(b).leftJustified(bDigits);
            QString match = results[a][b].match ? "true" : "false";
            rpt << "  Match " + match.leftJustified(5);
            if (ui->samePixelsCB->isChecked()) {
                QString delta = QString::number(results[a][b].deltaPixels).leftJustified(5);
                rpt << "    SAME PIXELS   Delta " << delta;
            }
            else {
                if (ui->sameFileNameCB->isChecked()) {
                    QString sameType = results[a][b].sameName ? "true" : "false";
                    QString typeB = bItems.at(b).name;
                    rpt << "    NAME " + sameType.leftJustified(25) + (typeA + s + typeB).leftJustified(25);
                }
                if (ui->sameFileTypeCB->isChecked()) {
                    QString sameType = results[a][b].sameType ? "true" : "false";
                    QString typeB = bItems.at(b).type;
                    rpt << "    TYPE " + sameType.leftJustified(6) + (typeA + s + typeB).leftJustified(9);
                }
                if (ui->sameCreationDateCB->isChecked()) {
                    QString sameCreationDate = results[a][b].sameCreationDate ? "true" : "false";
                    QString dateB = bItems.at(b).createdDate.leftJustified(23);     // might be blank date
                    rpt << "  DATE " + sameCreationDate.leftJustified(6) + dateA + s + dateB;
                }
                if (ui->sameAspectCB->isChecked()) {
                    QString sameAspect = results[a][b].sameAspect ? "true" : "false";
                    QString aspectB = bItems.at(b).aspect;
                    rpt << "    ASPECT "  + sameAspect.leftJustified(6) + (aspectA + s + aspectB).leftJustified(7);
                }
                if (ui->sameDurationCB->isChecked()) {
                    QString sameDuration = results[a][b].sameDuration ? "true" : "false";
                    QString durationB = bItems.at(b).duration;
                    rpt << "    DURATION "  + sameDuration.leftJustified(6) + (durationA + s + durationB).leftJustified(7);
                }
            }
            rpt << "    A: " + fileNameA;
            // rpt << "    A: " + fileNameA.leftJustified(10);
            rpt << "   B: " + bItems.at(b).fPath;
            rpt << "   B: " + bItems.at(b).name;
            rpt << "\n";
        }
    }
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    md.textBrowser->setStyleSheet(G::css);
    QFont courier("Courier", 12);
    md.textBrowser->setFont(courier);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    QFontMetrics metrics(md.textBrowser->font());
    md.textBrowser->setTabStopDistance(3 * metrics.horizontalAdvance(' '));
    // get width in pixels of the longest line of text in report
    QString text = md.textBrowser->toPlainText();
    QStringList lines = text.split("\n");
    int longestLinePixelWidth = 0;
    for (const QString& line : lines) {
        int lineWidth = metrics.horizontalAdvance(line);
        longestLinePixelWidth = qMax(longestLinePixelWidth, lineWidth);
    }
    longestLinePixelWidth += 50;    // add dialog borders
    int widthToUse = longestLinePixelWidth < G::displayVirtualHorizontalPixels
                         ? G::displayVirtualHorizontalPixels : longestLinePixelWidth;
    qDebug() << "longestLinePixelWidth =" << longestLinePixelWidth
             << "G::displayVirtualHorizontalPixels =" << G::displayVirtualHorizontalPixels;
    dlg->resize(widthToUse, dlg->height());
    dlg->exec();
}


void FindDuplicatesDlg::on_samePixelsCB_clicked()
{
/*
    When selecting criteria for a match search, clicking on "Same Pixels" toggles
    all the other criteria.  Comparisons can only be executed for pixel matches
    or metadata matches.
*/
    // qDebug() << "FindDuplicatesDlg::on_samePixels_clicked";
    bool isPix = ui->samePixelsCB->isChecked();
    ui->sameFileNameCB->setEnabled(!isPix);
    ui->sameFileTypeCB->setEnabled(!isPix);
    ui->sameCreationDateCB->setEnabled(!isPix);
    ui->sameAspectCB->setEnabled(!isPix);
    ui->sameDurationCB->setEnabled(!isPix);
    ui->deltaThresholdLbl->setEnabled(isPix);
    ui->deltaThreshold->setEnabled(isPix);
    ui->deltaThresholdToolBtn->setEnabled(isPix);
    clear();
}

void FindDuplicatesDlg::on_compareBtn_clicked()
{
/*
    Clicking on "Find Duplicates" searches for any matches between the candidate
    images in the target images.

    - the bItems, matches and results data is cleared
    - the candidates table dup column is unchecked
    - the bItems list is built and populated
    - the checked criteria is used to compare candidates to targets
    - the candidates table is updated
*/
    // check target folder(s) assigned
    if (ui->includeSubfolders->count() == 0) {
        QString msg = "Comparison requires target folder(s)";
        G::popup->showPopup(msg, 3000);
        return;
    }

    /* Guard against re-entry: the compare loops pump the event loop, so without
       this a second click (or other UI action) could start a nested run. */
    if (isRunning) return;
    isRunning = true;
    ui->compareBtn->setEnabled(false);
    ui->updateDupsAndQuitBtn->setEnabled(false);
    clear();
    buildBList();
    if (bItems.size() == 0) {
        QString msg = "There are no images in the target folder(s).<br>"
                      "Include subfolders or use another folder."
            ;
        QMessageBox::warning(this, tr("Empty Folder(s)"), msg);
        isRunning = false;
        ui->compareBtn->setEnabled(true);
        updateApplyState();
        return;
    }
    getMetadataBItems();
    if (ui->samePixelsCB->isChecked()) {
        pixelCompare();
    }
    else {
        findMatches();
    }

    isRunning = false;
    // window closed mid-search (see closeEvent)
    if (closePending) {
        close();
        return;
    }

    // candidates with duplicates
    int candidatesWithDups = 0;
    for (int a = 0; a < model.rowCount(); a++) {
        if (model.itemFromIndex(model.index(a, MC::CheckBox))->checkState() == Qt::Checked)
            candidatesWithDups++;
    }
    QString x = QString::number(candidatesWithDups);
    if (candidatesWithDups == 1)
        ui->progressLbl->setText(x + " candidate is duplicated. See candidate images table.");
    else
        ui->progressLbl->setText(x + " candidates are duplicated. See candidate images table.");
    ui->progressBar->setValue(0);

    // enable candidate table
    ui->tv->setEnabled(true);
    ui->compareBtn->setEnabled(true);
    updateApplyState();
}

void FindDuplicatesDlg::on_prevToolBtn_clicked()
{
/*
    If there are multiple matches, the previous matched target image is displayed in
    the Compare Images section.
*/
    int a = ui->tv->currentIndex().row();
    if (currentMatch > 0) {
        QPixmap pmNull;
        ui->matchLbl->setPixmap(pmNull);
        ui->matchLbl->setText("Loading image...");
        ui->matchLbl->repaint();
        currentMatch--;
        int a = ui->tv->currentIndex().row();
        int b = currentMatch;
        QString bPath = matches[a].at(b).path;
        if (isDebug)
        {
        qDebug() << "FindDuplicatesDlg::on_prevToolBtn_clicked currentMatch =" << currentMatch << "a =" << a << "b =" << b;
        }
        QImage image;
        getPreview(bPath, image, "FindDupMatch");
        showPreview(bPath, image, "FindDupMatch");
        showImageComparisonStuff(a, b, bPath);
        // if (ui->samePixelsCB->isChecked()) {
        //     ui->deltaLbl->setText(QString::number(matches[a].at(b).deltaPixels));
        // }
        // ui->currentLbl->setText(currentMatchString(a, b));
        // ui->targetPathLbl->setVisible(true);
        // ui->targetPathLbl->setText(bPath);
        // ui->targetPathLbl->setToolTip(bPath);
    }
    else {
        G::popup->showPopup("Start of match images");
    }
}

void FindDuplicatesDlg::on_nextToolBtn_clicked()
{
/*
    If there are multiple matches, the next matched target image is displayed in
    the Compare Images section.
*/
    int a = ui->tv->currentIndex().row();
    if (currentMatch + 1 < matches[a].count()) {
        QPixmap pmNull;
        ui->matchLbl->setPixmap(pmNull);
        ui->matchLbl->setText("Loading image...");
        ui->matchLbl->repaint();
        currentMatch++;
        int b = currentMatch;
        QString bPath = matches[a].at(b).path;
        // if (isDebug)
        {
        qDebug() << "FindDuplicatesDlg::on_nextToolBtn_clicked"
                 << "a =" << a
                 << "b =" << b
                 << "bPath =" << bPath;
        }
        QImage image;
        getPreview(bPath, image, "FindDupMatch");
        showPreview(bPath, image, "FindDupMatch");
        showImageComparisonStuff(a, b, bPath);
        // if (ui->samePixelsCB->isChecked()) {
        //     ui->deltaLbl->setText(QString::number(matches[a].at(b).deltaPixels));
        // }
        // ui->currentLbl->setText(currentMatchString(a, b));
        // ui->targetPathLbl->setVisible(true);
        // ui->targetPathLbl->setText(bPath);
        // ui->targetPathLbl->setToolTip(bPath);
    }
    else {
        G::popup->showPopup("End of match images");
    }
}


void FindDuplicatesDlg::on_tv_clicked(const QModelIndex &index)
{
/*
    tv = tableview of candidate images

    Initiate the "Compare Images", with the selected candidate image on the left,
    and the first target image on the right.
*/
    qDebug() << "FindDuplicatesDlg::on_tv_clicked" << index;
    // do nothing if click on checkbox column
    if (index.column() == 0) return;

    // larger A image (candidate)
    int a = index.row();

    // wait
    QPixmap pmNull;
    ui->candidateLbl->setPixmap(pmNull);
    ui->candidateLbl->setText("Loading image...");
    ui->candidateLbl->repaint();
    ui->matchLbl->setPixmap(pmNull);
    ui->matchLbl->setText("Loading image...");
    ui->matchLbl->repaint();
    if (G::useProcessEvents) qApp->processEvents();

    currentMatch = 0;
    // larger A image (candidate)
    QString aName = aItems.at(a).name;
    // candidate image path
    QString aPath = aItems.at(a).path;
    QString bPath = "";
    bool isMatch = matches.contains(a);
    if (isMatch) {
        if (matches[a].count() > 0) bPath = matches[a].at(0).path;
        showImageComparisonStuff(a, 0, bPath);
    }
    ui->candidateFilenameLbl->setVisible(true);
    ui->candidateFilenameLbl->setText(aName);

    // show candidate and match images
    QImage image;
    // candidate image
    getPreview(aPath, image, "FindDupCandidate");
    showPreview(aPath, image, "FindDupCandidate");
    // if no matches
    // if (matches[a].count() == 0) {
    if (isMatch) {
        // best match image
        if (matches[a].count() < 2) {
            ui->prevToolBtn->setVisible(false);
            ui->nextToolBtn->setVisible(false);
        }
        else {
            ui->prevToolBtn->setVisible(true);
            ui->nextToolBtn->setVisible(true);
        }
        if (matches[a].count() > 0) {
            getPreview(bPath, image, "FindDupMatch");
            showPreview(bPath, image, "FindDupMatch");
        }
    }
    else {
        ui->matchLbl->setText("No match");
        ui->currentLbl->setVisible(false);
    }
}

void FindDuplicatesDlg::on_abortBtn_clicked()
{
    if (isRunning) abort = true;
    else clear();
}

void FindDuplicatesDlg::on_closeBtn_clicked()
{
    // close(), not reject(): closeEvent defers the close when a search is running
    close();
}

void FindDuplicatesDlg::on_updateDupsAndQuitBtn_clicked()
{
    /* Clear the compare flag on every row first (candidates are only a selected
       subset, so a candidate-only loop would leave stale flags on other rows),
       then mark the checked candidates. */
    if (isRunning) return;
    /* Candidates are a snapshot, and Winnow may have changed folders since, so map
       each back to its current row by path; ones no longer loaded are skipped. */
    if (candidatesInDataModel() == 0) {
        updateApplyState();
        return;
    }
    for (int sfRow = 0; sfRow < dm->sf->rowCount(); sfRow++) {
        dm->sf->setData(dm->sf->index(sfRow, G::CompareColumn), false);
    }
    for (int a = 0; a < aItems.count(); a++) {
        if (model.itemFromIndex(model.index(a, MC::CheckBox))->checkState() == Qt::Checked) {
            int sfRow = dm->proxyRowFromPath(aItems.at(a).path);
            if (sfRow >= 0) dm->sf->setData(dm->sf->index(sfRow, G::CompareColumn), true);
        }
    }
    accept();
}

void FindDuplicatesDlg::fit(QPixmap &pm, QLabel *label)
{
    int w = label->width();
    int h = label->height();
    // int d;
    // // if (pm.width() / pm.height() > 1) {
    // double aspectPM = pm.width() / pm.height();
    // double aspectLabel = label->width() / label->height();
    // if (aspectPM > aspectLabel) {
    //     d = label->height();
    // }
    // else {
    //     d = label->width();
    // }
    label->setPixmap(pA.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    // label->setPixmap(pA.scaled(d, d, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void FindDuplicatesDlg::resizeEvent(QResizeEvent *event)
{
    //QDialog::resizeEvent(event);
    fit(pA, ui->candidateLbl);
    fit(pB, ui->matchLbl);
}

void FindDuplicatesDlg::on_helpBtn_clicked()
{
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Find Duplicates",
                   ":/Docs/findduplicateshelp.html",
                   QSize(800, 700), r, this);
}

void FindDuplicatesDlg::on_deltaThresholdToolBtn_clicked()
{
/*
    The delta or difference in pixels between two images is determined by taking the
    difference of the red, green and blue component values (RGB), and then averaging the
    sum of all the differences. The values range of a red, green or blue component is 0 -
    255, therefore the maximum pixel delta = 255 and two identical images would have a
    pixel delta = 0.
*/
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Pixel Delta",
                   ":/Docs/pixeldeltahelp.html",
                   QSize(600, 300), r, this);
}

void FindDuplicatesDlg::progressMsg(QString msg)
{
    ui->progressLbl->setText(msg);
    if (G::useProcessEvents) QApplication::processEvents();
}

void FindDuplicatesDlg::on_toggleTvHideChecked_clicked()
{
    for (int row = 0; row < model.rowCount(); ++row) {
        QModelIndex index = model.index(row, MC::CheckBox);
        Qt::CheckState state = model.data(index, Qt::CheckStateRole).value<Qt::CheckState>();
        if (state != Qt::Checked) ui->tv->setRowHidden(row, okToHide);
    }
    okToHide = !okToHide;
    if (okToHide) ui->toggleTvHideChecked->setText("Hide unchecked images");
    else ui->toggleTvHideChecked->setText("Show unchecked images");
}


void FindDuplicatesDlg::on_tv_doubleClicked(const QModelIndex &index)
{
    reportMatches();
}


void FindDuplicatesDlg::on_reportBtn_clicked()
{
    reportResults();
}

// Debugging helpers

void::FindDuplicatesDlg::reportMatches()
{
    // titles
    QString t0 = "Candidate";
    QString t1 = "#";
    QString t2 = "Delta";
    QString t3 = "Duplicate";
    t0 = t0.leftJustified(40);
    t1 = t1.rightJustified(3);
    qDebug().noquote() << t0 << t1 << t2 << t3;

    bool isSamePixels = ui->samePixelsCB->isChecked();
    QString matchCount;
    QString delta;
    QString mPath;
    for (int a = 0; a < aItems.count(); a++) {
        QString candidate = aItems.at(a).name.leftJustified(40);
        if (matches[a].count() == 0) {
            matchCount = "  0";
            delta = "  n/a";
            QString nada = "No match found";
            qDebug().noquote() << candidate << matchCount << delta << nada;
            continue;
        }
        for (int b = 0; b < matches[a].count(); b++) {
            matchCount = QString::number(matches[a].count()).rightJustified(3);
            if (isSamePixels)
                delta = QString::number(matches[a].at(b).deltaPixels).rightJustified(5);
            else
                delta = "  n/a";
            mPath = matches[a].at(b).path;
            qDebug().noquote() << candidate << matchCount << delta << mPath;
        }
    }
}

void FindDuplicatesDlg::reportbItems()
{
    qDebug() << "FindDuplicatesDlg::reportbItems";
    int counter = 0;
    foreach (B bItem, bItems) {
        qDebug().noquote()
            << QString::number(counter).leftJustified(5)
            << "TYPE" << bItem.type.leftJustified(8)
            << "DATE" << bItem.createdDate.leftJustified(25)
            << "ASPECT" << bItem.aspect.leftJustified(6)
            << "DURATION" << bItem.duration.leftJustified(10)
            << bItem.fPath;
        counter++;
    }
}

void FindDuplicatesDlg::reportAspects()
{
    qDebug() << "\n" << "FindDuplicatesDlg::reportAspects";
    for (int a = 0, b = 0; static_cast<void>(a < aItems.count()), b < bItems.count(); a++, b++) {
        QFileInfo fInfo(bItems.at(b).fPath);
        int row = dm->proxyRowFromPath(bItems.at(b).fPath);
        QString fileNameB  = (QFileInfo(bItems.at(b).fPath)).fileName();
        metadata->loadImageMetadata(fInfo, row, dm->instance, true, true, false, true);
        ImageMetadata *m = &metadata->m;
        // QString::number().rightJustified(3)
        qDebug().noquote()
            << QString::number(a).rightJustified(3)
            << QString::number(b).rightJustified(3)
            //<< aItems.at(a).name
            << "aspectA/B" << QString::number(aItems.at(a).aspect, 'f', 2)
            << bItems.at(b).aspect
            << "m >> w" << QString::number(m->width).rightJustified(5)
            << "h" << QString::number(m->height).rightJustified(5)
            << "orientation" << QString::number(m->orientation).rightJustified(1)
            << fInfo.fileName()
            ;
    }
}

void::FindDuplicatesDlg::reportFindMatch(int a, int b)
{
    QString s = " ";
    QString rpt;
    rpt = "a = " + QString::number(a).leftJustified(5) + " b = " + QString::number(b).leftJustified(5);

    // A items
    QString fileNameA = aItems.at(a).name.leftJustified(20);
    QString pathA = aItems.at(a).path;
    QString nameA = QFileInfo(pathA).fileName().toLower();
    QString typeA = QFileInfo(pathA).suffix().toLower();
    QString dateA = aItems.at(a).created;
    QString aspectA = QString::number(aItems.at(a).aspect,'f', 2);
    QString durationA = aItems.at(a).duration;
    if (durationA.length() == 0) durationA = "00:00";

    QString same;
    if (ui->sameFileNameCB->isChecked()) {
        QString typeB = bItems.at(b).name;
        bool same = typeA == typeB;
        QString sameName = same ? "true" : "false";
        rpt += "    NAME " + sameName.leftJustified(25) + (typeA + s + typeB).leftJustified(25);
    }
    if (ui->sameFileTypeCB->isChecked()) {
        QString typeB = bItems.at(b).type;
        bool same = typeA == typeB;
        QString sameType = same ? "true" : "false";
        rpt += "    TYPE " + sameType.leftJustified(6) + (typeA + s + typeB).leftJustified(9);
    }
    if (ui->sameCreationDateCB->isChecked()) {
        QString dateB = bItems.at(b).createdDate.leftJustified(23);     // might be blank date
        bool same = dateA == dateB;
        QString sameDate = same ? "true" : "false";
        rpt += "    DATE " + sameDate.leftJustified(6) + dateA + s + dateB;
    }
    if (ui->sameAspectCB->isChecked()) {
        QString aspectB = bItems.at(b).aspect;
        bool same = aspectA == aspectB;
        QString sameAspect = same ? "true" : "false";
        rpt += "    ASPECT "  + sameAspect.leftJustified(6) + (aspectA + s + aspectB).leftJustified(7);
    }
    if (ui->sameDurationCB->isChecked()) {
        QString durationB = bItems.at(b).duration;
        bool same = durationA == durationB;
        QString sameDuration = same ? "true" : "false";
        rpt += "    DURATION "  + sameDuration.leftJustified(6) + (durationA + s + durationB).leftJustified(7);
    }
    rpt += "    A: " + fileNameA.leftJustified(40);
    rpt += "B: " + bItems.at(b).fPath;

    qDebug().noquote() << rpt;
}

void FindDuplicatesDlg::reportRGB(QImage &im)
{
    /*
    For debugging
*/
    int xBound = 15;
    int yBound = 1;

    int w = im.width();
    int h = im.height();
    qDebug() << "FindDuplicatesDlg::reportRGB:"
             << "w =" << w
             << "h =" << h
        ;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            QColor p = im.pixelColor(x,y);
            if (x < xBound && y < yBound)
                qDebug() << "  " << x
                         << "\t red:" << p.red()
                         << "\t green:" << p.green()
                         << "\t blue:" << p.blue()
                    ;
        }
    }
}

