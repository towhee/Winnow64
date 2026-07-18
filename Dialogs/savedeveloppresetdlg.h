#ifndef SAVEDEVELOPPRESETDLG_H
#define SAVEDEVELOPPRESETDLG_H

#include <QtWidgets>

/*
    Save Develop Preset dialog (Lightroom-style "New Develop Preset"). Presents a
    collapsible, scrollable checklist of every settable Develop item, grouped as
    Global settings / Base layer / Layer N. Items that differ from their default on
    the current image are pre-checked (and their group pre-expanded).

    The dialog is UI-only: it renders a model handed to it and reports the preset
    name plus the checked leaf keys per group back to the caller (DevelopProperties),
    which owns the QSettings write. This keeps the checklist reusable and free of any
    knowledge of the develop data model or settings layout.
*/

struct PresetLeaf {
    QString key;                // stable id; also the caller's QSettings write key
    QString label;              // UI text
    bool    changed = false;    // pre-check: differs from default on the current image
};

struct PresetGroup {
    QString title;              // "Global settings" / "Base layer" / "Layer 1"
    bool    checkable = true;   // false => a non-checkable container header (Global)
    QVector<PresetLeaf> leaves;
};

class SaveDevelopPresetDlg : public QDialog
{
    Q_OBJECT
public:
    SaveDevelopPresetDlg(const QVector<PresetGroup> &groups,
                         const QStringList &existingNames,
                         QWidget *parent = nullptr);

    QString presetName() const;
    /* Checked leaf keys per group title (only the leaves the user left checked). */
    QHash<QString, QSet<QString>> selected() const;

private slots:
    void onAccept();            // validate name (+ overwrite confirm) then accept

private:
    void setAllChecked(bool on);
    void updateSaveEnabled();

    QLineEdit   *nameEdit = nullptr;
    QTreeWidget *tree     = nullptr;
    QPushButton *saveBtn  = nullptr;
    QStringList  existingNames;

    static constexpr int KeyRole   = Qt::UserRole + 1;   // leaf: its key
    static constexpr int GroupRole = Qt::UserRole + 2;   // leaf: its group title
};

#endif // SAVEDEVELOPPRESETDLG_H
