#include "tokendlg.h"
#include "ui_tokendlg.h"
#include <QDebug>

/*******************************************************************************
   TokenList Class
*******************************************************************************/

TokenList::TokenList(QWidget *parent): QListWidget(parent) { }

void TokenList::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        startPos = event->pos();
    QListWidget::mousePressEvent(event);
}

void TokenList::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int distance = (event->pos() - startPos).manhattanLength();
        if (distance >= QApplication::startDragDistance())
            startDrag();
    }
    QListWidget::mouseMoveEvent(event);
}

void TokenList::startDrag()
{
    QString token = "{" + currentItem()->text() +"}";

    QLabel *label = new QLabel(token);
    label->adjustSize();
    QPixmap pixmap(label->size());
    label->render(&pixmap);

    QMimeData *data = new QMimeData;
    data->setText(token);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(data);
    drag->setPixmap(pixmap);

    Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);
}

/*******************************************************************************
   TokenEdit Class
*******************************************************************************/

/* TokenEdit is a subclass of QTextEdit to manage tokenized text. It tokenizes
dragged text in insertFromMimeData and has a parse function to use the tokens
to look up corresponding metadata.  The keys in tokenMap are the available
tokens, while the values in tokenMap are example metadata to show an example
result as the template string is constructed.

Some tokenMap items:

    tokenMap["YYYY"] = "2018";
    tokenMap["YY"] = "18";
    tokenMap["MONTH"] = "JANUARY";
    tokenMap["Month"] = "January";

*/

TokenEdit::TokenEdit(QWidget *parent) :
    QTextEdit(parent)
{
    setAcceptDrops(true);
    setLineWrapMode(QTextEdit::NoWrap);
    textDoc = new QTextDocument(this);
    lastPosition = 0;
    setDocument(textDoc);

    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(positionChanged()));
    connect(this, SIGNAL(textChanged()), this, SLOT(parse()));
}

QString TokenEdit::parse()
{
/*
Convert the token string into the metadata it represents. In this case, using
the example values contained in tokenMap.
*/
    QString s;
    int i = 0;
    while (i < textDoc->characterCount()) {
        if (isToken(i + 1)) {
            s.append(tokenMap[currentToken]);
            i = tokenEnd;
        }
        else {
            s.append(textDoc->characterAt(i));
            i++;
        }
    }
    parseUpdated(s);
    return s;
}

bool TokenEdit::isToken(int pos)
{
/*
Report if the text cursor is in a token.  If it is, set the token item, and the
position of the start and end of the token.  This is used to select the token.
When the token is selected the right and left arrow jump across the entire token
instead of moving through each character in the token.  Delete and backspace
removes the entire selection which removes the token.
*/
    QTextCursor cursor;
    QChar ch = textDoc->characterAt(pos);
    if (ch.unicode() == 8233) return false;
    if (ch == "{") return false;
    if (pos == 0) return false;

    // look backwards
    bool foundPossibleTokenStart = false;
    int startPos;
    for (int i = pos; i >= 0; i--) {
        ch = textDoc->characterAt(i);
        if (i < pos && ch == "}") return false;
        if (ch == "{") {
            foundPossibleTokenStart = true;
            startPos = i + 1;
        }
        if (foundPossibleTokenStart) break;
    }

    if (!foundPossibleTokenStart) return false;

    // look forwards
    QString token;
    for (int i = pos; i <= textDoc->characterCount(); i++) {
        ch = textDoc->characterAt(i);
        if (ch == "}") {
            for (int j = startPos; j < i; j++) {
                token.append(textDoc->characterAt(j));
            }
            if (tokenMap.contains(token)) {
                currentToken = token;
                tokenStart = startPos - 1;
                tokenEnd = i + 1;
                return true;
            }
        }
    }
    return false;
}

void TokenEdit::showEvent(QShowEvent *event)
{
    tokenFormat.setForeground(QColor(Qt::white));
    setTextColor(Qt::white);
    setStyleSheet(QStringLiteral("background-image: url(:/images/tokenBackground.png)"));
    parse();
}

void TokenEdit::selectToken(int position)
{
    if (position < 0) return;
    if (position > textDoc->characterCount()) return;
    QTextCursor cursor = textCursor();
    if (isToken(position)) {
        cursor.setPosition(tokenStart);
        cursor.setPosition(tokenEnd, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
    }
}

void TokenEdit::positionChanged()
{
    static bool ignore = false;
    if (ignore) return;

    QTextCursor cursor = textCursor();
    int position = cursor.position();
    if (isToken(position)) {
         ignore = true;
         if (lastPosition > position) {
            cursor.setPosition(tokenEnd);
            cursor.setPosition(tokenStart, QTextCursor::KeepAnchor);
        }
        else {
            cursor.setPosition(tokenStart);
            cursor.setPosition(tokenEnd, QTextCursor::KeepAnchor);
        }
        ignore = false;
        setTextCursor(cursor);
    }
    lastPosition = textCursor().position();
}

void TokenEdit::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Delete:
        selectToken(textCursor().position() + 1);
    case Qt::Key_Backspace:
        selectToken(textCursor().position() - 1);
        break;
    }
    QTextEdit::keyPressEvent(event);
}

void TokenEdit::insertFromMimeData(const QMimeData *source)
{
    QString token = source->text();
    QTextCursor cursor = textCursor();
    cursor.insertText(token, tokenFormat);
    setTextCursor(cursor);
    lastPosition = cursor.position();
    setFocus();
    parse();
}

