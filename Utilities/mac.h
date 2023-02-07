#ifndef MAC_H
#define MAC_H

//#include "Foundation/Foundation.h"
//#include "Foundation/NSDictionary.h"

#include <sys/types.h>
#include <sys/sysctl.h>

#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>

//#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#include <CoreGraphics/CoreGraphics.h>

// share
//#include <AppKit/NSSharingService.h>

#include "Main/global.h"                // req'd by availableMemory

class Mac
{
public:
    static void availableMemory();
    static QString getDisplayProfileURL();
    static float getMouseCursorMagnification();
    static void share(QList<QUrl> urls);

private:
    typedef struct {
        CFUUIDRef dispuuid;
        CFURLRef url;
    } ColorSyncIteratorData;
    static bool colorSyncIterateCallback(CFDictionaryRef dict, void *data);
};

#endif // MAC_H
