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
    ui->trackpadIterateRadio->setChecked(!mw->imageView->isTrackpadScroll);
    ui->trackpadScrollRadio->setChecked(mw->imageView->isTrackpadScroll);

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

    // thumb dock
    ui->wrapThumbsChk->setChecked(mw->thumbView->isThumbWrapWhenTopOrBottomDock);
    ui->autoFitChk->setChecked(mw->thumbView->isAutoFit);
    ui->autoFit2Chk->setChecked(mw->thumbView->isAutoFit);
    ui->vertTitleChk->setChecked(mw->isThumbDockVerticalTitle);

    // slideshow
    ui->slideshowDelaySpinbox->setValue(mw->slideShowDelay);
    ui->slideshowRandomChk->setChecked(mw->slideShowRandom);

    // cache
    ui->cacheSizeSpinbox->setValue(mw->cacheSizeMB / 1000);
    ui->showCacheStatusChk->setChecked(mw->isShowCacheStatus);
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
    ui->previewWidthSB->setValue(mw->cachePreviewWidth);
    ui->previewHeightSB->setValue(mw->cachePreviewHeight);

    // full screen
    ui->foldersChk->setChecked(mw->fullScreenDocks.isFolders);
    ui->favsChk->setChecked(mw->fullScreenDocks.isFavs);
    ui->metadataChk->setChecked(mw->fullScreenDocks.isMetadata);
    ui->thumbsChk->setChecked(mw->fullScreenDocks.isThumbs);
    ui->statusBarChk->setChecked(mw->fullScreenDocks.isStatusBar);

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

void Prefdlg::on_listWidget_currentItemChanged(QListWidgetItem *current)
// sync preference category list and preference items in stacked form
{
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
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
                                   ui->previewWidthSB->value(),
                                   ui->previewHeightSB->value(),
                                   ui->showCacheThreadActivityChk->isChecked());
    }
}

void Prefdlg::on_cachePreviewsChk_clicked()
{
    if (ui->cachePreviewsChk->isChecked()) {
        ui->previewWidthSB->setEnabled(true);
        ui->previewHeightSB->setEnabled(true);
    } else {
        ui->previewWidthSB->setEnabled(false);
        ui->previewHeightSB->setEnabled(false);
    }
    emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                               ui->showCacheStatusChk->isChecked(),
                               ui->cacheStatusWidthSpin->value(),
                               cacheWtAhead,
                               ui->cachePreviewsChk->isChecked(),
                               ui->previewWidthSB->value(),
                               ui->previewHeightSB->value(),
                               ui->showCacheThreadActivityChk->isChecked());
}

void Prefdlg::on_previewWidthSB_valueChanged(int value)
{
    emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                               ui->showCacheStatusChk->isChecked(),
                               ui->cacheStatusWidthSpin->value(),
                               cacheWtAhead,
                               ui->cachePreviewsChk->isChecked(),
                               ui->previewWidthSB->value(),
                               ui->previewHeightSB->value(),
                               ui->showCacheThreadActivityChk->isChecked());
}

void Prefdlg::on_previewHeightSB_valueChanged(int value)
{
    emit updateCacheParameters(ui->cacheSizeSpinbox->value(),
                               ui->showCacheStatusChk->isChecked(),
                               ui->cacheStatusWidthSpin->value(),
                               cacheWtAhead,
                               ui->cachePreviewsChk->isChecked(),
                               ui->previewWidthSB->value(),
                               ui->previewHeightSB->value(),
                               ui->showCacheThreadActivityChk->isChecked());
}

void Prefdlg::on_rememberFolderChk_clicked()
{
    if (okToUpdate) {
        emit updateRememberFolder(ui->rememberFolderChk->isChecked());
    }
}

// thumb dock

void Prefdlg::on_wrapThumbsChk_clicked()
{
    if (okToUpdate) {
        emit updateThumbDockParameters(ui->wrapThumbsChk->isChecked(),
                                       ui->autoFitChk->isChecked(),
                                       ui->vertTitleChk->isChecked());
    }
}

void Prefdlg::on_autoFitChk_clicked()
{
    if (okToUpdate) {
        ui->autoFit2Chk->setChecked(ui->autoFitChk->isChecked());
        emit updateThumbDockParameters(ui->wrapThumbsChk->isChecked(),
                                       ui->autoFitChk->isChecked(),
                                       ui->vertTitleChk->isChecked());
    }
}

void Prefdlg::on_autoFit2Chk_clicked()
{
    if (okToUpdate) {
        ui->autoFitChk->setChecked(ui->autoFit2Chk->isChecked());
        emit updateThumbDockParameters(ui->wrapThumbsChk->isChecked(),
                                       ui->autoFitChk->isChecked(),
                                       ui->vertTitleChk->isChecked());
    }
}

void Prefdlg::on_vertTitleChk_clicked()
{
    if (okToUpdate) {
        emit updateThumbDockParameters(ui->wrapThumbsChk->isChecked(),
                                       ui->autoFitChk->isChecked(),
                                       ui->vertTitleChk->isChecked());
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
    emit updateFullScreenDocks(ui->foldersChk->isChecked(), ui->favsChk->isChecked(),
                               ui->metadataChk->isChecked(), ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

void Prefdlg::on_favsChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(), ui->favsChk->isChecked(),
                               ui->metadataChk->isChecked(), ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

void Prefdlg::on_metadataChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(), ui->favsChk->isChecked(),
                               ui->metadataChk->isChecked(), ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

void Prefdlg::on_thumbsChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(), ui->favsChk->isChecked(),
                               ui->metadataChk->isChecked(), ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}


void Prefdlg::on_statusBarChk_clicked()
{
    emit updateFullScreenDocks(ui->foldersChk->isChecked(), ui->favsChk->isChecked(),
                               ui->metadataChk->isChecked(), ui->thumbsChk->isChecked(),
                               ui->statusBarChk->isChecked());
}

