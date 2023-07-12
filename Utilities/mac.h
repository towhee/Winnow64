#ifndef MAC_H
#define MAC_H

#include <sys/types.h>
#include <sys/sysctl.h>

#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>

//#include <Carbon/Carbon.h>

#include "Main/global.h"                // req'd by availableMemory

class Mac
{
public:
    static void availableMemory();
    static QString getDisplayProfileURL();
    static float getMouseCursorMagnification();
    static void share(QList<QUrl> &urls, WId wId);
//    static void spaces();

private:
    typedef struct {
        CFUUIDRef dispuuid;
        CFURLRef url;
    } ColorSyncIteratorData;
    static bool colorSyncIterateCallback(CFDictionaryRef dict, void *data);



//#define CGSDefaultConnection _CGSDefaultConnection()

//    typedef uint64_t CGSConnection;
//    typedef uint64_t CGSSpace;

//    typedef enum _CGSSpaceType {
//        kCGSSpaceUser,
//        kCGSSpaceFullscreen,
//        kCGSSpaceSystem,
//        kCGSSpaceUnknown
//    } CGSSpaceType;

//    typedef enum _CGSSpaceSelector {
//        kCGSSpaceCurrent = 5,
//        kCGSSpaceAll = 7
//    } CGSSpaceSelector;

//    CFArrayRef CGSCopySpaces(const CGSConnection cid, CGSSpaceSelector type);

//    CFArrayRef CGSSpaceCopyOwners(const CGSConnection cid, CGSSpace space);

//    int CGSSpaceGetAbsoluteLevel(const CGSConnection cid, CGSSpace space);
//    void CGSSpaceSetAbsoluteLevel(const CGSConnection cid, CGSSpace space, int level);

//    int CGSSpaceGetCompatID(const CGSConnection cid, CGSSpace space);
//    void CGSSpaceSetCompatID(const CGSConnection cid, CGSSpace space, int compatID);

//    CGSSpaceType CGSSpaceGetType(const CGSConnection cid, CGSSpace space);
//    void CGSSpaceSetType(const CGSConnection cid, CGSSpace space, CGSSpaceType type);

//    CFStringRef CGSSpaceCopyName(const CGSConnection cid, CGSSpace space);
//    void CGSSpaceSetName(const CGSConnection cid, CGSSpace space, CFStringRef name);

//    CFArrayRef CGSSpaceCopyValues(const CGSConnection cid, CGSSpace space);
//    void CGSSpaceSetValues(const CGSConnection cid, CGSSpace space, CFArrayRef values);

//    typedef uint64_t CGSManagedDisplay;

//    CFArrayRef CGSCopyManagedDisplaySpaces(const CGSConnection cid);

//    bool CGSManagedDisplayIsAnimating(const CGSConnection cid, CGSManagedDisplay display);
//    void CGSManagedDisplaySetIsAnimating(const CGSConnection cid, CGSManagedDisplay display, bool isAnimating);

//    CGSSpace PKGManagedDisplayGetCurrentSpace(const CGSConnection cid, CGSManagedDisplay display);
//    void CGSManagedDisplaySetCurrentSpace(const CGSConnection cid, CGSManagedDisplay display, CGSSpace space);

//    CGSManagedDisplay kCGSPackagesMainDisplayIdentifier;

//    CGSConnection _CGSDefaultConnection(void);

};

#endif // MAC_H
