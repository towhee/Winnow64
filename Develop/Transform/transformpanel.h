#ifndef TRANSFORMPANEL_H
#define TRANSFORMPANEL_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QPair>

class QComboBox;
class QToolButton;
class QLineEdit;
class QLabel;
class QButtonGroup;
class QSettings;
class BarBtn;

/*
    The Develop dock's Transform (crop + straighten + perspective) panel: a compact control strip
    placed BELOW the scopes strip and ABOVE the property tree (see MW::createDevelopDock). It is the
    sibling of ScopesView -- a self-contained QWidget that owns only its widgets and emits a signal
    for each user action. It carries NO geometry/pixel state: the actual crop rectangle, straighten
    angle and perspective homography live in the (non-destructive) develop edit stack and are applied
    last in the pipeline (see notes/Documentation.txt "Transform"). This class is the UI surface for
    that model; the image-side overlay and the warp engine are wired to these signals via MW.

    Layout:
        - A property-style gradient header ("Transform") with three trailing BarBtns: [?] tips,
          [E] preview eye (show/ignore the whole transform), [R] reset (clear crop/straighten/warp).
        - A three-way MODE toggle (Crop / Level / Warp -- only one selected at a time; shortcuts
          C / L / W whenever Transform is active, whether the panel or the crop overlay holds
          focus) down the left, each row carrying that mode's controls
          and its own [R] reset (all reset buttons kept vertically aligned):
            Crop : aspect combo + aspect-lock padlock + flip (F) + reset
            Level: straighten-angle field                        + reset
            Warp : perspective hint                              + reset

    Custom aspects, the last-used aspect, the lock state, the flip (portrait) state and the
    selected mode persist to QSettings under "Develop/Transform/".
*/
class TransformPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TransformPanel(QWidget *parent = nullptr, QSettings *settings = nullptr);

    /* Mode toggle. Kept in the model as an int so MW can switch on it without this header. */
    enum Mode { CropMode = 0, LevelMode = 1, WarpMode = 2 };

    bool    isAspectLocked() const { return aspectLocked; }
    bool    isAspectFlipped() const { return aspectFlipped; }  // portrait orientation
    QString aspectKey() const;      // the selected entry's key, e.g. "asShot", "3:2", "21:9"
    double  aspectRatio() const;    // w/h of the selected entry; 0.0 for "As shot" (free)
    int     mode() const { return currentMode; }

    /* Sync the Preview eye to the current image's stored Geometry::show (no signal emitted). */
    void setPreviewShown(bool shown);
    /* Sync the Level angle field to the current image's stored straighten (no signal emitted). */
    void setLevelAngle(double degrees);
    /* Select a mode programmatically (no modeChanged emitted). */
    void setMode(int mode);
    /* Select the "As shot" (free) aspect programmatically (no aspectChanged emitted).
       Used by the crop reset so the full-frame reset is not re-inscribed by a locked
       aspect. */
    void setAspectAsShot();

signals:
    void aspectChanged(const QString &key, double ratio);  // ratio = w/h, 0.0 = As shot / free
    void aspectLockToggled(bool locked);
    void aspectFlipToggled(bool flipped);  // flip crop between landscape and portrait
    void modeChanged(int mode);            // Crop / Level / Warp selected (see Mode)
    void levelAngleEntered(double degrees);// absolute straighten typed into the Level field
    void rectifyRequested();               // apply perspective rectify to the drawn quad
    void tipsRequested();                  // [?] clicked
    void previewToggled(bool shown);       // eye: show (true) or ignore (false) the transform
    void resetRequested();                 // header reset: clear crop/straighten/warp to identity
    void resetModeRequested(int mode);     // per-row reset: clear just this mode's contribution

public slots:
    void toggleAspectLock();               // lock button click, or the "A" key (see eventFilter)
    void toggleAspectFlip();               // flip button click, or the "F" key
    /* Act on a bare A/F/C/L/W transform shortcut. Called from eventFilter (panel-
       focused) and, while the crop overlay holds focus, routed here from ImageView
       via MW. Returns true if the key was one of ours and was acted on. */
    bool handleTransformShortcut(int key);

protected:
    /* Single-letter shortcuts (A lock, C Crop, L Level, W Warp) are claimed via ShortcutOverride
       so a bare letter acts on the focused panel instead of a window-level shortcut. Text editors
       (the aspect combo, the angle field) are deliberately NOT filtered so typing still works. */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onAspectActivated(int index);     // combo choice (intercepts "Add custom aspect...")

private:
    void buildUi();
    void selectMode(int mode);             // user-driven mode change (emits modeChanged + persists)
    void populateAspectCombo();            // (re)fill presets + persisted custom aspects
    void addAspectItem(const QString &caption, const QString &key, double ratio);
    void promptAddCustomAspect();          // dialog -> append + persist a custom ratio
    void updateLockButton();               // swap the padlock glyph + tooltip for aspectLocked
    void updateFlipButton();               // reflect aspectFlipped in the flip button
    void updatePreviewButton();            // set the eye glyph/tooltip from previewShown
    void loadCustomAspects();
    void saveCustomAspects();

    QSettings   *setting = nullptr;

    QToolButton *cropModeBtn  = nullptr;
    QToolButton *levelModeBtn = nullptr;
    QToolButton *warpModeBtn  = nullptr;
    QButtonGroup *modeGroup   = nullptr;

    QComboBox   *aspectCombo  = nullptr;
    QToolButton *lockBtn      = nullptr;
    BarBtn      *flipBtn      = nullptr;    // swap aspect landscape <-> portrait
    QLineEdit   *angleEdit    = nullptr;
    BarBtn      *tipBtn       = nullptr;
    BarBtn      *previewBtn   = nullptr;    // eye: show/ignore the transform (Geometry::show)
    BarBtn      *headerResetBtn = nullptr;  // clear crop/straighten/warp back to identity

    bool previewShown = true;              // mirror of the current image's Geometry::show
    bool aspectLocked = false;
    bool aspectFlipped = false;            // true = portrait: honour 1/ratio
    int  currentMode = CropMode;
    int  lastAspectIndex = 0;              // restore selection after a "custom" cancel

    /* Persisted custom aspects: caption ("21 x 9") -> ratio (w/h). */
    QVector<QPair<QString, double>> customAspects;

    /* UserRole payloads on each combo item. */
    enum AspectRole { KeyRole = Qt::UserRole + 1, RatioRole, ActionRole };
    enum AspectAction { NoAction = 0, AddCustomAction = 1 };
};

#endif // TRANSFORMPANEL_H
