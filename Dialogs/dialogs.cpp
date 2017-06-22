

#include "dialogs.h"
#include "global.h"

WsDlg::WsDlg(QList<QString> *wsList, QWidget *parent)
    : QDialog(parent)
{
    cb = new QComboBox;
    cb->setEditable(true);
    for (int i=0; i < wsList->count(); i++) {
        cb->addItem(wsList->at(i));
    }
    QPushButton *deleteBtn = new QPushButton("Delete");
    QPushButton *reassignBtn = new QPushButton("Reassign");
    QPushButton *doneBtn = new QPushButton("Done");
    doneBtn->setDefault(true);

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(cb, 0, 0);
    layout->addWidget(deleteBtn, 0, 1);
    layout->addWidget(reassignBtn, 0, 2);
    layout->addWidget(doneBtn, 0, 3);
    setLayout(layout);
    setWindowTitle("Manage Workspaces");
    connect(deleteBtn, SIGNAL(clicked()), SLOT(on_deleteBtn_clicked()));
    connect(reassignBtn, SIGNAL(clicked()), SLOT(on_reassignBtn_clicked()));
    connect(doneBtn, SIGNAL(clicked()), SLOT(on_doneBtn_clicked()));

    connect(cb, SIGNAL(activated(QString)), SLOT(on_cb_activated(QString)));
    connect(cb, SIGNAL(editTextChanged(QString)),
            SLOT(on_cb_editTextChanged(QString)));
}

WsDlg::~WsDlg()
{
}

void WsDlg::on_deleteBtn_clicked()
{
    qDebug() << "Delete";
    int n = cb->currentIndex();
    emit deleteWorkspace(n);
    cb->removeItem(n);
}

void WsDlg::on_reassignBtn_clicked()
{
    int n = cb->currentIndex();
    emit reassignWorkspace(n);
}

void WsDlg::on_doneBtn_clicked()
{

}

void WsDlg::on_cb_highlighted(int idx) {
    qDebug() << "on_cb_highlighted(idx)" << idx;
    isNewItem = true;
}

void WsDlg::on_cb_currentIndexChanged(const int idx) {
    qDebug() << "on_cb_currentIndexChanged(idx)" << idx;
    isNewItem = true;
}

void WsDlg::on_cb_activated(const QString name) {
    qDebug() << "on_cb_activated" << name;
    currentName = name;
}

void WsDlg::on_cb_currentTextChanged(const QString &name)
{
    static int x = 0;
    x++;
    qDebug() << x << "on_cb_currentTextChanged" << name;
    if (isNewItem) return;
    isNewItem = false;
    int n = cb->currentIndex();
    emit renameWorkspace(n, name);
    cb->setItemText(cb->currentIndex(), name);
}

void WsDlg::on_cb_editTextChanged(const QString &name)
{
    qDebug() << "on_cb_editTextChanged" << name;
    if (name == currentName) return;
    int n = cb->currentIndex();
    emit renameWorkspace(n, name);
    cb->setItemText(cb->currentIndex(), name);
}

//CpMvDialog::CpMvDialog(QWidget *parent) : QDialog(parent)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "CpMvDialog::CpMvDialog";
//    #endif
//    }
//	abortOp = false;

//    opLabel = new QLabel("");
    
//    cancelButton = new QPushButton(tr("Cancel"));
//   	cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));
//    cancelButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
//    connect(cancelButton, SIGNAL(clicked()), this, SLOT(abort()));

//    QHBoxLayout *topLayout = new QHBoxLayout;
//    topLayout->addWidget(opLabel);

//    QHBoxLayout *buttonsLayout = new QHBoxLayout;
//    buttonsLayout->addWidget(cancelButton);

//    QVBoxLayout *mainLayout = new QVBoxLayout;
//    mainLayout->addLayout(topLayout);
//    mainLayout->addLayout(buttonsLayout, Qt::AlignRight);
//    setLayout(mainLayout);
//}

//void CpMvDialog::abort()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "CpMvDialog::abort";
//    #endif
//    }
//	abortOp = true;
//}

//static QString autoRename(QString &destDir, QString &currFile)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "CpMvDialog::autoRename";
//    #endif
//    }
//	int extSep = currFile.lastIndexOf(".");
//	QString nameOnly = currFile.left(extSep);
//	QString extOnly = currFile.right(currFile.size() - extSep - 1);
//	QString newFile;

//	int idx = 1;

//	do
//	{
//		newFile = QString(nameOnly + "_copy_%1." + extOnly).arg(idx);
//		++idx;
//	}
//	while (idx && (QFile::exists(destDir + QDir::separator() + newFile)));

//	return newFile;
//}

//int cpMvFile(bool isCopy, QString &srcFile, QString &srcPath, QString &dstPath, QString &dstDir)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "CpMvDialog::cpMvFile";
//    #endif
//    }
//	int res;
	
//	if (isCopy)
//		res = QFile::copy(srcPath, dstPath);
//	else
//		res = QFile::rename(srcPath, dstPath);
		
//	if (!res && QFile::exists(dstPath))
//	{
//		QString newName = autoRename(dstDir, srcFile);
//		QString newDestPath = dstDir + QDir::separator() + newName;
		
//		if (isCopy)
//			res = QFile::copy(srcPath, newDestPath);
//		else
//			res = QFile::rename(srcPath, newDestPath);
//		dstPath = newDestPath;
//	}

//	return res;
//}

