#include "workspacedlg.h"
#include "ui_workspacedlg.h"
#include "Main/global.h"
#include <QDebug>
#include <QTimer>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QPainter>
#include <QStyledItemDelegate>

/*
    QComboBox::insertSeparator marks the item with Qt::AccessibleDescriptionRole ==
    "separator", which the native style paints as a line.  Winnow styles the popup
    (WidgetCSS::comboBox "QComboBox QAbstractItemView::item"), so the popup is drawn by
    QStyleSheetStyle, which paints the separator as an ordinary (blank) item row and the
    line disappears.  This delegate draws the line itself.
*/
class ComboSeparatorDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static bool isSeparator(const QModelIndex &idx)
    {
        return idx.data(Qt::AccessibleDescriptionRole).toString() == "separator";
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (!isSeparator(index)) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        painter->save();
        QColor lineColor = option.palette.color(QPalette::Disabled, QPalette::Text);
        painter->setPen(lineColor);
        int y = option.rect.center().y();
        painter->drawLine(option.rect.left() + 4, y, option.rect.right() - 4, y);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (isSeparator(index)) return QSize(option.rect.width(), 9);
        return QStyledItemDelegate::sizeHint(option, index);
    }
};

WorkspaceDlg::WorkspaceDlg(QList<QString> *wsList,
                           const QList<bool> &isGeometryIncluded,
                           QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Workspacedlg)
{
    ui->setupUi(this);
    this->mainWindow = parent;
    isGeometry = isGeometryIncluded;

    // get height for dropdown items from combobox height
    int h = ui->workspaceCB->height() - 8;

    /* The dropdown lists the user's saved workspaces only.  The per-workflow default and
       override layouts (Library, Develop, Keywords, Embellish, Focus Stack) are managed
       from Window > Workspace, not here. */
    ui->workspaceCB->view()->setItemDelegate(new ComboSeparatorDelegate(ui->workspaceCB));

    // populate the dropdown list
    for (int i=0; i < wsList->count(); i++) {
        ui->workspaceCB->addItem(wsList->at(i));
    }
    /* Set height for each row (setStylesheet approach did not work).  The separator sizes
       itself (see ComboSeparatorDelegate), so it is skipped. */
    for (int i=0; i < ui->workspaceCB->count(); i++) {
        QModelIndex idx = ui->workspaceCB->model()->index(i, 0);
        if (ComboSeparatorDelegate::isSeparator(idx)) continue;
        ui->workspaceCB->model()->setData(idx, QSize(h, h), Qt::SizeHintRole);
    }
    ui->workspaceCB->setCurrentIndex(0);
    editMode = true;
    updateForSelection();
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

WorkspaceDlg::~WorkspaceDlg()
{
    delete ui;
}

int WorkspaceDlg::workspaceIndex() const
{
/*
    The index into MW::workspaces for the selected item.  The dropdown holds nothing but
    the saved workspaces, so the combo index IS the workspace index.
*/
    return ui->workspaceCB->currentIndex();
}

void WorkspaceDlg::updateForSelection()
{
/*
    The window position/size checkbox shows the selected workspace's choice.  With no
    workspaces saved there is nothing to show, and the reason goes in the status line.
*/
    int n = workspaceIndex();
    bool isChecked = false;
    bool isEnabled = true;
    QString reason;
    const bool isWorkspace = n >= 0 && n < isGeometry.count();
    ui->deleteBtn->setEnabled(isWorkspace);
    ui->reassignBtn->setEnabled(isWorkspace);
    if (isWorkspace) {
        isChecked = isGeometry.at(n);
    }
    else {
        isEnabled = false;
        reason = "There are no saved workspaces yet.  Window > Workspace > New "
                 "Workspace saves the current layout.";
    }
    /* setChecked would emit toggled and write the choice back to the workspace the user
       has just moved away from. */
    ui->geometryCB->blockSignals(true);
    ui->geometryCB->setChecked(isChecked);
    ui->geometryCB->blockSignals(false);
    ui->geometryCB->setEnabled(isEnabled);
    ui->status->setText(reason);
}

void WorkspaceDlg::on_geometryCB_toggled(bool isChecked)
{
    if (G::isLogger) G::log("WorkspaceDlg::on_geometryCB_toggled");
    int n = workspaceIndex();
    if (n < 0 || n >= isGeometry.count()) return;
    isGeometry[n] = isChecked;
    emit setWorkspaceGeometryIncluded(n, isChecked);
}

void WorkspaceDlg::on_deleteBtn_clicked()
{
    if (G::isLogger) G::log("WorkspaceDlg::on_deleteBtn_clicked");
    int n = workspaceIndex();
    if (n < 0) return;
    emit deleteWorkspace(n);
    if (n < isGeometry.count()) isGeometry.removeAt(n);
    ui->workspaceCB->removeItem(ui->workspaceCB->currentIndex());
    ui->workspaceCB->setCurrentIndex(0);
    updateForSelection();
}

void WorkspaceDlg::on_reassignBtn_clicked()
{
    if (G::isLogger) G::log("WorkspaceDlg::on_reassignBtn_clicked");
    int n = workspaceIndex();
    if (n < 0) return;
    emit reassignWorkspace(n);
    ui->status->setText(ui->workspaceCB->currentText() + " has been reassigned");
    QTimer::singleShot(2000, this, SLOT(clearStatus()));
}

void WorkspaceDlg::clearStatus()
{
    /* updateForSelection, not setText(""), so a reason for a disabled control is put
       back rather than wiped by a transient message timing out. */
    updateForSelection();
}

void WorkspaceDlg::on_workspaceCB_editTextChanged(const QString &name)
{
    if (G::isLogger) G::log("WorkspaceDlg::on_workspaceCB_editTextChanged");
    if (editMode) {
        bool isOkay = true;
        int n = workspaceIndex();
        if (n < 0) {
            editMode = true;
            return;
        }
        ui->workspaceCB->setStyleSheet("");
        // make sure another workspace name is not being duplicated
        for (int i=0; i<ui->workspaceCB->count(); i++) {
            // ignore current item
            if (i == ui->workspaceCB->currentIndex()) continue;
            // different, try next item
            if (name != ui->workspaceCB->itemText(i)) continue;
            // oh, oh.  Duplicate of another workspace name
            isOkay = false;
            ui->workspaceCB->setStyleSheet(G::cssError);
        }
        if (isOkay) emit renameWorkspace(n, name);
        ui->workspaceCB->setItemText(ui->workspaceCB->currentIndex(), name);
    }
    editMode = true;
}

void WorkspaceDlg::on_workspaceCB_highlighted(int /*index*/)
{
    if (G::isLogger) G::log("WorkspaceDlg::on_workspaceCB_highlighted");
    editMode = false;
}

void WorkspaceDlg::report(QString signalName)
{
    int n = ui->workspaceCB->currentIndex();
    QString text = ui->workspaceCB->currentText();
    qDebug() << G::t.restart() << "\t" << signalName
             << "currentIndex =" << n
             << "editMode =" << editMode
             << "text =" << text;
}

void WorkspaceDlg::on_reportLinkButton_clicked()
{
//    int n = ui->workspaceCB->currentIndex();
//    emit reportWorkspaceNum(n);
}

void WorkspaceDlg::on_workspaceCB_currentIndexChanged(int /*index*/)
{
    if (G::isLogger) G::log("WorkspaceDlg::on_workspaceCB_currentIndexChanged");
    updateForSelection();
}
