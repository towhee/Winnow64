#include "win.h"

// --- C++/WinRT, for Win::share (system Share flyout). ----------------------
// win.h pulls in <Windows.h>, whose min/max macros break the C++/WinRT (and
// STL) headers below, so undefine them first.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <QDir>
#include <memory>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
#pragma comment(lib, "windowsapp.lib")      // C++/WinRT runtime (desktop)

// The Win32 interop that bridges DataTransferManager to an HWND. Declared
// inline (by its documented IID) so we don't depend on which SDK header
// happens to expose it.
MIDL_INTERFACE("3A3DCD6C-3EAB-43DC-BCDE-45671CE800C8")
IDataTransferManagerInterop : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND appWindow, REFIID riid,
                                                   void **dataTransferManager) = 0;
    virtual HRESULT STDMETHODCALLTYPE ShowShareUIForWindow(HWND appWindow) = 0;
};
// ---------------------------------------------------------------------------

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

quint64 Win::totalMemoryMB()
{
    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    if (!GlobalMemoryStatusEx(&statex)) return 0;
    return static_cast<quint64>(statex.ullTotalPhys / (1024ull * 1024ull));
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
    // WINVER not working - same for Win10 and Win11, must compile on Win11
    double osVersion = QSysInfo::productVersion().toDouble();
    if (osVersion < 11) return;
        HWND hWnd = reinterpret_cast<HWND>(winId);
        COLORREF color = RGB(bgColor.red(), bgColor.green(), bgColor.blue());
        DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
}

void Win::share(QList<QUrl> &urls, WId wId)
{
/*
    Raise the Windows system Share flyout for the given files, anchored to the
    app window. This is the counterpart to Mac::share (NSSharingServicePicker)
    and lets the user send the selected images via Mail, nearby sharing, etc.

    Must run on the GUI (STA) thread, where Qt has already initialised COM.
*/
    using namespace winrt;
    using namespace winrt::Windows::ApplicationModel::DataTransfer;
    using namespace winrt::Windows::Storage;
    using namespace winrt::Windows::Foundation::Collections;

    if (G::isLogger) G::log("Win::share");
    if (urls.isEmpty()) return;

    HWND hwnd = reinterpret_cast<HWND>(wId);

    // Capture native paths for the (deferred) data request. shared_ptr keeps
    // them alive for the lifetime of the coroutine below.
    auto paths = std::make_shared<std::vector<hstring>>();
    for (const QUrl &url : urls) {
        const QString native = QDir::toNativeSeparators(url.toLocalFile());
        if (!native.isEmpty())
            paths->push_back(hstring(reinterpret_cast<const wchar_t *>(native.utf16())));
    }
    if (paths->empty()) return;

    try {
        // Bind a DataTransferManager to our HWND via the desktop interop API.
        auto interop = get_activation_factory<DataTransferManager>().as<IDataTransferManagerInterop>();
        DataTransferManager dtm{nullptr};
        check_hresult(interop->GetForWindow(hwnd, guid_of<DataTransferManager>(), put_abi(dtm)));

        // Populate the data package when the system requests it. A deferral lets
        // us load the StorageFiles asynchronously without blocking the UI thread.
        dtm.DataRequested([paths](DataTransferManager const &,
                                  DataRequestedEventArgs const &args) -> fire_and_forget
        {
            // This lambda is a coroutine: captures are NOT in the coroutine
            // frame, so copy what we need to locals before the first co_await.
            // (args is a by-value-projected param and survives suspension.)
            auto pathsLocal = paths;
            auto request = args.Request();
            auto deferral = request.GetDeferral();
            auto data = request.Data();
            data.Properties().Title(L"Winnow images");
            auto items = single_threaded_vector<IStorageItem>();
            for (const auto &p : *pathsLocal) {
                try {
                    StorageFile f = co_await StorageFile::GetFileFromPathAsync(p);
                    items.Append(f);
                }
                catch (...) { /* skip files that can't be opened */ }
            }
            if (items.Size() > 0) data.SetStorageItems(items);
            deferral.Complete();
        });

        check_hresult(interop->ShowShareUIForWindow(hwnd));
    }
    catch (const hresult_error &e) {
        qWarning() << "Win::share failed:" << QString::fromWCharArray(e.message().c_str());
    }
}


