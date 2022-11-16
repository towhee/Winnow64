#include "Views/infoview.h"
#include "Main/global.h"

class InfoDelegate : public QStyledItemDelegate
{
public:
    explicit InfoDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &index) const override
    {
//        qDebug() << "InfoDelegate::sizeHint" << index;
        static int count = 0;
        count++;
        index.isValid();          // suppress compiler warning
        int height = qRound(G::fontSize.toInt() * 1.7 * G::ptToPx);
        return QSize(option.rect.width(), height);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        int leftOffset = 4;
        int rightOffset = 19;
        int topOffset = 1;
        QPoint topLeft(option.rect.left() - leftOffset, option.rect.top() + topOffset);
        QPoint bottomRight(option.rect.right() + rightOffset, option.rect.bottom());
        QRect editRect(topLeft, bottomRight);
        editor->setGeometry(editRect);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();

        int y1 = option.rect.top();
        int y2 = option.rect.bottom();

        int a = G::backgroundShade + 5;
        int b = G::backgroundShade - 15;

        QLinearGradient categoryBackground;
        categoryBackground.setStart(0, y1);
        categoryBackground.setFinalStop(0, y2);
        categoryBackground.setColorAt(0, QColor(a,a,a));
        categoryBackground.setColorAt(1, QColor(b,b,b));

        int hOffset = 11;
        int vOffset = 1;
        QPoint topLeft(option.rect.left() + hOffset, option.rect.top() - vOffset);
        QPoint bottomRight(option.rect.bottomRight());
        QRect textRect(topLeft, bottomRight);

        if (index.parent() == QModelIndex() && index.column() == 0) {
            painter->fillRect(option.rect, categoryBackground);
            QPen catPen(Qt::white);
            painter->setPen(catPen);
        }
        QString text = index.data().toString();
        QFont font = painter->font();
        font.setPointSize(G::fontSize.toInt());
        painter->setFont(font);

        QString elidedText;
        if (index.column() == 0)
            elidedText = text;
        else
            elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());

        painter->setPen(QColor(G::textShade,G::textShade,G::textShade));
        painter->drawText(textRect, Qt::AlignVCenter|Qt::TextSingleLine, elidedText);

        painter->setPen(QColor(75,75,75));
        painter->drawRect(option.rect);

        painter->restore();
    }
};


/*
    This class shows information in a two column table.

    Column 0 = Item Description
    Column 1 = Item Value
    Column 2 = Flag to show or hide row (hidden)

    It is used to show some file, image and application state information.
*/

InfoView::InfoView(QWidget *parent, DataModel *dm, Metadata *metadata, IconView *thumbView)
    : QTreeView(parent)
{
    if (G::isLogger) G::log("InfoView::InfoView");    this->dm = dm;
    this->metadata = metadata;
    this->thumbView = thumbView;        // req'd to update metadata in dm for selections

    ok = new QStandardItemModel(this);
    setupOk();
    setModel(ok);
//    selectionModel()->setModel(ok);

    setRootIsDecorated(true);
    setIndentation(0);
    setExpandsOnDoubleClick(false);
    setHeaderHidden(true);
    setAlternatingRowColors(true);
    setEditTriggers(QAbstractItemView::SelectedClicked);
    // setup headers
    setFirstColumnSpanned(0, QModelIndex(), true);  // File:
    setFirstColumnSpanned(1, QModelIndex(), true);  // Camera:
    setFirstColumnSpanned(2, QModelIndex(), true);  // Tags:
    setFirstColumnSpanned(3, QModelIndex(), true);  // Status:
    expandAll();
    hideColumn(2);
    setColumn0Width();
    setTabKeyNavigation(false);

    setItemDelegate(new InfoDelegate(this));

    // setStyleSheet - see InfoView::mousePressEvent

    // InfoView menu
	infoMenu = new QMenu("");
    copyInfoAction = new QAction(tr("Copy item"), this);

    // metadata fields that can be edited
    editFields << "Title" << "Creator" << "Copyright" << "Email" << "Url";

    connect(this, &InfoView::setValueSf, dm, &DataModel::setValueSf);

    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(showInfoViewMenu(QPoint)));

    connect(ok, &QStandardItemModel::dataChanged, this, &InfoView::dataChanged);
    /*
    connect(itemDelegate(), &QStyledItemDelegate::closeEditor, this, &InfoView::editorClosed);
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &InfoView::selChanged);
    connect(this, &QAbstractItemView::activated, this, &InfoView::styleBackground);
    //*/
}

