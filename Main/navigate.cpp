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

    {
        G::ScrollSignalGuard scrollGuard;   // our own scrolls, not the user's
        if (thumbView->isVisible() && !thumbView->isCellVisible(sfRow))
            thumbView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
        if (gridView->isVisible() && !gridView->isCellVisible(sfRow))
            gridView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
        if (tableView->isVisible() && !tableView->isRowVisible(sfRow))
            tableView->scrollTo(idx,
             QAbstractItemView::ScrollHint::PositionAtCenter);
    }

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

void MW::resyncIconLoaderAfterReorder(const QString &src)
{
/*
    A SORT MOVES EVERY IMAGE AND TELLS THE LOADER NOTHING.

    The icon chunk is a range of PROXY rows, and MetaRead walks proxy rows. A sort keeps
    the same images and the same instance but puts different ones at every position, so
    the chunk that was full a moment ago now holds mostly other images -- and nothing
    restarts the loader for them. A filter change does (it bumps the instance and
    re-initializes MetaRead); a sort never did. scheduleIconRangeSettle does not either:
    it re-dispatches only when the range MOVES, and after a sort the range is usually
    the same numbers over different rows.

    Found with the headless --catalogload probe: the restored Library sort (File Name)
    runs at load completion, after MetaRead has filled rows 0-10,000 in PATH order. By
    name, duplicates from a second folder interleave with the first, so every other cell
    on screen was an image the loader had never been asked for -- blank until a scroll
    happened to restart it.

    So: re-measure the visible window, which re-centres the chunk and recounts what it
    is missing (setIconRange), and if anything is missing, restart the loader there.
    The hydrated catalog scope gets its bulk index prefetch first, as at load, so the
    icons the index holds arrive in one pass rather than a Reader round trip each.

    QUEUED AND COALESCED, like scheduleIconRangeSettle: the sort's own
    scrollToCurrentRowIfNotVisible and any layout it posts must land first, or this
    measures where the view was.
*/
    if (G::isInitializing || dm == nullptr) return;
    if (iconReorderResyncQueued) return;
    iconReorderResyncQueued = true;
    QTimer::singleShot(0, this, [this, src]{
        iconReorderResyncQueued = false;
        if (G::isInitializing || G::stop || dm == nullptr || dm->abort) return;
        if (dm->sf->rowCount() == 0) return;
        if (G::isLogger || G::isFlowLogger)
            G::log("MW::resyncIconLoaderAfterReorder", src);
        updateIconRange(src + " reorder");
        if (G::iconChunkLoaded) return;
        const bool hydrated = dm->scopeRequest().scope == G::Scope::Catalog
                              && !dm->scopeRequest().rows.isEmpty();
        if (hydrated) prefetchIconsFromIndex(src + " reorder");
        reloadIconChunk();          // flushProxySnapshot + queued MetaRead::setStartRow
    });
}

void MW::startLibraryWhenExposed(int attempt)
{
/*
    Reopen the Library at start, AFTER the window is on screen. showEvent put up the
    message and the cover; this waits for the native window to be exposed (polling a
    zero/20 ms timer rather than trusting that a zero timer lands after the first paint,
    which the window system does not promise), paints the cover once synchronously so
    the message is on screen, and only then starts the load -- which blocks the GUI
    thread for the layout switch and the query dispatch. Bounded: after ~2 s it starts
    anyway.
*/
    QTimer::singleShot(attempt == 0 ? 0 : 20, this, [this, attempt]{
        const bool exposed = windowHandle() && windowHandle()->isExposed();
        if (!exposed && attempt < 100) { startLibraryWhenExposed(attempt + 1); return; }
        timelineMark("start Library (window exposed)");
        if (centralCurtain && loadCurtainUp) centralCurtain->repaint();
        if (restoreLibraryState) queueLibraryStateRestore();
        chooseSource(true, "MW::showEvent reopen Library");
        /* chooseSource applied Browse's Library layout, collapse state included; the
           sides the session left collapsed are the ones the user expects back. */
        applySessionAreaCollapse();
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
