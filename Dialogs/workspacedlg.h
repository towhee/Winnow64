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
    /*  isGeometryIncluded holds, for each workspace in wsList, whether it restores the
        Winnow window position and size.  The per-workflow default and override layouts
        are NOT managed here -- they live in Window > Workspace (see the Workflow enum in
        MW) -- so the dropdown lists only the user's saved workspaces. */
    explicit WorkspaceDlg(QList<QString> *wsList,
                          const QList<bool> &isGeometryIncluded,
                          QWidget *parent = 0) ;
    ~WorkspaceDlg();
    Ui::Workspacedlg *ui;

signals:
    void deleteWorkspace(int);
    void reassignWorkspace(int);
    void renameWorkspace(int, QString);
    void reportWorkspaceNum(int n);
    /*  n is the index into MW::workspaces. */
    void setWorkspaceGeometryIncluded(int n, bool isIncluded);

private slots:
    void on_deleteBtn_clicked();
    void on_reassignBtn_clicked();
    void on_workspaceCB_editTextChanged(const QString &name);
    void clearStatus();
    void on_workspaceCB_highlighted(int index);
    void on_reportLinkButton_clicked();
    void on_workspaceCB_currentIndexChanged(int index);
    void on_geometryCB_toggled(bool isChecked);

private:
    QWidget *mainWindow;
    bool editMode;
    void report(QString signalName);
    /*  Window position/size choice per workspace, indexed as MW::workspaces.  Held here
        so the checkbox can be set without a round trip to MW. */
    QList<bool> isGeometry;
    int workspaceIndex() const;
    void updateForSelection();
};

#endif // WORKSPACEDLG_H
