#ifndef SCOPEHEADER_H
#define SCOPEHEADER_H

#include "Develop/Properties/scopeheaderbase.h"

#include <QIcon>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class BarBtn;

/*
    The Develop dock's scope header: a compact gradient band (styled like TransformPanel's header)
    that sits ABOVE the DevelopProperties tree and replaces the old in-tree "Scopes" header. It owns
    the scope dropdown and the whole-mask preview eye; it carries no model state and simply emits a
    signal per user action (DevelopProperties does the work and drives this widget's display back).

    Layout:
        > Scope [ Scope dropdown                     v ]              [E]
        |       |                                                      |
        |       selected scope's items shown in the tree below        whole-mask preview eye
        collapse arrow (hide/show the tree = the scope's items)

    The dropdown lists ONLY the image's scopes (the active one carries a checkmark):

        Global
        Scope 1
      v Scope 2                (active scope)
        More Scopes

    Picking a scope emits scopeSelected. The scope actions live on a separate menu button
    (scopeMenuBtn, an ellipsis) that pops up a QMenu (like EmbelProperties::effectContextMenu):

        Add new scope
        Reset all edits        (wipes the image's whole recipe, history and sidecar)
        ---                    (per-scope group, hidden when Global is active)
        Add mask to Scope 2
        Show mask overlay     (checkable; only while a mask tool is being edited)
        Reset Scope 2
        Remove Scope 2
        Rename Scope 2

    Each menu row emits its signal. The per-scope group (Add mask / Show mask overlay / Reset /
    Remove / Rename) is omitted for the Global scope (index 0), which applies globally.
*/
class ScopeHeader : public ScopeHeaderBase
{
    Q_OBJECT
public:
    explicit ScopeHeader(QWidget *parent = nullptr);

    /* Refill the dropdown and select currentIndex WITHOUT emitting scopeSelected. */
    void setScopes(const QStringList &names, int currentIndex) override;
    void setPreviewShown(bool shown) override;           // eye icon (whole-mask preview)
    void setGlobalActive(bool isGlobal) override;            // Global: omit per-scope group
    /* Mask overlay tint state, pushed by DevelopProperties so the menu row shows the
       right check state and only appears while a mask tool is being edited. */
    void setMaskOverlayAvailable(bool available) override;
    void setMaskOverlayShown(bool shown) override;
    bool isCollapsed() const override { return collapsed; }
    /* Programmatic collapse (Expand all / Collapse all / Solo) -- updates the arrow
       WITHOUT emitting collapseToggled; the caller drives the tree itself. */
    void setCollapsed(bool collapsed) override;
    QString currentScopeName() const override;

protected:
    void paintEvent(QPaintEvent *) override;         // the property-header gradient band
    /* Clicking the "Scope" label toggles collapse just like the arrow (filtered here
       because QLabel has no clicked signal). */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updatePreviewIcon();
    void updateCollapseIcon();
    void toggleCollapsed();             // arrow click or "Scope" label click
    void showScopeMenu();               // scopeMenuBtn: pop up the scope-actions menu

    BarBtn    *collapseBtn  = nullptr;
    QLabel    *scopeLabel   = nullptr;
    QComboBox *combo        = nullptr;
    BarBtn    *scopeMenuBtn = nullptr;
    BarBtn    *previewBtn   = nullptr;

    QIcon checkIcon;                    // marks the active scope in the dropdown
    QIcon blankIcon;                    // same-size spacer so other scopes stay aligned

    int  scopeCount   = 0;              // number of scope rows at the top of the combo
    int  activeIndex  = 0;              // the selected scope's row (the box reverts here)
    bool previewShown = true;
    bool collapsed    = false;
    bool globalActive   = true;           // the selected scope is Global (index 0)
    bool maskOverlayAvailable = false;  // a mask tool is being edited -> menu row applies
    bool maskOverlayShown     = true;   // the red coverage tint is currently visible
};

#endif // SCOPEHEADER_H
