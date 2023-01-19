#include "Main/mainwindow.h"

void MW::slideShow()
{
    if (G::isLogger) G::log("MW::slideShow");
    if (G::isSlideShow) {
        // stop slideshow
        G::popUp->showPopup("Slideshow has been terminated.", 2000);
        G::isSlideShow = false;
        slideCount = 0;
        // not finisheduseImageCache = true;
        imageView->setCursor(Qt::ArrowCursor);
        slideShowStatusLabel->setText("");
        updateStatus(true, "", "MW::slideShow");
        updateStatusBar();
        slideShowAction->setText(tr("Slide Show"));
        slideShowTimer->stop();
        delete slideShowTimer;
        cacheProgressBar->setVisible(true);
        // change to ImageCache
        if (G::useImageCache)
            imageCacheThread->setCurrentPosition(dm->currentFilePath, "MW::slideShow");
        // enable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WindowShortcut);
    }
    else {
        // start slideshow
        slideCount = 0;
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

        G::popUp->showPopup(msg, 3000, true, 0.75, Qt::AlignLeft);

        // No image caching if random slide show
        if (isSlideShowRandom) G::useImageCache = false;
        else G::useImageCache = true;

        // disable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WidgetShortcut);

        if (isStressTest) getSubfolders("/users/roryhill/pictures");

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

    QString msg = "  Slide # "+ QString::number(slideCount) +
            "  (<font color=\"red\">press H for slideshow shortcuts</font>)";
    updateStatus(true, msg, "MW::nextSlide");

}

void MW::prevRandomSlide()
{
    if (G::isLogger) G::log("MW::prevRandomSlide");
    if (slideshowRandomHistoryStack->isEmpty()) {
        G::popUp->showPopup("End of random slide history");
        return;
    }
//    isSlideshowPaused = true;
    QString prevPath = slideshowRandomHistoryStack->pop();
    sel->select(prevPath);
    updateStatus(false,
                 "Slideshow random history."
                 "  Press <font color=\"white\"><b>Spacebar</b></font> to continue slideshow, "
                 "press <font color=\"white\"><b>Esc</b></font> to quit slideshow."
                 , "MW::prevRandomSlide");
    // hide popup if showing
    G::popUp->end();
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
        progressLabel->setVisible(false);
    }
    else {
        msg = msg + "sequential";
        progressLabel->setVisible(true);
    }
    G::popUp->showPopup(msg);
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
        "<tr><td><font color=\"red\"><b>Backspace </b></font></td><td>Go back to a previous random slide</td></tr>"
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
    G::popUp->showPopup(msg, 0, true, 1.0, Qt::AlignLeft);
}

