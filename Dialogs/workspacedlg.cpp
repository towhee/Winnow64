#include "workspacedlg.h"
#include "Main/global.h"
#include "Utilities/htmlwindow.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

/*
    MANAGE CUSTOM WORKSPACES.  A table, one row per custom workspace:

        Rename custom workspace   the name, edited in place (one click) to rename
        Window *            restore the window position and size (WorkspaceData::
                            isGeometryIncluded)
        Delete              ticked rows are deleted

    Nothing is applied until Update, which checks the names, applies everything and
    closes.  Closing any other way (Esc, the title bar) discards the edits.  MW reads the
    result with rows() -- see MW::manageWorkspaces.
*/

static QWidget *centredCheckBox(bool isChecked, const QString &tip)
{
    /* A checkbox centred in its cell: a checkable QTableWidgetItem sits at the left. */
    QWidget *box = new QWidget;
    QHBoxLayout *lay = new QHBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setAlignment(Qt::AlignCenter);
    QCheckBox *cb = new QCheckBox;
    cb->setChecked(isChecked);
    cb->setToolTip(tip);
    lay->addWidget(cb);
    return box;
}

static bool cellChecked(QTableWidget *table, int row, int col)
{
    QWidget *box = table->cellWidget(row, col);
    QCheckBox *cb = box ? box->findChild<QCheckBox *>() : nullptr;
    return cb && cb->isChecked();
}

WorkspaceDlg::WorkspaceDlg(const QList<Row> &rows, const QStringList &otherNames,
                           const QString &title, QWidget *parent) :
    QDialog(parent),
    otherNames(otherNames)
{
    setWindowTitle(title);
    resize(480, height());

    table = new QTableWidget(rows.count(), ColCount);
    table->setHorizontalHeaderLabels(
        QStringList() << tr("Rename custom workspace") << tr("Window *") << tr("Delete"));
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::EditKeyPressed
                           | QAbstractItemView::AnyKeyPressed);
    table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(ColWindow,
                                                    QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(ColDelete,
                                                    QHeaderView::ResizeToContents);
    for (int r = 0; r < rows.count(); ++r) {
        QTableWidgetItem *name = new QTableWidgetItem(rows.at(r).name);
        name->setToolTip(tr("Click to rename"));
        table->setItem(r, ColName, name);
        table->setCellWidget(r, ColWindow, centredCheckBox(rows.at(r).isGeometryIncluded,
            tr("Also restore the window position and size")));
        table->setCellWidget(r, ColDelete, centredCheckBox(false,
            tr("Delete this workspace when Update is pressed")));
    }
    /* One click edits a name: NoSelection leaves nothing for a first click to select. */
    connect(table, &QTableWidget::itemClicked, table, [this](QTableWidgetItem *item) {
        if (item->column() == ColName) table->editItem(item);
    });
    /* As tall as its rows (up to the ten allowed), so there is no empty band. */
    const int rowsShown = qBound(1, int(rows.count()), 10);
    table->setFixedHeight(table->horizontalHeader()->sizeHint().height()
                          + rowsShown * table->verticalHeader()->defaultSectionSize()
                          + 2 * table->frameWidth());

    QLabel *footnote = new QLabel(tr("* Restore window position and size"));
    footnote->setStyleSheet(G::labelCss(G::secondaryTextColor));     // a note

    status = new QLabel;
    status->setWordWrap(true);

    helpBtn = new QPushButton("?");
    helpBtn->setToolTip(tr("How custom workspaces work"));
    helpBtn->setStyleSheet("background-color: " + G::helpColor.name() + ";");
    helpBtn->setAutoDefault(false);
    helpBtn->setFixedWidth(helpBtn->sizeHint().height() * 2);
    updateBtn = new QPushButton(tr("Update"));
    updateBtn->setDefault(true);

    if (rows.isEmpty()) {
        updateBtn->setEnabled(false);
        updateBtn->setToolTip(tr("There are no custom workspaces to update"));
        status->setText(tr("There are no custom workspaces yet.  Window > Workspace > "
                           "New Custom Workspace saves the current layout."));
    }
    else {
        updateBtn->setToolTip(tr("Apply the new names, Window * choices and deletions, "
                                 "and close.  Closing without Update discards them."));
    }

    QHBoxLayout *btns = new QHBoxLayout;
    btns->setContentsMargins(0, 0, 0, 0);
    btns->addWidget(helpBtn);
    btns->addStretch(1);
    btns->addWidget(updateBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(table);
    layout->addWidget(footnote);
    layout->addWidget(status);
    layout->addSpacing(8);
    layout->addLayout(btns);

    connect(helpBtn, &QPushButton::clicked, this, &WorkspaceDlg::showHelp);
    connect(updateBtn, &QPushButton::clicked, this, &WorkspaceDlg::applyUpdate);
    /* A fixed name problem clears its message as soon as it is fixed. */
    connect(table, &QTableWidget::itemChanged, this, [this] {
        if (!status->text().isEmpty() && updateBtn->isEnabled())
            status->setText(validate());
    });

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

QList<WorkspaceDlg::Row> WorkspaceDlg::rows() const
{
    QList<Row> out;
    for (int r = 0; r < table->rowCount(); ++r) {
        Row row;
        row.name = table->item(r, ColName)->text().trimmed();
        row.isGeometryIncluded = cellChecked(table, r, ColWindow);
        row.isDelete = cellChecked(table, r, ColDelete);
        out.append(row);
    }
    return out;
}

QString WorkspaceDlg::validate() const
{
/*
    The reason Update cannot apply, or empty.  Names index the workspaces (and the
    front-tab memory), so the kept ones must be unique and not blank.  A row being
    deleted does not count.
*/
    const QSet<QString> others(otherNames.cbegin(), otherNames.cend());
    QSet<QString> seen;
    for (const Row &row : rows()) {
        if (row.isDelete) continue;
        if (row.name.isEmpty()) return tr("A workspace needs a name.");
        if (seen.contains(row.name))
            return tr("Two workspaces are named \"%1\".  Rename one.").arg(row.name);
        if (others.contains(row.name))
            return tr("Another workflow already has a workspace named \"%1\".")
                .arg(row.name);
        seen.insert(row.name);
    }
    return QString();
}

void WorkspaceDlg::applyUpdate()
{
    if (G::isLogger) G::log("WorkspaceDlg::applyUpdate");
    const QString why = validate();
    if (!why.isEmpty()) {
        status->setText(why);
        return;
    }
    accept();
}

void WorkspaceDlg::showHelp()
{
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Custom Workspaces", ":/Docs/customworkspaceshelp.html",
                   QSize(640, 600), r, this);
}
