#include "Dialogs/prefdlg.h"
#include "ui_prefdlg.h"
#include "Main/mainwindow.h"
#include "Main/global.h"

#include <QDebug>

// this works because prefdlg is a friend class of MW
MW *m0;

Prefdlg::Prefdlg(QWidget *parent, int lastPrefPage) :
    QDialog(parent),
    ui(new Ui::Prefdlg)
{
    okToUpdate = false;
    ui->setupUi(this);
    ui->listWidget->setCurrentRow(lastPrefPage);

//    // this works because prefdlg is a friend class of MW
//    MW *mw = qobject_cast<MW*>(parent);

//    MW *mw;
    m0 = qobject_cast<MW*>(parent);
    // general
    ui->globalFontSizeSlider->setTickInterval(1);
    ui->globalFontSizeSlider->setMinimum(8);
    ui->globalFontSizeSlider->setMaximum(20);
    ui->globalFontSizeSlider->setValue(G::fontSize.toInt());
    ui->globalFontSizeLbl->setText(G::fontSize);

    ui->infoFontSizeSlider->setTickInterval(2);
    ui->infoFontSizeSlider->setMinimum(6);
    ui->infoFontSizeSlider->setMaximum(34);
    ui->infoFontSizeLbl->setText(QString::number(m0->imageView->infoOverlayFontSize));
    qDebug() << ui->infoFontSizeLbl->text();

    ui->classificationBadgeImageDiamSlider->setValue(m0->classificationBadgeInImageDiameter);
    ui->classificationBadgeImageDiamLbl->setText(QString::number(m0->classificationBadgeInImageDiameter));
    ui->classificationBadgeThumbDiamSlider->setValue(m0->classificationBadgeInThumbDiameter);
    ui->classificationBadgeThumbDiamLbl->setText(QString::number(m0->classificationBadgeInThumbDiameter));

    ui->rememberFolderChk->setChecked(m0->rememberLastDir);
    ui->updateAppChk->setChecked(m0->checkIfUpdate);
    ui->trackpadIterateRadio->setChecked(!m0->imageView->useWheelToScroll);
    ui->trackpadScrollRadio->setChecked(m0->imageView->useWheelToScroll);
    ui->mouseClickScrollChk->setChecked((m0->mouseClickScroll));

    // thumbs
    ui->iconWidthSlider->setSingleStep(1);
    ui->iconWidthSlider->setTickInterval(50);
    ui->iconWidthSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconWidthSlider->setMaximum(THUMB_MAX);
    ui->iconWidthSlider->setValue(m0->thumbView->thumbWidth);
    ui->iconHeightSlider->setSingleStep(1);
    ui->iconHeightSlider->setTickInterval(50);
    ui->iconHeightSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconHeightSlider->setMaximum(THUMB_MAX);
    ui->iconHeightSlider->setValue(m0->thumbView->thumbHeight);
    ui->thumbSpacingSlider->setSingleStep(1);
    ui->thumbSpacingSlider->setTickInterval(3);
    ui->thumbSpacingSlider->setTickPosition(QSlider::TicksAbove);
    ui->thumbSpacingSlider->setValue(m0->thumbView->thumbSpacing);
    ui->iconPaddingSlider->setSingleStep(1);
    ui->iconPaddingSlider->setTickInterval(1);
    ui->iconPaddingSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconPaddingSlider->setValue(m0->thumbView->thumbPadding);
    ui->fontSizeSlider->setSingleStep(1);
    ui->fontSizeSlider->setTickInterval(1);
    ui->fontSizeSlider->setTickPosition(QSlider::TicksAbove);
    ui->fontSizeSlider->setValue(m0->thumbView->labelFontSize);
    ui->showThumbLabelChk->setChecked(m0->thumbView->showThumbLabels);
    ui->wrapChk->setChecked(m0->thumbView->wrapThumbs);
    ui->bestAspectChk->setChecked(m0->thumbView->isBestAspect);
    if (ui->bestAspectChk->isChecked()) ui->lockDimChk->setChecked(false);
    else ui->lockDimChk->setChecked(true);

    // grid
    ui->iconWidthSlider_2->setSingleStep(1);
    ui->iconWidthSlider_2->setTickInterval(50);
    ui->iconWidthSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconWidthSlider_2->setMaximum(THUMB_MAX);
    ui->iconWidthSlider_2->setValue(m0->gridView->thumbWidth);
    ui->iconHeightSlider_2->setSingleStep(1);
    ui->iconHeightSlider_2->setTickInterval(50);
    ui->iconHeightSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconHeightSlider_2->setMaximum(THUMB_MAX);
    ui->iconHeightSlider_2->setValue(m0->gridView->thumbHeight);
    ui->thumbSpacingSlider_2->setSingleStep(1);
    ui->thumbSpacingSlider_2->setTickInterval(3);
    ui->thumbSpacingSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->thumbSpacingSlider_2->setValue(m0->gridView->thumbSpacing);
    ui->iconPaddingSlider_2->setSingleStep(1);
    ui->iconPaddingSlider_2->setTickInterval(1);
    ui->iconPaddingSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconPaddingSlider_2->setValue(m0->gridView->thumbPadding);
    ui->fontSizeSlider_2->setTickInterval(1);
    ui->fontSizeSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->fontSizeSlider_2->setValue(m0->thumbView->labelFontSize);
    ui->fontSizeSlider_2->setValue(m0->gridView->labelFontSize);
    ui->showThumbLabelChk_2->setChecked(m0->gridView->showThumbLabels);
    ui->bestAspectChk_2->setChecked(m0->gridView->isBestAspect);
    if (ui->bestAspectChk_2->isChecked()) ui->lockDimChk_2->setChecked(false);
    else ui->lockDimChk_2->setChecked(true);

    // slideshow
    ui->slideshowDelaySpinbox->setValue(m0->slideShowDelay);
    ui->slideshowRandomChk->setChecked(m0->slideShowRandom);

    // cache
    ui->cacheSizeSpinbox->setValue(m0->cacheSizeMB / 1000);
    ui->showCacheStatusChk->setChecked(m0->isShowCacheStatus);
    ui->cacheDelaySpinbox->setValue(m0->cacheDelay);
    ui->showCacheThreadActivityChk->setChecked(m0->isShowCacheThreadActivity);
    ui->progressWidthSlider->setValue(m0->progressWidth);
    switch (m0->cacheWtAhead) {
    case 5: ui->cache50AheadRadio->setChecked(true); break;
    case 6: ui->cache60AheadRadio->setChecked(true); break;
    case 7: ui->cache70AheadRadio->setChecked(true); break;
    case 8: ui->cache80AheadRadio->setChecked(true); break;
    case 9: ui->cache90AheadRadio->setChecked(true); break;
    case 10: ui->cache100AheadRadio->setChecked(true);
    }
    ui->cachePreviewsChk->setChecked(m0->isCachePreview);
    ui->coresSpinBox->setValue(G::cores);
    ui->aSynchChk->setChecked(G::aSync);

    // full screen
    ui->foldersChk->setChecked(m0->fullScreenDocks.isFolders);
    ui->favsChk->setChecked(m0->fullScreenDocks.isFavs);
    ui->filtersChk->setChecked(m0->fullScreenDocks.isFilters);
    ui->metadataChk->setChecked(m0->fullScreenDocks.isMetadata);
    ui->thumbsChk->setChecked(m0->fullScreenDocks.isThumbs);
    ui->statusBarChk->setChecked(m0->fullScreenDocks.isStatusBar);

    // InfoView fields to show
    okInfo = m0->infoView->ok;
    ui->infoFieldsTreeView->setModel(okInfo);
    for(int row = 0; row < okInfo->rowCount(); row++) {
        QModelIndex parentIdx = okInfo->index(row, 0);
        QModelIndex idx = okInfo->index(row, 2);
        ui->infoFieldsTreeView->setIndexWidget(idx, new QCheckBox);
        // set state to match data model value
        QWidget *wid = ui->infoFieldsTreeView->indexWidget(idx);
        QCheckBox *box= qobject_cast<QCheckBox*>(wid);
        box->setChecked(idx.data().toBool());
        connect(box, SIGNAL(clicked(bool)), this, SLOT(on_infoField_changed()));
        for (int childRow = 0; childRow < okInfo->rowCount(parentIdx); childRow++) {
            QModelIndex idx = okInfo->index(childRow, 2, parentIdx);
            ui->infoFieldsTreeView->setIndexWidget(idx, new QCheckBox);
            // set state to match data model value
            QWidget *wid = ui->infoFieldsTreeView->indexWidget(idx);
            QCheckBox *box= qobject_cast<QCheckBox*>(wid);
            box->setChecked(idx.data().toBool());
            connect(box, SIGNAL(clicked(bool)), this, SLOT(on_infoField_changed()));
        }
    }
    ui->infoFieldsTreeView->setRootIsDecorated(false);
    ui->infoFieldsTreeView->setWordWrap(false);
    ui->infoFieldsTreeView->header()->moveSection(2, 0);
    ui->infoFieldsTreeView->setHeaderHidden(true);
    ui->infoFieldsTreeView->setColumnWidth(2, 20);
    ui->infoFieldsTreeView->hideColumn(1);
    ui->infoFieldsTreeView->expandAll();
    ui->infoFieldsTreeView->setStyleSheet("QTreeView {border: none;}");

    // TableView fields to show
    okTable = m0->tableView->ok;
    ui->tableFieldsTable->setModel(okTable);
    for(int row = 0; row < okTable->rowCount(); row++) {
        QModelIndex idx = okTable->index(row, 1);
        ui->tableFieldsTable->setIndexWidget(idx, new QCheckBox);
        // set state to match data model value
        QWidget *wid1 = ui->tableFieldsTable->indexWidget(idx);
        QCheckBox *box1 = qobject_cast<QCheckBox*>(wid1);
        box1->setChecked(idx.data().toBool());
        connect(box1, SIGNAL(clicked(bool)), this, SLOT(on_tableField_changed()));
    }
    // NOTE: row heights controlled by vertical header height in design form
    ui->tableFieldsTable->horizontalHeader()->moveSection(1, 0);
    ui->tableFieldsTable->horizontalHeader()->setVisible(false);
    ui->tableFieldsTable->horizontalHeader()->setStretchLastSection(true);
    ui->tableFieldsTable->verticalHeader()->setVisible(false);
    ui->tableFieldsTable->resizeColumnsToContents();
    ui->tableFieldsTable->setStyleSheet("QTableView {border:none; gridline-color:rgb(85,85,85)}");

    okToUpdate = true;
}

