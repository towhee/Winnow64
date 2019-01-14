#include "Dialogs/prefdlg.h"
#include "ui_prefdlg.h"
#include "Main/global.h"

#include <QDebug>

Prefdlg::Prefdlg(QWidget *parent, int lastPrefPage) :
    QDialog(parent),
    ui(new Ui::Prefdlg)
{
    okToUpdate = false;
    ui->setupUi(this);
    ui->listWidget->setCurrentRow(lastPrefPage);

    // this works because prefdlg is a friend class of MW
    MW *mw = qobject_cast<MW*>(parent);

    // general
    ui->globalFontSizeSlider->setTickInterval(1);
    ui->globalFontSizeSlider->setMinimum(8);
    ui->globalFontSizeSlider->setMaximum(20);
    ui->globalFontSizeSlider->setValue(mw->fontSize.toInt());
    ui->classificationBadgeImageDiamSlider->setValue(mw->classificationBadgeInImageDiameter);
    ui->classificationBadgeThumbDiamSlider->setValue(mw->classificationBadgeInThumbDiameter);
    ui->rememberFolderChk->setChecked(mw->rememberLastDir);
    ui->trackpadIterateRadio->setChecked(!mw->imageView->useWheelToScroll);
    ui->trackpadScrollRadio->setChecked(mw->imageView->useWheelToScroll);
    ui->mouseClickScrollChk->setChecked((mw->mouseClickScroll));
//    ui->displayHorizontalPixelsSB->setValue(mw->displayHorizontalPixels);
//    ui->displayVerticalPixelsSB->setValue(mw->displayVerticalPixels);

    // thumbs
    ui->iconWidthSlider->setSingleStep(1);
    ui->iconWidthSlider->setTickInterval(50);
    ui->iconWidthSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconWidthSlider->setMaximum(THUMB_MAX);
    ui->iconWidthSlider->setValue(mw->thumbView->thumbWidth);
    ui->iconHeightSlider->setSingleStep(1);
    ui->iconHeightSlider->setTickInterval(50);
    ui->iconHeightSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconHeightSlider->setMaximum(THUMB_MAX);
    ui->iconHeightSlider->setValue(mw->thumbView->thumbHeight);
    ui->thumbSpacingSlider->setSingleStep(1);
    ui->thumbSpacingSlider->setTickInterval(3);
    ui->thumbSpacingSlider->setTickPosition(QSlider::TicksAbove);
    ui->thumbSpacingSlider->setValue(mw->thumbView->thumbSpacing);
    ui->iconPaddingSlider->setSingleStep(1);
    ui->iconPaddingSlider->setTickInterval(1);
    ui->iconPaddingSlider->setTickPosition(QSlider::TicksAbove);
    ui->iconPaddingSlider->setValue(mw->thumbView->thumbPadding);
    ui->fontSizeSlider->setSingleStep(1);
    ui->fontSizeSlider->setTickInterval(1);
    ui->fontSizeSlider->setTickPosition(QSlider::TicksAbove);
    ui->fontSizeSlider->setValue(mw->thumbView->labelFontSize);
    ui->showThumbLabelChk->setChecked(mw->thumbView->showThumbLabels);
    ui->wrapChk->setChecked(mw->thumbView->wrapThumbs);
    ui->lockDimChk->setChecked(true);

    // thumbsGrid
    ui->iconWidthSlider_2->setSingleStep(1);
    ui->iconWidthSlider_2->setTickInterval(50);
    ui->iconWidthSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconWidthSlider_2->setMaximum(THUMB_MAX);
    ui->iconWidthSlider_2->setValue(mw->gridView->thumbWidth);
    ui->iconHeightSlider_2->setSingleStep(1);
    ui->iconHeightSlider_2->setTickInterval(50);
    ui->iconHeightSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconHeightSlider_2->setMaximum(THUMB_MAX);
    ui->iconHeightSlider_2->setValue(mw->gridView->thumbHeight);
    ui->thumbSpacingSlider_2->setSingleStep(1);
    ui->thumbSpacingSlider_2->setTickInterval(3);
    ui->thumbSpacingSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->thumbSpacingSlider_2->setValue(mw->gridView->thumbSpacing);
    ui->iconPaddingSlider_2->setSingleStep(1);
    ui->iconPaddingSlider_2->setTickInterval(1);
    ui->iconPaddingSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->iconPaddingSlider_2->setValue(mw->gridView->thumbPadding);
    ui->fontSizeSlider_2->setTickInterval(1);
    ui->fontSizeSlider_2->setTickPosition(QSlider::TicksAbove);
    ui->fontSizeSlider_2->setValue(mw->thumbView->labelFontSize);
    ui->fontSizeSlider_2->setValue(mw->gridView->labelFontSize);
    ui->showThumbLabelChk_2->setChecked(mw->gridView->showThumbLabels);
    ui->lockDimChk_2->setChecked(true);

    // slideshow
    ui->slideshowDelaySpinbox->setValue(mw->slideShowDelay);
    ui->slideshowRandomChk->setChecked(mw->slideShowRandom);

    // cache
    ui->cacheSizeSpinbox->setValue(mw->cacheSizeMB / 1000);
    ui->showCacheStatusChk->setChecked(mw->isShowCacheStatus);
    ui->cacheDelaySpinbox->setValue(mw->cacheDelay);
    ui->showCacheThreadActivityChk->setChecked(mw->isShowCacheThreadActivity);
    ui->cacheStatusWidthSpin->setValue(mw->progressWidth);
    cacheWtAhead = mw->cacheWtAhead;
    switch (cacheWtAhead) {
    case 5: ui->cache50AheadRadio->setChecked(true); break;
    case 6: ui->cache60AheadRadio->setChecked(true); break;
    case 7: ui->cache70AheadRadio->setChecked(true); break;
    case 8: ui->cache80AheadRadio->setChecked(true); break;
    case 9: ui->cache90AheadRadio->setChecked(true); break;
    case 10: ui->cache100AheadRadio->setChecked(true);
    }
    ui->cachePreviewsChk->setChecked(mw->isCachePreview);

    // full screen
    ui->foldersChk->setChecked(mw->fullScreenDocks.isFolders);
    ui->favsChk->setChecked(mw->fullScreenDocks.isFavs);
    ui->filtersChk->setChecked(mw->fullScreenDocks.isFilters);
    ui->metadataChk->setChecked(mw->fullScreenDocks.isMetadata);
    ui->thumbsChk->setChecked(mw->fullScreenDocks.isThumbs);
    ui->statusBarChk->setChecked(mw->fullScreenDocks.isStatusBar);

    // InfoView fields to show
    okInfo = mw->infoView->ok;
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
    okTable = mw->tableView->ok;
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
        if (ui->lockDimChk->isChecked()) {
            ui->iconHeightSlider->setValue(value);
        }
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_iconHeightSlider_valueChanged(int value)
{
    if (okToUpdate) {
        if (ui->lockDimChk->isChecked()) {
            ui->iconWidthSlider->setValue(value);
        }
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_thumbSpacingSlider_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_iconPaddingSlider_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
    }
}


void Prefdlg::on_showThumbLabelChk_clicked()
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_wrapChk_clicked()
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_fontSizeSlider_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
    }
}

