#ifndef TOKENDLG_H
#define TOKENDLG_H

#include <QtWidgets>
//#include <QDialog>
//#include <QTextEdit>
//#include <QDropEvent>
//#include <QMimeData>

//class TokenList : public QListWidget
//{
//    Q_OBJECT

//public:
//    explicit TokenList(QWidget *parent = nullptr);

//    static QString puzzleMimeType() { return QStringLiteral("application/x-hotspot"); }

//protected:
//    void dragMoveEvent(QDragMoveEvent *event) override;
//    void startDrag(Qt::DropActions supportedActions) override;
//};

class TokenEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit TokenEdit(QWidget *parent = nullptr);

protected:
//    void dragEnterEvent(QDragEnterEvent *event) override;
//    void dropEvent(QDropEvent *event) override;
    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;

private:
    QTextDocument *textDoc;
};

namespace Ui {
class TokenDlg;
}

class TokenDlg : public QDialog
{
    Q_OBJECT

public:
    explicit TokenDlg(QStringList &tokenList, QWidget *parent = 0);
    ~TokenDlg();

private:
    Ui::TokenDlg *ui;
    QString parse();
};

#endif // TOKENDLG_H
