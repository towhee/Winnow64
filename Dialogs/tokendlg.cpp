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

TokenEdit::TokenEdit(QWidget *parent) : QTextEdit(parent)
{
    setAcceptDrops(true);
    setLineWrapMode(QTextEdit::NoWrap);
    textDoc = new QTextDocument(this);
    lastPosition = 0;
    setDocument(textDoc);


//    QImage image(pixmap.toImage());
//    QUrl url("bk");
//    textDoc->addResource(QTextDocument::ImageResource, url, image);

//    setStyleSheet("QTextEdit {background-image: url(bk);}");
    //  background-image: url(bk);

    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(positionChanged()));
}

bool TokenEdit::isToken(int pos)
{
    QTextCursor cursor;
//    int pos = textCursor().position();
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
            if (tokenList.contains(token)) {
                currentToken = token;
                tokenStart = startPos - 1;
                tokenEnd = i + 1;
                return true;
            }
            qDebug() << "token =" << token;
        }
    }
    return false;
}

void TokenEdit::showEvent(QShowEvent *event)
{
    tokenFormat.setForeground(QColor(Qt::white));
    qDebug() << "tokenFormat.foreground =" << tokenFormat.foreground();
    setTextColor(Qt::white);

    QLabel bgLbl("Drag tokens here");
    qDebug() << "size()" << size();
    bgLbl.resize(size());
    qDebug() << "lbl size()" << bgLbl.size();

    bgLbl.setAlignment(Qt::AlignHCenter | Qt::AlignCenter);
    QFont font = bgLbl.font();
    font.setPointSize(24);
    bgLbl.setFont(font);
    bgLbl.setStyleSheet("QLabel {background-color: rgb(60,60,60); color: rgb(80,80,80);}");
    QPixmap pixmap(bgLbl.size());
    bgLbl.render(&pixmap);

    QMessageBox msg;
    msg.setIconPixmap(pixmap);
    msg.exec();

    QPalette palette;
    palette.setBrush(QPalette::Window, QBrush(pixmap));
    setPalette(palette);
    setAutoFillBackground(true);

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
//         cursor.setBlockCharFormat(tokenFormat);
//         setTextCursor(cursor);
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
//        mergeCurrentCharFormat(tokenFormat);
        setTextCursor(cursor);
//        setTextColor(Qt::yellow);
    }
    else {
//        setTextColor(Qt::white);
    }
    lastPosition = textCursor().position();
}

void TokenEdit::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Delete:
        selectToken(textCursor().position() + 1);
    case Qt::Key_Backspace:
        qDebug() << "Qt::Key_Back";
        selectToken(textCursor().position() - 1);
    }
    QTextEdit::keyPressEvent(event);
}

void TokenEdit::keyReleaseEvent(QKeyEvent *event)
{
    QTextEdit::keyReleaseEvent(event);
}

void TokenEdit::insertFromMimeData(const QMimeData *source)
{
    QString token = source->text();
    QTextCursor cursor = textCursor();
    cursor.insertText(token, tokenFormat);
    setTextCursor(cursor);
    lastPosition = cursor.position();
//    selectToken(lastPosition - 1);
    setFocus();
}

/*******************************************************************************
   TokenDlg Class
*******************************************************************************/

TokenDlg::TokenDlg(QStringList &tokenList, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TokenDlg)
{
    ui->setupUi(this);
    setAcceptDrops(true);
    ui->tokenList->insertItems(0, tokenList);
    ui->tokenList->setDragEnabled(true);
    ui->tokenList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tokenEdit->tokenList = tokenList;


//    ui->bgLbl->setText("Drag tokens here");
//    ui->bgLbl->setAlignment(Qt::AlignHCenter | Qt::AlignCenter);
//    QFont font = ui->bgLbl->font();
//    font.setPointSize(24);
//    ui->bgLbl->setFont(font);
//    ui->bgLbl->setStyleSheet("QLabel {background-color: rgb(60,60,60); color: rgb(80,80,80);}");

}

TokenDlg::~TokenDlg()
{
    delete ui;
}
