#include "infoview.h"
#include "global.h"


InfoView::InfoView(QWidget *parent, Metadata *metadata) : QTableView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::InfoView";
    #endif
    }
    this->metadata = metadata;

    ok = new QStandardItemModel(this);
    setupOk();
    setModel(ok);

    setSelectionMode(QAbstractItemView::NoSelection);
	verticalHeader()->setVisible(false);
	verticalHeader()->setDefaultSectionSize(verticalHeader()->minimumSectionSize());
    horizontalHeader()->setVisible(true);
    horizontalHeader()->setStretchLastSection(true);
    hideColumn(0);
    setAlternatingRowColors(true);
    setShowGrid(true);

	setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
	setTabKeyNavigation(false);

    // InfoView menu
	infoMenu = new QMenu("");
	copyAction = new QAction(tr("Copy"), this);

	connect(copyAction, SIGNAL(triggered()), this, SLOT(copyEntry()));
	infoMenu->addAction(copyAction);
	setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(showInfoViewMenu(QPoint)));
}

void InfoView::showInfoViewMenu(QPoint pt)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::showInfoViewMenu";
    #endif
    }
    selectedEntry = indexAt(pt);
	if (selectedEntry.isValid())
    	infoMenu->popup(viewport()->mapToGlobal(pt));
}

void InfoView::tweakHeaders()
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::tweakHeaders";
    #endif
    }
    horizontalHeader()->setFixedHeight(1);
}

void InfoView::setupOk()
{
/*
The datamodel called (ok) holds all metadata items shown in the InfoView
QTableView. It contains three columns:

    ● A boolean flag to show or hide a row of the table
    ● The name of the information item
    ● The value of the information item

The information items are metadata about the file, such as name or path;
information about the image, such as aperture or dimensions; and application
status information, such as number of items picked or current item selected.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::createOkToShow";
    #endif
    }
    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Value")));
    ok->setHorizontalHeaderItem(2, new QStandardItem(QString("Show")));

    // create all the rows req'd using global enum infoModelItems
    ok->insertRows(0, AfterLastItem);

    // Set field description
    ok->setData(ok->index(FolderRow, 1), "Folder");
    ok->setData(ok->index(FileNameRow, 1), "File name");
    ok->setData(ok->index(LocationRow, 1), "Location");
    ok->setData(ok->index(SizeRow, 1), "Size");
    ok->setData(ok->index(DateTimeRow, 1), "Created");
    ok->setData(ok->index(ModifiedRow, 1), "Modified");
    ok->setData(ok->index(DimensionsRow, 1), "Dimensions");
    ok->setData(ok->index(MegaPixelsRow, 1), "Megapixels");
    ok->setData(ok->index(ModelRow, 1), "Model");
    ok->setData(ok->index(LensRow, 1), "Lens");
    ok->setData(ok->index(ShutterSpeedRow, 1), "Shutter speed");
    ok->setData(ok->index(ApertureRow, 1), "Aperture");
    ok->setData(ok->index(ISORow, 1), "ISO");
    ok->setData(ok->index(FocalLengthRow, 1), "Focal length");
    ok->setData(ok->index(TitleRow, 1), "Title");
    ok->setData(ok->index(CreatorRow, 1), "Creator");
    ok->setData(ok->index(CopyrightRow, 1), "Copyright");
    ok->setData(ok->index(EmailRow, 1), "Email");
    ok->setData(ok->index(UrlRow, 1), "Url");
    ok->setData(ok->index(PositionRow, 1), "Position");
    ok->setData(ok->index(ZoomRow, 1), "Zoom");
    ok->setData(ok->index(PickedRow, 1), "Picked");

    // set default to show all rows - overridden in preferences
    for(int row = 0; row < ok->rowCount(); row++)
        ok->setData(ok->index(row, 0), true);

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
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::showOrHide()";
    #endif
    }
    for(int row = 0; row < ok->rowCount(); row++) {
        bool showField = ok->index(row, 0).data().toBool();
        if (showField) showRow(row);
        else hideRow(row);
    }
}

void InfoView::clearInfo()
{
    ok->clear();
}

void InfoView::copyEntry()
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::copyEntry";
    #endif
    }
	if (selectedEntry.isValid())
        QApplication::clipboard()->setText(ok->itemFromIndex(selectedEntry)->toolTip());
}

void InfoView::updateInfo(const QString &fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::updateInfo";
    #endif
    }

    QFileInfo imageInfo = QFileInfo(fPath);

    // make sure there is metadata for this image
    if (!metadata->isLoaded(fPath)) {
        metadata->loadImageMetadata(fPath, true, true);
    }

    uint width = metadata->getWidth(fPath);
    uint height = metadata->getHeight(fPath);

    // update items
    ok->setData(ok->index(FolderRow, 2), imageInfo.dir().dirName());
    ok->setData(ok->index(FileNameRow, 2), imageInfo.fileName());
    ok->setData(ok->index(LocationRow, 2), imageInfo.path());
    ok->setData(ok->index(SizeRow, 2), QString::number(imageInfo.size() / 1024000.0, 'f', 2) + " MB");
    ok->setData(ok->index(DateTimeRow, 2), metadata->getDateTime(fPath));
    ok->setData(ok->index(ModifiedRow, 2), imageInfo.lastModified().toString(Qt::SystemLocaleShortDate));
    ok->setData(ok->index(DimensionsRow, 2), QString::number(width) + "x" + QString::number(height));
    ok->setData(ok->index(MegaPixelsRow, 2), QString::number((width * height) / 1000000.0, 'f', 1));
    ok->setData(ok->index(ModelRow, 2), metadata->getModel(fPath));
    ok->setData(ok->index(LensRow, 2), metadata->getLens(fPath));
    ok->setData(ok->index(ShutterSpeedRow, 2), metadata->getExposureTime(fPath));
    ok->setData(ok->index(ApertureRow, 2), metadata->getAperture(fPath));
    ok->setData(ok->index(ISORow, 2), metadata->getISO(fPath));
    ok->setData(ok->index(FocalLengthRow, 2), metadata->getFocalLength(fPath));
    ok->setData(ok->index(TitleRow, 2), metadata->getTitle(fPath));
    ok->setData(ok->index(CreatorRow, 2), metadata->getCreator(fPath));
    ok->setData(ok->index(CopyrightRow, 2), metadata->getCopyright(fPath));
    ok->setData(ok->index(EmailRow, 2), metadata->getEmail(fPath));
    ok->setData(ok->index(UrlRow, 2), metadata->getUrl(fPath));

    // set tooltips
    for (int row = 0; row < ok->rowCount(); row++) {
        QModelIndex idx = ok->index(row, 2);
        QString value = qvariant_cast<QString>(idx.data());
        qDebug() << "Value =" << value;
        ok->setData(idx, value, Qt::ToolTipRole);
    }

    if (G::isThreadTrackingOn) qDebug()
        << "ThumbView::updateExifInfo - loaded metadata display info for"
        << fPath;

    tweakHeaders();
    showOrHide();
}

