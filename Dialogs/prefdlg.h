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
    explicit Prefdlg(QWidget *parent = 0);
    ~Prefdlg();

signals:
//    void updateGeneralParameters(bool rememberFolder, bool inclSubfolders,
//                                 int maxRecent);
    void updateRememberFolder(bool rememberFolder);
    void updateInclSubfolders(bool inclSubfolders);
    void updateMaxRecentFolders(int maxRecentFolders);
    void updateThumbParameters(int thumbWidth, int thumbHeight, int thumbSpacing,
            int thumbPadding, int labelFontSize, bool showThumbLabels);
    void updateThumbDockParameters(bool isThumbWrap, bool isVerticalTitle);
    void updateSlideShowParameters(int slideShowDelay, bool slideShowRandom);
    void updateCacheParameters(int cacheSizeMB, bool isShowCacheStatus,
            int cacheStatusWidth, int cacheWtAhead);

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


    void on_maxRecentSB_valueChanged(int arg1);

private:

    Ui::Prefdlg *ui;
    void accept();
    bool okToUpdate;

    int thumbWidth;
    int thumbHeight;
    int thumbSpacing;
    int thumbPadding;
    int labelFontSize;
    bool showThumbLabels;

    bool isThumbWrap;
    bool isVerticalTitle;

    int slideShowDelay;
    bool slideShowRandom;

    int cacheSizeMB;
    bool isShowCacheStatus;
    int cacheStatusWidth;
    int cacheWtAhead;


};

#endif // PREFDLG_H