/*******************************************************************************
   TokenDlg Class
*******************************************************************************/

TokenDlg::TokenDlg(QMap<QString, QString> &tokenMap,
                   QMap<QString, QString> &templatesMap,
                   int &index,
                   QString &currentKey,
                   QString title,
                   QWidget *parent) :
    tokenMap(tokenMap),
    templatesMap(templatesMap),
    index(index),
    currentKey(currentKey),
    QDialog(parent),
    ui(new Ui::TokenDlg)
{
    ui->setupUi(this);
    setWindowTitle(title);
    setAcceptDrops(true);
    ui->templatesCB->setView(new QListView());      // req'd for setting row height in stylesheet

    // Populate token list
    QMap<QString, QString>::iterator i;
    for (i = tokenMap.begin(); i != tokenMap.end(); ++i)
        ui->tokenList->addItem(i.key());

    /* The template token data is stored in a QMap and its keys are used to
    populate the templateCB dropdown. The values are shown in tokenEdit. Since
    the keys (descriptions) can be edited in templateCB (renaming) we need
    something to hold the QMap keys to sync templatesCB, tokenEdit and the
    QMap. This is accomplished by also saving the keys in the templateCB as
    tooltips. When the dialog closes via the Ok button the QMap is updated to
    match any changes made to templatesCB and tokenEdit. */
    int row = 0;
    for (i = templatesMap.begin(); i != templatesMap.end(); ++i) {
        ui->templatesCB->addItem(i.key());
        ui->templatesCB->setItemData(row, i.key(), Qt::ToolTipRole);
        if (i.key() == currentKey) ui->tokenEdit->setText(i.value());
        row++;
    }

    // select same item as parent
    ui->templatesCB->setCurrentIndex(index);
    ui->tokenList->setDragEnabled(true);
    ui->tokenList->setSelectionMode(QAbstractItemView::SingleSelection);

    ui->tokenEdit->tokenMap = tokenMap;

    connect(ui->tokenEdit, SIGNAL(parseUpdated(QString)),
            this, SLOT(updateExample(QString)));
    connect(ui->tokenEdit, SIGNAL(textChanged()),
            this, SLOT(updateTemplate()));
}

TokenDlg::~TokenDlg()
{
    delete ui;
}

void TokenDlg::updateExample(QString s)
{
    ui->resultLbl->setText(s);
}

void TokenDlg::updateTemplate()
{
    QString key = ui->templatesCB->currentData(Qt::ToolTipRole).toString();
    templatesMap[key] = ui->tokenEdit->toPlainText();
}

void TokenDlg::on_okBtn_clicked()
{
/*
Check if any templates have been renamed. If so, make a new template QMap,
using templatesCB to supply the new keys. Populate the values from the original
template QMap, using the old keys which are retained in templatesCB in the
toolTipRole. Then swap templatesMap with newTemplatesMap.
*/
    QMap<QString, QString> newTemplatesMap;
    for (int i = 0; i < ui->templatesCB->count(); i++) {
        QString key = ui->templatesCB->itemText(i);
        QString oldKey = ui->templatesCB->itemData(i, Qt::ToolTipRole).toString();
        newTemplatesMap[key] = templatesMap[oldKey];
    }
    templatesMap.swap(newTemplatesMap);
    currentKey = ui->templatesCB->currentText();
    accept();
}

void TokenDlg::on_deleteBtn_clicked()
{
    QString key = ui->templatesCB->currentData(Qt::ToolTipRole).toString();
    ui->templatesCB->removeItem(ui->templatesCB->currentIndex());
    templatesMap.remove(key);
}

QStringList TokenDlg::existingTemplates(int row)
{
    QStringList list;
    for (int i = 0; i < ui->templatesCB->count(); ++i) {
        if (i != row) {
            list.append(ui->templatesCB->itemText(i));
        }
    }
    return list;
}

void TokenDlg::on_newBtn_clicked()
{
    QString newTemplate;

    QStringList existing = existingTemplates(-1);
    RenameDlg *nameDlg = new RenameDlg(newTemplate, existing,
             "New Template", "Enter new template name:", this);
    if (!nameDlg->exec()) {
        delete nameDlg;
        return;
    }
    delete nameDlg;

    ui->templatesCB->addItem(newTemplate);
    int i = ui->templatesCB->findText(newTemplate);
    ui->templatesCB->setCurrentIndex(i);
    ui->templatesCB->setItemData(i, newTemplate, Qt::ToolTipRole);
    ui->tokenEdit->setText("");
}

void TokenDlg::on_renameBtn_clicked()
{
    bool ok;
    int row = ui->templatesCB->currentIndex();
    QString name = ui->templatesCB->currentText();
    QStringList existing = existingTemplates(row);
    RenameDlg *nameDlg = new RenameDlg(name, existing,
             "Rename Template", "Rename template name:", this);
    if (!nameDlg->exec()) {
        delete nameDlg;
        return;
    }
    ui->templatesCB->setItemText(row, name);
    delete nameDlg;
}

void TokenDlg::on_templatesCB_currentIndexChanged(int row)
{
/*
Update tokenEdit with the template token string stored in templatesMap
*/
    QString key = ui->templatesCB->itemData(row, Qt::ToolTipRole).toString();
    if (templatesMap.contains(key))
        ui->tokenEdit->setText(templatesMap.value(key));

}