void InfoView::dataChanged(const QModelIndex &idx1, const QModelIndex&, const QVector<int> &roles)
{
/*
    Updates the datamodel for info items that can be edited: title, creator, copyright, email
    and url (at present).  This is used in Embel (ie image title) and "Show shooting data".

    The dataChanged signal is triggered twice for the same edit so count is used to only
    process once.

    The signal dataEdited is emitted, which triggers Embel to update the text fields. This
    will only work if Embel::isRemote == false.
*/
    bool isSidecarChange = G::useSidecar && !isNewImageDataChange;
    bool usedPopUp = false;
    static int count = 0;
    QString src = "InfoView::dataChanged";
    if (count == 0) {
        bool isEditable = ok->itemFromIndex(idx1)->isEditable();
        if (isEditable) {
            QModelIndexList selection = thumbView->selectionModel()->selectedRows();
            QModelIndex idx0 = ok->index(idx1.row(), 0, idx1.parent());
            QString field = idx0.data().toString();

            int n = selection.count();
            if (isSidecarChange) {
                usedPopUp = true;
                G::popUp->setProgressVisible(true);
                G::popUp->setProgressMax(n + 1);
                QString txt = "Writing to XMP sidecar for " + QString::number(n) + " images." +
                              "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
//                G::popUp->showPopup(txt, 0, true, 1);
            }

            for (int i = 0; i < n; i++) {
                int row = selection.at(i).row();
                QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
                if (field == "Title") {
                    QString s = idx1.data().toString();
                    emit setValueSf(dm->sf->index(row, G::TitleColumn), s, dm->instance, src, Qt::EditRole);
                    emit setValueSf(dm->sf->index(row, G::TitleColumn), s, dm->instance, src, Qt::ToolTipRole);
                    /*
                    qDebug() << "InfoView::dataChanged"
                             << "dm->currentRow =" << dm->currentRow
                             << "idx0.data() =" << idx0.data()
                             << "idx1.data() =" << idx1.data()
                             << "isEditable =" << isEditable
                                ;
    //                            */
                }
                if (field == "Creator") {
                    QString s = idx1.data().toString();
                    emit setValueSf(dm->sf->index(row, G::CreatorColumn), s, dm->instance, src, Qt::EditRole);
                    emit setValueSf(dm->sf->index(row, G::CreatorColumn), s, dm->instance, src, Qt::ToolTipRole);
                }
                if (field == "Copyright") {
                    QString s = idx1.data().toString();
                    emit setValueSf(dm->sf->index(row, G::CopyrightColumn), s, dm->instance, src, Qt::EditRole);
                    emit setValueSf(dm->sf->index(row, G::CopyrightColumn), s, dm->instance, src, Qt::ToolTipRole);
                }
                if (field == "Email") {
                    QString s = idx1.data().toString();
                    emit setValueSf(dm->sf->index(row, G::EmailColumn), s, dm->instance, src, Qt::EditRole);
                    emit setValueSf(dm->sf->index(row, G::EmailColumn), s, dm->instance, src, Qt::ToolTipRole);
                }
                if (field == "Url") {
                    QString s = idx1.data().toString();
                    emit setValueSf(dm->sf->index(row, G::UrlColumn), s, dm->instance, src, Qt::EditRole);
                    emit setValueSf(dm->sf->index(row, G::UrlColumn), s, dm->instance, src, Qt::ToolTipRole);
                }

                // write to sidecar
                if (isSidecarChange) {
                    dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
                    metadata->writeXMP(metadata->sidecarPath(fPath), "InfoView::dataChanged");
                    G::popUp->setProgress(i+1);
                }
            }

            emit dataEdited();
        }
    }
    count++;
    if (count > 1) count = 0;
    if (usedPopUp) {
        G::popUp->setProgressVisible(false);
        G::popUp->end();
    }
}