void Prefdlg::on_classificationBadgeThumbDiamSlider_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked(),
                                   ui->wrapChk->isChecked(),
                                   ui->classificationBadgeThumbDiamSlider->value());
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

// slideshow
void Prefdlg::on_slideshowDelaySpinbox_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateSlideShowParameters(ui->slideshowDelaySpinbox->value(),
                                       ui->slideshowRandomChk->isChecked());
    }
}

void Prefdlg::on_slideshowRandomChk_clicked()
{
    if (okToUpdate) {
        emit updateSlideShowParameters(ui->slideshowDelaySpinbox->value(),
                                       ui->slideshowRandomChk->isChecked());
    }
}

// cache
void Prefdlg::on_cacheSizeSpinbox_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_showCacheStatusChk_clicked()
{
    if (okToUpdate) {
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cacheDelaySpinbox_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_showCacheThreadActivityChk_clicked()
{
    if (okToUpdate) {
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cacheStatusWidthSpin_valueChanged(int /* not used */)
{
    if (okToUpdate) {
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cache50AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 5;
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
   }
}

void Prefdlg::on_cache60AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 6;
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                  ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cache70AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 7;
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cache80AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 8;
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cache90AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 9;
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cache100AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 10;
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
                                   ui->cacheDelaySpinbox->value(),
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cachePreviewsChk_clicked()
{
    emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                               ui->showCacheStatusChk->isChecked(),
                               ui->cacheDelaySpinbox->value(),
                               ui->cacheStatusWidthSpin->value(),
                               cacheWtAhead,
                               ui->cachePreviewsChk->isChecked(),
                               ui->showCacheThreadActivityChk->isChecked());
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


void Prefdlg::on_globalFontSizeSlider_valueChanged(int value)
{
    // used to prevent font size updates while slider is pressed to prevent slider latency
    // if user just clicks on slider then updateFontSize is emitted
    if(isSliderPressed) return;
    emit updateFontSize(QString::number(value));
}

void Prefdlg::on_globalFontSizeSlider_sliderReleased()
{
    int value = ui->globalFontSizeSlider->value();
    emit updateFontSize(QString::number(value));
}

void Prefdlg::on_classificationBadgeImageDiamSlider_valueChanged(int value)
{
    emit updateClassificationBadgeImageDiam(value);
}

