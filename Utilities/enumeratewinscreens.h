#ifndef ENUMERATEWINSCREENS_H
#define ENUMERATEWINSCREENS_H

#include <QDebug>

#include <Windows.h>
#include <Icm.h>

#include <strsafe.h>
#include <iostream>

#include "Main/global.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Mscms.lib")

class EnumerateWinScreens
{
public:
    static void collectScreensInfo();
};

#endif // ENUMERATEWINSCREENS_H
