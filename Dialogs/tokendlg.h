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
    QMap<QString, QString> tokenMap;
    QString parse();

protected:
    void keyPressEvent(QKeyEvent *event) override;
//    void keyReleaseEvent(QKeyEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void positionChanged();
    void selectToken(int position);

signals:
    void parseUpdated(QString s);

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
    explicit TokenDlg(QMap<QString, QString>& tokenMap,
                      QMap<QString, QString> &templatesMap,
                      QString title,
                      QWidget *parent = 0);
    ~TokenDlg();

public slots:
    void updateExample(QString s);
    void updateTemplate();

private slots:
    void on_okBtn_clicked();
    void on_deleteBtn_clicked();
    void on_newBtn_clicked();

    void on_templatesCB_editTextChanged(const QString &arg1);

    void on_templatesCB_activated(const QString &arg1);

private:
    Ui::TokenDlg *ui;
    QMap<QString, QString>& tokenMap;
    QMap<QString, QString>& templatesMap;
};

#endif // TOKENDLG_H