Prefdlg::~Prefdlg()
{
    delete ui;
}

void Prefdlg::reject()
{
    QDialog::reject();
}

void Prefdlg::on_slider_pressed()
{
    // used to prevent font size updates while slider is pressed to prevent slider latency
    isSliderPressed = true;
}

void Prefdlg::on_slider_released()
{
    // used to prevent font size updates while slider is pressed to prevent slider latency
    isSliderPressed = false;
}

void Prefdlg::on_infoField_changed()
{
/*
Update the datamodel okInfo (infoView->ok) to match ui->infoFieldsTable.  This
will trigger a datachanged signal which in turn will fire the slot
infoView->showOrHide(), which will show or hide each metadata item in the
Metadata panel.
*/
    for(int row = 0; row < okInfo->rowCount(); row++) {
        QModelIndex parentIdx = okInfo->index(row, 0);
        QModelIndex idx = okInfo->index(row, 2);
        QWidget *wid = ui->infoFieldsTreeView->indexWidget(idx);
        QCheckBox *box = qobject_cast<QCheckBox*>(wid);
        okInfo->setData(idx, box->isChecked());
        for (int childRow = 0; childRow < okInfo->rowCount(parentIdx); childRow++) {
            QModelIndex idx = okInfo->index(childRow, 2, parentIdx);
            QWidget *wid = ui->infoFieldsTreeView->indexWidget(idx);
            QCheckBox *box = qobject_cast<QCheckBox*>(wid);
            okInfo->setData(idx, box->isChecked());
        }
    }
}

