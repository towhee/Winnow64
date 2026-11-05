#ifndef WORKSPACEDLG_H
#define WORKSPACEDLG_H

#include <QDialog>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

class QTableWidget;
class QLabel;
class QPushButton;

class WorkspaceDlg : public QDialog
{
    Q_OBJECT

public:
    inline static const QString defaultWorkspaceName = "Winnow default workspace";
    /*  One row per custom workspace, in MW::workspaces order.  The workflow layouts are
        NOT managed here -- they live in Window > Workspace (see the Workflow enum in
        MW). */
    struct Row {
        QString name;
        bool isGeometryIncluded = true;     // "Window *": restore position and size
        bool isDelete = false;
    };
    /*  otherNames: workspaces not listed (the dialog can be filtered to one workflow)
        whose names a rename must not take. */
    explicit WorkspaceDlg(const QList<Row> &rows, const QStringList &otherNames,
                          const QString &title, QWidget *parent = nullptr);

    /*  The edited rows, read by MW::manageWorkspaces after Update (exec() Accepted).
        Nothing is applied before then. */
    QList<Row> rows() const;

private:
    enum Col {ColName, ColWindow, ColDelete, ColCount};
    QTableWidget *table;
    QLabel *status;
    QPushButton *helpBtn;
    QPushButton *updateBtn;
    QStringList otherNames;
    QString validate() const;
    void applyUpdate();
    void showHelp();
};

#endif // WORKSPACEDLG_H
