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
#include <functional>
#include <QColor>
// #include <QStringList>

class Mac
{
public:
    static void disableExtraEditMenuItems();
    static void stripEditMenuExtras();
    static void availableMemory();
    static qint64 getResidentMemoryUsageBytes();

    static uint64_t totalMemoryMB();
    static uint64_t processFootprintMB();
    static uint64_t aggressiveAvailMB();
    static int memoryPressureLevel();
    static void startMemoryPressureMonitor(void (^handler)(int level));

    static void joinAllSpaces(WId wId);
    static void setTitleBarColor(WId wId, const QColor &bgColor);
    static void setTitleTextVisible(WId wId, bool visible);
    static void titleBarDoubleClick(WId wId);
    static int titleBarButtonsRight(WId wId);
    static QString getDisplayProfileURL();
    static float getMouseCursorMagnification();
    static void share(QList<QUrl> &urls, WId wId);
    static QString defaultMailAppId();      // bundle id of the mailto: handler
    static QStringList listMountedVolumes();

    /* DRAG COUNT BADGE. installDragBadgeHook() once at start-up; a drop target then
       calls setDragBadgeCount(n) from its dragEnter/dragMove handler to make the
       Finder-style count on the drag cursor read n while over it. See mac.mm. */
    static void installDragBadgeHook();
    static void setDragBadgeCount(int n);

    /* DRAG OPERATION MASK. installDragMaskHook() returns true once QNSView's source
       mask is wrapped; while setDragOutsideCopyOnly(true) is on, a drag offers Copy
       alone to other applications and its full actions inside Winnow. See mac.mm. */
    static bool installDragMaskHook();
    static void setDragOutsideCopyOnly(bool on);
    /* DRAG MOVED. While set, onMoved runs each time AppKit reports the drag pointer
       moved, anywhere on screen (draggingSession:movedToPoint:). nullptr clears it.
       Returns false if the hook could not be installed. See mac.mm. */
    static bool setDragMovedHandler(std::function<void()> onMoved);

private:
    typedef struct {
        CFUUIDRef dispuuid;
        CFURLRef url;
    } ColorSyncIteratorData;
    static bool colorSyncIterateCallback(CFDictionaryRef dict, void *data);
};

#endif // MAC_H
