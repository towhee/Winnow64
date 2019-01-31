#include "Dialogs/prefdlg.h"
#include "ui_prefdlg.h"
#include "Main/global.h"
#include <QDebug>

// this works because prefdlg is a friend class of MW
MW *m;

Prefdlg::Prefdlg(QWidget *parent, int lastPrefPage) :
    QDialog(parent),
    ui(new Ui::Prefdlg)
{
    okToUpdate = false;
    ui->setupUi(this);
    ui->listWidget->setCurrentRow(lastPrefPage);

//    // this works because prefdlg is a friend class of MW
//    MW *m = qobject_cast<MW*>(parent);
    m = qobject_cast<MW*>(parent);

    // general
    ui->globalFontSizeSlider->setTickInterval(1);
    ui->globalFontSizeSlider->setMinimum(8);
    ui->globalFontSizeSlider->setMaximum(20);
    ui->globalFontSizeSlider->setValue(m->fontSize.toInt());
    ui->classificationBadgeImageDiamSlider->setValue(m->classificationBadgeInImageDiameter);
    ui->classificationBadgeThumbDiamSlider->setValue(m->classificationBadgeInThumbDiameter);
    ui->rememberFolderChk->setChecked(m->rememberLastDir);
    ui->updateAppChk->setChecked(m->checkIfUpdate);
    ui->trackpadIterateRadio->setChecked(!m->imageView->useWheelToScroll);
    ui->trackpadScrollRadio->setChecked(m->imageView->useWheelToScroll);
    ui->mouseClickScrollChk->setChecked((m->mouseClickScroll));

    // thumbs
    ui->iconWidthSlider->setSingleStep(1);
    ui->iconWidthSlider->setTickInterval(50);
    ui->iconWidthSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconWidthSlider->setMaximum(THUMB_MAX);
    ui->iconWidthSlider->setValue(m->thumbView->thumbWidth);
    ui->iconHeightSlider->setSingleStep(1);
    ui->iconHeightSlider->setTickInterval(50);
    ui->iconHeightSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconHeightSlider->setMaximum(THUMB_MAX);
    ui->iconHeightSlider->setValue(m->thumbView->thumbHeight);
    ui->thumbSpacingSlider->setSingleStep(1);
    ui->thumbSpacingSlider->setTickInterval(3);
    ui->thumbSpacingSlider->setTickPosition(QSlider::TicksAbove);
    ui->thumbSpacingSlider->setValue(m->thumbView->thumbSpacing);
    ui->iconPaddingSlider->setSingleStep(1);
    ui->iconPaddingSlider->setTickInterval(1);
    ui->iconPaddingSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconPaddingSlider->setValue(m->thumbView->thumbPadding);
    ui->fontSizeSlider->setSingleStep(1);
    ui->fontSizeSlider->setTickInterval(1);
    ui->fontSizeSlider->setTickPosition(QSlider::TicksAbove);
    ui->fontSizeSlider->setValue(m->thumbView->labelFontSize);
    ui->showThumbLabelChk->setChecked(m->thumbView->showThumbLabels);
    ui->wrapChk->setChecked(m->thumbView->wrapThumbs);
    ui->lockDimChk->setChecked(true);

    // thumbsGrid
    ui->iconWidthSlider_2->setSingleStep(1);
    ui->iconWidthSlider_2->setTickInterval(50);
    ui->iconWidthSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconWidthSlider_2->setMaximum(THUMB_MAX);
    ui->iconWidthSlider_2->setValue(m->gridView->thumbWidth);
    ui->iconHeightSlider_2->setSingleStep(1);
    ui->iconHeightSlider_2->setTickInterval(50);
    ui->iconHeightSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconHeightSlider_2->setMaximum(THUMB_MAX);
    ui->iconHeightSlider_2->setValue(m->gridView->thumbHeight);
    ui->thumbSpacingSlider_2->setSingleStep(1);
    ui->thumbSpacingSlider_2->setTickInterval(3);
    ui->thumbSpacingSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->thumbSpacingSlider_2->setValue(m->gridView->thumbSpacing);
    ui->iconPaddingSlider_2->setSingleStep(1);
    ui->iconPaddingSlider_2->setTickInterval(1);
    ui->iconPaddingSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconPaddingSlider_2->setValue(m->gridView->thumbPadding);
    ui->fontSizeSlider_2->setTickInterval(1);
    ui->fontSizeSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->fontSizeSlider_2->setValue(m->thumbView->labelFontSize);
    ui->fontSizeSlider_2->setValue(m->gridView->labelFontSize);
    ui->showThumbLabelChk_2->setChecked(m->gridView->showThumbLabels);
    ui->lockDimChk_2->setChecked(true);

    // slideshow
    ui->slideshowDelaySpinbox->setValue(m->slideShowDelay);
    ui->slideshowRandomChk->setChecked(m->slideShowRandom);

    // cache
    ui->cacheSizeSpinbox->setValue(m->cacheSizeMB / 1000);
    ui->showCacheStatusChk->setChecked(m->isShowCacheStatus);
    ui->cacheDelaySpinbox->setValue(m->cacheDelay);
    ui->showCacheThreadActivityChk->setChecked(m->isShowCacheThreadActivity);
    ui->cacheStatusWidthSpin->setValue(m->progressWidth);
    switch (m->cacheWtAhead) {
    case 5: ui->cache50AheadRadio->setChecked(true); break;
    case 6: ui->cache60AheadRadio->setChecked(true); break;
    case 7: ui->cache70AheadRadio->setChecked(true); break;
    case 8: ui->cache80AheadRadio->setChecked(true); break;
    case 9: ui->cache90AheadRadio->setChecked(true); break;
    case 10: ui->cache100AheadRadio->setChecked(true);
    }
    ui->cachePreviewsChk->setChecked(m->isCachePreview);

    // full screen
    ui->foldersChk->setChecked(m->fullScreenDocks.isFolders);
    ui->favsChk->setChecked(m->fullScreenDocks.isFavs);
    ui->filtersChk->setChecked(m->fullScreenDocks.isFilters);
    ui->metadataChk->setChecked(m->fullScreenDocks.isMetadata);
    ui->thumbsChk->setChecked(m->fullScreenDocks.isThumbs);
    ui->statusBarChk->setChecked(m->fullScreenDocks.isStatusBar);

    // InfoView fields to show
    okInfo = m->infoView->ok;
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
    okTable = m->tableView->ok;
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

//    connect(ui->globalFontSizeSlider, SIGNAL(sliderPressed()), this, SLOT(on_slider_pressed()));
//    connect(ui->globalFontSizeSlider, SIGNAL(sliderReleased()), this, SLOT(on_slider_released()));
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
    emit updatePage(ui->listWidget->currentRow());
}

