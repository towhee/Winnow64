#ifndef MANAGETILESDLG_H
#define MANAGETILESDLG_H

#include <QtWidgets>

namespace Ui {
class ManageTilesDlg;
}

class ManageTilesDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ManageTilesDlg(QSettings *setting, QWidget *parent = nullptr);
    ~ManageTilesDlg();

private slots:
    void on_deleteBtn_clicked();
    void on_closeBtn_clicked();
    void textChange(QString text);
    void activate(int index);
    void editingFinished();

private:
    Ui::ManageTilesDlg *ui;
    QStringListModel *tileBoxModel;
    QStringList tiles;
    QSettings *setting;
    bool textHasBeenEdited;
    int textEditedIndex;
//    QString key;
};

#endif // MANAGETILESDLG_H