void Prefdlg::on_tableField_changed()
{
/*
Update the datamodel okInfo (tableView->ok) to match ui->infoFieldsTable.  This
will trigger a datachanged signal which in turn will fire the slot
tableView->showOrHide(), which will show or hide each column in the tableView.
*/
    for(int row = 0; row < okTable->rowCount(); row++) {
        QModelIndex idx = okTable->index(row, 1);
        QWidget *wid1 = ui->tableFieldsTable->indexWidget(idx);
        QCheckBox *box1 = qobject_cast<QCheckBox*>(wid1);
        okTable->setData(idx, box1->isChecked());
    }
}

void Prefdlg::on_listWidget_currentItemChanged(QListWidgetItem *current)
{
/*
Sync preference category list and preference items in stacked form
*/
    ui->stackedWidget->setCurrentIndex(ui->listWidget->row(current));

//    qDebug() << "on_listWidget_currentItemChanged" << ui->listWidget->currentRow();
//    int page = ui->listWidget->currentRow();
    // for some bizarre reason this doesn't work and causes crash
//    mw->setPrefPage(page);
//    mw->lastPrefPage = page;

    // hence send signal instead
    emit updatePage(ui->listWidget->currentRow());
}

// general
void Prefdlg::on_classificationBadgeThumbDiamSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m0->classificationBadgeInThumbDiameter = value;
        m0->thumbView->badgeSize = value;
        m0->gridView->badgeSize = value;
        m0->thumbView->setThumbParameters();
        m0->gridView->setThumbParameters();
    }
}