//void CpMvDialog::exec(ThumbView *thumbView, QString &destDir, bool pasteInCurrDir)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "CpMvDialog::exec";
//    #endif
//    }
//	int res = 0;
//	QString sourceFile;
//	QFileInfo fileInfo;
//	QString currFile;
//	QString destFile;
//	int tn;

//	show();

//	if (pasteInCurrDir)
//	{
//		for (tn = 0; tn < GData::copyCutFileList.size(); ++tn)
//		{
//			sourceFile = GData::copyCutFileList[tn];
//			fileInfo = QFileInfo(sourceFile);
//			currFile = fileInfo.fileName();
//			destFile = destDir + QDir::separator() + currFile;

//			opLabel->setText((GData::copyOp? tr("Copying \"%1\" to \"%2\".") : tr("Moving \"%1\" to \"%2\"."))
//											.arg(sourceFile).arg(destFile));
//			QApplication::processEvents();

//			res = cpMvFile(GData::copyOp, currFile, sourceFile, destFile, destDir);

//			if (!res || abortOp)
//			{
//				break;
//			}
//			else
//			{
//				GData::copyCutFileList[tn] = destFile;
//			}
//		}
//	}
//	else
//	{
//		QList<int> rowList;
//		for (tn = GData::copyCutIdxList.size() - 1; tn >= 0 ; --tn)
//		{
//			sourceFile = thumbView->thumbViewModel->item(GData::copyCutIdxList[tn].row())->
//											data(thumbView->FileNameRole).toString();
//			fileInfo = QFileInfo(sourceFile);
//			currFile = fileInfo.fileName();
//			destFile = destDir + QDir::separator() + currFile;

//			opLabel->setText((GData::copyOp?
//				tr("Copying \"%1\" to \"%2\".")
//				: tr("Moving \"%1\" to \"%2\".")).arg(sourceFile).arg(destFile));
//			QApplication::processEvents();

//			res = cpMvFile(GData::copyOp, currFile, sourceFile, destFile, destDir);

//			if (!res || abortOp) {
//				break;
//			}

//			rowList.append(GData::copyCutIdxList[tn].row());
//		}

//		if (!GData::copyOp)
//		{
//			qSort(rowList);
//			for (int t = rowList.size() - 1; t >= 0; --t)
//				thumbView->thumbViewModel->removeRow(rowList.at(t));
//		}
//		latestRow = rowList.at(0);
//	}

//	nfiles = GData::copyCutIdxList.size();
//	close();
//}

ShortcutsTableView::ShortcutsTableView()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ShortcutsTableView::ShortcutsTableView";
    #endif
    }
	keysModel = new QStandardItemModel();
	keysModel->setHorizontalHeaderItem(0, new QStandardItem(tr("Action")));
	keysModel->setHorizontalHeaderItem(1, new QStandardItem(tr("Shortcut")));
	keysModel->setHorizontalHeaderItem(2, new QStandardItem(""));
	setModel(keysModel);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	verticalHeader()->hide();
	verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
	horizontalHeader()->setHighlightSections(false);
	horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	setColumnHidden(2, true);

	shortcutsMenu = new QMenu("");
	clearAction = new QAction(tr("Delete shortcut"), this);
	connect(clearAction, SIGNAL(triggered()), this, SLOT(clearShortcut()));
	shortcutsMenu->addAction(clearAction);
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showShortcutsTableMenu(QPoint)));
}

void ShortcutsTableView::addRow(QString action, QString description, QString shortcut)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ShortcutsTableView::addRow";
    #endif
    }
	keysModel->appendRow(QList<QStandardItem*>() << new QStandardItem(description) << new QStandardItem(shortcut) << new QStandardItem(action));
}

void ShortcutsTableView::keyPressEvent(QKeyEvent *e)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ShortcutsTableView::keyPressEvent";
    #endif
    }
	if (!this->selectedIndexes().count()) {
		return;
	}
	QString keySeqText;
	QString keyText("");
	QString modifierText("");

	if (e->modifiers() & Qt::ShiftModifier)
		modifierText += "Shift+";		
	if (e->modifiers() & Qt::ControlModifier)
		modifierText += "Ctrl+";
	if (e->modifiers() & Qt::AltModifier)
		modifierText += "Alt+";

	if (	(e->key() >= Qt::Key_Shift &&  e->key() <= Qt::Key_ScrollLock)
		|| 	(e->key() >= Qt::Key_Super_L &&  e->key() <= Qt::Key_Direction_R)
		||	e->key() == Qt::Key_AltGr
		||	e->key() < 0) 
	{
		return;
	}

	keyText = QKeySequence(e->key()).toString();
	keySeqText = modifierText + keyText;

	if (e->modifiers() & Qt::AltModifier && (e->key() > Qt::Key_0 &&  e->key() <= Qt::Key_Colon))
	{
		QMessageBox msgBox;
		msgBox.warning(this, tr("Set shortcut"),
						tr("\"%1\" is reserved for shortcuts to external applications.").arg(keySeqText));
		return;
	}

	QMapIterator<QString, QAction *> it(G::actionKeys);
	while (it.hasNext())
	{
		it.next();
		if (it.value()->shortcut().toString() == keySeqText)
		{
			QMessageBox msgBox;
			msgBox.warning(this, tr("Set shortcut"),
						tr("\"%1\" is already assigned to \"%2\" action.").arg(keySeqText).arg(it.key()));
			return;
		}
	}
	
	QStandardItemModel *mod = (QStandardItemModel*)model();
	int row = selectedIndexes().first().row();
	mod->item(row, 1)->setText(keySeqText);
	G::actionKeys.value(mod->item(row, 2)->text())->setShortcut(QKeySequence(keySeqText));
}

