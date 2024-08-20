#include "tokendlg.h"
#include "ui_tokendlg.h"
#include <QDebug>

/*
This group of classes provides a tool to tokenize a string.

For example a date:  2018-03-17 cab be deconstructed into

{YYYY}-{MM}-{DD} where the year, month and day will be substituted from file
metadata.

Classes:

TokenList = a dragable list of all available tokens.

TokenEdit = a subclassed QTestEdit that accepts drops and treats tokens as
            characters (the cursor only selects the entire token, not individual
            char).

TokenDlg = a dialog that houses the token editor.

TokenDlg is called from IngestDlg and InfoString.
*/

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
            startDrag(Qt::CopyAction);
    }
}

void TokenList::startDrag(Qt::DropActions /* supportedActions */)
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

    drag->exec();
}

/*******************************************************************************
   TokenEdit Class
*******************************************************************************/

/*
    TokenEdit is a subclass of QTextEdit to manage tokenized text. It tokenizes
    dragged text in insertFromMimeData and has a parse function to use the tokens
    to look up corresponding metadata.  The keys in exampleMap are the available
    tokens, while the values in exampleMap are example metadata to show an example
    result as the template string is constructed.

    Some exampleMap items:

        exampleMap["YYYY"] = "2018";
        exampleMap["YY"] = "18";
        exampleMap["MONTH"] = "JANUARY";
        exampleMap["Month"] = "January";

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
    the example values contained in exampleMap.
*/
    QString s;
    int i = 0;
    while (i < textDoc->characterCount()) {
        if (isToken(i + 1)) {
            if (currentToken == "⏎") s.append("\n");
            else s.append(exampleMap[currentToken]);
            i = tokenEnd;
        }
        else {
            s.append(textDoc->characterAt(i));
            i++;
        }
    }
    parseUpdated(s);
    if(textDoc->toPlainText().length() > 0) emit isUnique(isLikelyUnique());
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
    if (ch == '{') return false;
    if (pos == 0) return false;

    // look backwards
    bool foundPossibleTokenStart = false;
    int startPos;
    for (int i = pos; i >= 0; i--) {
        ch = textDoc->characterAt(i);
        if (i < pos && ch == '}') return false;
        if (ch == '{') {
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
        if (ch == '}') {
            for (int j = startPos; j < i; j++) {
                token.append(textDoc->characterAt(j));
            }
            if (exampleMap.contains(token)) {
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
/*
    These actions must be run after the dialog constructor is finished.
*/
    tokenFormat.setForeground(QColor(Qt::white));
    setTextColor(Qt::white);
    setStyleSheet(QStringLiteral("background-image: url(:/images/tokenBackground.png)"));
    parse();
    event->accept();
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

bool TokenEdit::isLikelyUnique()
{
    QString tokenString = textDoc->toPlainText();
    if(tokenString.contains("{ORIGINAL FILENAME}")) return true;
    if(tokenString.contains("XX")) return true;
    if(tokenString.contains("{SECOND}")) return true;
    return false;
}

/*******************************************************************************
   TokenDlg Class
*******************************************************************************/

/*  The TokenDlg has a list of tokens on the left and a textbox on the right
where the tokens can be dragged.  The calling function must define a token
list, an example token list and a token parser routine.  TokenDlg's function
is to select and order the tokens to create the desired string.  The user can
inserrt any text, except / or \, in the token string.

For example, the token string:

    {MODEL} | {ShutterSpeed} sec at f/{Aperture}

would produce the token string result:

    Nikon D5 | 1/250 sec at f/5.6

where the values of the tokens is extracted from the image file metadata.

TokenDlg builds the token string.  The calling function parses it to produce
the result.
*/

TokenDlg::TokenDlg(QStringList &tokens,
                   QMap<QString, QString> &exampleMap,
                   QMap<QString, QString> &templatesMap,
                   QMap<QString, QString> &usingTokenMap,
                   int &index,
                   QString &currentKey,
                   QString title,
                   QWidget *parent) :

                   QDialog(parent),
                   ui(new Ui::TokenDlg),
                   exampleMap(exampleMap),
                   templatesMap(templatesMap),
                   usingTokenMap(usingTokenMap),
                   index(index),
                   currentKey(currentKey)
{
/*
    The TokenDlg is a tool to aggregate tokens to built larger strings of token data.  A token
    is a placeholder/variable for a value from metadata and tokens are shown in curly brackets.
    For example, the token {Filename} will be replaced with the file name for a specific image.
    In addition to tokens, any other text, not surrounded by {} can also be included in the
    token string.  For example {YYYY}-{MM} might equal 2021-08.  A token string is referred to
    as a template.  Each template has a used defined name.  Templates can be deleted or
    renamed.

    The template token data is stored in a QMap and its keys are used to populate the
    templateCB dropdown. The values are shown in tokenEdit. Since the keys (descriptions) can
    be edited in templateCB (renaming) we need something to hold the QMap keys to sync
    templatesCB, tokenEdit and the QMap. This is accomplished by also saving the keys in the
    templateCB as tooltips. When the dialog closes via the Ok button the QMap is updated to
    match any changes made to templatesCB and tokenEdit.
*/
    ui->setupUi(this);
    this->title = title;
    setWindowTitle(title);
    setAcceptDrops(true);

    ui->templatesCB->setView(new QListView());  // req'd for setting row height in stylesheet
    ui->templatesCB->setMaxCount(100);

    // minimum button size override G::css (widgetcss)
    ui->okBtn->setStyleSheet("QPushButton {min-width: 60px;}");
    ui->renameBtn->setStyleSheet("QPushButton {min-width: 60px;}");
    ui->newBtn->setStyleSheet("QPushButton {min-width: 60px;}");
    ui->deleteBtn->setStyleSheet("QPushButton {min-width: 60px;}");

    setStyleSheet(G::css);

    // Populate token list
    for (const auto &i : tokens)
        ui->tokenList->addItem(i);

    int row = 0;
    QMap<QString, QString>::iterator i;
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

    ui->tokenEdit->exampleMap = exampleMap;
    ui->uniqueWarningLabel->setVisible(false);

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif

    connect(ui->tokenEdit, SIGNAL(parseUpdated(QString)),
            this, SLOT(updateExample(QString)));
    connect(ui->tokenEdit, SIGNAL(isUnique(bool)),
            this, SLOT(updateUniqueFileNameWarning(bool)));
    connect(ui->tokenEdit, SIGNAL(textChanged()),
            this, SLOT(updateTemplate()));
    connect(ui->chkUseInLoupeView, &QCheckBox::stateChanged, this,
            &TokenDlg::on_chkUseInLoupeView_checked);
}

TokenDlg::~TokenDlg()
{
    delete ui;
}

void TokenDlg::updateUniqueFileNameWarning(bool isProbablyUnique)
{
    if (title == "Token Editor - Destination File Name") {
        if (isProbablyUnique)
            ui->uniqueWarningLabel->setVisible(false);
        else
            ui->uniqueWarningLabel->setVisible(true);
    }
}

void TokenDlg::updateExample(QString s)
{
    ui->resultText->setText(s);
}

void TokenDlg::updateTemplate()
{
    QString key = ui->templatesCB->currentData(Qt::ToolTipRole).toString();
    templatesMap[key] = ui->tokenEdit->toPlainText();
}

void TokenDlg::on_okBtn_clicked()
{
/*
    Check if any templates have been renamed. If so, make a new template QMap, using
    templatesCB to supply the new keys. Populate the values from the original template QMap,
    using the old keys which are retained in templatesCB in the toolTipRole. Then swap
    templatesMap with newTemplatesMap.
*/
    if (G::isLogger) G::log("TokenDlg::on_okBtn_clicked");
    //qDebug() << "TokenDlg::on_okBtn_clicked";
    QMap<QString, QString> newTemplatesMap;
    for (int i = 0; i < ui->templatesCB->count(); i++) {
        QString key = ui->templatesCB->itemText(i);
        QString oldKey = ui->templatesCB->itemData(i, Qt::ToolTipRole).toString();
        newTemplatesMap[key] = templatesMap[oldKey];
    }
    templatesMap.swap(newTemplatesMap);
//    currentKey = ui->templatesCB->currentText();
    accept();
}

void TokenDlg::on_deleteBtn_clicked()
{
    QString key = ui->templatesCB->currentData(Qt::ToolTipRole).toString();
    qDebug() << "TokenDlg::on_deleteBtn_clicked";
    return;

    // check to see if the token template is being used
    QString msg = "";
    QMapIterator<QString, QString> i(usingTokenMap);
    while (i.hasNext()) {
        i.next();
        if (i.key() == key) msg += i.value() + "\n";
    }

    // if being used explain to user and return without deleting
    if (msg != "") {
        msg = "The token template " + key + " is being used by:\n\n" + msg +
              "\nUnable to delete.";
        QMessageBox::warning(this, "Delete token template", msg, QMessageBox::Ok);
        return;
    }
    // okay to delete
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
    qDebug() << "TokenDlg::on_newBtn_clicked";

    QStringList existing = existingTemplates(-1);
    RenameDlg *nameDlg = new RenameDlg(newTemplate, existing,
             "New Template", "Enter new template name:", this);
    if (!nameDlg->exec()) {
        delete nameDlg;
        return;
    }
    delete nameDlg;

    //qDebug() << "TokenDlg::on_newBtn_clicked" << newTemplate << existing;

    ui->templatesCB->addItem(newTemplate);
    int i = ui->templatesCB->findText(newTemplate);
    ui->templatesCB->setCurrentIndex(i);
    ui->templatesCB->setItemData(i, newTemplate, Qt::ToolTipRole);
    ui->tokenEdit->setText("");
}

void TokenDlg::on_renameBtn_clicked()
{
/*
    As noted in on_okBtn_clicked, renaming the template changes the QMap key.
*/
    //qDebug() << "TokenDlg::on_renameBtn_clicked";
    // current name
    int row = ui->templatesCB->currentIndex();
    QString name = ui->templatesCB->currentText();
    QString oldName = name;
    QString value = templatesMap.value(name);
    QStringList existing = existingTemplates(row);
    // get new name
    RenameDlg *nameDlg = new RenameDlg(name, existing,
             "Rename Template", "Template:", this);
    if (!nameDlg->exec()) {
        delete nameDlg;
        return;
    }
    // update dialog token template name
    if (name != oldName) {
        ui->templatesCB->setItemText(row, name);
        emit rename(oldName, name);
        // update templatesMap
        templatesMap.insert(name, value);
        templatesMap.remove(oldName);
    }
    /* update in QSettings done when Winnow closes
    QSettings *setting = new QSettings("Winnow", "winnow_100");
    QString tokenString = setting->value("InfoTemplates/" + oldName).toString();
    setting->remove("InfoTemplates/" + oldName);
    setting->setValue("InfoTemplates/" + name, tokenString);  */
    delete nameDlg;
}

void TokenDlg::on_templatesCB_currentIndexChanged(int row)
{
/*
    Update tokenEdit with the template token string stored in templatesMap
*/
    QString key = ui->templatesCB->itemData(row, Qt::ToolTipRole).toString();
    //qDebug() << "TokenDlg::on_templatesCB_currentIndexChanged" << row << key << "current" << currentKey;
    indexJustChanged = true;
    if (templatesMap.contains(key)) {
        ui->tokenEdit->setText(templatesMap.value(key));
        if (key == currentKey) {
            ui->chkUseInLoupeView->setChecked(true);
        }
        else {
            ui->chkUseInLoupeView->setChecked(false);
        }
    }
    indexJustChanged = false;
}

void TokenDlg::on_chkUseInLoupeView_checked(int state)
{
    qDebug() << "TokenDlg::on_chkUseInLoupeView_checked" << "state +" << state;
    if (!indexJustChanged) {
        if (ui->chkUseInLoupeView->isChecked()) {
            currentKey = ui->templatesCB->currentText();
        }
        else {
            ui->chkUseInLoupeView->setChecked(true);
            QString msg = "Select another template and then click checkbox to make it<br>"
                          "the one used for the loupe view overlay info.<p>Press ESC to<br>"
                          "close this message.";
            G::popUp->showPopup(msg, 0, true, 0.75, Qt::AlignLeft);
        }
    }
}

void TokenDlg::reject()
{
    qDebug() << "TokenDlg::reject";
    if (G::popUp->isVisible()) {
        G::popUp->setVisible(false);
        return;
    }
    QDialog::reject();
}
