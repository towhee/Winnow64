#ifndef SAVEDEVELOPPRESETDLG_H
#define SAVEDEVELOPPRESETDLG_H

#include <QtWidgets>

/*
    Save Develop Preset dialog (Lightroom-style "New Develop Preset"). Presents a
    collapsible, scrollable checklist of every settable Develop item, grouped exactly
    like the Develop panel's Scope tree -- Global settings, then Basic / Color /
    Color Grade / Effects -- plus a Scope combo choosing WHICH scope's values are
    captured. Items that differ from their default in the selected scope are pre-checked
    (and their group pre-expanded), so the user can Save immediately or fine-tune.

    TWO MODES, one dialog. "Pick a set of develop settings, from a scope" is the whole
    job, and Copy Develop Settings (Lightroom's Copy Settings, Cmd+Opt+C) asks exactly
    that question -- it just has nowhere to put a name, since the copy buffer is a single
    unnamed slot. Mode::CopySettings therefore drops the name field (and its overwrite
    check) and relabels the dialog; everything else -- the groups, the Scope combo, the
    pre-checked "what differs from default", the All / None buttons -- is identical.

    Each adjustment group carries its own [All] / [None] buttons, and the button row
    carries Select all / Select none for the whole checklist.

    The dialog is UI-only: it renders a model handed to it and reports the preset name,
    the chosen scope and the checked leaf keys per group back to the caller
    (DevelopProperties), which owns the QSettings write. This keeps the checklist
    reusable and free of any knowledge of the develop data model or settings layout.

    Because the adjustment leaves are the same keys in every scope, switching the Scope
    combo does not rebuild the tree -- it just re-applies that scope's "changed" set
    (changedPerScope) to the check states. The Global settings group is PER IMAGE and is
    not affected by the combo.
*/

struct PresetLeaf {
    QString key;                // stable id; also the caller's QSettings write key
    QString label;              // UI text
    bool    changed = false;    // pre-check: only used by the non-scope groups
};

struct PresetGroup {
    QString title;              // "Global settings" / "Basic" / "Color" / ...
    bool    checkable = true;   // false => a non-checkable container header (Global)
    bool    showAllNone = true; // an [All] / [None] pair on the group row
    QVector<PresetLeaf> leaves;
};

class SaveDevelopPresetDlg : public QDialog
{
    Q_OBJECT
public:
    /* SavePreset: name the selection and store it. CopySettings: no name -- the selection
       goes to the single-slot develop clipboard. */
    enum class Mode { SavePreset, CopySettings };

    /* changedPerScope[i] = the leaf keys that differ from default in scope i; it drives
       the pre-checked state of every checkable group as the Scope combo changes.
       existingNames is the overwrite check, and is unused (pass {}) in CopySettings. */
    SaveDevelopPresetDlg(const QVector<PresetGroup> &groups,
                         const QStringList &scopeNames,
                         const QVector<QSet<QString>> &changedPerScope,
                         int activeScope,
                         const QStringList &existingNames,
                         Mode mode = Mode::SavePreset,
                         QWidget *parent = nullptr);

    QString presetName() const;     // always empty in CopySettings (there is no name)
    int     scopeIndex() const;     // index into the scopeNames handed in
    /* Checked leaf keys per group title (only the leaves the user left checked). */
    QHash<QString, QSet<QString>> selected() const;

private slots:
    void onAccept();            // validate name (+ overwrite confirm) then accept

private:
    void setAllChecked(bool on);
    void setGroupChecked(QTreeWidgetItem *group, bool on);
    void applyScopeChanged(int scope);   // re-check the scope groups for that scope
    void updateSaveEnabled();

    QLineEdit   *nameEdit  = nullptr;   // null in CopySettings
    QComboBox   *scopeCombo = nullptr;
    QTreeWidget *tree      = nullptr;
    QPushButton *saveBtn   = nullptr;   // "Save" / "Copy"
    QStringList  existingNames;
    QVector<QSet<QString>> changedPerScope;

    static constexpr int KeyRole   = Qt::UserRole + 1;   // leaf: its key
    static constexpr int GroupRole = Qt::UserRole + 2;   // leaf: its group title
    static constexpr int ScopeRole = Qt::UserRole + 3;   // group: follows the combo
};

#endif // SAVEDEVELOPPRESETDLG_H
