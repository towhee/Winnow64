#include "Main/mainwindow.h"

/*
Navigation can be initiated from the following:

    • QActions: if there is a shortcut, then it is executed and the keystroke(s) are not
      registered by any of the QKey events.

    • Overriding key and mouse events in IconView, ImageView, VideoView

    • Overriding key press and mouse press in the MW eventFilter

Keyboard modifiers

    Shift, Control/Cmd, Alt/option and Meta/Control can be used in Winnow.
    Get state with qApp->keyboardModifiers().
    Determine exact match with Utilities::modifier().
    ie Utilities::modifier(qApp->keyboardModifiers(Qt::ShiftModifier | Qt::AltModifier))

Program

    mainwindow
    navigate
    menusandactions
    selection
    IconView, TableView, ImageView
*/

void MW::mouseSideKeyPress(int direction)
{
/*
    back/forward buttons on Logitech mouse sent from central widget.
    direction == 0 forward, else back
*/
    if (G::isLogger || G::isFlowLogger) {
        G::log("MW::nativeLeftRight", "ROW: " + QString::number(dm->currentSfRow));
    }

    if (direction == 0) sel->next();
    else sel->prev();
}

void MW::keyRight(/*Qt::KeyboardModifiers modifier*/)
{
    qDebug() << "\nKEY RIGHT\n";
    // qDebug() << "MW::keyRight";
    if (G::isLogger || G::isFlowLogger)
        G::log("MW::keyRight", "ROW: " + QString::number(dm->currentSfRow));

    if (G::mode == "Loupe" || G::mode == "Table" || G::mode == "Grid") {
        sel->next(qApp->keyboardModifiers());
    }
    if (G::mode == "Compare") {
        sel->select(compareImages->go("Right"), Qt::NoModifier, "MW::keyRight");
    }
}

void MW::keyLeft()
{
    qDebug() << "\nKEY LEFT\n";
    if (G::isLogger || G::isFlowLogger) {
        G::log("MW::keyLeft", "ROW: " + QString::number(dm->currentSfRow));
    }
    if (G::mode == "Compare") {
        sel->select(compareImages->go("Left"), Qt::NoModifier, "MW::keyLeft");
    }
    if (G::mode == "Loupe" || G::mode == "Table" || G::mode == "Grid") {
        sel->prev(qApp->keyboardModifiers());
    }
}

void MW::keyUp()
{
    if (G::isLogger) G::log("MW::keyUp");
    if (G::mode == "Loupe") sel->up(qApp->keyboardModifiers());
    if (G::mode == "Table") sel->up(qApp->keyboardModifiers());
    if (G::mode == "Grid") sel->up(qApp->keyboardModifiers());
}

void MW::keyDown()
{
    if (G::isLogger) G::log("MW::keyDown");
    if (G::mode == "Loupe") sel->down(qApp->keyboardModifiers());
    if (G::mode == "Table") sel->down(qApp->keyboardModifiers());
    if (G::mode == "Grid") sel->down(qApp->keyboardModifiers());
}

void MW::keyPageUp()
{
    if (G::isLogger)
        G::log("MW::keyPageUp");
    if (G::mode == "Loupe") sel->prevPage(qApp->keyboardModifiers());
    if (G::mode == "Table") sel->prevPage(qApp->keyboardModifiers());
    if (G::mode == "Grid") sel->prevPage(qApp->keyboardModifiers());
}

void MW::keyPageDown()
{
    if (G::isLogger)
        G::log("MW::keyPageDown");
    if (G::mode == "Loupe") sel->nextPage(qApp->keyboardModifiers());
    if (G::mode == "Table") sel->nextPage(qApp->keyboardModifiers());
    if (G::mode == "Grid") sel->nextPage(qApp->keyboardModifiers());
}

void MW::keyHome()
{
/*

*/
    qDebug() << "\nKEY HOME\n";
    if (G::isLogger) G::log("MW::keyHome");
    if (G::isInitializing) return;
    if (G::mode == "Compare") compareImages->go("Home");
    sel->first(qApp->keyboardModifiers());
}

void MW::keyEnd()
{
/*

*/
    qDebug() << "\nKEY END\n";
    if (G::isLogger || G::isFlowLogger) G::log("MW::keyEnd");
    if (G::isInitializing) return;
    if (G::mode == "Compare") compareImages->go("End");
    else sel->last(qApp->keyboardModifiers());
}

void MW::keyScrollDown()
{
    if (G::isLogger) G::log("MW::keyScrollDown");
    if (G::mode == "Grid") gridView->scrollDown(0);
    if (thumbView->isVisible()) thumbView->scrollDown(0);
}