void ShortcutsTableView::clearShortcut()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ShortcutsTableView::clearShortcut";
    #endif
    }
	if (selectedEntry.isValid()) {
		QStandardItemModel *mod = (QStandardItemModel*)model();
		mod->item(selectedEntry.row(), 1)->setText("");
		G::actionKeys.value(mod->item(selectedEntry.row(), 2)->text())->setShortcut(QKeySequence(""));
	}
}

void ShortcutsTableView::showShortcutsTableMenu(QPoint pt)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ShortcutsTableView::showShortcutsTableMenu";
    #endif
    }
	selectedEntry = indexAt(pt);
	if (selectedEntry.isValid())
		shortcutsMenu->popup(viewport()->mapToGlobal(pt));

}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::SettingsDialog";
    #endif
    }
	setWindowTitle(tr("Preferences"));
	setWindowIcon(QIcon::fromTheme("preferences-other", QIcon(":/images/phototonic.png")));

	// Image Viewer Options
	// Zoom large images
	QGroupBox *fitLargeGroupBox = new QGroupBox(tr("Fit Large Images"));
	fitLargeRadios[0] = new QRadioButton(tr("Disable"));
	fitLargeRadios[1] = new QRadioButton(tr("By width or height"));
	fitLargeRadios[2] = new QRadioButton(tr("By width"));
	fitLargeRadios[3] = new QRadioButton(tr("By height"));
	fitLargeRadios[4] = new QRadioButton(tr("Stretch disproportionately"));
	QVBoxLayout *fitLargeVbox = new QVBoxLayout;
	for (int i = 0; i < nZoomRadios; ++i)
	{
		fitLargeVbox->addWidget(fitLargeRadios[i]);
		fitLargeRadios[i]->setChecked(false);
	}
	fitLargeVbox->addStretch(1);
	fitLargeGroupBox->setLayout(fitLargeVbox);
//	fitLargeRadios[GData::zoomOutFlags]->setChecked(true);
 	
	// Zoom small images
	QGroupBox *fitSmallGroupBox = new QGroupBox(tr("Fit Small Images"));
	fitSmallRadios[0] = new QRadioButton(tr("Disable"));
	fitSmallRadios[1] = new QRadioButton(tr("By width or height"));
	fitSmallRadios[2] = new QRadioButton(tr("By width"));
	fitSmallRadios[3] = new QRadioButton(tr("By height"));
	fitSmallRadios[4] = new QRadioButton(tr("Stretch disproportionately"));
	QVBoxLayout *fitSmallVbox = new QVBoxLayout;
	for (int i = 0; i < nZoomRadios; ++i)
	{
		fitSmallVbox->addWidget(fitSmallRadios[i]);
		fitSmallRadios[i]->setChecked(false);
	}
	fitSmallVbox->addStretch(1);
	fitSmallGroupBox->setLayout(fitSmallVbox);
//	fitSmallRadios[GData::zoomInFlags]->setChecked(true);

	// imageView background color
	QLabel *backgroundColorLab = new QLabel(tr("Background color:"));
	backgroundColorButton = new QToolButton();
	backgroundColorButton->setFixedSize(48, 24);
	QHBoxLayout *bgColBox = new QHBoxLayout;
	bgColBox->addWidget(backgroundColorLab);
	bgColBox->addWidget(backgroundColorButton);
	bgColBox->addStretch(1);
	connect(backgroundColorButton, SIGNAL(clicked()), this, SLOT(pickColor()));
//	setButtonBgColor(GData::backgroundColor, backgroundColorButton);
//	backgroundColorButton->setAutoFillBackground(true);
//	bgColor = GData::backgroundColor;

	// Exit when opening image
	exitCliCb = new 
				QCheckBox(tr("Exit instead of closing, when image is loaded from command line"), this);
//	exitCliCb->setChecked(GData::exitInsteadOfClose);

	// Wrap image list
	wrapListCb = new QCheckBox(tr("Wrap image list when reaching last or first image"), this);
//	wrapListCb->setChecked(GData::wrapImageList);

	// Save quality
	QLabel *saveQualityLab = new QLabel(tr("Default quality when saving images:"));
	saveQualitySpin = new QSpinBox;
	saveQualitySpin->setRange(0, 100);
//	saveQualitySpin->setValue(GData::defaultSaveQuality);
	QHBoxLayout *saveQualityHbox = new QHBoxLayout;
	saveQualityHbox->addWidget(saveQualityLab);
	saveQualityHbox->addWidget(saveQualitySpin);
	saveQualityHbox->addStretch(1);

	// Enable animations
	enableAnimCb = new QCheckBox(tr("Enable GIF animation"), this);
//	enableAnimCb->setChecked(GData::enableAnimations);

	// Enable image Exif rotation
	enableExifCb = new QCheckBox(tr("Rotate image according to Exif orientation"), this);
//	enableExifCb->setChecked(GData::exifRotationEnabled);

	// Image Info
	imageInfoCb = new QCheckBox(tr("Show image file name in viewer"), this);
