#include "Main/mainwindow.h"

void MW::slideShow()
{
    if (G::isLogger) G::log("MW::slideShow");
    if (G::isSlideShow) {
        // stop slideshow
        G::popup->showPopup("Slideshow has been terminated.", 2000);
        G::isSlideShow = false;
        slideCount = 0;
        G::useImageCache = prevUseImageCache;
        QApplication::restoreOverrideCursor();
        imageView->setCursor(Qt::ArrowCursor);
        slideShowStatusLabel->setText("");
        updateStatus(true, "", "MW::slideShow");
        updateStatusBar();
        slideShowAction->setText(tr("Slide Show"));
        slideShowTimer->stop();
        delete slideShowTimer;
        progress->setSuppressed(false);   // end slideshow: allow progress to show again
        // change to ImageCache
        if (G::useImageCache)
            imageCache->setCurrentPosition(dm->currentFilePath, "MW::slideShow");
        // enable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WindowShortcut);
        // started from the Slide Show module: back to the workflow it was entered from
        leaveSlideShowWorkflow();
    }
    else {
        // start slideshow
        slideCount = 0;
        prevUseImageCache = G::useImageCache;
        QApplication::setOverrideCursor(Qt::BlankCursor);
        imageView->setCursor(Qt::BlankCursor);
        G::isSlideShow = true;
//        isSlideshowPaused = false;
        updateStatusBar();
        QString msg = "<h2>Press <font color=\"red\"><b>Esc</b></font> to exit slideshow</h2><p>";
        msg += "Press <font color=\"red\"><b>H</b></font> during slideshow for tips"
                      "<p>Starting slideshow";
        msg += "<p>Current settings:<p>";
        msg += "Interval = " + QString::number(slideShowDelay) + " second(s)";
        if (isSlideShowRandom)  msg += "<br>Random selection";
        else msg += "<br>Sequential selection";
        if (isSlideShowWrap) msg += "<br>Wrap at end of slides";
        else msg += "<br>Stop at end of slides";
        msg += "<p>Press <font color=\"red\"><b>SpaceBar</b></font> to start slideshow.";

        G::popup->showPopup(msg, 3000, true, 0.75, Qt::AlignLeft);

        // No image caching if random slide show
        if (isSlideShowRandom) G::useImageCache = false;
        else G::useImageCache = true;

        // disable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WidgetShortcut);

        if (G::isStressTest) getSubfolders("/users/roryhill/pictures");

        slideShowAction->setText(tr("Stop Slide Show"));
        slideShowTimer = new QTimer(this);
        connect(slideShowTimer, SIGNAL(timeout()), this, SLOT(nextSlide()));
        nextSlide();
        slideShowTimer->start(slideShowDelay * 1000);
    }
}

void MW::nextSlide()
{
    if (G::isLogger) G::log("MW::nextSlide");
    slideCount++;
    if (isSlideShowRandom) {
        // push previous image path onto the slideshow history stack
        int row = thumbView->currentIndex().row();
        QString fPath = dm->sf->index(row, 0).data(G::PathRole).toString();
        slideshowRandomHistoryStack->push(fPath);
        sel->random();
    }
    else {
        if (dm->currentSfRow == dm->sf->rowCount() - 1) {
            if (isSlideShowWrap) sel->first();
            else slideShow();
        }
        else sel->next();
    }

    QString msg = "  Slideshow count:"+ QString::number(slideCount) +
            "  (<font color=\"red\">press H for slideshow shortcuts</font>)";
    updateStatus(true, msg, "MW::nextSlide");

}

void MW::prevRandomSlide()
{
    if (G::isLogger) G::log("MW::prevRandomSlide");
    if (slideshowRandomHistoryStack->isEmpty()) {
        G::popup->showPopup("End of random slide history");
        return;
    }
//    isSlideshowPaused = true;
    QString prevPath = slideshowRandomHistoryStack->pop();
    sel->setCurrentPath(prevPath);
    updateStatus(false,
                 "Slideshow random history."
                 "  Press <font color=\"white\"><b>Spacebar</b></font> to continue slideshow, "
                 "press <font color=\"white\"><b>Esc</b></font> to quit slideshow."
                 , "MW::prevRandomSlide");
    // hide popup if showing
    G::popup->reset();
}

