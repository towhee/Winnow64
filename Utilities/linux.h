#ifndef LINUX_H
#define LINUX_H

#include <QtGlobal>

/*
    Linux counterparts of the Mac (mac.mm) and Win (win.cpp) platform helpers. Only
    what the shared code needs a Linux value for: memory figures from /proc.
*/

class Linux
{
public:
    static void availableMemory();      // sets G::availableMemoryMB
    static quint64 totalMemoryMB();
    static quint64 processFootprintMB(); // resident set size of this process
};

#endif // LINUX_H