//    imageInfoCb->setChecked(G::shootingInfoVisible);

	// Viewer options
	QVBoxLayout *viewerOptsBox = new QVBoxLayout;
	QHBoxLayout *zoomOptsBox = new QHBoxLayout;
	zoomOptsBox->setAlignment(Qt::AlignTop);
	zoomOptsBox->addWidget(fitLargeGroupBox);
	zoomOptsBox->addWidget(fitSmallGroupBox);
	zoomOptsBox->addStretch(1);

	viewerOptsBox->addLayout(zoomOptsBox);
	viewerOptsBox->addLayout(bgColBox);
	viewerOptsBox->addWidget(enableExifCb);
	viewerOptsBox->addWidget(imageInfoCb);
	viewerOptsBox->addWidget(wrapListCb);
	viewerOptsBox->addWidget(enableAnimCb);
	viewerOptsBox->addLayout(saveQualityHbox);
	viewerOptsBox->addWidget(exitCliCb);
	viewerOptsBox->addStretch(1);

	// thumbView background color
	QLabel *bgThumbTxtLab = new QLabel(tr("Background color:"));
	colThumbButton = new QToolButton();
	colThumbButton->setFixedSize(48, 24);
	QHBoxLayout *bgThumbColBox = new QHBoxLayout;
	bgThumbColBox->addWidget(bgThumbTxtLab);
	bgThumbColBox->addWidget(colThumbButton);
	connect(colThumbButton, SIGNAL(clicked()), this, SLOT(pickThumbsColor()));
//	setButtonBgColor(GData::thumbsBackgroundColor, colThumbButton);
	colThumbButton->setAutoFillBackground(true);
//	thumbBgColor = GData::thumbsBackgroundColor;

	// thumbView text color
	QLabel *txtThumbTxtLab = new QLabel("\t" + tr("Label color:"));
	colThumbTextButton = new QToolButton();
	colThumbTextButton->setFixedSize(48, 24);
	bgThumbColBox->addWidget(txtThumbTxtLab);
	bgThumbColBox->addWidget(colThumbTextButton);
	bgThumbColBox->addStretch(1);
	connect(colThumbTextButton, SIGNAL(clicked()), this, SLOT(pickThumbsTextColor()));
//	setButtonBgColor(GData::thumbsTextColor, colThumbTextButton);
	colThumbTextButton->setAutoFillBackground(true);
//	thumbTextColor = GData::thumbsTextColor;

	// thumbview background image
	QLabel *thumbsBackImageLab = new QLabel(tr("Background image:"));
	thumbsBackImageEdit = new QLineEdit;
	thumbsBackImageEdit->setClearButtonEnabled(true);
	thumbsBackImageEdit->setMinimumWidth(200);

	QToolButton *chooseThumbsBackImageButton = new QToolButton();
	chooseThumbsBackImageButton->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/open.png")));
	chooseThumbsBackImageButton->setFixedSize(26, 26);
	chooseThumbsBackImageButton->setIconSize(QSize(16, 16));
	connect(chooseThumbsBackImageButton, SIGNAL(clicked()), this, SLOT(pickBgImage()));
	
	QHBoxLayout *thumbsBackImageEditBox = new QHBoxLayout;
	thumbsBackImageEditBox->addWidget(thumbsBackImageLab);
	thumbsBackImageEditBox->addWidget(thumbsBackImageEdit);
	thumbsBackImageEditBox->addWidget(chooseThumbsBackImageButton);
	thumbsBackImageEditBox->addStretch(1);
//	thumbsBackImageEdit->setText(GData::thumbsBackImage);

	// Thumbnail spacing
	QLabel *thumbSpacingLab = new QLabel(tr("Add space between thumbnails:"));
	thumbSpacingSpin = new QSpinBox;
	thumbSpacingSpin->setRange(0, 15);
//	thumbSpacingSpin->setValue(G::thumbSpacing);
	QHBoxLayout *thumbSpacingHbox = new QHBoxLayout;
	thumbSpacingHbox->addWidget(thumbSpacingLab);
	thumbSpacingHbox->addWidget(thumbSpacingSpin);
	thumbSpacingHbox->addStretch(1);

	// Do not enlarge small thumbs
	noSmallThumbCb = new 
				QCheckBox(tr("Show original size of images smaller than the thumbnail size"), this);
//	noSmallThumbCb->setChecked(GData::noEnlargeSmallThumb);

	// Thumbnail pages to read ahead
	QLabel *thumbPagesLab = new QLabel(tr("Number of thumbnail pages to read ahead:"));
	thumbPagesSpin = new QSpinBox;
    thumbPagesSpin->setRange(1, 20);    // rgh increased from 10 to 20
//	thumbPagesSpin->setValue(GData::thumbPagesReadahead);
	QHBoxLayout *thumbPagesHbox = new QHBoxLayout;
	thumbPagesHbox->addWidget(thumbPagesLab);
	thumbPagesHbox->addWidget(thumbPagesSpin);
	thumbPagesHbox->addStretch(1);

	enableThumbExifCb = new QCheckBox(tr("Rotate thumbnails according to Exif orientation"), this);
//	enableThumbExifCb->setChecked(GData::exifThumbRotationEnabled);

	// Thumbnail options
	QVBoxLayout *thumbsOptsBox = new QVBoxLayout;
	thumbsOptsBox->addLayout(bgThumbColBox);
	thumbsOptsBox->addLayout(thumbsBackImageEditBox);
	thumbsOptsBox->addLayout(thumbSpacingHbox);
	thumbsOptsBox->addWidget(enableThumbExifCb);
	thumbsOptsBox->addLayout(thumbPagesHbox);
	thumbsOptsBox->addWidget(noSmallThumbCb);
	thumbsOptsBox->addStretch(1);

	// Slide show delay
	QLabel *slideDelayLab = new QLabel(tr("Delay between slides in seconds:"));
	slideDelaySpin = new QSpinBox;
	slideDelaySpin->setRange(1, 3600);
