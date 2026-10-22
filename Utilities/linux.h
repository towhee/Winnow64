#ifndef LINUX_H
#define LINUX_H

#include <QColor>

/*
    Linux counterparts of the Mac (mac.mm) and Win (win.cpp) platform helpers. Only
    what the shared code needs a Linux value for: memory figures from /proc and the
    title bar colour.
*/

class Linux
{
public:
    static void availableMemory();      // sets G::availableMemoryMB
    static quint64 totalMemoryMB();
    static quint64 processFootprintMB(); // resident set size of this process
    static void useBradientDecoration(); // before the QApplication exists
    static void setTitleBarColor(const QColor &bgColor);
};

#endif // LINUX_H
