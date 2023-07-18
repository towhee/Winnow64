#include "Main/mainwindow.h"

void MW::enableGoKeyActions(bool ok)
{
    if (G::isLogger) G::log("MW::enableGoKeyActions");
    if (ok) {
        keyRightAction->setEnabled(true);
        keyLeftAction->setEnabled(true);
        keyUpAction->setEnabled(true);
        keyDownAction->setEnabled(true);
        keyHomeAction->setEnabled(true);
        keyEndAction->setEnabled(true);
        keyPageUpAction->setEnabled(true);
        keyPageDownAction->setEnabled(true);
    }
    else {
        keyRightAction->setEnabled(false);
        keyLeftAction->setEnabled(false);
        keyUpAction->setEnabled(false);
        keyDownAction->setEnabled(false);
        keyHomeAction->setEnabled(false);
        keyEndAction->setEnabled(false);
        keyPageUpAction->setEnabled(false);
        keyPageDownAction->setEnabled(false);
    }
}

void MW::keyRight()
{
    if (G::isLogger || G::isFlowLogger) {
        G::log("MW::keyRight", "ROW: " + QString::number(dm->currentSfRow));
    }
    if (G::mode == "Compare") {
        sel->currentIndex(compareImages->go("Right"));
    }
    if (G::mode == "Loupe" || G::mode == "Table" || G::mode == "Grid") {
        sel->next();
    }
}

void MW::keyLeft()
{
    if (G::isLogger || G::isFlowLogger) {
        G::log("MW::keyLeft", "ROW: " + QString::number(dm->currentSfRow));
    }
    if (G::mode == "Compare") {
        sel->currentIndex(compareImages->go("Left"));
    }
    if (G::mode == "Loupe" || G::mode == "Table" || G::mode == "Grid") {
        sel->prev();
    }
}

void MW::keyUp()
{
    if (G::isLogger) G::log("MW::keyUp");
    if (G::mode == "Loupe") sel->up();
    if (G::mode == "Table") sel->up();
    if (G::mode == "Grid") sel->up();
}

void MW::keyDown()
{
    if (G::isLogger) G::log("MW::keyDown");
    if (G::mode == "Loupe") sel->down();
    if (G::mode == "Table") sel->down();
    if (G::mode == "Grid") sel->down();
}

void MW::keyPageUp()
{
    if (G::isLogger) G::log("MW::keyPageUp");
    if (G::mode == "Loupe") sel->nextPage();
    if (G::mode == "Table") sel->nextPage();
    if (G::mode == "Grid") sel->nextPage();
}

void MW::keyPageDown()
{
    if (G::isLogger) G::log("MW::keyPageDown");
    if (G::mode == "Loupe") sel->prevPage();
    if (G::mode == "Table") sel->prevPage();
    if (G::mode == "Grid") sel->prevPage();
}

void MW::keyHome()
{
/*

*/
    if (G::isLogger) G::log("MW::keyHome");
    if (G::isInitializing) return;
    if (G::isLoadLinear) {
        if (G::isLinearLoadDone) {
            metadataCacheThread->stop();
            if (G::mode == "Compare") compareImages->go("Home");
            else sel->first();
        }
    }
    else {
//        G::ignoreScrollSignal = true;
        if (G::mode == "Compare") compareImages->go("Home");
        sel->first();
    }
}

void MW::keyEnd()
{
/*

*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::keyEnd");
    if (G::isInitializing) return;
    if (G::isLoadLinear) {
        if (G::isLinearLoadDone) {
            metadataCacheThread->stop();
            if (G::mode == "Compare") compareImages->go("End");
            else sel->last();
        }
    }
    else {
//        G::ignoreScrollSignal = true;
        if (G::mode == "Compare") compareImages->go("End");
        else sel->last();
    }
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

void MW::scrollToCurrentRow()
{
/*
    Called after a sort or when thumbs shrink/enlarge. The current image may no longer be
    visible hence need to scroll to the current row.
*/
    if (G::isLogger) G::log("MW::scrollToCurrentRow");
    qDebug() << "MW::scrollToCurrentRow";
    dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
//    G::wait(100);

    G::ignoreScrollSignal = true;
    if (thumbView->isVisible()) thumbView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
    if (gridView->isVisible()) gridView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
    if (tableView->isVisible()) tableView->scrollTo(idx,
         QAbstractItemView::ScrollHint::PositionAtCenter);
    G::ignoreScrollSignal = false;

    updateIconRange(dm->currentSfRow, "MW::scrollToCurrentRow");
    metadataCacheThread->scrollChange("MW::scrollToCurrentRow");
}