void Prefdlg::on_classificationBadgeImageDiamSlider_valueChanged(int value)
{
    m0->classificationBadgeInImageDiameter = value;
    m0->imageView->setClassificationBadgeImageDiam(value);
}

void Prefdlg::on_globalFontSizeSlider_valueChanged(int value)
{
    // used to prevent font size updates while slider is pressed to prevent slider latency
    // if user just clicks on slider then updateFontSize is emitted
//    if(isSliderPressed) return;
    if (okToUpdate) {
        m0->setFontSize(value);
    }
}

void Prefdlg::on_globalFontSizeSlider_sliderReleased()
{
    int value = ui->globalFontSizeSlider->value();
    m0->setFontSize(value);
}


void Prefdlg::on_infoFontSizeSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m0->imageView->infoOverlayFontSize = value;
        ui->infoFontSizeLbl->setText(QString::number(value));
        m0->setInfoFontSize();
    }
}
void Prefdlg::on_cachePreviewsChk_clicked()
{
    if (okToUpdate) {
        m0->isCachePreview = ui->showCacheThreadActivityChk->isChecked();
    }
}

void Prefdlg::on_rememberFolderChk_clicked()
{
    if (okToUpdate) {
        m0->rememberLastDir = ui->rememberFolderChk->isChecked();
    }
}

void Prefdlg::on_updateAppChk_stateChanged(int /*value*/)
{
    if (okToUpdate) {
        m0->checkIfUpdate = ui->updateAppChk->isChecked();
    }
}

void Prefdlg::on_mouseClickScrollChk_clicked()
{
    if (okToUpdate) {
        m0->mouseClickScroll = ui->mouseClickScrollChk->isChecked();
    }
}

void Prefdlg::on_trackpadIterateRadio_clicked()
{
    if(okToUpdate) {
        m0->imageView->useWheelToScroll = false;
    }
}

void Prefdlg::on_trackpadScrollRadio_clicked()
{
    if(okToUpdate) {
        m0->imageView->useWheelToScroll = true;
    }
}

// thumbs
void Prefdlg::on_iconWidthSlider_valueChanged(int value)
{
    if (okToUpdate) {
        if (!ignoreThumbBestAspectChange) {
            m0->thumbView->thumbWidth = value;
            if (ui->lockDimChk->isChecked()) ui->iconHeightSlider->setValue(value);
            m0->thumbView->setThumbParameters();
            ui->bestAspectChk->setChecked(false);
        }
    }
}

