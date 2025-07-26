#ifndef USBUTIL_H
#define USBUTIL_H

#include <QtWidgets>

#if defined(Q_OS_WIN)
#include <ioapiset.h>
#include <tchar.h>
#endif

class UsbUtil final
{
public:
    static bool eject(QString driveName);
    static QStringList dcimCards();
    static bool isMemCardWithDCIM(const QString &rootPath);
    static QStringList ejectableDrives();
    static bool isEjectable(const QString &rootPath);
    static bool isRemovable(const QString &rootPath);
private:
    static bool isValid(const QStorageInfo &storage);
};

#endif // USBUTIL_H
