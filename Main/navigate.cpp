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
    // qDebug() << "\nKEY HOME\n";
    if (G::isLogger) G::log("MW::keyHome");
    if (G::isInitializing) return;
    if (G::mode == "Compare") compareImages->go("Home");
    sel->first(qApp->keyboardModifiers());
}

void MW::keyEnd()
{
/*

*/
    // qDebug() << "\nKEY END\n";
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

    /*  NO NESTED EVENT LOOP HERE. This was G::wait(100), which is not a sleep: it runs a
        nested event loop, so it cost whatever was queued when it was reached rather than
        the 100 ms asked for -- 3,710 ms after a filter invalidate, 123 ms once the
        accessibility rebuilds behind that backlog were suspended (see G::A11ySuspend) --
        and it re-entered the GUI in the middle of a filter change.

        THE DEFERRED LAYOUT DOES NOT NEED IT. After an invalidate QAbstractItemView posts
        its layout to a timer, but every reader below forces it synchronously already:
        QListView::visualRect goes through rectForIndex and indexAt through
        intersectingSet, both of which call executePostedLayout(); QHeaderView does the
        same in its section lookups, which is what TableView::updateVisible reads through
        rowAt(). The one reader that does NOT self-flush is TableView::isRowVisible, which
        tests a cached firstVisibleRow/lastVisibleRow window refreshed from resize and
        scroll events -- so refresh it here rather than pumping events until one arrives.

        WHAT THE PUMP ALSO DID, AND WHAT REPLACES IT. It let the queued tail of the caller
        run first -- the deferred layouts and the resize/rejustify they trigger -- so the
        visible window updateIconRange measures below was taken over a settled view.
        scheduleIconRangeSettle re-measures it ONCE when the queue turns, coalesced, and
        re-dispatches only if the range actually moved: the same shape as
        scheduleVisibleEmit and scheduleCompileFilters, with none of the re-entrancy. */
    if (tableView->isVisible())
        tableView->updateVisible("MW::scrollToCurrentRowIfNotVisible");

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
    scheduleIconRangeSettle("MW::scrollToCurrentRow");
}

void MW::scheduleIconRangeSettle(const QString &src)
{
/*
    Re-measure the visible window once, after the event queue has run.

    The views defer work that MOVES CELLS: a layout posted by an invalidate, and the
    resize (and rejustify) that showing or hiding a scrollbar causes, each of which calls
    MW::updateIconRange and nothing else. Measuring the window before those have run can
    leave the icon chunk centred on where the view was rather than where it ended up --
    and the chunk is what the loader reads. The old answer was to run a nested event loop
    until they had happened; this one asks the same question again on the other side of
    the queue.

    COALESCED AND CHEAP: one pending settle at a time, a zero timer (which Qt runs at the
    lowest priority, so it lands after the posted layouts and resizes it is waiting for),
    and a re-dispatch ONLY if the range actually moved -- in the ordinary case the second
    measurement agrees with the first and this costs one updateIconRange.
*/
    if (G::isInitializing || dm == nullptr) return;
    if (iconRangeSettleQueued) return;
    iconRangeSettleQueued = true;
    QTimer::singleShot(0, this, [this, src]{
        iconRangeSettleQueued = false;
        if (G::isInitializing || G::stop || dm == nullptr) return;
        const int before1 = dm->startIconRange;
        const int before2 = dm->endIconRange;
        updateIconRange(src + " settle");
        if (dm->startIconRange != before1 || dm->endIconRange != before2)
            reloadIconChunk();      // flushProxySnapshot + queued MetaRead::setStartRow
    });
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
            if (G::isEnterKey(event)) {
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
