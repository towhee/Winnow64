#ifndef RAWPANEL_H
#define RAWPANEL_H

#include <QWidget>
#include <QString>

class QLabel;
class QRadioButton;
class QComboBox;
class QCheckBox;
class QSettings;
class BarBtn;
class PanelEditor;

/*
    RawPanel -- the Develop dock's raw-decode strip, hoisted OUT of the property tree's
    Global "Core" rows into a self-contained panel that sits ABOVE the scope bar. Shown
    only for raw files (DevelopProperties toggles its visibility via currentIsRaw()).

        | Raw                               [?] |
        | Edit: (o) Raw  ( ) Embedded Preview   |
        | Demosaic   [ Winnow            v ]     |   <- Demosaic/Denoise hidden unless Raw
        | Noise Reduction:                      |   <- Denoise block hidden on Apple
        |   Auto run [ ]   Run on this image [ ] |   <- "Run on..." only while Auto off
        |   <reason>                            |   <- only when denoise cannot run
        |   Lum   ------o------                 |   <- only once this image is
        |   Color -----------o-                 |      denoised (setDenoiseRunState)

    Like TransformPanel it carries NO model state: every control emits a signal and every
    display is pushed back in by DevelopProperties (setEditSource / setEngine /
    setDenoiseRunState / setDenoiseValues). The raw params themselves live on the Global
    scope (scope 0) of the edit stack. Signal/param contract mirrors the old Core rows so
    the existing MW wiring (useRawRequested, demosaicEngineChanged, autoRunDenoiseToggled,
    runRawDenoiseRequested, clearRawDenoiseRequested) is reused unchanged.
*/
class RawPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RawPanel(QWidget *parent = nullptr, QSettings *settings = nullptr);

    void setEditSource(bool raw);                 // radios + show/hide the raw block
    void setEngine(bool apple);                   // combo + show/hide the denoise block
    /* The Apple decoder versions the current image supports (newest first) and the one
       its decode used; empty / 0 before it has been decoded. */
    void setAppleVersions(const QList<int> &newestFirst, int used);
    /* denoised: a PMRID base is ready for this image -> the Lum / Color sliders SHOW
       (they only scale that base, so they are hidden until it exists). */
    void setDenoiseRunState(bool denoised);
    /* "Run on this image" shows the image's SAVED choice (EditParams::denoiseRaw == 1),
       not whether the denoise has finished -- so it does not read unticked while a
       ticked image is still denoising. Silent: no denoiseRunToggled. */
    void setRunOnImage(bool on);
    /* available == false greys the denoise group (checkbox, Auto run), hides the amounts
       and shows `reason` in its place, so a denoise that CANNOT work says so up front
       instead of accepting a click and quietly changing nothing. Sticky:
       setDenoiseRunState will not show the amounts while it is false. */
    void setDenoiseAvailable(bool available, const QString &reason = QString());
    void setAutoRun(bool on);                     // also shows/hides "Run on this image"
    void setDenoiseValues(int luma0to100, int chroma0to100);
    void setCaptionWidth(int w);                  // align denoise sliders to the tree
    /* Collapse state, so the panel can take part in the tree's Solo / Expand-all /
       Collapse-all behaviour: it is a peer of the Basic / Color / Effects sections
       and the Scope row. setCollapsed is programmatic (persists, no signal). */
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return collapsed; }

signals:
    void tipsRequested();                         // [?]
    void collapseToggled(bool collapsed);         // USER collapsed / expanded the panel
    void editSourceChanged(bool raw);             // Raw / Embedded Preview
    void demosaicChanged(bool apple);             // Apple (true) / Winnow (false)
    void appleVersionChanged(int pin);            // an Apple entry: 0 = newest, else N
    void denoiseRunToggled(bool on);              // "Run on this image" (run / clear)
    void autoRunToggled(bool on);                 // "Auto run" checkbox
    void denoiseLumaChanged(int value0to100);
    void denoiseChromaChanged(int value0to100);

protected:
    void paintEvent(QPaintEvent *) override;      // gradient behind the header band
    /* Clicking the "Raw" caption toggles collapse like the arrow. */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void toggleCollapsed();                       // arrow / caption click
    void updateCollapseIcon();
    void updateRunOnImageVisible();               // hidden while Auto run is on
    void updateAmountsVisible();                  // denoiseReady && denoiseAvailable
    void rebuildEngineCombo();                    // from the three members below

    QSettings    *setting      = nullptr;
    QWidget      *headerBand   = nullptr;
    BarBtn       *collapseBtn  = nullptr;
    QLabel       *titleLabel   = nullptr;
    QWidget      *body         = nullptr;         // editRow + rawBlock (collapse hides)
    BarBtn       *tipBtn       = nullptr;
    bool          collapsed    = false;
    QRadioButton *rawRadio     = nullptr;
    QRadioButton *prevRadio    = nullptr;
    QWidget      *rawBlock     = nullptr;         // Demosaic + Denoise (Embedded hides)
    QComboBox    *engineCombo  = nullptr;
    QList<int>    appleVersions;                  // this image's, newest first
    int           appleUsed    = 0;               // the version its decode used
    bool          engineApple  = true;
    QWidget      *denoiseBlock = nullptr;         // Denoise row + sliders (Apple hides)
    QWidget      *appleBlock   = nullptr;         // "View cameras supported" (Apple only)
    QCheckBox    *denoiseCheck = nullptr;         // "Run on this image"
    QCheckBox    *autoRunCheck = nullptr;
    PanelEditor  *denoiseEditor = nullptr;        // tree-styled Lum/Color sliders
    QLabel       *denoiseNote  = nullptr;         // why the group is greyed (hidden when ok)
    bool          denoiseAvailable = true;        // false: PMRID cannot run here
    bool          denoiseReady = false;           // a PMRID base exists for this image
};

#endif // RAWPANEL_H
