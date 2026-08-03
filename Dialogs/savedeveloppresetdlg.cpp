#include "savedeveloppresetdlg.h"

/*
    See savedeveloppresetdlg.h. Builds a QTreeWidget from the supplied groups: each group
    is a top-level row; adjustment groups carry an auto-tristate parent checkbox (so
    toggling the group toggles all its leaves and the parent reflects the children) plus
    an [All] / [None] pair in column 1, while the Global settings group is a plain
    container header whose leaves are individually checkable.

    Group rows tagged ScopeRole follow the Scope combo: changing it re-checks their
    leaves from changedPerScope. Global settings is per image and does not.
*/

SaveDevelopPresetDlg::SaveDevelopPresetDlg(const QVector<PresetGroup> &groups,
                                           const QStringList &scopeNames,
                                           const QVector<QSet<QString>> &changedPerScope,
                                           int activeScope,
                                           const QStringList &existingNames,
                                           QWidget *parent)
    : QDialog(parent), existingNames(existingNames), changedPerScope(changedPerScope)
{
    setWindowTitle(tr("Create Develop Preset"));
    setModal(true);

    QVBoxLayout *lay = new QVBoxLayout(this);

    /* Preset name + the scope the values are captured from. */
    QFormLayout *form = new QFormLayout;
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("Untitled preset"));
    form->addRow(tr("Preset name:"), nameEdit);
    scopeCombo = new QComboBox(this);
    scopeCombo->addItems(scopeNames);
    if (activeScope >= 0 && activeScope < scopeNames.size())
        scopeCombo->setCurrentIndex(activeScope);
    scopeCombo->setEnabled(scopeNames.size() > 1);
    scopeCombo->setToolTip(tr("Which scope's adjustments to capture. Applying the preset "
                              "writes them to whatever scope is active then."));
    form->addRow(tr("Scope:"), scopeCombo);
    lay->addLayout(form);

    /* Checklist. Column 1 holds each group's [All] / [None] buttons. */
    tree = new QTreeWidget(this);
    tree->setHeaderHidden(true);
    tree->setColumnCount(2);
    tree->setUniformRowHeights(true);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    lay->addWidget(tree, 1);

    /* Right-click the checklist: Expand all / Collapse all the groups. */
    connect(tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        QAction *expandAll   = menu.addAction(tr("Expand all"));
        QAction *collapseAll = menu.addAction(tr("Collapse all"));
        QAction *chosen = menu.exec(tree->mapToGlobal(pos));
        if      (chosen == expandAll)   tree->expandAll();
        else if (chosen == collapseAll) tree->collapseAll();
    });

    for (const PresetGroup &g : groups) {
        QTreeWidgetItem *gi = new QTreeWidgetItem(tree);
        gi->setText(0, g.title);
        gi->setData(0, ScopeRole, g.checkable);   // scope groups follow the combo
        bool anyChanged = false;
        if (g.checkable) {
            gi->setFlags(gi->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
            gi->setCheckState(0, Qt::Unchecked);  // required for auto-tristate to engage
        }
        else {
            gi->setFlags(gi->flags() & ~Qt::ItemIsUserCheckable);
        }

        for (const PresetLeaf &leaf : g.leaves) {
            QTreeWidgetItem *li = new QTreeWidgetItem(gi);
            li->setText(0, leaf.label);
            li->setFlags((li->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsAutoTristate);
            li->setCheckState(0, leaf.changed ? Qt::Checked : Qt::Unchecked);
            li->setData(0, KeyRole, leaf.key);
            li->setData(0, GroupRole, g.title);
            anyChanged = anyChanged || leaf.changed;
        }
        /* Expand Global always (a container) and any group with a changed leaf. */
        gi->setExpanded(!g.checkable || anyChanged);

        if (!g.showAllNone) continue;
        /* Per-group All / None. The global stylesheet sets QPushButton min-width to
           100px, which would force the dialog absurdly wide, so clear it here (the same
           override MaskPanel needs for its tool buttons). */
        QWidget *w = new QWidget(tree);
        w->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(w);
        hb->setContentsMargins(0, 0, 4, 0);
        hb->setSpacing(4);
        QPushButton *all  = new QPushButton(tr("All"), w);
        QPushButton *none = new QPushButton(tr("None"), w);
        for (QPushButton *b : {all, none}) {
            b->setStyleSheet("QPushButton { min-width: 0; padding: 0px 6px; }");
            b->setFocusPolicy(Qt::NoFocus);
            hb->addWidget(b);
        }
        all->setToolTip(tr("Check every %1 setting").arg(g.title));
        none->setToolTip(tr("Uncheck every %1 setting").arg(g.title));
        connect(all,  &QPushButton::clicked, this,
                [this, gi]{ setGroupChecked(gi, true); });
        connect(none, &QPushButton::clicked, this,
                [this, gi]{ setGroupChecked(gi, false); });
        tree->setItemWidget(gi, 1, w);
    }

    /* Button row: Select all / Select none on the left, Cancel / Save on the right. */
    QHBoxLayout *btnLay = new QHBoxLayout;
    QPushButton *selAll  = new QPushButton(tr("Select all"), this);
    QPushButton *selNone = new QPushButton(tr("Select none"), this);
    QPushButton *cancel  = new QPushButton(tr("Cancel"), this);
    saveBtn = new QPushButton(tr("Save"), this);
    saveBtn->setDefault(true);
    btnLay->addWidget(selAll);
    btnLay->addWidget(selNone);
    btnLay->addStretch(1);
    btnLay->addWidget(cancel);
    btnLay->addWidget(saveBtn);
    lay->addLayout(btnLay);

    connect(selAll,  &QPushButton::clicked, this, [this]{ setAllChecked(true); });
    connect(selNone, &QPushButton::clicked, this, [this]{ setAllChecked(false); });
    connect(cancel,  &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &SaveDevelopPresetDlg::onAccept);
    connect(nameEdit, &QLineEdit::textChanged, this,
            &SaveDevelopPresetDlg::updateSaveEnabled);
    connect(scopeCombo, &QComboBox::currentIndexChanged, this,
            &SaveDevelopPresetDlg::applyScopeChanged);

    applyScopeChanged(scopeCombo->currentIndex());
    updateSaveEnabled();
    resize(460, 580);
}

QString SaveDevelopPresetDlg::presetName() const
{
    return nameEdit->text().trimmed();
}

int SaveDevelopPresetDlg::scopeIndex() const
{
    return scopeCombo->currentIndex();
}

QHash<QString, QSet<QString>> SaveDevelopPresetDlg::selected() const
{
    QHash<QString, QSet<QString>> out;
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *gi = tree->topLevelItem(g);
        for (int i = 0; i < gi->childCount(); ++i) {
            QTreeWidgetItem *li = gi->child(i);
            if (li->checkState(0) == Qt::Checked)
                out[li->data(0, GroupRole).toString()].insert(li->data(0, KeyRole).toString());
        }
    }
    return out;
}

