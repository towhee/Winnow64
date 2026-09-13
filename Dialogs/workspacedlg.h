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
        Winnow window position and size.  isDefaultGeometryIncluded is the same for the
        Winnow default workspace, and isCustomDefaultWorkspace says whether that default
        has been defined yet (if not, the built-in layout is used and the choice does not
        apply). */
    explicit WorkspaceDlg(QList<QString> *wsList,
                          const QList<bool> &isGeometryIncluded,
                          bool isDefaultGeometryIncluded,
                          bool isCustomDefaultWorkspace,
                          QWidget *parent = 0) ;
    ~WorkspaceDlg();
    Ui::Workspacedlg *ui;

signals:
    void deleteWorkspace(int);
    void updateDefaultWorkspace();
    void reassignWorkspace(int);
    void renameWorkspace(int, QString);
    void reportWorkspaceNum(int n);
    /*  n is the index into MW::workspaces, or -1 for the Winnow default workspace. */
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
    /* When G::isRory the dropdown lists the Winnow default workspace, a separator, and
       then the saved workspaces, so the combo index of saved workspace n is
       n + firstWorkspaceIndex.  Otherwise only the saved workspaces are listed and
       firstWorkspaceIndex is 0.  Both are set in the constructor. */
    int defaultIndex;
    int firstWorkspaceIndex;
    /*  Window position/size choice per workspace, indexed as MW::workspaces, plus the
        same for the Winnow default workspace.  Held here so the checkbox can be set
        without a round trip to MW. */
    QList<bool> isGeometry;
    bool isDefaultGeometry;
    bool isCustomDefault;
    bool isDefaultSelected() const;
    int workspaceIndex() const;
    void updateForSelection();
};

#endif // WORKSPACEDLG_H
