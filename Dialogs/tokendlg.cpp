#include "tokendlg.h"
#include "ui_tokendlg.h"
#include <QDebug>

/*******************************************************************************
   TokenEdit Class
*******************************************************************************/

TokenEdit::TokenEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptDrops(true);
    QTextCharFormat format;
    format.setVerticalAlignment(QTextCharFormat::AlignBottom);
    setCurrentCharFormat(format);
    textDoc = new QTextDocument(this);
    setDocument(textDoc);
}

//void TokenEdit::dragEnterEvent(QDragEnterEvent *event)
//{
//    if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
//        event->acceptProposedAction();
//    }
//    if (event->mimeData()->hasText()) {
//        event->acceptProposedAction();
//    }

//}

bool TokenEdit::canInsertFromMimeData(const QMimeData *source) const
{
    if (source->hasFormat("application/x-qabstractitemmodeldatalist"))
        return true;

//    if (source->hasText())
//        return true;

    return QTextEdit::canInsertFromMimeData(source);
}

void TokenEdit::insertFromMimeData(const QMimeData *source)
{
    if (source->hasFormat("application/x-qabstractitemmodeldatalist")) {
        QByteArray encoded = source->data("application/x-qabstractitemmodeldatalist");
        QDataStream strm(&encoded, QIODevice::ReadOnly);
        QString token;

        while(!strm.atEnd()){
            int row, col;
            QMap<int,  QVariant> data;
            strm >> row >> col >> data;
            token = data[0].toString();
        }

        QLabel *label = new QLabel(token);
        label->adjustSize();
        label->resize(label->width() + 6, label->height() - 2);
        label->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
        label->setStyleSheet("QLabel {border:0; background-color: rgb(97,123,155);}");

        QPixmap pixmap(label->size());
        label->render(&pixmap);
        QImage image(pixmap.toImage());
        QRect rect(0, 0, image.width(), image.height() - 3);
        QImage cropImage = image.copy(rect);

//        QUrl url(token);
//        textDoc->addResource(QTextDocument::ImageResource, url, cropImage);
        QTextCursor cursor = this->textCursor();
        cursor.insertImage(cropImage, token);
    }
    else {
        QTextEdit::insertFromMimeData(source);
    }
}

//void TokenEdit::dropEvent(QDropEvent *event)
//{
//    if (event->mimeData()->hasText()) {
////        textCursor().insertText(event->mimeData()->text());
////        QTextEdit::dropEvent(event);
//        event->acceptProposedAction();
//        return;
//    }

//    QByteArray encoded = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
//    QDataStream strm(&encoded, QIODevice::ReadOnly);
//    QString token;

//    while(!strm.atEnd()){
//        int row, col;
//        QMap<int,  QVariant> data;
//        strm >> row >> col >> data;
//        token = data[0].toString();
//    }

//    QLabel *label = new QLabel(token, this);
//    label->adjustSize();
//    label->resize(label->width() + 6, label->height() - 2);
//    label->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
//    label->setStyleSheet("QLabel {border:0; background-color: rgb(97,123,155);}");

//    QPixmap pixmap(label->size());
//    label->render(&pixmap);
//    QImage image(pixmap.toImage());
//    QRect rect(0, 0, image.width(), image.height() - 3);
//    QImage cropImage = image.copy(rect);

//    QTextCursor* cursor = new QTextCursor(textDoc);
//    cursor->setPosition(textCursor().position());
//    cursor->insertImage(cropImage, token);
//    setFocus();

///*
//    QTextCharFormat format;
//    format.setBackground(Qt::darkGray);
//    cursor->insertText(token, format);


//    QTextTableFormat tableFormat;
//    tableFormat.setBackground(Qt::lightGray);
//    QTextTable *table = cursor->insertTable(1, 1, tableFormat);
//    cursor->insertText(token);

//    QTextCursor* cursor = new QTextCursor(textDoc);
//    QTextBlockFormat format;
//    format.setBackground(Qt::blue);
//    cursor->setBlockFormat(format);
//    cursor->beginEditBlock();
//    cursor->insertText(token);
//    cursor->endEditBlock();
//    QTextFrame* frame = new QTextFrame(textDoc);
//    QTextFrameFormat format;

//    format.setBackground(QColor(97,123,155));
//    frame->setFrameFormat(format);
//    cursor->insertFrame(format);
//    cursor->insertText(token);
//    */
//}


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
}

TokenDlg::~TokenDlg()
{
    delete ui;
}

