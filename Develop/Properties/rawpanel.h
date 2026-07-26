#ifndef RAWPANEL_H
#define RAWPANEL_H

#include <QWidget>
#include <QString>

class QLabel;
class QRadioButton;
class QComboBox;
class QCheckBox;
class QSlider;
class QSettings;
class BarBtn;

/*
    RawPanel (experimental, behind G::useLayerHeaderLab) -- the Develop dock's raw-decode
    strip, hoisted OUT of the property tree's Base "Core" rows into a self-contained panel
    that sits ABOVE the Layers list. Shown only for raw files (DevelopProperties toggles
    its visibility via currentIsRaw()).

        | Raw                               [?] |
        | Edit: (o) Raw  ( ) Embedded Preview   |
        | Demosaic   [ Winnow            v ]     |   <- Demosaic/Denoise hidden unless Raw
        | Denoise [x]            Auto run [x]    |   <- Denoise block hidden on Apple
        |   Lum   ------o------                 |
        |   Color -----------o-                 |

    Like TransformPanel it carries NO model state: every control emits a signal and every
    display is pushed back in by DevelopProperties (setEditSource / setEngine /
    setDenoiseRunState / setDenoiseValues). The raw params themselves live on the Base
    layer (layer 0) of the edit stack. Signal/param contract mirrors the old Core rows so
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
    /* denoised: a PMRID base is ready (checkbox -> "Denoised", sliders enabled). */
    void setDenoiseRunState(bool denoised);
    void setAutoRun(bool on);
    void setDenoiseValues(int luma0to100, int chroma0to100);

signals:
    void tipsRequested();                         // [?]
    void editSourceChanged(bool raw);             // Raw / Embedded Preview
    void demosaicChanged(bool apple);             // Apple (true) / Winnow (false)
    void denoiseRunToggled(bool on);              // "Denoise" checkbox (run / clear)
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
    QWidget      *denoiseBlock = nullptr;         // Denoise row + sliders (Apple hides)
    QCheckBox    *denoiseCheck = nullptr;
    QCheckBox    *autoRunCheck = nullptr;
    QSlider      *lumaSlider   = nullptr;
    QSlider      *chromaSlider = nullptr;
    QLabel       *lumaValue    = nullptr;
    QLabel       *chromaValue  = nullptr;
};

#endif // RAWPANEL_H
