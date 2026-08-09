#ifndef SCOPEHEADERBASE_H
#define SCOPEHEADERBASE_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

/*
    Abstract seam for the Develop dock's scope header. It lets a shipping
    implementation (ScopeHeader) and an experimental one (ScopeHeaderLab) be
    swapped at construction time behind G::useScopeHeaderLab, without either
    concrete type leaking past DevelopProperties::bindScopeHeader().

    DevelopProperties talks to the header through this interface ONLY: it connects
    the signals below and drives the pure-virtual setters. Both concrete classes
    therefore present an identical contract, so the lab can be reshaped freely and,
    once it proves out, its file contents replace scopeheader.* and the flag/base
    can be retired.

    Signals are declared here (not in the concrete classes) so a single
    connect(&ScopeHeaderBase::scopeSelected, ...) binds whichever class was built.
*/
class ScopeHeaderBase : public QWidget
{
    Q_OBJECT
public:
    explicit ScopeHeaderBase(QWidget *parent = nullptr) : QWidget(parent) {}

    /* One scope's display state for the list panel (ScopeHeaderLab). The dropdown header
       ignores enabled/isGlobal and uses only the name (via the default setScopeRows). */
    struct ScopeRowInfo {
        QString name;
        bool    enabled = true;    // EditScope::enabled -> the row's show/hide checkbox
        bool    isGlobal  = false;   // index 0: no checkbox, applies globally
    };

    /* Refill the dropdown and select currentIndex WITHOUT emitting scopeSelected. */
    virtual void setScopes(const QStringList &names, int currentIndex) = 0;

    /* Richer refresh used by the list panel: names + per-scope enabled + which is Global.
       Default forwards to setScopes so the dropdown header needs no change; the list
       panel overrides it to build the per-row checkboxes. DevelopProperties always calls
       THIS (the dropdown just drops the extra state). */
    virtual void setScopeRows(const QVector<ScopeRowInfo> &rows, int active) {
        QStringList names;
        names.reserve(rows.size());
        for (const ScopeRowInfo &r : rows) names << r.name;
        setScopes(names, active);
    }
    virtual void setPreviewShown(bool shown) = 0;           // eye icon (scope preview)
    virtual void setGlobalActive(bool isGlobal) = 0;            // Global: omit per-scope group
    virtual void setMaskOverlayAvailable(bool available) = 0;
    virtual void setMaskOverlayShown(bool shown) = 0;
    virtual void setCollapsed(bool collapsed) = 0;          // programmatic, no signal
    virtual bool isCollapsed() const = 0;
    virtual QString currentScopeName() const = 0;

signals:
    void scopeSelected(const QString &name);    // user picked a different scope
    void renameRequested();                      // menu: rename the selected scope
    void resetScopeRequested();                  // menu: reset the scope to identity
    void removeScopeRequested();                 // menu: remove the selected scope
    void addScopeRequested();                    // menu: add a new scope
    void addMaskRequested();                     // menu: add a mask tool to this scope
    void maskOverlayToggled();                   // menu: show/hide the mask overlay
    void previewToggled(bool shown);             // [E] show/ignore the whole scope
    void collapseToggled(bool collapsed);        // > hide/show the scope's tree items
    /* List panel only: a scope row's show/hide checkbox was toggled (index into the
       scope stack, 0 = Global which has no checkbox). */
    void scopeEnabledToggled(int index, bool on);
};

#endif // SCOPEHEADERBASE_H