void InfoView::refreshLayout()
{
    if (G::isLogger) G::log("InfoView::refreshLayout");
    setColumn0Width();
    scheduleDelayedItemsLayout();
}

void InfoView::showInfoViewMenu(QPoint pt)
{
    if (G::isLogger) G::log("InfoView::showInfoViewMenu");    selectedEntry = indexAt(pt);
	if (selectedEntry.isValid())
    	infoMenu->popup(viewport()->mapToGlobal(pt));
}

void InfoView::setColumn0Width()
{
    if (G::isLogger) G::log("InfoView::setColumn0Width");    QFont ft = this->font();
    ft.setPixelSize(static_cast<int>(G::fontSize.toInt() * 1.333/* * G::ptToPx*/));
    QFontMetrics fm(ft);
    #ifdef Q_OS_WIN
    setColumnWidth(0, fm.boundingRect("---Shutter speed---").width());
    #endif
    #ifdef Q_OS_MAC
    setColumnWidth(0, fm.boundingRect("Shutter speed").width());
    #endif
}

void InfoView::setupOk()
{
/*
The datamodel called (ok) holds all metadata items shown in the InfoView
QTableView. It contains three columns:

    ● (0) The name of the information item
    ● (1) The value of the information item
    ● (2) A boolean flag to show or hide a row of the table

The information items are metadata about the file, such as name or path;
information about the image, such as aperture or dimensions; and application
status information, such as number of items picked or current item selected.

If any of the editable fields change then MW::metadataChanged is triggered.
*/
    if (G::isLogger) G::log("InfoView::setupOk");    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Value")));
    ok->setHorizontalHeaderItem(2, new QStandardItem(QString("Show")));

    // create all the category rows req'd using global enum categories
    ok->insertRows(0, categoriesEnd);

    ok->setData(ok->index(fileInfoCat, 0), "File:");
    ok->setData(ok->index(imageInfoCat, 0), "Camera:");
    ok->setData(ok->index(tagInfoCat, 0), "Tags:");
    ok->setData(ok->index(statusInfoCat, 0), "Status:");

    fileInfoIdx = ok->index(fileInfoCat, 0, QModelIndex());
    imageInfoIdx = ok->index(imageInfoCat, 0);
    tagInfoIdx = ok->index(tagInfoCat, 0);
    statusInfoIdx = ok->index(statusInfoCat, 0);

    // create the data rows for each category
    ok->insertRows(0, fileInfoRowsEnd, fileInfoIdx);
    ok->insertColumns(0, 3, fileInfoIdx);

    ok->insertRows(0, imageInfoRowsEnd, imageInfoIdx);
    ok->insertColumns(0, imageInfoRowsEnd, imageInfoIdx);

    ok->insertRows(0, tagInfoRowsEnd, tagInfoIdx);
    ok->insertColumns(0, tagInfoRowsEnd, tagInfoIdx);

    ok->insertRows(0, statusInfoRowsEnd, statusInfoIdx);
    ok->insertColumns(0, statusInfoRowsEnd, statusInfoIdx);

    // Set field description
    ok->setData(ok->index(FolderRow, 0, fileInfoIdx), "Folder");
    ok->setData(ok->index(FileNameRow, 0, fileInfoIdx), "File name");
    ok->setData(ok->index(LocationRow, 0, fileInfoIdx), "Path");
    ok->setData(ok->index(SizeRow, 0, fileInfoIdx), "Size");
    ok->setData(ok->index(CreatedRow, 0, fileInfoIdx), "Created");
    ok->setData(ok->index(ModifiedRow, 0, fileInfoIdx), "Modified");
    ok->setData(ok->index(DimensionsRow, 0, fileInfoIdx), "Dimensions");
    ok->setData(ok->index(MegaPixelsRow, 0, fileInfoIdx), "Megapixels");
    ok->setData(ok->index(ModelRow, 0, imageInfoIdx), "Model");
    ok->setData(ok->index(LensRow, 0, imageInfoIdx), "Lens");
    ok->setData(ok->index(ShutterSpeedRow, 0, imageInfoIdx), "Shutter speed");
    ok->setData(ok->index(ApertureRow, 0, imageInfoIdx), "Aperture");
    ok->setData(ok->index(ISORow, 0, imageInfoIdx), "ISO");
    ok->setData(ok->index(ExposureCompensationRow, 0, imageInfoIdx), "EC");
    ok->setData(ok->index(FocalLengthRow, 0, imageInfoIdx), "Focal length");
    ok->setData(ok->index(TitleRow, 0, tagInfoIdx), "Title");
    ok->setData(ok->index(CreatorRow, 0, tagInfoIdx), "Creator");
    ok->setData(ok->index(CopyrightRow, 0, tagInfoIdx), "Copyright");
    ok->setData(ok->index(EmailRow, 0, tagInfoIdx), "Email");
    ok->setData(ok->index(UrlRow, 0, tagInfoIdx), "Url");
    ok->setData(ok->index(GPSCoordRow, 0, tagInfoIdx), "GPS");
    ok->setData(ok->index(KeywordRow, 0, tagInfoIdx), "Keywords");
    ok->setData(ok->index(PositionRow, 0, statusInfoIdx), "Position");
    ok->setData(ok->index(ZoomRow, 0, statusInfoIdx), "Zoom");
    ok->setData(ok->index(SelectedRow, 0, statusInfoIdx), "Selected");
    ok->setData(ok->index(PickedRow, 0, statusInfoIdx), "Picked");
    ok->setData(ok->index(CacheRow, 0, statusInfoIdx), "Cache");
    ok->setData(ok->index(MonitorRow, 0, statusInfoIdx), "Monitor");

    // tooltip for EC
    ok->setData(ok->index(ExposureCompensationRow, 0, imageInfoIdx), "Exposure Compensation", Qt::ToolTipRole);

    // set default to show all rows - overridden in preferences
    // set all items not editable
    for(int row = 0; row < ok->rowCount(); row++) {
        ok->setData(ok->index(row, 2), true);
        ok->itemFromIndex(ok->index(row, 0))->setEditable(false);
        ok->itemFromIndex(ok->index(row, 1))->setEditable(false);
        for (int childRow = 0; childRow < ok->rowCount(ok->index(row, 0)); childRow++) {
            ok->setData(ok->index(childRow, 2, ok->index(row, 0)), true);
            ok->itemFromIndex(ok->index(childRow, 0, ok->index(row, 0)))->setEditable(false);
            ok->itemFromIndex(ok->index(childRow, 1, ok->index(row, 0)))->setEditable(false);
        }
    }

    // set editable fields
//    ok->itemFromIndex(ok->index(ModelRow, 1, imageInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(TitleRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(CreatorRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(CopyrightRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(EmailRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(UrlRow, 1, tagInfoIdx))->setEditable(true);

    connect(ok, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            this, SLOT(showOrHide()));
}

