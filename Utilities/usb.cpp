#include "usb.h"

Usb::Usb()
{

}

bool Usb::eject(QString driveName)
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
    // ie tell application "Finder" to eject disk "Untitled"
    // where driveName is "Untitled"
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "eject disk \"" + driveName + "\"";
    args << "-e";
    args << "end tell";
    if (!QProcess::execute("/usr/bin/osascript", args))
        return true;
#endif
    return false;
}

bool Usb::isUsb(QString path)
{
#if defined(Q_OS_WIN)
    int type = 0;
    char driveLetter = path[0].unicode();
    WCHAR wDrive[] = L"?:\\"; // use as a drive letter template
    wDrive[0] = driveLetter;
    type = GetDriveType(wDrive);
    return (type == DRIVE_REMOVABLE);
#elif defined(Q_OS_MAC)
    // ADD MAC CODE HERE
    return true;
#endif
}
