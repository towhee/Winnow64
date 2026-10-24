#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QObject>
#include <QColor>
#include <QWidget>

/*
    Paints the title bar of every Winnow window, the main window and every dialog, in
    the Winnow background (G::backgroundColor) on all three platforms. Call setColor
    when the background is first known and again whenever it changes.

    Linux: app-wide already (Linux::setTitleBarColor sets the palette the bradient
    decoration paints from).
    Windows and macOS: the colour is a property of each native window
    (Win::setTitleBarColor, Mac::setTitleBarColor), so setColor recolours the windows
    that are open and an application event filter colours every window that opens
    later, as it is shown. No dialog has to remember to do it.
*/

class TitleBar : public QObject
{
public:
    static void setColor(const QColor &bgColor);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit TitleBar(QObject *parent) : QObject(parent) {}
    static void apply(QWidget *w);
    static QColor color;
};

/*
    A strip of widgets IN the title bar of a top-level window: the window's client area
    is extended under the title bar (Qt::ExpandedClientAreaHint) and the strip is laid
    over the part the system reports as covered (QWindow::safeAreaMargins().top()). The
    window's own layout already keeps clear of it (top levels respect the safe area), so
    nothing else moves. The title text is hidden; windowTitle() still names the window
    in the Window menu, Mission Control and the Dock.

    THREE PIECES, placed by hand (no layout): left starts after the window buttons,
    centre is centred on the WINDOW (not on the space between the others) and right
    ends at the right edge; centre gives way rather than overlap a narrow window. The
    bar is as tall as the system's, or taller if the content needs it: then the
    window's top contents margin grows to match, so the panels move down with it.

    Empty strip is still title bar: a press drags the window and a double click does
    what the system setting says (zoom or minimise). Children that take the mouse
    (buttons) keep it. Hidden when the window has no title bar to sit in (full screen).

    macOS only so far (isSupported). Windows has the hint from Qt 6.9 but draws its own
    caption over the strip, and on Linux the window manager owns the title bar, so
    there the caller keeps the plain title.
*/

class TitleBarStrip : public QWidget
{
public:
    explicit TitleBarStrip(QWidget *window);
    static bool isSupported();
    void setContent(QWidget *left, QWidget *centre, QWidget *right);
    void relayout() { place(); }     // after the content's size changes (restyle)

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void place();
    QWidget *left = nullptr;
    QWidget *centre = nullptr;
    QWidget *right = nullptr;
};

#endif // TITLEBAR_H
