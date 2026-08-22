#ifndef WORKSPACEDLG_H
#define WORKSPACEDLG_H

#include <QDialog>
#include <QMenu>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class Workspacedlg;
}

class WorkspaceDlg : public QDialog
{
    Q_OBJECT

public:
    inline static const QString defaultWorkspaceName = "Winnow default workspace";
    explicit WorkspaceDlg(QList<QString> *wsList, QWidget *parent = 0) ;
    ~WorkspaceDlg();
    Ui::Workspacedlg *ui;

signals:
    void deleteWorkspace(int);
    void updateDefaultWorkspace();
    void reassignWorkspace(int);
    void renameWorkspace(int, QString);
    void reportWorkspaceNum(int n);

private slots:
    void on_deleteBtn_clicked();
    void on_reassignBtn_clicked();
    void on_workspaceCB_editTextChanged(const QString &name);
    void clearStatus();
    void on_workspaceCB_highlighted(int index);
    void on_reportLinkButton_clicked();
    void on_workspaceCB_currentIndexChanged(int index);

private:
    QWidget *mainWindow;
    bool editMode;
    void report(QString signalName);
    /* When G::isRory the dropdown lists the Winnow default workspace, a separator, and
       then the saved workspaces, so the combo index of saved workspace n is
       n + firstWorkspaceIndex.  Otherwise only the saved workspaces are listed and
       firstWorkspaceIndex is 0.  Both are set in the constructor. */
    int defaultIndex;
    int firstWorkspaceIndex;
    bool isDefaultSelected() const;
    int workspaceIndex() const;
    void updateForSelection();
};

#endif // WORKSPACEDLG_H
