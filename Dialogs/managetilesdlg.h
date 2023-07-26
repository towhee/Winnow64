#ifndef MANAGETILESDLG_H
#define MANAGETILESDLG_H

#include <QtWidgets>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class ManageTilesDlg;
}

class ManageTilesDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ManageTilesDlg(QSettings *setting, QWidget *parent = nullptr);
    ~ManageTilesDlg() override;

signals:
    void extractTile();

private slots:
    void on_deleteBtn_clicked();
    void on_closeBtn_clicked();
    void textChange(QString text);
    void activate(int index);
    void editingFinished();
    void on_newBtn_clicked();

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
