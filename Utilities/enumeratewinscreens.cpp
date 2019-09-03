#include "enumeratewinscreens.h"

void EnumerateWinScreens::collectScreensInfo()
{
    DISPLAY_DEVICE displayDevice = {};
    displayDevice.cb = sizeof(DISPLAY_DEVICE);

    // Cycle through adaptors
    std::wstring adaptorName;
    LPCWSTR adaptorNameW;
    std::wstring adaptorString;
    LPCWSTR adaptorStringW;
    DWORD adaptorIndex = 0;
    while (/*::*/EnumDisplayDevicesW(nullptr, adaptorIndex++, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME))
    {
//        if (displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP &&
//            displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            adaptorName = displayDevice.DeviceName;
            adaptorNameW = adaptorName.c_str();
            adaptorString = displayDevice.DeviceString;
            adaptorStringW = adaptorString.c_str();

            // Cycle throughattached monitors
            std::wstring deviceName;
            LPCWSTR deviceNameW;
            std::wstring deviceString;
            LPCWSTR deviceStringW;
            DWORD deviceIndex = 0;
            while (/*::*/EnumDisplayDevicesW(adaptorNameW, deviceIndex++, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME))
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

                    // Save the screen data - for example:
//                    tadaptorName = "\\\\.\\DISPLAY1" :: adaptorStringW = "NVIDIA GeForce GTX 1050  " tdeviceStringW =  "BenQ SW320           " tprofileName = "BenQ SW320.icm"
//                    tadaptorName = "\\\\.\\DISPLAY5" :: adaptorStringW = "NVIDIA GeForce GT 740    " tdeviceStringW =  "Dell U3011 (Digital) " tprofileName = "DELL 3007WFP_i1_2018-01-10.icm"
//                    tadaptorName = "\\\\.\\DISPLAY6" :: adaptorStringW = "NVIDIA GeForce GT 740    " tdeviceStringW =  "Dell 3007WFP         " tprofileName = "3007WFP.icm"

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