//	slideDelaySpin->setValue(G::slideShowDelay);
	QHBoxLayout *slideDelayHbox = new QHBoxLayout;
	slideDelayHbox->addWidget(slideDelayLab);
	slideDelayHbox->addWidget(slideDelaySpin);
	slideDelayHbox->addStretch(1);

	// Slide show random
	slideRandomCb = new QCheckBox(tr("Show random images"), this);
//	slideRandomCb->setChecked(G::slideShowRandom);

	// Slide show options
	QVBoxLayout *slideShowVbox = new QVBoxLayout;
	slideShowVbox->addLayout(slideDelayHbox);
	slideShowVbox->addWidget(slideRandomCb);
	slideShowVbox->addStretch(1);

	// Startup directory
	QGroupBox *startupDirGroupBox = new QGroupBox(tr("Startup folder"));
//	startupDirRadios[GData::defaultDir] =
//					new QRadioButton(tr("Default, or specified by command line argument"));
//	startupDirRadios[GData::rememberLastDir] = new QRadioButton(tr("Remember last"));
//	startupDirRadios[GData::specifiedDir] = new QRadioButton(tr("Specify:"));
	
	startupDirEdit = new QLineEdit;
	startupDirEdit->setClearButtonEnabled(true);
	startupDirEdit->setMinimumWidth(300);
	startupDirEdit->setMaximumWidth(400);

	QToolButton *chooseStartupDirButton = new QToolButton();
	chooseStartupDirButton->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/open.png")));
	chooseStartupDirButton->setFixedSize(26, 26);
	chooseStartupDirButton->setIconSize(QSize(16, 16));
	connect(chooseStartupDirButton, SIGNAL(clicked()), this, SLOT(pickStartupDir()));
	
	QHBoxLayout *startupDirEditBox = new QHBoxLayout;
	startupDirEditBox->addWidget(startupDirRadios[2]);
	startupDirEditBox->addWidget(startupDirEdit);
	startupDirEditBox->addWidget(chooseStartupDirButton);
	startupDirEditBox->addStretch(1);

	QVBoxLayout *startupDirVbox = new QVBoxLayout;
	for (int i = 0; i < 2; ++i)
	{
		startupDirVbox->addWidget(startupDirRadios[i]);
		startupDirRadios[i]->setChecked(false);
	}
	startupDirVbox->addLayout(startupDirEditBox);
	startupDirVbox->addStretch(1);
	startupDirGroupBox->setLayout(startupDirVbox);

//	if (GData::startupDir == GData::specifiedDir)
//		startupDirRadios[GData::specifiedDir]->setChecked(true);
//	else if (GData::startupDir == GData::rememberLastDir)
//		startupDirRadios[GData::rememberLastDir]->setChecked(true);
//	else
//		startupDirRadios[GData::defaultDir]->setChecked(true);
//	startupDirEdit->setText(GData::specifiedStartDir);

	// Keyboard shortcuts widgets
	ShortcutsTableView *keysTable = new ShortcutsTableView();
	QMapIterator<QString, QAction *> it(G::actionKeys);
	while (it.hasNext())
	{
		it.next();
		keysTable->addRow(it.key(), G::actionKeys.value(it.key())->text(), G::actionKeys.value(it.key())->shortcut().toString());
	}

	// Mouse settings
	reverseMouseCb = new QCheckBox(tr("Swap mouse left-click and middle-click actions"), this);
//	reverseMouseCb->setChecked(GData::reverseMouseBehavior);

	// Delete confirmation setting
	deleteConfirmCb = new QCheckBox(tr("Delete confirmation"), this);
//	deleteConfirmCb->setChecked(GData::deleteConfirm);

	// Keyboard and mouse
	QGroupBox *keyboardGrp = new QGroupBox(tr("Keyboard Shortcuts"));
	QVBoxLayout *keyboardVbox = new QVBoxLayout;
	keyboardVbox->addWidget(keysTable);
	keyboardGrp->setLayout(keyboardVbox);

	QVBoxLayout *generalVbox = new QVBoxLayout;
	generalVbox->addWidget(keyboardGrp);
	generalVbox->addWidget(reverseMouseCb);
	generalVbox->addWidget(deleteConfirmCb);
	generalVbox->addWidget(startupDirGroupBox);
	generalVbox->addStretch(1);
	
	/* Confirmation buttons */
	QHBoxLayout *buttonsHbox = new QHBoxLayout;
    QPushButton *okButton = new QPushButton(tr("OK"));
   	okButton->setIcon(QIcon::fromTheme("dialog-ok"));
    okButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	connect(okButton, SIGNAL(clicked()), this, SLOT(saveSettings()));
    okButton->setDefault(true);
	QPushButton *closeButton = new QPushButton(tr("Cancel"));
   	closeButton->setIcon(QIcon::fromTheme("dialog-cancel"));
	closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	connect(closeButton, SIGNAL(clicked()), this, SLOT(abort()));
	buttonsHbox->addWidget(closeButton, 1, Qt::AlignRight);
	buttonsHbox->addWidget(okButton, 0, Qt::AlignRight);

	/* Tabs */
	QTabWidget *tabs = new QTabWidget;

	QWidget *viewerSettings = new QWidget;
	viewerSettings->setLayout(viewerOptsBox);
	tabs->addTab(viewerSettings, tr("Viewer"));

	QWidget *thumbSettings = new QWidget;
	thumbSettings->setLayout(thumbsOptsBox);
	tabs->addTab(thumbSettings, tr("Thumbnails"));

	QWidget *slideSettings = new QWidget;
	slideSettings->setLayout(slideShowVbox);
	tabs->addTab(slideSettings, tr("Slide Show"));

	QWidget *generalSettings = new QWidget;
	generalSettings->setLayout(generalVbox);
	tabs->addTab(generalSettings, tr("General"));

	QVBoxLayout *mainVbox = new QVBoxLayout;
	mainVbox->addWidget(tabs);
	mainVbox->addLayout(buttonsHbox);
	setLayout(mainVbox);
}