void Prefdlg::on_iconHeightSlider_valueChanged(int value)
{
    if (okToUpdate) {
        if (!ignoreThumbBestAspectChange) {
            m0->thumbView->thumbHeight = value;
            if (ui->lockDimChk->isChecked()) ui->iconWidthSlider->setValue(value);
            m0->thumbView->setThumbParameters();
            ui->bestAspectChk->setChecked(false);
        }
    }
}

void Prefdlg::on_thumbSpacingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m0->thumbView->thumbSpacing = value;
        m0->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_iconPaddingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m0->thumbView->thumbPadding = value;
        m0->thumbView->setThumbParameters();
    }
}


void Prefdlg::on_showThumbLabelChk_clicked()
{
    if (okToUpdate) {
        m0->thumbView->showThumbLabels = ui->showThumbLabelChk->isChecked();
        m0->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_wrapChk_clicked()
{
    if (okToUpdate) {
        m0->thumbView->wrapThumbs = ui->wrapChk->isChecked();
        m0->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_lockDimChk_clicked(bool checked)
{
    if (okToUpdate) {
        if (checked) ui->bestAspectChk->setChecked(false);
   }
}

void Prefdlg::on_bestAspectChk_clicked(bool checked)
{
    if (okToUpdate) {
        m0->thumbView->isBestAspect = checked;
        if (checked) {
            ui->lockDimChk->setChecked(false);
            m0->thumbView->isBestAspect = checked;
            m0->thumbView->bestAspect();
            m0->thumbView->setThumbParameters();
            ignoreThumbBestAspectChange = true;
            ui->iconWidthSlider->setValue(m0->thumbView->thumbWidth);
            ui->iconHeightSlider->setValue(m0->thumbView->thumbHeight);
            ignoreThumbBestAspectChange = false;
        }
    }
}

void Prefdlg::on_fontSizeSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m0->thumbView->labelFontSize = value;
        m0->thumbView->setThumbParameters();
    }
}

// grid
void Prefdlg::on_iconWidthSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        if (!ignoreGridBestAspectChange) {
            m0->gridView->thumbWidth = value;
            if (ui->lockDimChk_2->isChecked()) ui->iconHeightSlider_2->setValue(value);
            m0->gridView->setThumbParameters();
//            m0->gridView->thumbsRejustify();
            ui->bestAspectChk_2->setChecked(false);
        }
    }
}

void Prefdlg::on_iconHeightSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        if (!ignoreGridBestAspectChange) {
            m0->gridView->thumbHeight = value;
            if (ui->lockDimChk_2->isChecked()) ui->iconWidthSlider_2->setValue(value);
            m0->gridView->setThumbParameters();
//            m0->gridView->thumbsRejustify();
            ui->bestAspectChk_2->setChecked(false);
        }
    }
}

void Prefdlg::on_thumbSpacingSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        m0->gridView->thumbSpacing = value;
        m0->gridView->setThumbParameters();
        m0->gridView->thumbsRejustify();
    }
}

void Prefdlg::on_iconPaddingSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        m0->gridView->thumbPadding = value;
        m0->gridView->setThumbParameters();
        m0->gridView->thumbsRejustify();
    }
}

void Prefdlg::on_showThumbLabelChk_2_clicked()
{
    if (okToUpdate) {
        m0->gridView->thumbPadding = ui->showThumbLabelChk_2->isChecked();
        m0->gridView->setThumbParameters();
    }
}

void Prefdlg::on_lockDimChk_2_clicked(bool checked)
{
    if (okToUpdate) {
        if (checked) ui->bestAspectChk_2->setChecked(false);
   }
}

void Prefdlg::on_bestAspectChk_2_clicked(bool checked)
{
    if (okToUpdate) {
        if (checked) {
            ui->lockDimChk_2->setChecked(false);
            m0->gridView->isBestAspect = checked;
            m0->gridView->bestAspect();
            m0->gridView->setThumbParameters();
            m0->gridView->thumbsRejustify();
            ignoreGridBestAspectChange = true;
            ui->iconWidthSlider_2->setValue(m0->gridView->thumbWidth);
            ui->iconHeightSlider_2->setValue(m0->gridView->thumbHeight);
            ignoreGridBestAspectChange = false;
        }
    }
}