void MW::keyScrollUp()
{
    if (G::isLogger) G::log("MW::keyScrollUp");
    if (G::mode == "Grid") gridView->scrollUp(0);
    if (thumbView->isVisible()) thumbView->scrollUp(0);
}

void MW::keyScrollPageDown()
{
    if (G::isLogger) G::log("MW::keyScrollPageDown");
    if (G::mode == "Grid") gridView->scrollPageDown(0);
    if (thumbView->isVisible()) thumbView->scrollPageDown(0);
}

void MW::keyScrollPageUp()
{
    if (G::isLogger) G::log("MW::keyScrollPageUp");
    if (G::mode == "Grid") gridView->scrollPageUp(0);
    if (thumbView->isVisible()) thumbView->scrollPageUp(0);
}

void MW::keyScrollHome()
{
    if (G::isLogger) G::log("MW::keyScrollHome");
    if (G::mode == "Grid") gridView->scrollToRow(0, "MW::keyScrollHome");
    if (thumbView->isVisible()) thumbView->scrollToRow(0, "MW::keyScrollHome");
}

void MW::keyScrollEnd()
{
    if (G::isLogger) G::log("MW::keyScrollEnd");
    int last = dm->sf->rowCount() - 1;
    if (G::mode == "Grid") gridView->scrollToRow(last, "MW::keyScrollEnd");
    if (thumbView->isVisible()) thumbView->scrollToRow(last, "MW::keyScrollEnd");
}

void MW::keyScrollCurrent()
{
    if (G::isLogger) G::log("MW::keyScrollCurrent");
    thumbView->scrollToCurrent("MW::keyScrollCurrent");
    gridView->scrollToCurrent("MW::keyScrollCurrent");
    tableView->scrollToCurrent();
}

void MW::scrollToCurrentRowIfNotVisible()
{
/*
    Called after a sort, when thumbs shrink/enlarge, or a filter change.

    The current image may no longer be visible hence need to scroll to
    the current row.

*/
    if (G::isLogger) G::log("MW::scrollToCurrentRow");
    dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    int sfRow = dm->currentSfRow;
    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    G::wait(100);

    G::ignoreScrollSignal = true;
    if (thumbView->isVisible() && !thumbView->isCellVisible(sfRow))
        thumbView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
    if (gridView->isVisible() && !gridView->isCellVisible(sfRow))
        gridView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
    if (tableView->isVisible() && !tableView->isRowVisible(sfRow))
        tableView->scrollTo(idx,
         QAbstractItemView::ScrollHint::PositionAtCenter);
    G::ignoreScrollSignal = false;

    updateIconRange("MW::scrollToCurrentRow");
}

void MW::jump()
{
    class LineEditDialog : public QDialog {

    public:
        LineEditDialog(QWidget *parent = nullptr) : QDialog(parent) {
            setWindowFlags(windowFlags() | Qt::FramelessWindowHint);       // Set on top of all windows
            QHBoxLayout *layout = new QHBoxLayout(this);
            QFontMetrics fm(this->font());
            QLabel *label = new QLabel;
            label->setText("Jump to row");
            label->setFixedWidth(fm.boundingRect("----Jump to row----").width());
            layout->addWidget(label);
            lineEdit = new QLineEdit(this);
            lineEdit->setFixedWidth(fm.boundingRect("9999999").width());
            layout->addWidget(lineEdit);
            setLayout(layout);
            layout->setSpacing(1);
            int w = label->width() + lineEdit->width() + 20;
            setFixedWidth(w);
        }

        QString text() const {
            return lineEdit->text();
        }

    protected:
        void keyPressEvent(QKeyEvent *event) override {
            if (event->key() == Qt::Key_Return || (event->key() == Qt::Key_Enter)) {
                accept();
            } else if (event->key() == Qt::Key_Escape) {
                reject();
            } else {
                QDialog::keyPressEvent(event);
            }
        }

    private:
        QLineEdit *lineEdit;
    };

    LineEditDialog dialog(this);
    QString srow;
    if(dialog.exec() == QDialog::Accepted) {
        srow = dialog.text();
    }

    bool ok;
    int sfRow = srow.toInt(&ok);
    if (ok) {
        G::fileSelectionChangeSource = "Key_Jump";
        sfRow--;        // IconView is 1 to rowCount
        if (sfRow >= dm->sf->rowCount()) sfRow = dm->sf->rowCount() - 1;
        if (sfRow < 0) sfRow = 0;
        sel->select(sfRow);
    }
}
