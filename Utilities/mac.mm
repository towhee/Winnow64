#include "mac.h"
#import <Foundation/Foundation.h>
#import <Social/Social.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/NSSharingService.h>
#import <Cocoa/Cocoa.h>

// Define an AppDelegate class
@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

// Implement the applicationSupportsSecureRestorableState method
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

@end

// Set up the delegate in your main application entry
void Mac::initializeAppDelegate() {
    static AppDelegate *appDelegate = nil;
    if (!appDelegate) {
        appDelegate = [[AppDelegate alloc] init];
        [NSApp setDelegate:appDelegate];
    }
}

@interface SharingDelegate : NSObject<NSSharingServicePickerDelegate>
@end

@implementation SharingDelegate
- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(NSSharingService *)service {
    NSLog(@"Sharing service chosen: %@", service);
}
@end

void Mac::availableMemory()
{
    if (G::isLogger) G::log("Mac::availableMemory");
    vm_size_t page_size;
    mach_port_t mach_port;
    mach_msg_type_number_t count;
    vm_statistics64_data_t vm_stats;

    mach_port = mach_host_self();
    count = sizeof(vm_stats) / sizeof(natural_t);
    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
        KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
        (host_info64_t)&vm_stats, &count))
    {
        long long free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
        /*
        long long used_memory = ((int64_t)vm_stats.active_count +
                                 (int64_t)vm_stats.inactive_count +
                                 (int64_t)vm_stats.wire_count) *  (int64_t)page_size;*/
        G::availableMemoryMB = static_cast<quint32>(free_memory / (1024 * 1024));
    }
}

void Mac::joinAllSpaces(WId wId)
{
/*
    This is required in case the user has set the MacOS docked Winnow app options
    (right click on docked app and choose options) to anything other than all
    desktops.  In that case Qt restoreGeometry was not working if the app last
    location was not on the same desktop assigned in the dock options.  Other
    weird behaviors, like tooltips and dropdowns on wrong monitor are also
    avoided.
*/
    NSView* view = reinterpret_cast<NSView*>(wId);
    NSWindow *myWindow = [view window];
    NSUInteger collectionBehavior;

    // Gets the current collection behavior of the window
    collectionBehavior = [ myWindow collectionBehavior ];

    // Adds the option to make the window visible on all spaces
    collectionBehavior |= NSWindowCollectionBehaviorCanJoinAllSpaces;

    // Sets the new collection behaviour
    [ myWindow setCollectionBehavior: collectionBehavior ];
}

bool Mac::colorSyncIterateCallback(CFDictionaryRef dict, void *data)
{
    ColorSyncIteratorData *iterData = (ColorSyncIteratorData *)data;
    CFStringRef str;
    CFUUIDRef uuid;
    CFBooleanRef iscur;

    if (!CFDictionaryGetValueIfPresent(dict, kColorSyncDeviceClass, (const void**)&str))
    {
        QString msg = "kColorSyncDeviceID failed.";
        G::issue("Warning", msg, "Mac::colorSyncIterateCallback");
        return true;
    }
    if (!CFEqual(str, kColorSyncDisplayDeviceClass))
    {
        return true;
    }
    if (!CFDictionaryGetValueIfPresent(dict, kColorSyncDeviceID, (const void**)&uuid))
    {
        QString msg = "kColorSyncDeviceID failed.";
        G::issue("Warning", msg, "Mac::colorSyncIterateCallback");
        return true;
    }
    if (!CFEqual(uuid, iterData->dispuuid))
    {
        return true;
    }
    if (!CFDictionaryGetValueIfPresent(dict, kColorSyncDeviceProfileIsCurrent, (const void**)&iscur))
    {
        QString msg = "kColorSyncDeviceProfileIsCurrent failed.";
        G::issue("Warning", msg, "Mac::colorSyncIterateCallback");
        return true;
    }
    if (!CFBooleanGetValue(iscur))
    {
        return true;
    }
    if (!CFDictionaryGetValueIfPresent(dict, kColorSyncDeviceProfileURL, (const void**)&(iterData->url)))
    {
        QString msg = "Could not get current profile URL.";
        G::issue("Warning", msg, "Mac::colorSyncIterateCallback");
        return true;
    }

    CFRetain(iterData->url);
    return false;
}

QString Mac::getDisplayProfileURL()
{
    if (G::isLogger) G::log("Mac::getDisplayProfileURL");
    ColorSyncIteratorData data;
    data.dispuuid = CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID());
    data.url = nullptr;
    ColorSyncIterateDeviceProfiles(colorSyncIterateCallback, (void *)&data);
    CFRelease(data.dispuuid);
    // can crash after macos in sleep mode
    CFStringRef urlstr;
    if (data.url != nullptr) {
        urlstr = CFURLCopyFileSystemPath(data.url, kCFURLPOSIXPathStyle);
        CFRelease(data.url);  // wakeup crash here - data.url == 0 null pointer argument
        return QString::fromCFString(urlstr);
    }
    // qDebug() << "Mac::getDisplayProfileURL" << QString::fromCFString(urlstr);
    return "";
}

float Mac::getMouseCursorMagnification()
{
    if (G::isLogger) G::log("Mac::getMouseCursorMagnification");
    NSDictionary *dict = [[NSUserDefaults standardUserDefaults] persistentDomainForName:@"com.apple.universalaccess"];
//    float cur_scale = dict[@"mouseDriverCursorSize"]; // returns 1.0 to 4.0
    float cur_scale = [dict[@"mouseDriverCursorSize"] floatValue]; // returns 1.0 to 4.0
    return cur_scale;
}

void Mac::share(QList<QUrl> &urls, WId wId)
{
    if (G::isLogger) G::log("Mac::share");
    NSView *view = reinterpret_cast<NSView *>(wId);
    NSRect r = [view bounds];
    NSRect rect = NSMakeRect(r.origin.x + r.size.width / 2,
                             r.origin.y + r.size.height / 2,
                             10, 10);
    NSMutableArray *nsFileUrls = [NSMutableArray array];
    for (const auto &url : urls) {
        NSURL *nsFileUrl = url.toNSURL();
        [nsFileUrls addObject:nsFileUrl];
    }

    SharingDelegate *delegate = [[SharingDelegate alloc] init];
    NSSharingServicePicker *picker = [[NSSharingServicePicker alloc] initWithItems:nsFileUrls];
    [picker setDelegate:delegate];
    [picker showRelativeToRect:rect ofView:view preferredEdge:NSMaxYEdge];
}
