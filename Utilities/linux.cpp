#include "linux.h"
#include "Main/global.h"

#include <QApplication>
#include <QFile>
#include <QWindow>
#include <unistd.h>

namespace {

quint64 meminfoMB(const QByteArray &key)
{
/*
    Returns the /proc/meminfo field named key ("MemTotal", "MemAvailable", ...) in MB,
    or 0 if it cannot be read. The file reports kB.
*/
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    const QByteArray prefix = key + ':';
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (!line.startsWith(prefix)) continue;
        const QList<QByteArray> parts = line.mid(prefix.size()).simplified().split(' ');
        bool ok = false;
        const quint64 kB = parts.isEmpty() ? 0 : parts.first().toULongLong(&ok);
        return ok ? kB / 1024 : 0;
    }
    return 0;
}

} // namespace

void Linux::availableMemory()
{
/*
    MemAvailable is the kernel's estimate of memory available to start new work
    without swapping (free + reclaimable cache), the analog of Mac::availableMemory's
    free + inactive pages.
*/
    if (G::isLogger) G::log("Linux::availableMemory");
    const quint64 mb = meminfoMB("MemAvailable");
    if (mb > 0) G::availableMemoryMB = mb;
}

quint64 Linux::totalMemoryMB()
{
    return meminfoMB("MemTotal");
}

quint64 Linux::processFootprintMB()
{
    // /proc/self/statm: size resident shared ... (in pages)
    QFile f("/proc/self/statm");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    const QList<QByteArray> parts = f.readAll().simplified().split(' ');
    if (parts.size() < 2) return 0;
    bool ok = false;
    const quint64 pages = parts.at(1).toULongLong(&ok);
    if (!ok) return 0;
    const quint64 pageSize = static_cast<quint64>(sysconf(_SC_PAGESIZE));
    return pages * pageSize / (1024ull * 1024ull);
}

void Linux::useBradientDecoration()
{
/*
    On Wayland Qt draws the title bar itself (GNOME has no server-side decorations).
    Its default there, the Adwaita decoration, has fixed colours: white, or #2e2e32 when
    the desktop prefers dark. The bradient decoration paints with the application
    palette's Window colour instead, which setTitleBarColor makes the Winnow background.
    Read once when the QApplication is created, so it is set here, before that. An
    explicit QT_WAYLAND_DECORATION from the user wins. No effect under X11, where the
    window manager draws the title bar.
*/
    if (!qEnvironmentVariableIsSet("QT_WAYLAND_DECORATION"))
        qputenv("QT_WAYLAND_DECORATION", "bradient");
}

void Linux::setTitleBarColor(const QColor &bgColor)
{
/*
    The Linux counterpart of Win::setTitleBarColor, but app-wide: the bradient
    decoration (see useBradientDecoration) paints every window's title bar from the
    application palette, so one call covers the main window and every dialog. Winnow's
    widgets are styled by stylesheet, which overrides the palette, so only the
    unstyled corners see this change.
*/
    QPalette pal = QApplication::palette();
    pal.setColor(QPalette::Window, bgColor);
    pal.setColor(QPalette::WindowText, QColor(190, 190, 190));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
    QApplication::setPalette(pal);
    // a decoration repaints only when it is marked dirty, which a title change does
    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (!w->isVisible()) continue;
        const QString title = w->title();
        w->setTitle(title + " ");
        w->setTitle(title);
    }
}
