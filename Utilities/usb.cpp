#include "usb.h"

Usb::Usb()
{

}

bool Usb::eject(char driveLetter)
{
#if defined(Q_OS_WIN)
    char devicepath[7];
    char format[] = "\\\\.\\?:";
    strcpy_s(devicepath, format);
    devicepath[4] = driveLetter;
    wchar_t wtext[7];
    size_t textlen = 7;
    mbstowcs_s(&textlen, wtext, devicepath,strlen(devicepath)+1);

    HANDLE handle = CreateFile(wtext, GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    DWORD bytes = 0;
    if(!DeviceIoControl(handle, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, &bytes, 0)) return false;
    if(!DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &bytes, 0)) return false;
    if(!DeviceIoControl(handle, IOCTL_STORAGE_EJECT_MEDIA, 0, 0, 0, 0, &bytes, 0)) return false;
    CloseHandle(handle);
    return true;
#elif defined(Q_OS_MAC)
    // ADD MAC CODE HERE
#endif
}

bool Usb::isUsb(char driveLetter)
{
#if defined(Q_OS_WIN)
    int type = 0;
    WCHAR wDrive[] = L"?:\\"; // use as a drive letter template
    wDrive[0] = driveLetter;
    type = GetDriveType(wDrive);
    return (type == DRIVE_REMOVABLE);
#elif defined(Q_OS_MAC)
    // ADD MAC CODE HERE
#endif
}