void SaveDevelopPresetDlg::setGroupChecked(QTreeWidgetItem *group, bool on)
{
    if (!group) return;
    const Qt::CheckState st = on ? Qt::Checked : Qt::Unchecked;
    for (int i = 0; i < group->childCount(); ++i)
        group->child(i)->setCheckState(0, st);
    if (on) group->setExpanded(true);
}

void SaveDevelopPresetDlg::setAllChecked(bool on)
{
    for (int g = 0; g < tree->topLevelItemCount(); ++g)
        setGroupChecked(tree->topLevelItem(g), on);
}

void SaveDevelopPresetDlg::applyScopeChanged(int scope)
{
/*
    The Scope combo moved: re-check every scope group from that scope's changed set and
    re-expand the groups that have something in them. The leaf keys are identical in
    every scope, so the tree itself never has to be rebuilt. Global settings is per image
    and keeps whatever the user has done to it.
*/
    if (scope < 0 || scope >= changedPerScope.size()) return;
    const QSet<QString> &changed = changedPerScope.at(scope);
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *gi = tree->topLevelItem(g);
        if (!gi->data(0, ScopeRole).toBool()) continue;
        bool anyChanged = false;
        for (int i = 0; i < gi->childCount(); ++i) {
            QTreeWidgetItem *li = gi->child(i);
            const bool on = changed.contains(li->data(0, KeyRole).toString());
            li->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
            anyChanged = anyChanged || on;
        }
        gi->setExpanded(anyChanged);
    }
}

void SaveDevelopPresetDlg::updateSaveEnabled()
{
    saveBtn->setEnabled(!presetName().isEmpty());
}

void SaveDevelopPresetDlg::onAccept()
{
    const QString name = presetName();
    if (name.isEmpty()) return;
    if (existingNames.contains(name, Qt::CaseInsensitive)) {
        const auto ret = QMessageBox::question(this, tr("Overwrite preset"),
            tr("A preset named \"%1\" already exists. Overwrite it?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }
    accept();
}
