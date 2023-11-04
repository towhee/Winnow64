#include "win.h"

void Win::availableMemory()
{
    /*
    Sets G::availableMemoryMB to the amount of free memory in windows. This is used to
    advise the user on how much memory to allocate to the image cache.

    The statex struct includes:
        DWORDLONG ullTotalPhys;
        DWORDLONG ullAvailPhys;
        DWORDLONG ullTotalPageFile;
        DWORDLONG ullAvailPageFile;
        DWORDLONG ullTotalVirtual;
        DWORDLONG ullAvailVirtual;
        DWORDLONG ullAvailExtendedVirtual;

        Really good resource for getting memory info for various OS
        https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
    */
    if (G::isLogger) G::log("Win::availableMemory");
    quint32 mb = 1024 *1024;
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx (&statex);
    /*
    qDebug() << "Win::availableMemory"
             << "Physical memory:" << statex.ullTotalPhys / mb
             << "Available memory:" << statex.ullAvailPhys / mb;
    */
    // allocate 100MB for datamodel memory usage (mostly thumbnails)
    G::availableMemoryMB = static_cast<quint32>(statex.ullAvailPhys / mb) - 100;

    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    SIZE_T virtualMemUsedByMe = pmc.PagefileUsage;
//    qDebug() << "Win::availableMemory" << virtualMemUsedByMe;
}

void Win::collectScreensInfo()
{
    /*
    Populates G::winScreenHash with the attached adaptors, monitors and icc profiles.
    */
    if (G::isLogger) G::log("Win::collectScreensInfo");
    DISPLAY_DEVICE displayDevice = {};
    displayDevice.cb = sizeof(DISPLAY_DEVICE);

    // Cycle through adaptors
    std::wstring adaptorName;
    LPCWSTR adaptorNameW;
    std::wstring adaptorString;
    LPCWSTR adaptorStringW;
    DWORD adaptorIndex = 0;
    while (EnumDisplayDevicesW(nullptr, adaptorIndex++, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME))
    {
//        if (displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP &&
//            displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            adaptorName = displayDevice.DeviceName;
            adaptorNameW = adaptorName.c_str();
            adaptorString = displayDevice.DeviceString;
            adaptorStringW = adaptorString.c_str();

            // Cycle through attached monitors
            std::wstring deviceName;
            LPCWSTR deviceNameW;
            std::wstring deviceString;
            LPCWSTR deviceStringW;
            DWORD deviceIndex = 0;
            while (EnumDisplayDevicesW(adaptorNameW, deviceIndex++, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME))
            {
                if (displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE &&
                    displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED)
                {
                    deviceName = displayDevice.DeviceKey;
                    deviceNameW = deviceName.c_str();
                    deviceString = displayDevice.DeviceString;
                    deviceStringW = deviceString.c_str();

                    // Find out whether to use the global or user profile
                    BOOL usePerUserProfiles = FALSE;
                    WcsGetUsePerUserProfiles(deviceNameW, CLASS_MONITOR, &usePerUserProfiles);

                    // Finally, get the profile name
                    const WCS_PROFILE_MANAGEMENT_SCOPE scope = usePerUserProfiles ? WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER : WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE;

                    DWORD profileNameLength = 0; // In bytes
                    WcsGetDefaultColorProfileSize(scope, deviceNameW, CPT_ICC, CPST_RGB_WORKING_SPACE, 0, &profileNameLength);

                    wchar_t *const profileName = new wchar_t[profileNameLength / sizeof(wchar_t)];
                    WcsGetDefaultColorProfile(scope, deviceNameW, CPT_ICC, CPST_RGB_WORKING_SPACE, 0, profileNameLength, profileName);

                    /* Save the screen data - for example:
                    tadaptorName = "\\\\.\\DISPLAY1" :: adaptorStringW = "NVIDIA GeForce GTX 1050  " tdeviceStringW =  "BenQ SW320           " tprofileName = "BenQ SW320.icm"
                    tadaptorName = "\\\\.\\DISPLAY5" :: adaptorStringW = "NVIDIA GeForce GT 740    " tdeviceStringW =  "Dell U3011 (Digital) " tprofileName = "DELL 3007WFP_i1_2018-01-10.icm"
                    tadaptorName = "\\\\.\\DISPLAY6" :: adaptorStringW = "NVIDIA GeForce GT 740    " tdeviceStringW =  "Dell 3007WFP         " tprofileName = "3007WFP.icm"
                    */
                    G::WinScreen winScreen;
                    winScreen.device = QString::fromUtf16((const ushort *)(deviceStringW));
                    winScreen.adaptor = QString::fromUtf16((const ushort *)(adaptorStringW));
                    winScreen.profile = QString::fromUtf16((const ushort *)(profileName));
                    QString name = QString::fromUtf16((const ushort *)(adaptorNameW));
                    G::winScreenHash.insert(name, winScreen);
                    /*
                    qDebug() << "tadaptorName =" << QString::fromUtf16((const ushort *)(adaptorNameW)).leftJustified(25, ' ')
                             << "adaptorStringW =" << QString::fromUtf16((const ushort *)(adaptorStringW)).leftJustified(25, ' ')
                             << "tdeviceStringW = " << QString::fromUtf16((const ushort *)(deviceStringW)).leftJustified(25, ' ')
                             << "tprofileName =" << QString::fromUtf16((const ushort *)(profileName));
                    */
                    delete[] profileName;
                }
            }
        }
    }
}

void Win::setTitleBarColor(WId winId, QColor bgColor)
{
    #if _WIN32_WINNT >= 0x0B00  // windows 11 or later
        HWND hWnd = reinterpret_cast<HWND>(winId);
        COLORREF color = RGB(bgColor.red(), bgColor.green(), bgColor.blue());
        DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
    #endif
}


