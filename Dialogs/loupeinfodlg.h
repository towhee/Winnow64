#ifndef LOUPEINFODLG_H
#define LOUPEINFODLG_H

#include <QDialog>
#include "Dialogs/tokendlg.h"

namespace Ui {
class LoupeInfoDlg;
}

class LoupeInfoDlg : public QDialog
{
    Q_OBJECT

public:
    explicit LoupeInfoDlg(QStringList &tokens,
                          QMap<QString, QString> &exampleMap,
                          QMap<QString, QString> &infoTemplates,
                          QMap<QString, QString> &usingTokenMap,
                          int &index,
                          QString &currentKey,
                          std::function<void()> updateShootingInfoCallback,
                          QWidget *parent = nullptr);
    ~LoupeInfoDlg() override;

signals:
    void updateInfo();

private slots:
    void on_templateEditorBtn_clicked();
    void on_okBtn_clicked();
    void on_templatesCB_currentIndexChanged(int idx);

private:
    Ui::LoupeInfoDlg *ui;
    QStringList &tokens;
    QMap<QString, QString>& exampleMap;
    QMap<QString, QString>& infoTemplates;
    QMap<QString, QString>& usingTokenMap;
    int &index;
    QString &currentKey;
    std::function<void ()> updateShootingInfoCallback;
    QStringList existingTemplates(int row = -1);
    bool isInitializing = true;
};

#endif // LOUPEINFODLG_H
