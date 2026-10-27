// Unit tests for Utilities/screenfit.h -- keeping windows inside the usable screen area
// (QScreen::availableGeometry), so nothing opens under the Windows taskbar.

#include <QtTest>
#include <QDialog>
#include <QScreen>
#include "Utilities/screenfit.h"

class TstScreenFit : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void insideLeftAlone();
    void bottomOverhangMovedUp();
    void rightOverhangMovedLeft();
    void tooLargeShrunk();
    void minimumSizeKeepsTitleBar();
    void framelessLeftAlone();
    void maximisedLeftAlone();
    void showFitsAfterInstall();

private:
    QRect avail() const { return dlg->screen()->availableGeometry(); }
    void show(const QRect &r);
    QDialog *dlg = nullptr;
};

void TstScreenFit::init()
{
    dlg = new QDialog;
}

void TstScreenFit::cleanup()
{
    delete dlg;
    dlg = nullptr;
}

void TstScreenFit::show(const QRect &r)
{
    dlg->setGeometry(r);
    dlg->show();
    QVERIFY(QTest::qWaitForWindowExposed(dlg));
}

void TstScreenFit::insideLeftAlone()
{
    const QRect a = avail();
    const QRect r(a.left() + 50, a.top() + 50, a.width() / 3, a.height() / 3);
    show(r);
    const QRect before = dlg->geometry();
    ScreenFit::fit(dlg);
    QCOMPARE(dlg->geometry(), before);
}

void TstScreenFit::bottomOverhangMovedUp()
{
    const QRect a = avail();
    const QSize s(a.width() / 3, a.height() / 3);
    show(QRect(QPoint(a.left() + 40, a.bottom() - s.height() / 2), s));
    ScreenFit::fit(dlg);
    QVERIFY2(a.contains(dlg->frameGeometry()), "frame not inside the usable area");
    QCOMPARE(dlg->size(), s);                       // moved, not resized
    QCOMPARE(dlg->frameGeometry().bottom(), a.bottom());
}

void TstScreenFit::rightOverhangMovedLeft()
{
    const QRect a = avail();
    const QSize s(a.width() / 3, a.height() / 3);
    show(QRect(QPoint(a.right() - s.width() / 2, a.top() + 40), s));
    ScreenFit::fit(dlg);
    QVERIFY(a.contains(dlg->frameGeometry()));
    QCOMPARE(dlg->size(), s);
}

void TstScreenFit::tooLargeShrunk()
{
    const QRect a = avail();
    show(QRect(a.topLeft() + QPoint(20, 20), a.size() + QSize(300, 300)));
    ScreenFit::fit(dlg);
    QVERIFY(a.contains(dlg->frameGeometry()));
}

void TstScreenFit::minimumSizeKeepsTitleBar()
{
    const QRect a = avail();
    dlg->setMinimumSize(a.width() / 2, a.height() + 200);
    show(QRect(QPoint(a.left() + 20, a.top() + 100), dlg->minimumSize()));
    ScreenFit::fit(dlg);
    QCOMPARE(dlg->height(), a.height() + 200);       // never below the minimum
    QCOMPARE(dlg->frameGeometry().top(), a.top());   // title bar reachable
}

void TstScreenFit::framelessLeftAlone()
{
    dlg->setWindowFlags(dlg->windowFlags() | Qt::FramelessWindowHint);
    const QRect a = avail();
    show(QRect(QPoint(a.left() + 40, a.bottom() - 50), QSize(200, 200)));
    const QRect before = dlg->geometry();
    ScreenFit::fit(dlg);
    QCOMPARE(dlg->geometry(), before);
}

void TstScreenFit::maximisedLeftAlone()
{
    dlg->showMaximized();
    QVERIFY(QTest::qWaitForWindowExposed(dlg));
    const QRect before = dlg->geometry();
    ScreenFit::fit(dlg);
    QCOMPARE(dlg->geometry(), before);
}

void TstScreenFit::showFitsAfterInstall()
{
    /* The application filter: no explicit fit() call. install() declines the offscreen
       platform (headless app runs), so this only runs on a real display. */
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("ScreenFit is not installed on the offscreen platform");
    ScreenFit::install();
    const QRect a = avail();
    const QSize s(a.width() / 3, a.height() / 3);
    dlg->resize(s);
    dlg->move(a.left() + 40, a.bottom() - s.height() / 3);
    dlg->show();
    QVERIFY(QTest::qWaitForWindowExposed(dlg));
    QTRY_VERIFY(a.contains(dlg->frameGeometry()));
}

QTEST_MAIN(TstScreenFit)
#include "tst_screenfit.moc"
