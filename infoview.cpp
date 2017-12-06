

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
    ok = new QStandardItemModel;

    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Show")));

    ok->insertRow(0);  ok->setData(ok->index(0, 0), "File Name");
    ok->insertRow(1);  ok->setData(ok->index(1, 0), "Location");
    ok->insertRow(2);  ok->setData(ok->index(2, 0), "Size");
    ok->insertRow(3);  ok->setData(ok->index(3, 0), "Date/Time");
    ok->insertRow(4);  ok->setData(ok->index(4, 0), "Modified");
    ok->insertRow(5);  ok->setData(ok->index(5, 0), "Dimensions");
    ok->insertRow(6);  ok->setData(ok->index(6, 0), "Megapixels");
    ok->insertRow(7);  ok->setData(ok->index(7, 0), "Model");
    ok->insertRow(8);  ok->setData(ok->index(8, 0), "Lens");
    ok->insertRow(9);  ok->setData(ok->index(9, 0), "Shutter speed");
    ok->insertRow(10);  ok->setData(ok->index(10, 0), "Aperture");
    ok->insertRow(11);  ok->setData(ok->index(11, 0), "ISO");
    ok->insertRow(12);  ok->setData(ok->index(12, 0), "Focal length");
    ok->insertRow(13);  ok->setData(ok->index(13, 0), "Title");
    ok->insertRow(14);  ok->setData(ok->index(14, 0), "Creator");
    ok->insertRow(15);  ok->setData(ok->index(15, 0), "Copyright");
    ok->insertRow(16);  ok->setData(ok->index(16, 0), "Email");
    ok->insertRow(17);  ok->setData(ok->index(17, 0), "Url");

    for(int row = 0; row < ok->rowCount(); row++)
        ok->setData(ok->index(row, 1), true);

    connect(ok, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            this, SLOT(showOrHide()));
}

void InfoView::showOrHide()
{
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

    if (G::isThreadTrackingOn) qDebug()
        << "ThumbView::updateExifInfo - loaded metadata display info for"
        << fPath;

//    key = tr("Error");
//    val = metadata->getErr(imageFullPath);
//    addEntry(key, val);

    tweakHeaders();
    showOrHide();
}