void SettingsDialog::saveSettings()
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::saveSettings";
    #endif
    }
	int i;

	for (i = 0; i < nZoomRadios; ++i) {
		if (fitLargeRadios[i]->isChecked()) {
//			GData::zoomOutFlags = i;
//			GData::appSettings->setValue("zoomOutFlags", (int)GData::zoomOutFlags);
			break;
		}
	}

	for (i = 0; i < nZoomRadios; ++i) {
		if (fitSmallRadios[i]->isChecked()) {
//			GData::zoomInFlags = i;
//			GData::appSettings->setValue("zoomInFlags", (int)GData::zoomInFlags);
			break;
		}
	}

//	GData::backgroundColor = bgColor;
//	GData::thumbsBackgroundColor = thumbBgColor;
//	GData::thumbsTextColor = thumbTextColor;
//	GData::thumbsBackImage = thumbsBackImageEdit->text();
//	GData::thumbSpacing = thumbSpacingSpin->value();
//	GData::thumbPagesReadahead = thumbPagesSpin->value();
//	GData::exitInsteadOfClose = exitCliCb->isChecked();
//	GData::wrapImageList = wrapListCb->isChecked();
//	GData::defaultSaveQuality = saveQualitySpin->value();
//	GData::noEnlargeSmallThumb = noSmallThumbCb->isChecked();
//	GData::slideShowDelay = slideDelaySpin->value();
//	GData::slideShowRandom = slideRandomCb->isChecked();
//	GData::enableAnimations = enableAnimCb->isChecked();
//	GData::exifRotationEnabled = enableExifCb->isChecked();
//	GData::exifThumbRotationEnabled = enableThumbExifCb->isChecked();
//	GData::enableImageInfoFS = imageInfoCb->isChecked();
//	GData::reverseMouseBehavior = reverseMouseCb->isChecked();
//	GData::deleteConfirm = deleteConfirmCb->isChecked();

//	if (startupDirRadios[0]->isChecked())
//		GData::startupDir = GData::defaultDir;
//	else if (startupDirRadios[1]->isChecked())
//		GData::startupDir = GData::rememberLastDir;
//	else {
//		GData::startupDir = GData::specifiedDir;
//		GData::specifiedStartDir = startupDirEdit->text();
//	}

	accept();
}

void SettingsDialog::abort()
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::abort";
    #endif
    }
	reject();
}

void SettingsDialog::pickColor()
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::pickColor";
    #endif
    }
//	QColor userColor = QColorDialog::getColor(GData::backgroundColor, this);
//    if (userColor.isValid()) {
//		setButtonBgColor(userColor, backgroundColorButton);
//        bgColor = userColor;
//    }
}

void SettingsDialog::setButtonBgColor(QColor &color, QToolButton *button)
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::setButtonBgColor";
    #endif
    }
	QString style = "background: rgb(%1, %2, %3);";
	style = style.arg(color.red()).arg(color.green()).arg(color.blue());
	button->setStyleSheet(style);
}

void SettingsDialog::pickThumbsColor()
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::pickThumbsColor";
    #endif
    }
//	QColor userColor = QColorDialog::getColor(GData::thumbsBackgroundColor, this);
//	if (userColor.isValid()) {
//		setButtonBgColor(userColor, colThumbButton);
//		thumbBgColor = userColor;
//	}
}

void SettingsDialog::pickThumbsTextColor()
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::pickThumbsTextColor";
    #endif
    }
//	QColor userColor = QColorDialog::getColor(GData::thumbsTextColor, this);
//	if (userColor.isValid()) {
//		setButtonBgColor(userColor, colThumbTextButton);
//		thumbTextColor = userColor;
//	}
}

void SettingsDialog::pickStartupDir()
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::pickStartupDir";
    #endif
    }
	QString dirName = QFileDialog::getExistingDirectory(this, tr("Choose Startup Folder"), "",
									QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	startupDirEdit->setText(dirName);
}

void SettingsDialog::pickBgImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "SettingsDialog::pickBgImage";
    #endif
    }
	QString dirName = QFileDialog::getOpenFileName(this, tr("Open File"), "",
                   tr("Images") + " (*.jpg *.jpeg *.jpe *.png *.bmp *.tiff *.tif *.ppm *.xbm *.xpm)");
	thumbsBackImageEdit->setText(dirName);
}

void AppMgmtDialog::addTableModelItem(QStandardItemModel *model, QString &key, QString &val)
{
	int atRow = model->rowCount();
	QStandardItem *itemKey = new QStandardItem(key);
	QStandardItem *itemKey2 = new QStandardItem(val);
	model->insertRow(atRow, itemKey);
	model->setItem(atRow, 1, itemKey2);
}

