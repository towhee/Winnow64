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
    QString token = currentItem()->text();

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
    setDocument(textDoc);
}


void TokenEdit::showEvent(QShowEvent *event)
{
//    QTextEdit::showEvent(event);
    QTextFrameFormat frameFormat;
    frameFormat.setForeground(Qt::green);


    QTextBlockFormat blockFormat;
//    blockFormat.setLineHeight(50, QTextBlockFormat::FixedHeight);
    blockFormat.setForeground(Qt::green);
    textCursor().setBlockFormat(blockFormat);
    textCursor().insertBlock();
//    QTextCharFormat charFormat;
//    charFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
//    charFormat.setBackground(Qt::blue);
//    textCursor().setCharFormat(charFormat);
//    QTextTableCellFormat cellFormat;
//    cellFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
//    textCursor().insertTable(1,1);
//    textCursor().insertText("Test", charFormat);
//    qDebug() << textDoc->toHtml("utf-8") << "\n";
}

//bool TokenEdit::canInsertFromMimeData(const QMimeData *source) const
//{
//    if (source->hasFormat("application/x-qabstractitemmodeldatalist"))
//        return true;

////    if (source->hasText())
////        return true;

//    return QTextEdit::canInsertFromMimeData(source);
//}

void TokenEdit::insertFromMimeData(const QMimeData *source)
{
    QString token = "{" + source->text() + "}";

    QTextCursor cursor = textCursor();
    QTextCharFormat charFormat;
    charFormat.setForeground(Qt::yellow);
    cursor.insertText(token, charFormat);
    QString html = "<span style= \" color:#ffffff;\">";
    cursor.insertHtml(html);
    setFocus();
    qDebug() << textDoc->toHtml("utf-8") << "\n";
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
}

TokenDlg::~TokenDlg()
{
    delete ui;
}


//    if (source->hasFormat("application/x-qabstractitemmodeldatalist")) {
//        QByteArray encoded = source->data("application/x-qabstractitemmodeldatalist");
//        QDataStream strm(&encoded, QIODevice::ReadOnly);
//        QString token;

//        while(!strm.atEnd()){
//            int row, col;
//            QMap<int,  QVariant> data;
//            strm >> row >> col >> data;
//            token = data[0].toString();
//        }

//        QLabel *label = new QLabel(token);
//        label->adjustSize();
//        label->resize(label->width() + 6, label->height() + 2);
//        label->setAlignment(Qt::AlignCenter | Qt::AlignCenter);
//        label->setStyleSheet("QLabel {border:0; background-color: rgb(97,123,155);}");

//        QPixmap pixmap(label->size());
//        label->render(&pixmap);
//        QImage image(pixmap.toImage());
////        QRect rect(0, 0, image.width(), image.height() - 3);
////        QImage cropImage = image.copy(rect);

//        QUrl url(token);
//        textDoc->addResource(QTextDocument::ImageResource, url, image);
////        QTextImageFormat imageFormat;
////        imageFormat.setVerticalAlignment(QTextCharFormat::AlignBottom);
////        imageFormat.setBackground(Qt::red);
////        imageFormat.setName(url.toString());
//        QTextCursor cursor = this->textCursor();
////        cursor.insertImage(imageFormat);
////        QString html = "<img src=\"COPYRIGHT\" />";
////        QString html = "<span style=\"display:inline-block; vertical-align:bottom\"><img src=\"COPYRIGHT\" /></span>";
////        QString html = "<span style=\"display:inline-block; vertical-align:middle\"><img src=\"COPYRIGHT\" /></span>";
////        QString html = "<img src=\"COPYRIGHT\" style=\"vertical-align: middle;\" />";
//        QString html = "<img src=\"" + token + "\" style=\"vertical-align: middle;\" />";
//        cursor.insertHtml(html);
//        qDebug() << textDoc->toHtml("utf-8") << "\n";
////        cursor.insertImage(image, token);
//    }
//    else {
//        QTextEdit::insertFromMimeData(source);
//    }

//void TokenEdit::dragEnterEvent(QDragEnterEvent *event)
//{
//    if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
//        event->acceptProposedAction();
//    }
//    if (event->mimeData()->hasText()) {
//        event->acceptProposedAction();
//    }

//}


