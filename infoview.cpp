

#include "infoview.h"
#include "global.h"
#include "mainwindow.h"     // friend of MW

/*
Should change infoModel to create list of metadata items plus show column.  Then
update the metadata in the value column instead of the current clear and rebuild
table strategy.
*/

// friend of MW
MW *mwInfo;

InfoView::InfoView(QWidget *parent, Metadata *metadata) : QTableView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::InfoView";
    #endif
    }
    this->metadata = metadata;
    // this works because InfoView is a friend class of MW
    mwInfo = qobject_cast<MW*>(parent);

    infoModel = new QStandardItemModel(this);
    setModel(infoModel);

    setSelectionMode(QAbstractItemView::NoSelection);
	verticalHeader()->setVisible(false);
	verticalHeader()->setDefaultSectionSize(verticalHeader()->minimumSectionSize());
    horizontalHeader()->setVisible(true);
    horizontalHeader()->setStretchLastSection(true);
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

    createOkToShow();

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

void InfoView::createOkToShow()
{
/*
Create a datamodel called (ok) to hold all metadata items in infoModel and a
boolean flag indicating whether to show or hide each item. It would make sense
to consolidate this into the infoModel datamodel, which is currently cleared
every time a new image is selected.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::createOkToShow";
    #endif
    }
    ok = new QStandardItemModel;

    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Show")));

    // these fields must exactly match ones in updateInfo() !!
    int i = 0;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Folder");         i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "File name");      i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Location");       i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Size");           i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Date/Time");      i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Modified");       i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Dimensions");     i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Megapixels");     i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Model");          i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Lens");           i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Shutter speed");  i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Aperture");       i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "ISO");            i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Focal length");   i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Title");          i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Creator");        i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Copyright");      i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Email");          i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Url");            i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Position");       i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Zoom");           i++;
    ok->insertRow(i);   ok->setData(ok->index(i, 0), "Picked");         i++;

    for(int row = 0; row < ok->rowCount(); row++)
        ok->setData(ok->index(row, 1), true);

    connect(ok, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            this, SLOT(showOrHide()));
}

void InfoView::showOrHide()
{
/*
Shows or hides each metadata item in the metadata panel based on the boolean
flag in the datamodel ok.  The show/hide is set in the prefdlg, which is in
sync with ok.

When called, the function iterates through all the metadata items in ok and looks
for the field in infoModel.  It then shows or hides the table row based on the
ok show flag.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::showOrHide()";
    #endif
    }
    for(int row = 0; row < ok->rowCount(); row++) {
        QString field = ok->index(row, 0).data().toString();
        bool showField = ok->index(row, 1).data().toBool();
        int tRow;
        for(tRow = 0; tRow < infoModel->rowCount(); tRow++) {
            QString infoField = infoModel->index(tRow, 0).data().toString();
            if (infoField == field) break;
        }
        if (showField) showRow(tRow);
        else hideRow(tRow);
    }
}

void InfoView::addEntry(QString &key, QString &value)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::addEntry" << key << value;
    #endif
    }
	int atRow = infoModel->rowCount();
	QStandardItem *itemKey = new QStandardItem(key);
	infoModel->insertRow(atRow, itemKey);
	if (!value.isEmpty()) {
		QStandardItem *itemVal = new QStandardItem(value);
		itemVal->setToolTip(value);
		infoModel->setItem(atRow, 1, itemVal);
	}
}

void InfoView::addTitleEntry(QString title)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::addTitleEntry" << title;
    #endif
    }

	int atRow = infoModel->rowCount();
	QStandardItem *itemKey = new QStandardItem(title);
	infoModel->insertRow(atRow, itemKey);

	QFont boldFont;
	boldFont.setBold(true);
    itemKey->setData(boldFont, Qt::FontRole);
}

void InfoView::copyEntry()
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::copyEntry";
    #endif
    }
	if (selectedEntry.isValid())
		QApplication::clipboard()->setText(infoModel->itemFromIndex(selectedEntry)->toolTip());
}

void InfoView::clearInfo()
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::clearInfo";
    #endif
    }
    infoModel->clear();
}

// Get embedded jpg if raw file from here???
void InfoView::updateInfo(const QString &fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::updateInfo";
    #endif
    }

    infoModel->clear();

    QString key;
    QString val;

    QFileInfo imageInfo = QFileInfo(fPath);
//    infoView->addTitleEntry(tr("General             "));

//    qDebug() << "InfoView::updateInfo - check isLoaded" << fPath;
    if (!metadata->isLoaded(fPath)) {
        metadata->loadImageMetadata(fPath, true, true);
    }

    key = tr("Folder");
    val = imageInfo.dir().dirName();
    addEntry(key, val);

    key = tr("File name");
    val = imageInfo.fileName();
    addEntry(key, val);

    key = tr("Location");
    val = imageInfo.path();
    addEntry(key, val);

    key = tr("Size");
    val = QString::number(imageInfo.size() / 1024000.0, 'f', 2) + " MB";
    addEntry(key, val);

    key = tr("Date/Time");
    val = metadata->getDateTime(fPath);
    addEntry(key, val);

    key = tr("Modified");
    val = imageInfo.lastModified().toString(Qt::SystemLocaleShortDate);
    addEntry(key, val);

    key = tr("Dimensions");
    uint width = metadata->getWidth(fPath);
    uint height = metadata->getHeight(fPath);
    val = QString::number(width) + "x" + QString::number(height);
    addEntry(key, val);

    key = tr("Megapixels");
    val = QString::number((width * height) / 1000000.0, 'f', 2);
    addEntry(key, val);

    key = tr("Model");
    val = metadata->getModel(fPath);
    addEntry(key, val);

    key = tr("Lens");
    val = metadata->getLens(fPath);
    addEntry(key, val);

    key = tr("Shutter speed");
    val = metadata->getExposureTime(fPath);
    addEntry(key, val);

    key = tr("Aperture");
    val = metadata->getAperture(fPath);
    addEntry(key, val);

    key = tr("ISO");
    val = metadata->getISO(fPath);
    addEntry(key, val);

    key = tr("Focal length");
    val = metadata->getFocalLength(fPath);
    addEntry(key, val);

    key = tr("Title");
    val = metadata->getTitle(fPath);
    addEntry(key, val);

    key = tr("Creator");
    val = metadata->getCreator(fPath);
    addEntry(key, val);

    key = tr("Copyright");
    val = metadata->getCopyright(fPath);
    addEntry(key, val);

    key = tr("Email");
    val = metadata->getEmail(fPath);
    addEntry(key, val);

    key = tr("Url");
    val = metadata->getUrl(fPath);
    addEntry(key, val);

    key = tr("Position");
    val = mwInfo->getPosition();
    addEntry(key, val);

    key = tr("Zoom");
    val = mwInfo->getZoom();
    addEntry(key, val);

    key = tr("Picked");
    val = mwInfo->getPicked();
    addEntry(key, val);

    if (G::isThreadTrackingOn) qDebug()
        << "ThumbView::updateExifInfo - loaded metadata display info for"
        << fPath;

//    key = tr("Error");
//    val = metadata->getErr(imageFullPath);
//    addEntry(key, val);

    tweakHeaders();
    showOrHide();
}