AppMgmtDialog::AppMgmtDialog(QWidget *parent) : QDialog(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "AppMgmtDialog::AppMgmtDialog";
    #endif
    }
	setWindowTitle(tr("Manage External Applications"));
	setWindowIcon(QIcon::fromTheme("document-properties", QIcon(":/images/phototonic.png")));
	resize(350, 250);

	appsTable = new QTableView(this);
	appsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
	appsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
	appsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	appsTableModel = new QStandardItemModel(this);
	appsTable->setModel(appsTableModel);
	appsTable->verticalHeader()->setVisible(false);
	appsTable->verticalHeader()->setDefaultSectionSize(appsTable->verticalHeader()->minimumSectionSize());
	appsTableModel->setHorizontalHeaderItem(0, new QStandardItem(QString(tr("Name"))));
	appsTableModel->setHorizontalHeaderItem(1,
									new QStandardItem(QString(tr("Application path and arguments"))));
	appsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
	appsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	appsTable->	setShowGrid(false);

	QHBoxLayout *addRemoveHbox = new QHBoxLayout;
    QPushButton *addButton = new QPushButton(tr("Choose"));
   	addButton->setIcon(QIcon::fromTheme("list-add"));
	connect(addButton, SIGNAL(clicked()), this, SLOT(add()));
	addRemoveHbox->addWidget(addButton, 0, Qt::AlignRight);
    QPushButton *entryButton = new QPushButton(tr("Add manually"));
	entryButton->setIcon(QIcon::fromTheme("list-add"));
	connect(entryButton, SIGNAL(clicked()), this, SLOT(entry()));
	addRemoveHbox->addWidget(entryButton, 0, Qt::AlignRight);
    QPushButton *removeButton = new QPushButton(tr("Remove"));
   	removeButton->setIcon(QIcon::fromTheme("list-remove"));
	connect(removeButton, SIGNAL(clicked()), this, SLOT(remove()));
	addRemoveHbox->addWidget(removeButton, 0, Qt::AlignRight);
	addRemoveHbox->addStretch(1);	

	QHBoxLayout *buttonsHbox = new QHBoxLayout;
    QPushButton *okButton = new QPushButton(tr("OK"));
   	okButton->setIcon(QIcon::fromTheme("dialog-ok"));
    okButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	connect(okButton, SIGNAL(clicked()), this, SLOT(ok()));
	buttonsHbox->addWidget(okButton, 0, Qt::AlignRight);
	QVBoxLayout *mainVbox = new QVBoxLayout;
	mainVbox->addWidget(appsTable);
	mainVbox->addLayout(addRemoveHbox);
	mainVbox->addLayout(buttonsHbox);
	setLayout(mainVbox);

	// Load external apps list
	QString key, val;
	QMapIterator<QString, QString> it(G::externalApps);
	while (it.hasNext())
	{
		it.next();
		key = it.key();
		val = it.value();
		addTableModelItem(appsTableModel, key, val);
	}
}

void AppMgmtDialog::ok()
{
    {
    #ifdef ISDEBUG
    qDebug() << "AppMgmtDialog::ok";
    #endif
    }
	int row = appsTableModel->rowCount();
	G::externalApps.clear();
    for (int i = 0; i < row ; ++i)
    {
	    if (!appsTableModel->itemFromIndex(appsTableModel->index(i, 1))->text().isEmpty())
	    {
			G::externalApps[appsTableModel->itemFromIndex(appsTableModel->index(i, 0))->text()] =
						appsTableModel->itemFromIndex(appsTableModel->index(i, 1))->text();
		}
    }
	accept();
}

void AppMgmtDialog::add()
{
    {
    #ifdef ISDEBUG
    qDebug() << "AppMgmtDialog::add";
    #endif
    }
	QString fileName = QFileDialog::getOpenFileName(this, tr("Choose Application"), "", "");
	if (fileName.isEmpty())
		return;
		
	QFileInfo fileInfo = QFileInfo(fileName);
	QString appName = fileInfo.fileName();
	addTableModelItem(appsTableModel, appName, fileName);
}

void AppMgmtDialog::entry()
{
    {
    #ifdef ISDEBUG
    qDebug() << "AppMgmtDialog::entry";
    #endif
    }
	int atRow = appsTableModel->rowCount();
	QStandardItem *itemKey = new QStandardItem(QString(tr("New Application")));
	appsTableModel->insertRow(atRow, itemKey);
}

void AppMgmtDialog::remove()
{
    {
    #ifdef ISDEBUG
    qDebug() << "AppMgmtDialog::remove";
    #endif
    }
	QModelIndexList indexesList;
	while((indexesList = appsTable->selectionModel()->selectedIndexes()).size())
	{
		appsTableModel->removeRow(indexesList.first().row());
	}
}

