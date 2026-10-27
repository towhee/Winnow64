#ifndef SCREENFIT_H
#define SCREENFIT_H

#include <QObject>
#include <QWidget>

/*
    Keeps every Winnow window -- the main window and every dialog -- inside the usable
    area of its screen (QScreen::availableGeometry: the screen less the Windows taskbar,
    the macOS menu bar and Dock, the Linux panels).

    Qt does not: a restored geometry saved on a larger or differently scaled screen, a
    dialog whose layout is taller than a small or high-scaling screen (1080p at 150% is
    ~690 logical pixels of usable height), or a dialog centred on a parent that is itself
    partly under the taskbar all open with their bottom edge, and so their buttons,
    under the taskbar.

    install() puts an application event filter in place that fits each window as it is
    shown (and again once the show has settled, because MW::showEvent restores the main
    window's geometry inside the show). No dialog has to remember to do it. fit() is
    public for code that changes a visible window's geometry without a Show
    (workspaces).

    A window that is already wholly on usable screen area is left alone, so a window
    deliberately spanning two monitors stays as it is. Otherwise it is shrunk to its
    screen's usable area (never below its minimum size) and moved inside it, title bar
    first. Maximised, full screen and minimised windows, menus, tooltips, splash screens
    and frameless overlays are skipped.
*/

class ScreenFit : public QObject
{
public:
    static void install();
    static void fit(QWidget *w);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit ScreenFit(QObject *parent) : QObject(parent) {}
};

#endif // SCREENFIT_H
