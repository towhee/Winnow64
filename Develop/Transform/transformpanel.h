#ifndef TRANSFORMPANEL_H
#define TRANSFORMPANEL_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QPair>

class QComboBox;
class QToolButton;
class QCheckBox;
class QSettings;
class BarBtn;

/*
    The Develop dock's Transform (crop + perspective) panel: a compact control strip
    placed BELOW the scopes strip and ABOVE the property tree (see MW::createDevelopDock).
    It is the sibling of ScopesView -- a self-contained QWidget that owns only its widgets
    and emits a signal for each user action. It carries NO geometry/pixel state: the actual
    crop rectangle, straighten angle and perspective homography live in the (non-destructive)
    develop edit stack and are applied last in the pipeline (see notes/Documentation.txt
    "Transform"). This class is the UI surface for that model; the image-side overlay and the
    warp engine are wired to these signals in a later increment.

    Controls (top to bottom):
        - Aspect combo (As shot / fixed ratios / Add custom aspect...). userData is the w/h
          ratio as a double; 0.0 means "As shot" (free).
        - Aspect lock toggle (padlock), also driven by the "A" key while the panel has focus
          (see eventFilter).
        - Straighten ("draw a level") one-shot tool.
        - Crop tool toggle. With Alt held during the drag the crop becomes a 4-point polygon;
          Rectify warps the whole image so that quad is axis-aligned, then fits the largest
          inscribed rectangle as the new crop (user-confirmed behaviour).
        - "Fill extra canvas" checkbox (content-aware edge-extend for empty corners left by a
          straighten / rectify).
        - A [?] tip button (top-right) -- "Crop and transform tips".

    Custom aspects, the last-used aspect, the lock state and the fill-canvas state persist to
    QSettings under "Develop/Transform/".
*/
class TransformPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TransformPanel(QWidget *parent = nullptr, QSettings *settings = nullptr);

    bool    isAspectLocked() const { return aspectLocked; }
    bool    isFillCanvas() const;
    QString aspectKey() const;      // the selected entry's key, e.g. "asShot", "3:2", "21:9"
    double  aspectRatio() const;    // w/h of the selected entry; 0.0 for "As shot" (free)

signals:
    void aspectChanged(const QString &key, double ratio);  // ratio = w/h, 0.0 = As shot / free
    void aspectLockToggled(bool locked);
    void straightenToolRequested();        // "draw a level" clicked: enter the straighten tool
    void cropToolToggled(bool on);         // crop overlay activated / deactivated
    void fillCanvasToggled(bool on);       // content-aware edge-extend on / off
    void rectifyRequested();               // apply perspective rectify to the drawn quad
    void tipsRequested();                  // [?] clicked

public slots:
    void toggleAspectLock();               // lock button click, or the "A" key (see eventFilter)

protected:
    /* "A" toggles the aspect lock when the panel (or one of its non-text controls) has focus.
       A panel-scoped QAction would be an ambiguous overload with the window-level "A" (Run
       Droplet), so instead we claim the key by accepting its QEvent::ShortcutOverride and act on
       the following KeyPress. The aspect combo is deliberately not filtered so its type-ahead
       still works. */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onAspectActivated(int index);     // combo choice (intercepts "Add custom aspect...")

private:
    void buildUi();
    void populateAspectCombo();            // (re)fill presets + persisted custom aspects
    void addAspectItem(const QString &caption, const QString &key, double ratio);
    void promptAddCustomAspect();          // dialog -> append + persist a custom ratio
    void updateLockButton();               // swap the padlock glyph + tooltip for aspectLocked
    void loadCustomAspects();
    void saveCustomAspects();

    QSettings   *setting = nullptr;

    QComboBox   *aspectCombo   = nullptr;
    QToolButton *lockBtn       = nullptr;
    QToolButton *straightenBtn = nullptr;
    QToolButton *cropBtn       = nullptr;
    QToolButton *rectifyBtn    = nullptr;
    QCheckBox   *fillCanvasChk = nullptr;
    BarBtn      *tipBtn        = nullptr;

    bool aspectLocked = false;
    int  lastAspectIndex = 0;              // restore selection after a "custom" cancel

    /* Persisted custom aspects: caption ("21 x 9") -> ratio (w/h). */
    QVector<QPair<QString, double>> customAspects;

    /* UserRole payloads on each combo item. */
    enum AspectRole { KeyRole = Qt::UserRole + 1, RatioRole, ActionRole };
    enum AspectAction { NoAction = 0, AddCustomAction = 1 };
};

#endif // TRANSFORMPANEL_H
