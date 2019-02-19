#ifndef PREFDLG_H
#define PREFDLG_H

#include <QDialog>
#include <QtWidgets>

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
    void  updatePage(int page);

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
    void on_progressWidthSlider_valueChanged(int value);
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
    void on_wrapChk_clicked();
    void on_mouseClickScrollChk_clicked();
    void on_globalFontSizeSlider_valueChanged(int value);
    void on_globalFontSizeSlider_sliderReleased();
    void on_classificationBadgeImageDiamSlider_valueChanged(int value);
    void on_classificationBadgeThumbDiamSlider_valueChanged(int);
    void on_updateAppChk_stateChanged(int value);


    void on_infoFontSizeSlider_valueChanged(int value);

    void on_coresSpinBox_valueChanged(int arg1);


    void on_aSynchChk_clicked();

    void on_bestAspectChk_clicked(bool checked);

    void on_bestAspectChk_2_clicked(bool checked);

    void on_lockDimChk_clicked(bool checked);

    void on_lockDimChk_2_clicked(bool checked);
private:
    Ui::Prefdlg *ui;
    QStandardItemModel *okInfo;
    QStandardItemModel *okTable;
    void reject();
    bool okToUpdate;
    bool isSliderPressed;

    bool ignoreThumbBestAspectChange;
    bool ignoreGridBestAspectChange;

   int cacheWtAhead;
   QString source;     // "thumbView" or "gridView"

};

#endif // PREFDLG_H
