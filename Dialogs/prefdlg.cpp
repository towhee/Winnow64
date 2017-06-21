#include "prefdlg.h"
#include "ui_prefdlg.h"
#include "global.h"
//#include "mainwindow.h"

#include <QDebug>

Prefdlg::Prefdlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Prefdlg)
{
    MW *mw = qobject_cast<MW*>(parent);
    thumbWidth = mw->thumbView->thumbWidth;
    thumbHeight = mw->thumbView->thumbHeight;
    thumbSpacing = mw->thumbView->thumbSpacing;
    thumbPadding = mw->thumbView->thumbPadding;
    labelFontSize = mw->thumbView->labelFontSize;
    showThumbLabels = mw->thumbView->showThumbLabels;

    okToUpdate = false;
    ui->setupUi(this);
    // thumbs
    ui->iconWidthSlider->setValue(thumbWidth);
    ui->iconHeightSlider->setValue(thumbHeight);
    ui->iconPaddingSlider->setValue(thumbPadding);
    ui->thumbSpacingSlider->setValue(thumbSpacing);
    ui->fontSizeSlider->setValue(labelFontSize);
    ui->showThumbLabelChk->setChecked(showThumbLabels);
    // slideshow
    ui->slideshowDelaySpinbox->setValue(G::slideShowDelay);
    ui->slideshowRandomChk->setChecked(G::slideShowRandom);
    // cache
    ui->cacheSizeSpinbox->setValue(G::cacheSizeMB);
    ui->showCacheStatusChk->setChecked(G::showCacheStatus);
    ui->cacheStatusWidthSpin->setValue(G::cacheStatusWidth);
    switch (G::cacheWtAhead) {
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
        emit updateThumbs(thumbWidth, thumbHeight, thumbPadding,
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
        emit updateThumbs(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

void Prefdlg::on_thumbSpacingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        thumbSpacing = ui->thumbSpacingSlider->value();
        emit updateThumbs(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

void Prefdlg::on_iconPaddingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        thumbPadding = ui->iconPaddingSlider->value();
        emit updateThumbs(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}


void Prefdlg::on_showThumbLabelChk_clicked()
{
    if (okToUpdate) {
        showThumbLabels = ui->showThumbLabelChk->isChecked();
        emit updateThumbs(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

void Prefdlg::on_fontSizeSlider_valueChanged(int value)
{
    if (okToUpdate) {
        labelFontSize = ui->fontSizeSlider->value();
        emit updateThumbs(thumbWidth, thumbHeight, thumbPadding,
                          thumbSpacing, labelFontSize, showThumbLabels);
    }
}

// slideshow
void Prefdlg::on_slideshowDelaySpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        G::slideShowDelay = ui->slideshowDelaySpinbox->value();
        qDebug() << "G::slideShowDelay =" << G::slideShowDelay;
    }
}

void Prefdlg::on_slideshowRandomChk_clicked()
{
    if (okToUpdate) {
        G::slideShowRandom = ui->slideshowRandomChk->isChecked();
        qDebug() << "G::slideShowRandom =" << G::slideShowRandom;
    }
}

// cache
void Prefdlg::on_cacheSizeSpinbox_valueChanged(int value)
{
    if (okToUpdate) {
        G::cacheSizeMB = value;
    }
}

void Prefdlg::on_showCacheStatusChk_clicked()
{
    if (okToUpdate) {
        G::showCacheStatus = ui->showCacheStatusChk->isChecked();
        qDebug() << "G::showCacheStatus =" << G::showCacheStatus;
    }
}

void Prefdlg::on_cacheStatusWidthSpin_valueChanged(int value)
{
    if (okToUpdate) {
        G::cacheStatusWidth = value;
    }
}

void Prefdlg::on_cache50AheadRadio_clicked()
{
    if (okToUpdate) G::cacheWtAhead = 5;
}

void Prefdlg::on_cache60AheadRadio_clicked()
{
    if (okToUpdate) G::cacheWtAhead = 6;
}

void Prefdlg::on_cache70AheadRadio_clicked()
{
    if (okToUpdate) G::cacheWtAhead = 7;
}

void Prefdlg::on_cache80AheadRadio_clicked()
{
    if (okToUpdate) G::cacheWtAhead = 8;
}

void Prefdlg::on_cache90AheadRadio_clicked()
{
    if (okToUpdate) G::cacheWtAhead = 9;
}

void Prefdlg::on_cache100AheadRadio_clicked()
{
    if (okToUpdate) G::cacheWtAhead = 10;
}