// Docked thumbs
void Prefdlg::on_iconWidthSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m->thumbView->thumbWidth = value;
        if (ui->lockDimChk->isChecked()) ui->iconHeightSlider->setValue(value);
        m->thumbView->setThumbParameters();
    }
}

// general
void Prefdlg::on_globalFontSizeSlider_valueChanged(int value)
{
    // used to prevent font size updates while slider is pressed to prevent slider latency
    // if user just clicks on slider then updateFontSize is emitted
//    if(isSliderPressed) return;
    if (okToUpdate) {
        m->setFontSize(value);
    }
}

// thumbs
void Prefdlg::on_iconHeightSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m->thumbView->thumbHeight = value;
        if (ui->lockDimChk->isChecked()) ui->iconWidthSlider->setValue(value);
        m->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_thumbSpacingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m->thumbView->thumbSpacing = value;
        m->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_iconPaddingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m->thumbView->thumbPadding = value;
        m->thumbView->setThumbParameters();
    }
}


void Prefdlg::on_showThumbLabelChk_clicked()
{
    if (okToUpdate) {
        m->thumbView->thumbPadding = ui->showThumbLabelChk->isChecked();
        m->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_wrapChk_clicked()
{
    if (okToUpdate) {
        m->thumbView->wrapThumbs = ui->wrapChk->isChecked();
        m->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_fontSizeSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m->thumbView->labelFontSize = value;
        m->thumbView->setThumbParameters();
    }
}

void Prefdlg::on_classificationBadgeThumbDiamSlider_valueChanged(int value)
{
    if (okToUpdate) {
        m->classificationBadgeInThumbDiameter = value;
        m->thumbView->badgeSize = value;
        m->gridView->badgeSize = value;
        m->thumbView->setThumbParameters();
        m->gridView->setThumbParameters();
    }
}

void Prefdlg::on_classificationBadgeImageDiamSlider_valueChanged(int value)
{
    m->classificationBadgeInImageDiameter = value;
    m->imageView->setClassificationBadgeImageDiam(value);
}

// slideshow
void Prefdlg::on_slideshowDelaySpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        m->slideShowDelay = value;
    }
}

void Prefdlg::on_slideshowRandomChk_clicked()
{
    if (okToUpdate) {
        m->slideShowRandom = ui->slideshowRandomChk->isChecked();
    }
}

// cache
void Prefdlg::on_cacheSizeSpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        m->cacheSizeMB = value * 1000;
    }
}

void Prefdlg::on_showCacheStatusChk_clicked()
{
    if (okToUpdate) {
        m->isShowCacheStatus = ui->showCacheStatusChk->isChecked();
    }
}

void Prefdlg::on_cacheDelaySpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        m->cacheDelay = value;
    }
}

void Prefdlg::on_showCacheThreadActivityChk_clicked()
{
    if (okToUpdate) {
        m->isShowCacheThreadActivity = ui->showCacheThreadActivityChk->isChecked();
    }
}

