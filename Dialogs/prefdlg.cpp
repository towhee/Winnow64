#include "prefdlg.h"
#include "ui_prefdlg.h"
#include "global.h"
//#include "mainwindow.h"

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
    ui->rememberFolderChk->setChecked(mw->rememberLastDir);
    ui->maxRecentSB->setValue(mw->maxRecentFolders);
    ui->trackpadIterateRadio->setChecked(!mw->imageView->useWheelToScroll);
    ui->trackpadScrollRadio->setChecked(mw->imageView->useWheelToScroll);
    ui->displayHorizontalPixelsSB->setValue(mw->displayHorizontalPixels);
    ui->displayVerticalPixelsSB->setValue(mw->displayVerticalPixels);

    // thumbs
    ui->iconWidthSlider->setValue(mw->thumbView->thumbWidth);
    ui->iconHeightSlider->setValue(mw->thumbView->thumbHeight);
    ui->iconPaddingSlider->setValue(mw->thumbView->thumbPadding);
    ui->thumbSpacingSlider->setValue(mw->thumbView->thumbSpacing);
    ui->fontSizeSlider->setValue(mw->thumbView->labelFontSize);
    ui->showThumbLabelChk->setChecked(mw->thumbView->showThumbLabels);
    ui->lockDimChk->setChecked(true);

    // thumbsGrid
    ui->iconWidthSlider_2->setValue(mw->gridView->thumbWidth);
    ui->iconHeightSlider_2->setValue(mw->gridView->thumbHeight);
    ui->iconPaddingSlider_2->setValue(mw->gridView->thumbPadding);
    ui->thumbSpacingSlider_2->setValue(mw->gridView->thumbSpacing);
    ui->fontSizeSlider_2->setValue(mw->gridView->labelFontSize);
    ui->showThumbLabelChk_2->setChecked(mw->gridView->showThumbLabels);
    ui->lockDimChk_2->setChecked(true);

    // slideshow
    ui->slideshowDelaySpinbox->setValue(mw->slideShowDelay);
    ui->slideshowRandomChk->setChecked(mw->slideShowRandom);

    // cache
    ui->cacheSizeSpinbox->setValue(mw->cacheSizeMB / 1000);
    ui->showCacheStatusChk->setChecked(mw->isShowCacheStatus);
    ui->showCacheThreadActivityChk->setChecked(mw->isShowCacheThreadActivity);
    ui->cacheStatusWidthSpin->setValue(mw->cacheStatusWidth);
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
}

Prefdlg::~Prefdlg()
{
    delete ui;
}

void Prefdlg::accept()
{
    QDialog::accept();
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
            ui->iconHeightSlider->setValue(ui->iconWidthSlider->value());
        }
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked());
    }
}

void Prefdlg::on_iconHeightSlider_valueChanged(int value)
{
    if (okToUpdate) {
        if (ui->lockDimChk->isChecked()) {
            ui->iconWidthSlider->setValue(ui->iconHeightSlider->value());
        }
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked());
    }
}

void Prefdlg::on_thumbSpacingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked());
    }
}

void Prefdlg::on_iconPaddingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked());
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
                                   ui->showThumbLabelChk->isChecked());
    }
}

void Prefdlg::on_fontSizeSlider_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateThumbParameters(ui->iconWidthSlider->value(),
                                   ui->iconHeightSlider->value(),
                                   ui->thumbSpacingSlider->value(),
                                   ui->iconPaddingSlider->value(),
                                   ui->fontSizeSlider->value(),
                                   ui->showThumbLabelChk->isChecked());
    }
}

// slideshow
void Prefdlg::on_slideshowDelaySpinbox_valueChanged(int value)
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
void Prefdlg::on_cacheSizeSpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
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
                                   ui->cacheStatusWidthSpin->value(),
                                   cacheWtAhead,
                                   ui->cachePreviewsChk->isChecked(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cacheStatusWidthSpin_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                                   ui->showCacheStatusChk->isChecked(),
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
                               ui->cacheStatusWidthSpin->value(),
                               cacheWtAhead,
                               ui->cachePreviewsChk->isChecked(),
                               ui->showCacheThreadActivityChk->isChecked());
}

void Prefdlg::on_previewWidthSB_valueChanged(int value)
{
    emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                               ui->showCacheStatusChk->isChecked(),
                               ui->cacheStatusWidthSpin->value(),
                               cacheWtAhead,
                               ui->cachePreviewsChk->isChecked(),
                               ui->showCacheThreadActivityChk->isChecked());
}

void Prefdlg::on_previewHeightSB_valueChanged(int value)
{
    emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                               ui->showCacheStatusChk->isChecked(),
                               ui->cacheStatusWidthSpin->value(),
                               cacheWtAhead,
                               ui->cachePreviewsChk->isChecked(),
                               ui->showCacheThreadActivityChk->isChecked());
}

void Prefdlg::on_rememberFolderChk_clicked()
{
    if (okToUpdate) {
        emit updateRememberFolder(ui->rememberFolderChk->isChecked());
    }
}

void Prefdlg::on_maxRecentSB_valueChanged(int value)
{
//    if (okToUpdate) {
//        emit updateMaxRecentFolders(ui->maxRecentSB->value());
//    }
}

void Prefdlg::on_maxRecentSB_destroyed()
{
    /* Do not update until dialog closing.  If the previous value was 10 and
     * then changed to 50 this would be two updates: 5 and 50.  The change to
     * 5 would result in the list from 6-10 being pruned in the menu before
     * the max was increased to 50.
     */
    if (okToUpdate) {
        emit updateMaxRecentFolders(ui->maxRecentSB->value());
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

void Prefdlg::on_displayHorizontalPixelsSB_valueChanged(int arg1)
{
    if(okToUpdate) {
        emit updateDisplayResolution(ui->displayHorizontalPixelsSB->value(),
                                     ui->displayVerticalPixelsSB->value());
    }
}

void Prefdlg::on_displayVerticalPixelsSB_valueChanged(int arg1)
{
    if(okToUpdate) {
        emit updateDisplayResolution(ui->displayHorizontalPixelsSB->value(),
                                     ui->displayVerticalPixelsSB->value());
    }
}

// grid preferences
void Prefdlg::on_iconWidthSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        if (ui->lockDimChk_2->isChecked()) {
            ui->iconHeightSlider_2->setValue(ui->iconWidthSlider_2->value());
        }
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked());
    }
}

void Prefdlg::on_iconHeightSlider_2_valueChanged(int value)
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
                                       ui->showThumbLabelChk_2->isChecked());
    }
}

void Prefdlg::on_thumbSpacingSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked());
    }
}

void Prefdlg::on_iconPaddingSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked());
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
                                       ui->showThumbLabelChk_2->isChecked());
    }
}

void Prefdlg::on_fontSizeSlider_2_valueChanged(int value)
{
    if (okToUpdate) {
        emit updateThumbGridParameters(ui->iconWidthSlider_2->value(),
                                       ui->iconHeightSlider_2->value(),
                                       ui->iconPaddingSlider_2->value(),
                                       ui->thumbSpacingSlider_2->value(),
                                       ui->fontSizeSlider_2->value(),
                                       ui->showThumbLabelChk_2->isChecked());
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


