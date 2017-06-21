#ifndef WORKSPACEDLG_H
#define WORKSPACEDLG_H

#include <QDialog>
#include <QMenu>

namespace Ui {
class Workspacedlg;
}

class WorkspaceDlg : public QDialog
{
    Q_OBJECT

public:
    explicit WorkspaceDlg(QList<QString> *wsList, QWidget *parent = 0) ;
    ~WorkspaceDlg();
    Ui::Workspacedlg *ui;

signals:
    void deleteWorkspace(int);
    void reassignWorkspace(int);
    void renameWorkspace(int, QString);

private slots:
    void on_deleteBtn_clicked();
    void on_reassignBtn_clicked();
    void on_workspaceCB_editTextChanged(const QString &name);
    void on_workspaceCB_activated(int idx);
    void clearStatus();

private:
    QWidget *mainWindow;
    QString currentName;
    int activatedIdx;
};

#endif // WORKSPACEDLG_H