CopyMoveToDialog::CopyMoveToDialog(QWidget *parent, QString thumbsPath, bool move) : QDialog(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::CopyMoveToDialog";
    #endif
    }
	copyOp = !move;
	if (move) {
		setWindowTitle(tr("Move to..."));
		setWindowIcon(QIcon::fromTheme("go-next"));
	} else {
		setWindowTitle(tr("Copy to..."));
		setWindowIcon(QIcon::fromTheme("edit-copy"));
	}

	resize(350, 250);
	currentPath = thumbsPath;

	pathsTable = new QTableView(this);
	pathsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
	pathsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
	pathsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	pathsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	pathsTable->setSelectionMode(QAbstractItemView::SingleSelection);
	pathsTableModel = new QStandardItemModel(this);
	pathsTable->setModel(pathsTableModel);
	pathsTable->verticalHeader()->setVisible(false);
	pathsTable->horizontalHeader()->setVisible(false);
	pathsTable->verticalHeader()->setDefaultSectionSize(pathsTable->verticalHeader()->
																	minimumSectionSize());
	pathsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	pathsTable->	setShowGrid(false);

	connect(pathsTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), 
				this, SLOT(selection(QItemSelection, QItemSelection)));
   	connect(pathsTable, SIGNAL(doubleClicked(const QModelIndex &)), 
				this, SLOT(pathDoubleClick(const QModelIndex &)));

	QHBoxLayout *addRemoveHbox = new QHBoxLayout;
    QPushButton *addButton = new QPushButton(tr("Browse..."));
	connect(addButton, SIGNAL(clicked()), this, SLOT(add()));
    QPushButton *removeButton = new QPushButton(tr("Remove"));
	connect(removeButton, SIGNAL(clicked()), this, SLOT(remove()));
	addRemoveHbox->addWidget(removeButton, 0, Qt::AlignLeft);
	addRemoveHbox->addStretch(1);	
	addRemoveHbox->addWidget(addButton, 0, Qt::AlignRight);

	QHBoxLayout *buttonsHbox = new QHBoxLayout;
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
	cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));
    cancelButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(justClose()));

    QPushButton *okButton = new QPushButton(tr("OK"));
   	okButton->setIcon(QIcon::fromTheme("dialog-ok"));
    okButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    okButton->setDefault(true);

	connect(okButton, SIGNAL(clicked()), this, SLOT(copyOrMove()));

	buttonsHbox->addStretch(1);	
	buttonsHbox->addWidget(cancelButton, 0, Qt::AlignRight);
	buttonsHbox->addWidget(okButton, 0, Qt::AlignRight);

	destinationLab = new QLabel(tr("Destination:"));
	QFrame *line = new QFrame(this);
	line->setObjectName(QString::fromUtf8("line"));
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Sunken);

	QVBoxLayout *mainVbox = new QVBoxLayout;
	mainVbox->addWidget(pathsTable);
	mainVbox->addLayout(addRemoveHbox);
	mainVbox->addWidget(line);	
	mainVbox->addWidget(destinationLab);	
	mainVbox->addLayout(buttonsHbox);
	setLayout(mainVbox);

	// Load paths list
	QSetIterator<QString> it(G::bookmarkPaths);
	while (it.hasNext()) {
		QStandardItem *item = new QStandardItem(QIcon(":/images/bookmarks.png"), it.next());
		pathsTableModel->insertRow(pathsTableModel->rowCount(), item);
	}
	pathsTableModel->sort(0);
}

void CopyMoveToDialog::selection(const QItemSelection&, const QItemSelection&)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::selection";
    #endif
    }
	if (pathsTable->selectionModel()->selectedRows().size() > 0) {
		destinationLab->setText(tr("Destination:") + " " +
			pathsTableModel->item(pathsTable->selectionModel()->selectedRows().at(0).row())->text());
	}
}

void CopyMoveToDialog::pathDoubleClick(const QModelIndex &)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::pathDoubleClick";
    #endif
    }
	copyOrMove();
}

void CopyMoveToDialog::savePaths()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::savePaths";
    #endif
    }
	G::bookmarkPaths.clear();
    for (int i = 0; i < pathsTableModel->rowCount(); ++i) {
    	G::bookmarkPaths.insert
    						(pathsTableModel->itemFromIndex(pathsTableModel->index(i, 0))->text());
   	}
}

void CopyMoveToDialog::copyOrMove()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::copyOrMove";
    #endif
    }
	savePaths();

	QModelIndexList indexesList;
	if((indexesList = pathsTable->selectionModel()->selectedIndexes()).size()) {
		selectedPath = pathsTableModel->itemFromIndex(indexesList.first())->text();
		accept();
	} else {
		reject();
	}
}

void CopyMoveToDialog::justClose()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::justClose";
    #endif
    }
	savePaths();
	reject();
}

void CopyMoveToDialog::add()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::add";
    #endif
    }
	QString dirName = QFileDialog::getExistingDirectory(this, tr("Choose Folder"), currentPath,
									QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (dirName.isEmpty())
		return;
		
	QStandardItem *item = new QStandardItem(QIcon(":/images/bookmarks.png"), dirName);
	pathsTableModel->insertRow(pathsTableModel->rowCount(), item);

	pathsTable->selectionModel()->clearSelection();
	pathsTable->selectionModel()->select(pathsTableModel->index(pathsTableModel->rowCount() - 1, 0),
															QItemSelectionModel::Select);
}

void CopyMoveToDialog::remove()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CopyMoveToDialog::remove";
    #endif
    }
	QModelIndexList indexesList;
	if((indexesList = pathsTable->selectionModel()->selectedIndexes()).size()) {
		pathsTableModel->removeRow(indexesList.first().row());
	}
}

ProgressDialog::ProgressDialog(QWidget *parent) : QDialog(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ProgressDialog::ProgressDialog";
    #endif
    }
    opLabel = new QLabel("");
    abortOp = false;
    
    cancelButton = new QPushButton(tr("Cancel"));
   	cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));
    cancelButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(abort()));

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->addWidget(opLabel);

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(buttonsLayout, Qt::AlignRight);
    setLayout(mainLayout);
}

void ProgressDialog::abort()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ProgressDialog::abort";
    #endif
    }
	abortOp = true;
}
