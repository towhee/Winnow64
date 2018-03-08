#ifndef TOKENDLG_H
#define TOKENDLG_H

#include <QtWidgets>
#include "renamedlg.h"

/*****************************************************************************/
class TokenList : public QListWidget
{
    Q_OBJECT

public:
    explicit TokenList(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void startDrag(Qt::DropActions /* supportedActions */) override;

private:
//    void startDrag(Qt::DropActions /* supportedActions */) override;
//    Qt::DropAction supportedDropActions();
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

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void showEvent(QShowEvent *event) override;

public slots:
    QString parse();

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
    explicit TokenDlg(QMap<QString, QString> &tokenMap,
                      QMap<QString, QString> &templatesMap,
                      int &index,
                      QString &currentKey,
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
    void on_renameBtn_clicked();
    void on_templatesCB_currentIndexChanged(int row);

private:
    Ui::TokenDlg *ui;
    QMap<QString, QString>& tokenMap;
    QMap<QString, QString>& templatesMap;
    int &index;
    QString &currentKey;
    QStringList existingTemplates(int row = -1);
};

#endif // TOKENDLG_H
