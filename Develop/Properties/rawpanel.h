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
        | Denoise [x]            Auto run [x]    |   <- Denoise block hidden on Apple
        |   <reason>                            |   <- only when denoise cannot run
        |   Lum   ------o------                 |
        |   Color -----------o-                 |

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
    /* denoised: a PMRID base is ready (checkbox -> "Denoised", sliders enabled). */
    void setDenoiseRunState(bool denoised);
    /* available == false greys the whole denoise group (checkbox, Auto run, amounts) and
       shows `reason` in its place, so a denoise that CANNOT work says so up front instead
       of accepting a click and quietly changing nothing. Sticky: setDenoiseRunState will
       not re-enable the amounts while it is false. */
    void setDenoiseAvailable(bool available, const QString &reason = QString());
    void setAutoRun(bool on);
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
    PanelEditor  *denoiseEditor = nullptr;        // tree-styled Lum/Color sliders
    QLabel       *denoiseNote  = nullptr;         // why the group is greyed (hidden when ok)
    bool          denoiseAvailable = true;        // false: PMRID cannot run here
};

#endif // RAWPANEL_H
