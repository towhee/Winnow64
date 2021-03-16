#ifndef MANAGEGRAPHICSDLG_H
#define MANAGEGRAPHICSDLG_H

#include <QtWidgets>

namespace Ui {
class ManageGraphicsDlg;
}

class ManageGraphicsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ManageGraphicsDlg(QSettings *setting, QWidget *parent = nullptr);
    ~ManageGraphicsDlg() override;

signals:
    void getGraphic();

public slots:
    void updateList();

private slots:
    void on_deleteBtn_clicked();
    void on_closeBtn_clicked();
    void textChange(QString text);
    void activate(int index);
    void editingFinished();
    void on_newBtn_clicked();

private:
    Ui::ManageGraphicsDlg *ui;
    QStringListModel *graphicsBoxModel;
    QStringList graphicsList;
    QSettings *setting;
    bool textHasBeenEdited;
    int textEditedIndex;
};

#endif // MANAGEGRAPHICSDLG_H
