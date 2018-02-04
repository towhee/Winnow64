#ifndef TOKENDLG_H
#define TOKENDLG_H

#include <QtWidgets>

/*****************************************************************************/
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

/*****************************************************************************/
class TokenEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit TokenEdit(QWidget *parent = nullptr);
    QStringList tokenList;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void positionChanged();
    void selectToken(int position);

private:
    QTextDocument *textDoc;
    QTextCharFormat tokenFormat;
    int lastPosition;

    bool isToken(int pos);
    int tokenStart;
    int tokenEnd;
    QString currentToken;
};

/*****************************************************************************/
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
