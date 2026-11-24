#include "usbutil.h"
#include "Main/global.h"

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

    HANDLE handle = CreateFile(wtext, GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        G::issue("Warning", QString("Eject failed: the drive could not be opened (Windows "
                 "error %1)").arg(GetLastError()), "UsbUtil::eject", -1, driveName);
        return false;
    }

    DWORD bytes = 0;

    /* DeviceIoControl returns nonzero on success.  Dismount the volume, then eject the
       media, and report failure to the caller if either step fails. */
    if (!DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &bytes, 0)) {
        // usually a file on the drive is still open (error 5 or 32)
        G::issue("Warning", QString("Eject failed: the volume could not be dismounted "
                 "(Windows error %1)").arg(GetLastError()), "UsbUtil::eject", -1, driveName);
        CloseHandle(handle);
        return false;
    }
    BOOL ejected = DeviceIoControl(handle, IOCTL_STORAGE_EJECT_MEDIA, 0, 0, 0, 0, &bytes, 0);
    if (!ejected)
        G::issue("Warning", QString("Eject failed: the media was not ejected (Windows "
                 "error %1)").arg(GetLastError()), "UsbUtil::eject", -1, driveName);
    CloseHandle(handle);
    return ejected != 0;

#elif defined(Q_OS_MAC)
    QProcess p;
    p.start("diskutil", QStringList() << "eject" << driveName);
    const bool ok = p.waitForFinished() && p.exitStatus() == QProcess::NormalExit &&
                    p.exitCode() == 0;
    // diskutil names the cause ("in use by process ...") on stderr
    if (!ok)
        G::issue("Warning", "Eject failed: diskutil: " +
                 QString::fromUtf8(p.readAllStandardError()).trimmed(), "UsbUtil::eject",
                 -1, driveName);
    return ok;
#else
    // gio unmounts the mount at driveName and powers off its drive (GNOME/KDE/udisks)
    QProcess p;
    p.start("gio", QStringList() << "mount" << "-e" << driveName);
    const bool ok = p.waitForFinished() && p.exitStatus() == QProcess::NormalExit &&
                    p.exitCode() == 0;
    if (!ok)
        G::issue("Warning", "Eject failed: gio: " + (p.error() == QProcess::FailedToStart
                 ? QString("gio is not installed")
                 : QString::fromUtf8(p.readAllStandardError()).trimmed()),
                 "UsbUtil::eject", -1, driveName);
    return ok;
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

