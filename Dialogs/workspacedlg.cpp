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

WorkspaceDlg::WorkspaceDlg(QList<QString> *wsList, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Workspacedlg)
{
    ui->setupUi(this);
    this->mainWindow = parent;

    // get height for dropdown items from combobox height
    int h = ui->workspaceCB->height() - 8;

    /* The Winnow default workspace (Ctrl+Shift+W) heads the list, followed by a
       separator and then the saved workspaces.  Rory only: without G::isRory the
       dropdown holds just the saved workspaces. */
    defaultIndex = G::isRory ? 0 : -1;
    firstWorkspaceIndex = 0;
    if (G::isRory) {
        ui->workspaceCB->view()->setItemDelegate(new ComboSeparatorDelegate(ui->workspaceCB));
        ui->workspaceCB->addItem(WorkspaceDlg::defaultWorkspaceName);
        if (wsList->count()) ui->workspaceCB->insertSeparator(1);
        firstWorkspaceIndex = 2;
    }

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

bool WorkspaceDlg::isDefaultSelected() const
{
    return defaultIndex >= 0 && ui->workspaceCB->currentIndex() == defaultIndex;
}

int WorkspaceDlg::workspaceIndex() const
{
/*
    The index into MW::workspaces for the selected item, or -1 if the Winnow default
    workspace (or the separator) is selected.  Without the default item (see the
    constructor) firstWorkspaceIndex is 0 and the combo index is the workspace index.
*/
    int i = ui->workspaceCB->currentIndex();
    if (i < firstWorkspaceIndex) return -1;
    return i - firstWorkspaceIndex;
}

void WorkspaceDlg::updateForSelection()
{
/*
    The Winnow default workspace can be updated to the current layout, but it cannot be
    deleted or renamed.
*/
    bool isDefault = isDefaultSelected();
    ui->deleteBtn->setEnabled(!isDefault);
    ui->deleteBtn->setToolTip(isDefault
        ? "The Winnow default workspace cannot be deleted"
        : "");
    if (ui->workspaceCB->lineEdit())
        ui->workspaceCB->lineEdit()->setReadOnly(isDefault);
}

void WorkspaceDlg::on_deleteBtn_clicked()
{
    if (G::isLogger) G::log("WorkspaceDlg::on_deleteBtn_clicked");
    int n = workspaceIndex();
    if (n < 0) return;
    emit deleteWorkspace(n);
    ui->workspaceCB->removeItem(ui->workspaceCB->currentIndex());
    // last workspace deleted: remove the now trailing separator
    if (firstWorkspaceIndex && ui->workspaceCB->count() == firstWorkspaceIndex)
        ui->workspaceCB->removeItem(1);
    ui->workspaceCB->setCurrentIndex(0);
    updateForSelection();
}

void WorkspaceDlg::on_reassignBtn_clicked()
{
    if (G::isLogger) G::log("WorkspaceDlg::on_reassignBtn_clicked");
    if (isDefaultSelected()) {
        emit updateDefaultWorkspace();
        ui->status->setText(WorkspaceDlg::defaultWorkspaceName + " has been updated");
        QTimer::singleShot(2000, this, SLOT(clearStatus()));
        return;
    }
    int n = workspaceIndex();
    if (n < 0) return;
    emit reassignWorkspace(n);
    ui->status->setText(ui->workspaceCB->currentText() + " has been reassigned");
    QTimer::singleShot(2000, this, SLOT(clearStatus()));
}

void WorkspaceDlg::clearStatus()
{
    ui->status->setText("");
}

void WorkspaceDlg::on_workspaceCB_editTextChanged(const QString &name)
{
    if (G::isLogger) G::log("WorkspaceDlg::on_workspaceCB_editTextChanged");
    if (isDefaultSelected()) {
        editMode = true;
        return;
    }
    if (editMode) {
        bool isOkay = true;
        int n = workspaceIndex();
        // separator selected (should not happen)
        if (n < 0) {
            editMode = true;
            return;
        }
        ui->workspaceCB->setStyleSheet("");
        // make sure another workspace name is not being duplicated
        for (int i=0; i<ui->workspaceCB->count(); i++) {
            // ignore current item
            if (i == ui->workspaceCB->currentIndex()) continue;
            // ignore the default workspace and the separator
            if (i < firstWorkspaceIndex) continue;
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
