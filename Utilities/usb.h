#ifndef USB_H
#define USB_H

#include <QtWidgets>

#include <ioapiset.h>
#include <tchar.h>

class Usb
{
public:
    Usb();
    static bool eject(char driveLetter);
    static bool isUsb(char driveLetter);
};

#endif // USB_H
