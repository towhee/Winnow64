#ifndef MAC_H
#define MAC_H

#include <sys/types.h>
#include <sys/sysctl.h>

#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>

// #include <sys/mount.h>
// #include <IOKit/IOKitLib.h>
// #include <IOKit/storage/IOMedia.h>
// #include <IOKit/storage/IOBlockStorageDevice.h>
// #include <IOKit/usb/IOUSBLib.h>


#include "Main/global.h"                // req'd by availableMemory
#include <QtCore>
// #include <QStringList>

class Mac
{
public:
    static void initializeAppDelegate();
    static void availableMemory();
    static qint64 getResidentMemoryUsageBytes();

    static uint64_t totalMemoryMB();
    static uint64_t processFootprintMB();
    static uint64_t aggressiveAvailMB();
    static int memoryPressureLevel();
    static void startMemoryPressureMonitor(void (^handler)(int level));

    static void joinAllSpaces(WId wId);
    static QString getDisplayProfileURL();
    static float getMouseCursorMagnification();
    static void share(QList<QUrl> &urls, WId wId);
    static QStringList listMountedVolumes();

private:
    typedef struct {
        CFUUIDRef dispuuid;
        CFURLRef url;
    } ColorSyncIteratorData;
    static bool colorSyncIterateCallback(CFDictionaryRef dict, void *data);
};

#endif // MAC_H