void Prefdlg::on_cacheStatusWidthSpin_valueChanged(int value)
{
    if (okToUpdate) {
        m->progressWidth = value;
    }
}

void Prefdlg::on_cache50AheadRadio_clicked()
{
    if (okToUpdate) {
        m->cacheWtAhead = 5;
   }
}

void Prefdlg::on_cache60AheadRadio_clicked()
{
    if (okToUpdate) {
        m->cacheWtAhead = 6;
    }
}

void Prefdlg::on_cache70AheadRadio_clicked()
{
    if (okToUpdate) {
        m->cacheWtAhead = 7;
    }
}

void Prefdlg::on_cache80AheadRadio_clicked()
{
    if (okToUpdate) {
        m->cacheWtAhead = 8;
    }
}

void Prefdlg::on_cache90AheadRadio_clicked()
{
    if (okToUpdate) {
        m->cacheWtAhead = 9;
    }
}

void Prefdlg::on_cache100AheadRadio_clicked()
{
    if (okToUpdate) {
        m->cacheWtAhead = 10;
    }
}

void Prefdlg::on_cachePreviewsChk_clicked()
{
    if (okToUpdate) {
        m->isCachePreview = ui->showCacheThreadActivityChk->isChecked();
    }
}

void Prefdlg::on_rememberFolderChk_clicked()
{
    emit updateRememberFolder(ui->rememberFolderChk->isChecked());
}

void Prefdlg::on_mouseClickScrollChk_clicked()
{
    if (okToUpdate) {
        emit updateMouseClickScroll(ui->mouseClickScrollChk->isChecked());
    }
}

void Prefdlg::on_trackpadIterateRadio_clicked()
{
    if(okToUpdate) {
        emit updateTrackpadScroll(false);
    }
}

void Prefdlg::on_trackpadScrollRadio_clicked()
{
    if(okToUpdate) {
        emit updateTrackpadScroll(true);
    }
}

//void Prefdlg::on_displayHorizontalPixelsSB_valueChanged(int /* not used */)
//{
//    if(okToUpdate) {
//        emit updateDisplayResolution(ui->displayHorizontalPixelsSB->value(),
//                                     ui->displayVerticalPixelsSB->value());
//    }
//}

//void Prefdlg::on_displayVerticalPixelsSB_valueChanged(int /* not used */)
//{
//    if(okToUpdate) {
//        emit updateDisplayResolution(ui->displayHorizontalPixelsSB->value(),
//                                     ui->displayVerticalPixelsSB->value());
//    }
//}

// grid preferences
void Prefdlg::on_iconWidthSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        if (ui->lockDimChk_2->isChecked()) {
            ui->iconHeightSlider_2->setValue(value);
        }
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked(),
                                       true,
                                       ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_iconHeightSlider_2_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        if (ui->lockDimChk_2->isChecked()) {
            ui->iconWidthSlider_2->setValue(ui->iconHeightSlider_2->value());
        }
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked(),
                                       true,
                                       ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_thumbSpacingSlider_2_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked(),
                                       true,
                                       ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_iconPaddingSlider_2_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked(),
                                       true,
                                       ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_showThumbLabelChk_2_clicked()
{
    if (okToUpdate) {
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked(),
                                       true,
                                       ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_fontSizeSlider_2_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked(),
                                       true,
                                       ui->classificationBadgeThumbDiamSlider->value());
    }
}

// full screen preferences
void Prefdlg::on_foldersChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(),
                               ui->favsChk->isChecked(),
                               ui->filtersChk->isChecked(),
                               ui->metadataChk->isChecked(),
                               ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

void Prefdlg::on_favsChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(),
                               ui->favsChk->isChecked(),
                               ui->filtersChk->isChecked(),
                               ui->metadataChk->isChecked(),
                               ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

void Prefdlg::on_filtersChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(),
                               ui->favsChk->isChecked(),
                               ui->filtersChk->isChecked(),
                               ui->metadataChk->isChecked(),
                               ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

void Prefdlg::on_metadataChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(),
                               ui->favsChk->isChecked(),
                               ui->filtersChk->isChecked(),
                               ui->metadataChk->isChecked(),
                               ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

void Prefdlg::on_thumbsChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(),
                               ui->favsChk->isChecked(),
                               ui->filtersChk->isChecked(),
                               ui->metadataChk->isChecked(),
                               ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}


void Prefdlg::on_statusBarChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(),
                               ui->favsChk->isChecked(),
                               ui->filtersChk->isChecked(),
                               ui->metadataChk->isChecked(),
                               ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}


void Prefdlg::on_globalFontSizeSlider_sliderReleased()
{
    int value = ui->globalFontSizeSlider->value();
    emit updateFontSize(QString::number(value));
}


void Prefdlg::on_updateAppChk_stateChanged(int value)
{
    emit checkForUpdates(value);
}