void MW::slideShowResetDelay()
{
    if (G::isLogger) G::log("MW::slideShowResetDelay");
    slideShowTimer->setInterval(slideShowDelay * 1000);
}

void MW::slideShowResetSequence()
{
/*
    Called from MW::keyReleaseEvent when R is pressed and isSlideShow == true.
    The slideshow is toggled between sequential and random progress.
*/
    if (G::isLogger) G::log("MW::slideShowResetSequence");
    QString msg = "Setting slideshow progress to ";
    if (isSlideShowRandom) {
        msg += "random";
        progress->setSuppressed(true);    // no caching in random mode: hide progress
    }
    else {
        msg = msg + "sequential";
        progress->setSuppressed(false);   // let progress show per its row content
    }
    G::popup->showPopup(msg);
}

void MW::slideshowHelpMsg()
{
    if (G::isLogger) G::log("MW::slideshowHelpMsg");
    QString selection;
    if (isSlideShowRandom)  selection = "Random selection";
    else selection = "Sequential selection";
    QString wrap;
    if (isSlideShowWrap)  wrap = "Wrap at end of slides";
    else wrap = "Stop at end of slides";
    QString msg =
        "<p><b>Slideshow Shortcuts:</b><br/></p>"
        "<table border=\"0\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px;\" cellspacing=\"2\" cellpadding=\"0\">"
        "<tr><td width=\"120\"><font color=\"red\"><b>Esc</b></font></td><td>Exit slideshow</td></tr>"
        "<tr><td><font color=\"red\"><b>  S       </b></font></td><td>Exit slideshow</td></tr>"
        "<tr><td><font color=\"red\"><b>  W       </b></font></td><td>Toggle wrapping on and off</td></tr>"
        "<tr><td><font color=\"red\"><b>  R       </b></font></td><td>Toggle random vs sequential slide selection</td></tr>"
        //"<tr><td><font color=\"red\"><b>Backspace </b></font></td><td>Go back to a previous random slide</td></tr>"
        "<tr><td><font color=\"red\"><b>Spacebar  </b></font></td><td>Pause/Continue slideshow</td></tr>"
        "<tr><td><font color=\"red\"><b>  H       </b></font></td><td>Show this popup message</td></tr>"
        "</table>"
        "<p>Change the slideshow interval.  Go to preferences to set other interval.</p>"
        "<table border=\"0\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px;\" cellspacing=\"2\" cellpadding=\"0\">"
        "<tr><td width=\"120\"><font color=\"red\"><b>1</b></font></td><td>1 second</td></tr>"
        "<tr><td><font color=\"red\"><b>  2  </b></font></td><td>2 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  3  </b></font></td><td>3 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  4  </b></font></td><td>5 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  5  </b></font></td><td>10 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  6  </b></font></td><td>30 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  7  </b></font></td><td>1 minute</td></tr>"
        "<tr><td><font color=\"red\"><b>  8  </b></font></td><td>3 minutes</td></tr>"
        "<tr><td><font color=\"red\"><b>  9  </b></font></td><td>10 minutes</td></tr>"
        "</table>"
        "<p>Current settings:<p>"
        "<ul style=\"line-height:50%; list-style-type:none;\""
        "<li>Interval  = "  + QString::number(slideShowDelay) + " seconds</li>"
        "<li>Selection = " + selection + "</li>"
        "<li>Wrap      = " + wrap + "</li>"
        "</ul><p><p>"
        "Press <font color=\"red\"><b>Space Bar</b></font> continue slideshow and close this message";
    G::popup->showPopup(msg, 0, true, 1.0, Qt::AlignLeft);
}


/* ---------------------------------------------------------------------------
   The in-slideshow keys.

   One slot per key, so MW::keyReleaseEvent and the View > Slide Show menu items run the
   same code instead of two copies of it.  Each is a no-op unless a slideshow is running:
   slideShowTimer is deleted (and not nulled) when the slideshow ends, so G::isSlideShow
   has to be tested before the timer is touched.
   --------------------------------------------------------------------------- */

void MW::slideShowNext()
{
    if (G::isLogger) G::log("MW::slideShowNext");
    if (!G::isSlideShow || !slideShowTimer->isActive()) return;
    nextSlide();
}