void InfoView::showOrHide()
{
/*
Shows or hides each metadata item in the metadata panel based on the boolean
flag in the datamodel ok.  The show/hide is set in the prefdlg, which is in
sync with ok.

When called, the function iterates through all the metadata items in ok and
looks for the field in ok. It then shows or hides the table row based on the ok
show flag.
*/
    if (G::isLogger) G::log("InfoView::showOrHide");    bool okToShow;
    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        okToShow = ok->index(row, 2).data().toBool();
        setRowHidden(row, QModelIndex(), !okToShow);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
           okToShow = ok->index(childRow, 2, parentIdx).data().toBool();
            setRowHidden(childRow, parentIdx, !okToShow);
        }
    }
}

void InfoView::clearInfo()
{
/*
Clear all the values but leave the keys and flags alone
*/
    if (G::isLogger) G::log("InfoView::clearInfo");    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
            ok->setData(ok->index(childRow, 1, parentIdx), "");
        }
    }
}

void InfoView::copyEntry()
{
    if (G::isLogger) G::log("InfoView::copyEntry");	if (selectedEntry.isValid())
        QApplication::clipboard()->setText(ok->itemFromIndex(selectedEntry)->toolTip());
}

void InfoView::updateInfo(const int &row)
{
    if (G::isLogger) G::log("InfoView::updateInfo");//    qDebug() << "InfoView::" << row;

    // flag updates so itemChanged will be ignored in MW::metadataChanged
    isNewImageDataChange = true;

    QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
    QFileInfo imageInfo = QFileInfo(fPath);

    // make sure there is metadata for this image
    if (!dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) {
        metadata->loadImageMetadata(imageInfo, dm->instance, true, true, false, true, "InfoView::");
        int row = dm->rowFromPath(fPath);
        if (row == -1) return;
        metadata->m.row = row;
        metadata->m.instance = dm->instance;
        if (!dm->addMetadataForItem(metadata->m, "InfoView::updateInfo")) return;
    }

    QString s;
    QVariant value;

    // update items

    ok->setData(ok->index(FolderRow, 1, fileInfoIdx), imageInfo.dir().dirName());
    ok->setData(ok->index(FileNameRow, 1, fileInfoIdx), imageInfo.fileName());
    ok->setData(ok->index(LocationRow, 1, fileInfoIdx), imageInfo.path());
    ok->setData(ok->index(SizeRow, 1, fileInfoIdx), QString::number(imageInfo.size() / 1024000.0, 'f', 2) + " MB");
    s = dm->sf->index(row, G::CreatedColumn).data().toDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ok->setData(ok->index(CreatedRow, 1, fileInfoIdx), s);
    s = dm->sf->index(row, G::ModifiedColumn).data().toString();
    ok->setData(ok->index(ModifiedRow, 1, fileInfoIdx), s);

    s = dm->sf->index(row, G::DimensionsColumn).data().toString();
    ok->setData(ok->index(DimensionsRow, 1, fileInfoIdx), s);
    s = dm->sf->index(row, G::MegaPixelsColumn).data().toString();
    ok->setData(ok->index(MegaPixelsRow, 1, fileInfoIdx), s);
    s = dm->sf->index(row, G::CameraModelColumn).data().toString();
    ok->setData(ok->index(ModelRow, 1, imageInfoIdx), s);
    s = dm->sf->index(row, G::LensColumn).data().toString();
    ok->setData(ok->index(LensRow, 1, imageInfoIdx), s);

    value = dm->sf->index(row, G::ShutterspeedColumn).data();
    if (value == 0) s = "";
    else {
        if (value.toDouble() < 1.0) {
            double recip = 1 / value.toDouble();
            if (recip >= 2) s = "1/" + QString::number(qRound(recip));
            else s = QString::number(value.toDouble(), 'g', 2);
        } else {
            s = QString::number(value.toInt());
        }
        s += " sec";
    }
    ok->setData(ok->index(ShutterSpeedRow, 1, imageInfoIdx), s);

    value = dm->sf->index(row, G::ApertureColumn).data();
    if (value == 0) s = "";
    else s = "f/" + QString::number(value.toDouble(), 'f', 1);
    ok->setData(ok->index(ApertureRow, 1, imageInfoIdx), s);

    s = dm->sf->index(row, G::ISOColumn).data().toString();
    if (s == "0") s = "";
    ok->setData(ok->index(ISORow, 1, imageInfoIdx), s);
    s = dm->sf->index(row, G::ExposureCompensationColumn).data().toString();
//    s = QString::number(dm->sf->index(row, G::ExposureCompensationColumn).data().toDouble(), 'f', 1) + " EV";
    if (s != "") s += " EV";
    ok->setData(ok->index(ExposureCompensationRow, 1, imageInfoIdx), s);
    s = dm->sf->index(row, G::FocalLengthColumn).data().toString() + "mm";
    if (s == "0mm") s = "";
    ok->setData(ok->index(FocalLengthRow, 1, imageInfoIdx), s);

    s = dm->sf->index(row, G::TitleColumn).data().toString();
    ok->setData(ok->index(TitleRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::CreatorColumn).data().toString();
    ok->setData(ok->index(CreatorRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::CopyrightColumn).data().toString();
    ok->setData(ok->index(CopyrightRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::EmailColumn).data().toString();
    ok->setData(ok->index(EmailRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::UrlColumn).data().toString();
    ok->setData(ok->index(UrlRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::GPSCoordColumn).data().toString();
    ok->setData(ok->index(GPSCoordRow, 1, tagInfoIdx), s);
    s = Utilities::stringListToString(dm->sf->index(row, G::KeywordsColumn).data().toStringList());
    ok->setData(ok->index(KeywordRow, 1, tagInfoIdx), s);

    this->fPath = fPath;        // not used, convenience value for future use

    // set tooltips
    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        ok->setData(parentIdx, "Click category to expand/collapse", Qt::ToolTipRole);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
            QModelIndex idx = ok->index(childRow, 1, parentIdx);
            QString value = qvariant_cast<QString>(idx.data());
            ok->setData(idx, value, Qt::ToolTipRole);
        }
    }

    if (G::isThreadTrackingOn) qDebug()
        << "ThumbView::updateExifInfo - loaded metadata display info for"
        << row;

    setColumn0Width();
    showOrHide();

    isNewImageDataChange = false;
}

void InfoView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QModelIndex idx = indexAt(event->pos());
        // check if click on category row. If so, expand or collapse branch
        qDebug() << "InfoView::mousePressEvent" << idx.parent() << idx.parent().isValid();
        if (!idx.parent().isValid()) isExpanded(idx) ? collapse(idx) : expand(idx);

        else if (idx.column() == 1) { // column you want to use for one click
            QString name = ok->index(idx.row(), 0, idx.parent()).data().toString();
            QString selectedCount = ok->index(SelectedRow, 1, statusInfoIdx).data().toString();
            selectedCount.remove(selectedCount.indexOf(" "), selectedCount.length() - 1);
            if (selectedCount.toInt() > 1 && editFields.contains(name)) {
                QString msg = "<font color=\"red\"><b>Note: </b></font>"
                              "Any edits to " + name + " will be applied to all " +
                              selectedCount + " selected images.";
                G::popUp->showPopup(msg, 3000);
            }
            qDebug() << "InfoView::mousePressEvent" << "selected =" << selectedCount;
            // alternating colors
            int row = idx.row();
            int a = G::backgroundShade + 5;
            int b = G::backgroundShade;
            QColor altBackground(b,b,b);
            if (row % 2 != 0) altBackground = QColor(a,a,a);
            setStyleSheet(
                "QLineEdit {"
                    "border: none;"
                    "margin-left: 5px;"
                    "margin-right: 20px;"
                    "margin-bottom: -5px;"
                    "padding-top: -6px;"
                    "padding-left: 8px;"
                "}"
                "QLineEdit:focus {"
                    "background-color:" + altBackground.name() + ";"
                "}"
                ";"
            );
            edit(idx);
        }
    }
    QTreeView::mousePressEvent(event);
}
