#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QObject>
#include <QColor>

class QWidget;

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

#endif // TITLEBAR_H
