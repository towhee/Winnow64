#include "prefdlg.h"
#include "ui_prefdlg.h"
#include "global.h"
//#include "mainwindow.h"

#include <QDebug>

Prefdlg::Prefdlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Prefdlg)
{
    okToUpdate = false;
    ui->setupUi(this);
    // this works because friend class of MW
    MW *mw = qobject_cast<MW*>(parent);
    // thumbs
    thumbWidth = mw->thumbView->thumbWidth;
    thumbHeight = mw->thumbView->thumbHeight;
    thumbSpacing = mw->thumbView->thumbSpacing;
    thumbPadding = mw->thumbView->thumbPadding;
    labelFontSize = mw->thumbView->labelFontSize;
    showThumbLabels = mw->thumbView->showThumbLabels;
    ui->iconWidthSlider->setValue(thumbWidth);
    ui->iconHeightSlider->setValue(thumbHeight);
    ui->iconPaddingSlider->setValue(thumbPadding);
    ui->thumbSpacingSlider->setValue(thumbSpacing);
    ui->fontSizeSlider->setValue(labelFontSize);
    ui->showThumbLabelChk->setChecked(showThumbLabels);
    // slideshow
    slideShowDelay = mw->slideShowDelay;
    slideShowRandom = mw->slideShowRandom;
    ui->slideshowDelaySpinbox->setValue(slideShowDelay);
    ui->slideshowRandomChk->setChecked(slideShowRandom);
    // cache
    cacheSizeMB = mw->cacheSizeMB;
    isShowCacheStatus = mw->isShowCacheStatus;
    cacheStatusWidth = mw->cacheStatusWidth;
    cacheWtAhead = mw->cacheWtAhead;
    ui->cacheSizeSpinbox->setValue(cacheSizeMB);
    ui->showCacheStatusChk->setChecked(isShowCacheStatus);
    ui->cacheStatusWidthSpin->setValue(cacheStatusWidth);
    switch (cacheWtAhead) {
    case 5: ui->cache50AheadRadio->setChecked(true); break;
    case 6: ui->cache60AheadRadio->setChecked(true); break;
    case 7: ui->cache70AheadRadio->setChecked(true); break;
    case 8: ui->cache80AheadRadio->setChecked(true); break;
    case 9: ui->cache90AheadRadio->setChecked(true); break;
    case 10: ui->cache100AheadRadio->setChecked(true);
    }

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

void Prefdlg::on_iconWidthSlider_valueChanged(int value)
{
    if (okToUpdate) {
        thumbWidth = ui->iconWidthSlider->value();
        if (ui->lockDimChk->isChecked()) {
            ui->iconHeightSlider->setValue(thumbWidth);
            thumbHeight = thumbWidth;
        }
        emit updateThumbParameters(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

void Prefdlg::on_iconHeightSlider_valueChanged(int value)
{
    if (okToUpdate) {
        thumbHeight = ui->iconHeightSlider->value();
        if (ui->lockDimChk->isChecked()) {
            ui->iconWidthSlider->setValue(thumbHeight);
            thumbWidth = thumbHeight;
        }
        emit updateThumbParameters(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

void Prefdlg::on_thumbSpacingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        thumbSpacing = ui->thumbSpacingSlider->value();
        emit updateThumbParameters(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

void Prefdlg::on_iconPaddingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        thumbPadding = ui->iconPaddingSlider->value();
        emit updateThumbParameters(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}


void Prefdlg::on_showThumbLabelChk_clicked()
{
    if (okToUpdate) {
        showThumbLabels = ui->showThumbLabelChk->isChecked();
        emit updateThumbParameters(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

void Prefdlg::on_fontSizeSlider_valueChanged(int value)
{
    if (okToUpdate) {
        labelFontSize = ui->fontSizeSlider->value();
        emit updateThumbParameters(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

// slideshow
void Prefdlg::on_slideshowDelaySpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        slideShowDelay = ui->slideshowDelaySpinbox->value();
        emit updateSlideShowParameters(slideShowDelay, slideShowRandom);
    }
}

void Prefdlg::on_slideshowRandomChk_clicked()
{
    if (okToUpdate) {
        slideShowRandom = ui->slideshowRandomChk->isChecked();
        emit updateSlideShowParameters(slideShowDelay, slideShowRandom);
    }
}

// cache
void Prefdlg::on_cacheSizeSpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        cacheSizeMB = value;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_showCacheStatusChk_clicked()
{
    if (okToUpdate) {
        isShowCacheStatus = ui->showCacheStatusChk->isChecked();
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_cacheStatusWidthSpin_valueChanged(int value)
{
    if (okToUpdate) {
        cacheStatusWidth = value;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_cache50AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 5;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_cache60AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 6;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_cache70AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 7;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_cache80AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 8;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_cache90AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 9;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}

void Prefdlg::on_cache100AheadRadio_clicked()
{
    if (okToUpdate) {
        cacheWtAhead = 10;
        emit updateCacheParameters(cacheSizeMB, isShowCacheStatus, cacheStatusWidth,
            cacheWtAhead);
    }
}
