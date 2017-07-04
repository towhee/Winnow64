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
    void reportWorkspace(int n);

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
};

#endif // WORKSPACEDLG_H
