#include "usbutil.h"

bool UsbUtil::eject(QString driveName)
{
#if defined(Q_OS_WIN)
    char driveLetter = driveName[0].unicode();
    char devicepath[7];
    char format[] = "\\\\.\\?:";
    strcpy_s(devicepath, format);
    devicepath[4] = driveLetter;
    wchar_t wtext[7];
    size_t textlen = 7;
    mbstowcs_s(&textlen, wtext, devicepath, strlen(devicepath)+1);
    QString errmsg = "";

    HANDLE handle = CreateFile(wtext, GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD bytes = 0;
    long result;

    result = DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &bytes, 0);
//    if(result) {
////        FormatErrorMsg("Dismount Volume: ", errmsg);
//        qDebug() << "Dismount err msg:" << errmsg << result;
//        return false;
//    }
    result = DeviceIoControl(handle, IOCTL_STORAGE_EJECT_MEDIA, 0, 0, 0, 0, &bytes, 0);
//    if(result) {
////        FormatErrorMsg("Eject Volume: ", errmsg);
//        qDebug() << "Eject err msg:" << errmsg << result;
//        return false;
//    }
    CloseHandle(handle);
    return true;

#elif defined(Q_OS_MAC)
    QProcess p;
    p.start("diskutil", QStringList() << "eject" << driveName);
    return p.waitForFinished() && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
#endif
}

// bool UsbUtil::isUsb(QString path)
// {
// #if defined(Q_OS_WIN)
//     int type = 0;
//     char driveLetter = path[0].unicode();
//     WCHAR wDrive[] = L"?:\\"; // use as a drive letter template
//     wDrive[0] = driveLetter;
//     type = GetDriveType(wDrive);
//     path = ""; // suppress compiler warning;
//     return (type == DRIVE_REMOVABLE);
// #elif defined(Q_OS_MAC)
//     // ADD MAC CODE HERE
//     // Old version
//     // path = ""; // suppress compiler warning;
//     // return true;
//     // New version

//     Q_UNUSED(path)
// #endif
// }

bool UsbUtil::isValid(const QStorageInfo &storage)
{
    return storage.isValid() && storage.isReady() && !storage.isReadOnly();
}

bool UsbUtil::isRemovable(const QString &rootPath)
{
    if (rootPath.isEmpty() || rootPath.length() < 2)
        return false;

    #if defined(Q_OS_WIN)
        char letter = rootPath.at(0).toLatin1();
        if (letter < 'A' || letter > 'Z')
            return false;
        WCHAR driveLetter[] = L"?:\\";
        driveLetter[0] = rootPath.at(0).toLatin1();
        UINT type = GetDriveTypeW(driveLetter);
        return type == DRIVE_REMOVABLE || type == DRIVE_CDROM;

    #elif defined(Q_OS_MAC)
        return rootPath.startsWith("/Volumes/");

    #else
        // Generic fallback for Linux etc.
        return false;
    #endif
}

bool UsbUtil::isEjectable(const QString &rootPath)
{
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (storage.rootPath() != rootPath) continue;
        if (isValid(storage) && isRemovable(rootPath)) return true;
    }
    return false;
}

bool UsbUtil::isMemCardWithDCIM(const QString &rootPath)
{
    QStorageInfo storage(rootPath);
    QString dcimPath = storage.rootPath() + "/DCIM";
    return (QDir(dcimPath).exists(dcimPath) && isValid(storage));
}

QStringList UsbUtil::dcimCards()
{
    QStringList dcimList;

    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        const QString rootPath = storage.rootPath();
        if (isMemCardWithDCIM(rootPath))
            dcimList << storage.displayName();
    }

    return dcimList;
}