void Prefdlg::on_fontSizeSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        m0->gridView->labelFontSize = value;
        m0->gridView->setThumbParameters();
    }
}

// slideshow
void Prefdlg::on_slideshowDelaySpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        m0->slideShowDelay = value;
    }
}

void Prefdlg::on_slideshowRandomChk_clicked()
{
    if (okToUpdate) {
        m0->slideShowRandom = ui->slideshowRandomChk->isChecked();
    }
}

// cache
void Prefdlg::on_cacheSizeSpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        m0->cacheSizeMB = value * 1000;
        m0->setCacheParameters();
    }
}

void Prefdlg::on_showCacheStatusChk_clicked()
{
    if (okToUpdate) {
        m0->isShowCacheStatus = ui->showCacheStatusChk->isChecked();
        m0->setCacheParameters();
    }
}

void Prefdlg::on_cacheDelaySpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        m0->cacheDelay = value;
        m0->setCacheParameters();
    }
}

void Prefdlg::on_showCacheThreadActivityChk_clicked()
{
    if (okToUpdate) {
        m0->isShowCacheThreadActivity = ui->showCacheThreadActivityChk->isChecked();
        m0->setCacheParameters();
    }
}

void Prefdlg::on_progressWidthSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m0->progressWidth = value;
        m0->setCacheParameters();
    }
}

void Prefdlg::on_cache50AheadRadio_clicked()
{
    if (okToUpdate) {
        m0->cacheWtAhead = 5;
        m0->setCacheParameters();
   }
}

void Prefdlg::on_cache60AheadRadio_clicked()
{
    if (okToUpdate) {
        m0->cacheWtAhead = 6;
        m0->setCacheParameters();
    }
}

void Prefdlg::on_cache70AheadRadio_clicked()
{
    if (okToUpdate) {
        m0->cacheWtAhead = 7;
        m0->setCacheParameters();
    }
}

void Prefdlg::on_cache80AheadRadio_clicked()
{
    if (okToUpdate) {
        m0->cacheWtAhead = 8;
        m0->setCacheParameters();
    }
}

void Prefdlg::on_cache90AheadRadio_clicked()
{
    if (okToUpdate) {
        m0->cacheWtAhead = 9;
        m0->setCacheParameters();
    }
}

void Prefdlg::on_cache100AheadRadio_clicked()
{
    if (okToUpdate) {
        m0->cacheWtAhead = 10;
        m0->setCacheParameters();
    }
}

// full screen preferences
void Prefdlg::on_foldersChk_clicked()
{
    if (okToUpdate) {
        m0->fullScreenDocks.isFolders = ui->foldersChk->isChecked();
    }
}

void Prefdlg::on_favsChk_clicked()
{
    if (okToUpdate) {
        m0->fullScreenDocks.isFavs = ui->favsChk->isChecked();
    }
}

void Prefdlg::on_filtersChk_clicked()
{
    if (okToUpdate) {
        m0->fullScreenDocks.isFilters = ui->filtersChk->isChecked();
    }
}

void Prefdlg::on_metadataChk_clicked()
{
    if (okToUpdate) {
        m0->fullScreenDocks.isMetadata = ui->metadataChk->isChecked();
    }
}

void Prefdlg::on_thumbsChk_clicked()
{
    if (okToUpdate) {
        m0->fullScreenDocks.isThumbs = ui->thumbsChk->isChecked();
    }
}


void Prefdlg::on_statusBarChk_clicked()
{
    if (okToUpdate) {
        m0->fullScreenDocks.isStatusBar = ui->statusBarChk->isChecked();
    }
}

void Prefdlg::on_coresSpinBox_valueChanged(int arg1)
{
    G::cores = arg1;
}


void Prefdlg::on_aSynchChk_clicked()
{
    G::aSync = ui->aSynchChk->isChecked();
}


