#ifndef TRANSFORMPANEL_H
#define TRANSFORMPANEL_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QPair>

class QComboBox;
class QCheckBox;
class QRadioButton;
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
        - A property-style gradient header ("Transform") with four trailing BarBtns: [?] tips,
          [E] preview eye (show/ignore the whole transform), [R] reset (clear crop/straighten/warp)
          and [X] close (extreme right -- commits the session and hides the panel, same as "R").
        - Three groups, each opening with a heading row that names the group, the modifier
          that drives it on the image and its own [R] reset in the last column (aligned);
          the group's controls follow with indented captions:
            Crop:           No modifier                      reset (crop)
              Aspect [combo]
              Lock   [ ]    A
              Flip   o Landscape  o Portrait  X
            ----------------------------------------------------
            Level:          Cmd modifier                     reset (straighten)
              Angle  [field]
            ----------------------------------------------------
            Warp:           Opt modifier                     reset (perspective)
            ----------------------------------------------------
                            Space bar to pan                 (all modifiers)
          The grey tips (keys) share one column so they line up, except Flip's X, which
          follows the radios.
          There are no modes: the crop tool is always live on the image, Cmd (Ctrl) held
          draws a level line and Opt (Alt) held drags warp corners (see ImageView), so the
          group headings name the modifier.

    Custom aspects, the last-used aspect, the lock state and the flip (portrait) state
    persist to QSettings under "Develop/Transform/".
*/
class TransformPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TransformPanel(QWidget *parent = nullptr, QSettings *settings = nullptr);

    /* The part of the transform a per-row reset clears (resetPartRequested). */
    enum Part { CropPart = 0, LevelPart = 1, WarpPart = 2 };

    bool    isAspectLocked() const { return aspectLocked; }
    bool    isAspectFlipped() const { return aspectFlipped; }  // portrait orientation
    QString aspectKey() const;      // the selected entry's key, e.g. "asShot", "free", "3:2"
    /* w/h of the selected entry; 0.0 for "As shot" (the native ratio), kCustomRatio for
       "Custom" (whatever ratio the crop frame has now -- ImageView resolves it). */
    double  aspectRatio() const;
    static constexpr double kCustomRatio = -1.0;

    /* Sync the Preview eye to the current image's stored Geometry::show (no signal emitted). */
    void setPreviewShown(bool shown);
    /* Sync the Level angle field to the current image's stored straighten (no signal emitted). */
    void setLevelAngle(double degrees);
    /* Put the crop controls back to their neutral state -- "As shot", unlocked, unflipped
       -- with no signal emitted. Used by the crop reset: a reset means the WHOLE image, so
       every constraint has to go. Leaving the lock or the flip set would re-inscribe the
       full frame to that ratio and the reset would appear to do nothing (or, with the flip
       on, hand back a portrait box). */
    void resetCropState();
    /* The crop frame was resized freely (unlocked) off the selected ratio: show "Custom",
       with no aspectChanged emitted -- ImageView has already adopted the frame's ratio.
       The flip goes too: the frame's own ratio already carries its orientation. */
    void setCustomAspect();

signals:
    /* ratio = w/h, 0.0 = As shot / free. refit == false means the ratio was RELEASED
       rather than chosen -- the selected custom aspect was deleted out from under the
       crop -- so the constraint goes but the crop frame must be left where it is. */
    void aspectChanged(const QString &key, double ratio, bool refit = true);
    void aspectLockToggled(bool locked);
    void aspectFlipToggled(bool flipped);  // flip crop between landscape and portrait
    void levelAngleEntered(double degrees);// absolute straighten typed into the Level field
    void rectifyRequested();               // apply perspective rectify to the drawn quad
    void tipsRequested();                  // [?] clicked
    void previewToggled(bool shown);       // eye: show (true) or ignore (false) the transform
    void resetRequested();                 // header reset: all transforms to identity
    void closeRequested();                 // header [X]: close panel (commits session)
    void resetPartRequested(int part);     // per-row reset: clear just this Part's contribution

