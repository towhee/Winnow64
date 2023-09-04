#ifndef USB_H
#define USB_H

#include <QtWidgets>

#if defined(Q_OS_WIN)
#include <ioapiset.h>
#include <tchar.h>
#endif

class Usb
{
public:
    Usb();
    static bool eject(QString driveName);
    static bool isUsb(QString path);
    static bool isEjectable(QString path);
};

#endif // USB_H
