#ifndef REPLACEPANEL_H
#define REPLACEPANEL_H

#include <QWidget>

class QLabel;
class QRadioButton;
class QButtonGroup;
class QSettings;
class BarBtn;

/*
    The Develop dock's "Fill Replace" panel: the UI surface for the regenerative
    spot/fill/object replace tool. Sibling of TransformPanel -- a compact strip below the
    scopes and above the property tree (see MW::createDevelopDock), visible only while
    the replace tool is armed (the title-bar spot button or "F" toggles it). It carries
    no pixel state: committed replaces live in EditStack::spots (paramsJson carries the
    mode as "kind", see Develop/fillspot.h) and heal at render in lamafill.cpp.

    Layout:
        |---------------------------------------------------|
        | Fill Replace                                [?][E]|
        |---------------------------------------------------|
        | ( ) Spot     click a blemish (clone heal)         |
        | ( ) Fill     drag a brush over the area to replace|
        | ( ) Object   brush over an object (model fill)    |
        |---------------------------------------------------|

    Each row's shortcut letter (S / F / O) renders bold in G::header2Color. The letters
    select a mode while the panel or the loupe replace tool has focus; [?] shows tips;
    [E] is the preview eye (show / ignore all replaces, non-destructive). The selected
    mode persists to QSettings under "Develop/Replace/".
*/
class ReplacePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ReplacePanel(QWidget *parent = nullptr, QSettings *settings = nullptr);

    /* Mirrors FillSpotGeom::Kind so callers can pass it straight through. */
    enum Mode { SpotMode = 0, FillMode = 1, ObjectMode = 2 };

    int  mode() const { return currentMode; }
    void setMode(int mode);                // programmatic (no modeChanged emitted)
    void setPreviewShown(bool shown);      // sync the eye (no signal emitted)

public slots:
    /* User-driven mode change (emits modeChanged + persists): panel rows, S/F/O on the
       panel, and S/F/O on the loupe (routed through MW) all land here. */
    void selectMode(int mode);

signals:
    void modeChanged(int mode);            // Spot / Fill / Object selected (see Mode)
    void tipsRequested();                  // [?] clicked
    void previewToggled(bool shown);       // eye: show (true) or ignore (false) replaces

protected:
    /* Claim bare S/F/O for the focused panel (same idiom as TransformPanel's C/L/W). */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void updatePreviewButton();

    QSettings    *setting     = nullptr;
    QRadioButton *spotBtn     = nullptr;
    QRadioButton *fillBtn     = nullptr;
    QRadioButton *objectBtn   = nullptr;
    QButtonGroup *modeGroup   = nullptr;
    BarBtn       *tipBtn      = nullptr;
    BarBtn       *previewBtn  = nullptr;

    bool previewShown = true;              // eye state: replaces rendered or bypassed
    int  currentMode  = SpotMode;
};

#endif // REPLACEPANEL_H
