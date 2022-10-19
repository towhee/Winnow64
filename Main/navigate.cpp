#include "Main/mainwindow.h"

void MW::enableGoKeyActions(bool ok)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
/*

*/
    if (G::isLogger || G::isFlowLogger) {
        qDebug() << " ";
        G::log(CLASSFUNCTION, "ROW: " + QString::number(dm->currentSfRow));
    }
    if (G::mode == "Compare") compareImages->go("Right");
    if (G::mode == "Loupe") thumbView->selectNext();
    if (G::mode == "Table") thumbView->selectNext();
    if (G::mode == "Grid") gridView->selectNext();
}

void MW::keyLeft()
{
/*

*/
    if (G::isLogger || G::isFlowLogger) {
//        qDebug() << " ";
        G::log();   // show blank line
        G::log(CLASSFUNCTION, "ROW: " + QString::number(dm->currentSfRow));
    }
    if (G::mode == "Compare") compareImages->go("Left");
    if (G::mode == "Loupe") thumbView->selectPrev();
    if (G::mode == "Table") thumbView->selectPrev();
    if (G::mode == "Grid") gridView->selectPrev();
}

void MW::keyUp()
{
/*

*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Loupe") thumbView->selectUp();
    if (G::mode == "Table") thumbView->selectUp();
    if (G::mode == "Grid") gridView->selectUp();
}

void MW::keyDown()
{
/*

*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Loupe") thumbView->selectDown();
    if (G::mode == "Table") thumbView->selectDown();
    if (G::mode == "Grid") gridView->selectDown();
}

void MW::keyPageUp()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Loupe") thumbView->selectPageUp();
    if (G::mode == "Table") tableView->selectPageUp();
    if (G::mode == "Grid") gridView->selectPageUp();
}

void MW::keyPageDown()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Loupe") thumbView->selectPageDown();
    if (G::mode == "Table") tableView->selectPageDown();
    if (G::mode == "Grid") gridView->selectPageDown();
}

void MW::keyHome()
{
/*

*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isNewFolderLoaded /*&& !G::isInitializing*/) {
        if (G::mode == "Compare") compareImages->go("Home");
        if (G::mode == "Grid") gridView->selectFirst();
        else {
            thumbView->selectFirst();
        }
    }
}

void MW::keyEnd()
{
/*

*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isNewFolderLoaded /*&& !G::isInitializing*/) {
        metadataCacheThread->stop();
        if (G::mode == "Compare") compareImages->go("End");
        if (G::mode == "Grid") gridView->selectLast();
        else {
            thumbView->selectLast();
        }
    }
}

void MW::keyScrollDown()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Grid") gridView->scrollDown(0);
    if (thumbView->isVisible()) thumbView->scrollDown(0);
}

void MW::keyScrollUp()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Grid") gridView->scrollUp(0);
    if (thumbView->isVisible()) thumbView->scrollUp(0);
}

void MW::keyScrollPageDown()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Grid") gridView->scrollPageDown(0);
    if (thumbView->isVisible()) thumbView->scrollPageDown(0);
}

void MW::keyScrollPageUp()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Grid") gridView->scrollPageUp(0);
    if (thumbView->isVisible()) thumbView->scrollPageUp(0);
}

