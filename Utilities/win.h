#ifndef WIN_H
#define WIN_H

#include <QDebug>

#include <Windows.h>                    // req'd by all
#include <Icm.h>                        // req'd by collectScreensInfo
#include <sysinfoapi.h>                 // req'd by availableMemory
#include <psapi.h>                      // req'd by availableMemory
#include <dwmapi.h>                     // req'd by darkmode (win title bar)

#include "Main/global.h"                // req'd by collectScreensInfo

#pragma comment(lib, "user32.lib")      // req'd by collectScreensInfo
#pragma comment(lib, "Mscms.lib")       // req'd by collectScreensInfo
#pragma comment(lib, "kernel32.lib")    // req'd by availableMemory
#pragma comment(lib, "dwmapi.lib")      // req'd by setTitleBarColor

class Win
{
public:
    static void availableMemory();
    static void collectScreensInfo();
    static void setTitleBarColor(WId hWnd, QColor bgColor = QColor(50,50,50));
};

#endif // WIN_H