public slots:
    void toggleAspectLock();               // the "A" key (see eventFilter)
    void toggleAspectFlip();               // the "X" key
    /* Act on a bare A/X transform shortcut. Called from eventFilter (panel-
       focused) and, while the crop overlay holds focus, routed here from ImageView
       via MW. Returns true if the key was one of ours and was acted on. */
    bool handleTransformShortcut(int key);

protected:
    /* Separator rule (G::panelBorderHeight in G::panelSeparatorColor) across the bottom
       edge, so stacked Develop panels read as distinct blocks. */
    void paintEvent(QPaintEvent *event) override;
    /* Single-letter shortcuts (A lock, X flip) are claimed via
       ShortcutOverride so a bare letter acts on the focused panel instead of a
       window-level shortcut. Text editors (the aspect combo, the angle field) are
       deliberately NOT filtered so typing still works. */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onAspectActivated(int index);     // combo choice (intercepts "Add custom aspect...")

private:
    /* The bookkeeping behind a combo selection: remember it, persist it, and announce it.
       refit == false is the deletion path (see the aspectChanged signal). */
    void applyAspect(int index, bool refit);

private:
    void buildUi();
    void populateAspectCombo();            // (re)fill presets + persisted custom aspects
    void addAspectItem(const QString &caption, const QString &key, double ratio);
    void addActionItem(const QString &caption, int action);   // "Add..." / "Edit..." tail items
    void promptAddCustomAspect();          // dialog -> append + persist a custom ratio
    void manageCustomAspects();            // dialog -> rename / delete the persisted customs
    /* Both prompt, so both take the widget to parent that prompt to: the manage dialog
       when called from it, so the prompt is not left behind its own modal parent. */
    bool renameCustomAspect(int i, QWidget *parent);
    bool deleteCustomAspect(int i, QWidget *parent);
    /* Point the combo at the entry with this key (falling back to "As shot" when it has
       gone), with no aspectChanged emitted. Needed after every populateAspectCombo, which
       clears the combo and with it the selection. */
    void selectAspectKey(const QString &key);
    void setAspectLocked(bool locked);     // store + persist + announce (checkbox or "A")
    void setAspectFlipped(bool flipped);   // store + persist + announce (radios or "X")
    void updateLockButton();               // reflect aspectLocked in the checkbox (no signal)
    void updateFlipButton();               // reflect aspectFlipped in the radios (no signal)
    void updatePreviewButton();            // set the eye glyph/tooltip from previewShown
    void loadCustomAspects();
    void saveCustomAspects();

    QSettings   *setting = nullptr;

    /* Placeholders for the group dividers above the Angle and Warp rows: they reserve
       the space in the grid and paintEvent draws the G::groupSeparator* line in them,
       so the colour tracks theme changes. */
    QList<QWidget*> rowRules;

    QComboBox    *aspectCombo    = nullptr;
    QCheckBox    *lockCheck      = nullptr;
    QRadioButton *landscapeRadio = nullptr;   // flip: honour the ratio as w/h
    QRadioButton *portraitRadio  = nullptr;   // flip: honour 1/ratio
    QLineEdit   *angleEdit    = nullptr;
    BarBtn      *tipBtn       = nullptr;
    BarBtn      *previewBtn   = nullptr;    // eye: show/ignore the transform (Geometry::show)
    BarBtn      *headerResetBtn = nullptr;  // clear crop/straighten/warp back to identity
    BarBtn      *closeBtn     = nullptr;    // [X] at the extreme right: close the panel

    bool previewShown = true;              // mirror of the current image's Geometry::show
    bool aspectLocked = false;
    bool aspectFlipped = false;            // true = portrait: honour 1/ratio
    int  lastAspectIndex = 0;              // restore selection after a "custom" cancel

    /* Persisted custom aspects: caption ("21 x 9") -> ratio (w/h). */
    QVector<QPair<QString, double>> customAspects;

    /* UserRole payloads on each combo item. */
    enum AspectRole { KeyRole = Qt::UserRole + 1, RatioRole, ActionRole };
    enum AspectAction { NoAction = 0, AddCustomAction = 1, ManageCustomAction = 2 };
};

#endif // TRANSFORMPANEL_H
