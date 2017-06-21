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
    qDebug() << "MW::thumbWidth" << mw->thumbWidth;
    mw->thumbWidth = 99;  //this works

//    qDebug() << "test" << mw->test;
    okToUpdate = false;
    ui->setupUi(this);
    // thumbs
    ui->iconWidthSlider->setValue(G::thumbWidth);
    ui->iconHeightSlider->setValue(G::thumbHeight);
    ui->iconPaddingSlider->setValue(G::thumbPadding);
    ui->thumbSpacingSlider->setValue(G::thumbSpacing);
    ui->fontSizeSlider->setValue(G::labelFontSize);
    ui->showThumbLabelChk->setChecked(G::showThumbLabels);
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
        G::thumbWidth = ui->iconWidthSlider->value();
        if (ui->lockDimChk->isChecked()) {
            ui->iconHeightSlider->setValue(G::thumbWidth);
            G::thumbHeight = G::thumbWidth;
        }
        emit updateThumbs();
    }
}

void Prefdlg::on_iconHeightSlider_valueChanged(int value)
{
    if (okToUpdate) {
        G::thumbHeight = ui->iconHeightSlider->value();
        if (ui->lockDimChk->isChecked()) {
            ui->iconWidthSlider->setValue(G::thumbHeight);
            G::thumbWidth = G::thumbHeight;
        }
        emit updateThumbs();
    }
}

void Prefdlg::on_thumbSpacingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        G::thumbSpacing = ui->thumbSpacingSlider->value();
        emit updateThumbs();
    }
}

void Prefdlg::on_iconPaddingSlider_valueChanged(int value)
{
    if (okToUpdate) {
        G::thumbPadding = ui->iconPaddingSlider->value();
        emit updateThumbs();
    }
}


void Prefdlg::on_showThumbLabelChk_clicked()
{
    if (okToUpdate) {
        G::showThumbLabels = ui->showThumbLabelChk->isChecked();
        emit updateThumbs();
    }
}

void Prefdlg::on_fontSizeSlider_valueChanged(int value)
{
    if (okToUpdate) {
        G::labelFontSize = ui->fontSizeSlider->value();
        emit updateThumbs();
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
