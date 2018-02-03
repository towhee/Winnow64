#ifndef TOKENDLG_H
#define TOKENDLG_H

#include <QtWidgets>

class TokenList : public QListWidget
{
    Q_OBJECT

public:
    explicit TokenList(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void startDrag();
    QPoint startPos;
};

class TokenEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit TokenEdit(QWidget *parent = nullptr);

protected:
//    void dragEnterEvent(QDragEnterEvent *event) override;
//    void dropEvent(QDropEvent *event) override;
//    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;
    void showEvent(QShowEvent *event) override;

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
