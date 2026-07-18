#include "savedeveloppresetdlg.h"

/*
    See savedeveloppresetdlg.h. Builds a QTreeWidget from the supplied groups: each
    group is a top-level row; layer groups carry an auto-tristate parent checkbox (so
    toggling the layer toggles all its leaves and the parent reflects the children),
    while the Global group is a plain container header whose leaves are individually
    checkable. Leaves start checked when their `changed` flag is set.
*/

SaveDevelopPresetDlg::SaveDevelopPresetDlg(const QVector<PresetGroup> &groups,
                                           const QStringList &existingNames,
                                           QWidget *parent)
    : QDialog(parent), existingNames(existingNames)
{
    setWindowTitle(tr("Create Develop Preset"));
    setModal(true);

    QVBoxLayout *lay = new QVBoxLayout(this);

    /* Preset name. */
    QHBoxLayout *nameLay = new QHBoxLayout;
    nameLay->addWidget(new QLabel(tr("Preset name:"), this));
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("Untitled preset"));
    nameLay->addWidget(nameEdit);
    lay->addLayout(nameLay);

    /* Checklist. */
    tree = new QTreeWidget(this);
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setUniformRowHeights(true);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
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
        bool anyChanged = false;
        if (g.checkable)
            gi->setFlags(gi->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
        else
            gi->setFlags(gi->flags() & ~Qt::ItemIsUserCheckable);

        for (const PresetLeaf &leaf : g.leaves) {
            QTreeWidgetItem *li = new QTreeWidgetItem(gi);
            li->setText(0, leaf.label);
            li->setFlags((li->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsAutoTristate);
            li->setCheckState(0, leaf.changed ? Qt::Checked : Qt::Unchecked);
            li->setData(0, KeyRole, leaf.key);
            li->setData(0, GroupRole, g.title);
            anyChanged = anyChanged || leaf.changed;
        }
        /* Expand Global always (a container) and any layer with a changed leaf. */
        gi->setExpanded(!g.checkable || anyChanged);
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

    updateSaveEnabled();
    resize(420, 560);
}

QString SaveDevelopPresetDlg::presetName() const
{
    return nameEdit->text().trimmed();
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

void SaveDevelopPresetDlg::setAllChecked(bool on)
{
    const Qt::CheckState st = on ? Qt::Checked : Qt::Unchecked;
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *gi = tree->topLevelItem(g);
        for (int i = 0; i < gi->childCount(); ++i)
            gi->child(i)->setCheckState(0, st);
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
