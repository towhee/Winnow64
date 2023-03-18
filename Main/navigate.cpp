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
        sel->current(compareImages->go("Right"));
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
        sel->current(compareImages->go("Left"));
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
    if (G::isNewFolderLoaded /*&& !G::isInitializing*/) {
        if (G::mode == "Compare") compareImages->go("Home");
        else sel->first();
    }
}

void MW::keyEnd()
{
/*

*/
    if (G::isLogger) G::log("MW::keyEnd");
    if (!G::isLinearLoading) {
        qDebug() << "MW::keyEnd";
        if (G::mode == "Compare") compareImages->go("End");
        G::ignoreScrollSignal = true;
        sel->last();
        return;
    }
    if (G::isNewFolderLoaded /*&& !G::isInitializing*/) {
        if (G::isLinearLoading) metadataCacheThread->stop();
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