void MW::slideShowPrevRandom()
{
    if (G::isLogger) G::log("MW::slideShowPrevRandom");
    if (!G::isSlideShow || !slideShowTimer->isActive()) return;
    prevRandomSlide();
}

void MW::slideShowPauseOrContinue()
{
/*
    Space pauses a running slideshow and resumes a paused one.  The paused branch also
    clears the help popup, the way the key handler does when it wakes a paused slideshow.
*/
    if (G::isLogger) G::log("MW::slideShowPauseOrContinue");
    if (!G::isSlideShow) return;
    if (slideShowTimer->isActive()) {
        slideShowTimer->stop();
        G::popup->showPopup("Slideshow is paused", 0);
    }
    else {
        if (isSlideShowHelpVisible) {
            G::popup->reset();
            isSlideShowHelpVisible = false;
        }
        G::popup->showPopup("Slideshow is active");
        nextSlide();
        slideShowTimer->start(slideShowDelay * 1000);
    }
}

void MW::slideShowToggleWrap()
{
    if (G::isLogger) G::log("MW::slideShowToggleWrap");
    if (!G::isSlideShow || !slideShowTimer->isActive()) return;
    isSlideShowWrap = !isSlideShowWrap;
    isSlideShowHelpVisible = true;
    QString msg;
    if (isSlideShowWrap) msg = "Slide wrapping is on.";
    else msg = "Slide wrapping is off.";
    G::popup->showPopup(msg);
}

void MW::slideShowToggleRandom()
{
    if (G::isLogger) G::log("MW::slideShowToggleRandom");
    if (!G::isSlideShow || !slideShowTimer->isActive()) return;
    isSlideShowRandom = !isSlideShowRandom;
    slideShowResetSequence();
    QString msg;
    if (isSlideShowRandom) msg = "Random selection enabled.";
    else msg = "Sequential selection enabled.";
    G::popup->showPopup(msg);
}

void MW::slideShowKeysHelp()
{
    if (G::isLogger) G::log("MW::slideShowKeysHelp");
    if (!G::isSlideShow || !slideShowTimer->isActive()) return;
    slideShowTimer->stop();
    slideshowHelpMsg();
}

void MW::slideShowSetDelayFromAction(QAction *action)
{
/*
    The interval actions carry their seconds in QAction::data, so the same slot serves all
    nine.  Keys 1-9 reach it through MW::keyReleaseEvent.
*/
    if (G::isLogger) G::log("MW::slideShowSetDelayFromAction");
    if (action == nullptr) return;
    if (!G::isSlideShow || !slideShowTimer->isActive()) return;
    slideShowDelay = action->data().toInt();
    slideShowResetDelay();
    QString msg = "Slideshow interval set to " + QString::number(slideShowDelay) + " seconds.";
    G::popup->showPopup(msg);
}

void MW::syncSlideShowMenuEnabled()
{
/*
    Called as View > Slide Show opens.  Everything below Start / Stop only works while a
    slideshow is running -- outside one those keys belong to other actions entirely (X is
    Reject, W is New Workspace, R is unbound, Space is zoom toggle) -- so an enabled item
    would advertise a shortcut that does something else.  Same reasoning, and the same
    aboutToShow treatment, as MW::syncDevelopMenuEnabled.
*/
    if (G::isLogger) G::log("MW::syncSlideShowMenuEnabled");
    const bool running = G::isSlideShow;

    const QList<QAction *> runOnly {
        slideShowNextAction, slideShowPrevRandomAction, slideShowPauseAction,
        slideShowWrapAction, slideShowRandomAction, slideShowKeysAction
    };
    for (QAction *a : runOnly) if (a) a->setEnabled(running);
    if (slideShowIntervalMenu) slideShowIntervalMenu->setEnabled(running);

    // Reflect live state for the toggles
    if (slideShowWrapAction) {
        QSignalBlocker blocker(slideShowWrapAction);
        slideShowWrapAction->setChecked(isSlideShowWrap);
    }
    if (slideShowRandomAction) {
        QSignalBlocker blocker(slideShowRandomAction);
        slideShowRandomAction->setChecked(isSlideShowRandom);
    }
    for (QAction *a : slideShowIntervalActions) {
        QSignalBlocker blocker(a);
        a->setChecked(a->data().toInt() == slideShowDelay);
    }
}
