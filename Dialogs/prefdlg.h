#ifndef PREFDLG_H
#define PREFDLG_H

#include <QDialog>
#include "mainwindow.h"

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
    void updatePage(int page);
    void updateRememberFolder(bool rememberFolder);
    void updateInclSubfolders(bool inclSubfolders);
    void updateMaxRecentFolders(int maxRecentFolders);
    void updateThumbParameters(int thumbWidth, int thumbHeight, int thumbSpacing,
            int thumbPadding, int labelFontSize, bool showThumbLabels);
    void updateThumbGridParameters(int thumbWidthGrid, int thumbHeightGrid, int thumbSpacingGrid,
                               int thumbPaddingGrid, int labelFontSizeGrid, bool showThumbLabelsGrid);
    void updateThumbDockParameters(bool isThumbWrap, bool isAutoFit, bool isVerticalTitle);
    void updateSlideShowParameters(int slideShowDelay, bool slideShowRandom);
    void updateCacheParameters(int cacheSizeMB, bool isShowCacheStatus,
            int cacheStatusWidth, int cacheWtAhead, bool usePreview,
                               int previewWidth, int previewHeight);
    void updateFullScreenDocks(bool isFolders, bool isFavs, bool isMetadata,
                               bool isThumbs, bool isStatusBar);

private slots:
    // general
    void on_rememberFolderChk_clicked();
    void on_maxRecentSB_destroyed();
    // thumbs
    void on_iconWidthSlider_valueChanged(int value);
    void on_iconHeightSlider_valueChanged(int value);
    void on_thumbSpacingSlider_valueChanged(int value);
    void on_iconPaddingSlider_valueChanged(int value);
    void on_showThumbLabelChk_clicked();
    void on_fontSizeSlider_valueChanged(int value);
    void on_wrapThumbsChk_clicked();
    void on_vertTitleChk_clicked();
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
    void on_cacheStatusWidthSpin_valueChanged(int value);
    void on_cache50AheadRadio_clicked();
    void on_cache60AheadRadio_clicked();
    void on_cache70AheadRadio_clicked();
    void on_cache80AheadRadio_clicked();
    void on_cache90AheadRadio_clicked();
    void on_cache100AheadRadio_clicked();
    void on_cachePreviewsChk_clicked();
    void on_previewWidthSB_valueChanged(int value);
    void on_previewHeightSB_valueChanged(int value);
    void on_maxRecentSB_valueChanged(int value);
    void on_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_foldersChk_clicked();
    void on_favsChk_clicked();
    void on_metadataChk_clicked();
    void on_thumbsChk_clicked();
    void on_statusBarChk_clicked();

    void on_autoFitChk_clicked();

    void on_autoFit2Chk_clicked();

private:

    Ui::Prefdlg *ui;
    void accept();
    bool okToUpdate;

   int cacheWtAhead;


};

#endif // PREFDLG_H
