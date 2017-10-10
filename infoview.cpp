

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

    infoModel = new QStandardItemModel(this);
    setModel(infoModel);

//  setSelectionBehavior(QAbstractItemView::SelectItems);
//	setSelectionMode(QAbstractItemView::ExtendedSelection);
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

void InfoView::addEntry(QString &key, QString &value)
{
    {
    #ifdef ISDEBUG
    qDebug() << "InfoView::addEntry";
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
    qDebug() << "InfoView::addTitleEntry";
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

    qDebug() << "InfoView::updateInfo" << fPath;
    QString key;
    QString val;

    QFileInfo imageInfo = QFileInfo(fPath);
//    infoView->addTitleEntry(tr("General             "));

//    qDebug() << "InfoView::updateInfo - check isLoaded" << fPath;
    if (!metadata->isLoaded(fPath)) {
        metadata->loadImageMetadata(fPath);
    }
    key = tr("File name");
    val = imageInfo.fileName();
    addEntry(key, val);

    key = tr("Location");
    val = imageInfo.path();
    addEntry(key, val);

    key = tr("Size");
    val = QString::number(imageInfo.size() / 1024000.0, 'f', 2) + " MB";
    addEntry(key, val);

    key = tr("Date");
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

    key = tr("Megapixel");
    val = QString::number((width * height) / 1000000.0, 'f', 2);
    addEntry(key, val);

    key = tr("Model");
    val = metadata->getModel(fPath);
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

    if (G::isThreadTrackingOn) qDebug()
        << "ThumbView::updateExifInfo - loaded metadata display info for"
        << fPath;

//    key = tr("Error");
//    val = metadata->getErr(imageFullPath);
//    addEntry(key, val);

    tweakHeaders();
}

