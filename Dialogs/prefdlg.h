#ifndef PREFDLG_H
#define PREFDLG_H

#include <QDialog>
#include "Main/mainwindow.h"

namespace Ui {
class Prefdlg;
}

class Prefdlg : public QDialog
{
    Q_OBJECT

public:
    explicit Prefdlg(QWidget *parent = 0, int lastPrefPage = 0);
    ~Prefdlg();

signals:
    void updateFontSize(QString pixels);
    void updateClassificationBadgeImageDiam(int d);
//    void updateClassificationBadgeThumbDiam(int d);
    void updatePage(int page);
    void updateRememberFolder(bool rememberFolder);
    void updateMouseClickScroll(bool mouseClickScroll);
    void updateTrackpadScroll(bool trackpadScroll);
//    void updateDisplayResolution(int horizontalPixels, int verticalPixels);
    void updateThumbParameters(int thumbWidth, int thumbHeight, int thumbSpacing, int thumbPadding,
            int labelFontSize, bool showThumbLabels, bool wrapThumbs, int thumbBadgeSize);
    void updateThumbGridParameters(int thumbWidthGrid, int thumbHeightGrid, int thumbSpacingGrid,
            int thumbPaddingGrid, int labelFontSizeGrid, bool showThumbLabelsGrid, bool wrapThumbs,
            int thumbBadgeSize);
    void updateSlideShowParameters(int slideShowDelay, bool slideShowRandom);

    void updateCacheParameters(int cacheSizeMB, bool isShowCacheStatus, int cacheDelay,
            int cacheStatusWidth, int wtAhead, bool usePreview, bool activity);

    void updateFullScreenDocks(bool isFolders, bool isFavs, bool isFilters, bool isMetadata,
                               bool isThumbs, bool isStatusBar);

protected:

private slots:
    // general
    void on_rememberFolderChk_clicked();
    void on_trackpadIterateRadio_clicked();
    void on_trackpadScrollRadio_clicked();
    void on_slider_pressed();
    void on_slider_released();
    // thumbs
    void on_iconWidthSlider_valueChanged(int value);
    void on_iconHeightSlider_valueChanged(int value);
    void on_thumbSpacingSlider_valueChanged(int value);
    void on_iconPaddingSlider_valueChanged(int value);
    void on_showThumbLabelChk_clicked();
    void on_fontSizeSlider_valueChanged(int value);
    // thumbsGrid
    void on_iconWidthSlider_2_valueChanged(int value);
    void on_iconHeightSlider_2_valueChanged(int value);
    void on_thumbSpacingSlider_2_valueChanged(int value);
    void on_iconPaddingSlider_2_valueChanged(int value);
    void on_showThumbLabelChk_2_clicked();
    void on_fontSizeSlider_2_valueChanged(int value);
    // slideshow
    void on_slideshowDelaySpinbox_valueChanged(int value);
    void on_slideshowRandomChk_clicked();
    // cache
    void on_cacheSizeSpinbox_valueChanged(int value);
    void on_showCacheStatusChk_clicked();
    void on_cacheDelaySpinbox_valueChanged(int value);
    void on_showCacheThreadActivityChk_clicked();
    void on_cacheStatusWidthSpin_valueChanged(int value);
    void on_cache50AheadRadio_clicked();
    void on_cache60AheadRadio_clicked();
    void on_cache70AheadRadio_clicked();
    void on_cache80AheadRadio_clicked();
    void on_cache90AheadRadio_clicked();
    void on_cache100AheadRadio_clicked();
    void on_cachePreviewsChk_clicked();
    void on_listWidget_currentItemChanged(QListWidgetItem *current);
    void on_foldersChk_clicked();
    void on_favsChk_clicked();
    void on_filtersChk_clicked();
    void on_metadataChk_clicked();
    void on_thumbsChk_clicked();
    void on_statusBarChk_clicked();
    void on_infoField_changed();
    void on_tableField_changed();
//    void on_displayHorizontalPixelsSB_valueChanged(int arg1);
//    void on_displayVerticalPixelsSB_valueChanged(int arg1);
    void on_wrapChk_clicked();
    void on_mouseClickScrollChk_clicked();
    void on_globalFontSizeSlider_valueChanged(int value);
    void on_globalFontSizeSlider_sliderReleased();

    void on_classificationBadgeImageDiamSlider_valueChanged(int value);

    void on_classificationBadgeThumbDiamSlider_valueChanged(int);

private:
    Ui::Prefdlg *ui;
    QStandardItemModel *okInfo;
    QStandardItemModel *okTable;
    void reject();
    bool okToUpdate;
    bool isSliderPressed;

   int cacheWtAhead;
   QString source;     // "thumbView" or "gridView"

};

#endif // PREFDLG_H
