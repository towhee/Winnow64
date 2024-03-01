#include "mac.h"
#import <Foundation/Foundation.h>
#import <Social/Social.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/NSSharingService.h>

/*
First, you need to create an Objective-C++ source file (with the .mm extension) to
implement your NSApplicationDelegate. In this file, you can import both Qt and Cocoa
headers:

#import <Cocoa/Cocoa.h>
#include <QApplication>

@interface MyApplicationDelegate : NSObject <NSApplicationDelegate>
{
    QApplication *qtApplication;
}
@end

@implementation MyApplicationDelegate

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

// ... other delegate methods ...

@end

In your main.cpp file, you can create an instance of MyApplicationDelegate and set it as
the delegate of the NSApplication:

#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // ... setup your Qt application ...

    MyApplicationDelegate *delegate = [[MyApplicationDelegate alloc] init];
    [NSApp setDelegate:delegate];

    return app.exec();
}

This will set your custom delegate as the delegate of the NSApplication, and the applicationSupportsSecureRestorableState: method will be called by the system when it needs to know if your application supports secure restorable state.

Please note that mixing Qt and Cocoa in the same application can be complex and might lead to unexpected issues, so it’s important to thoroughly test your application on all supported macOS versions. Also, keep in mind that you need to follow the memory management rules of both C++ and Objective-C when using Objective-C++.
*/

//@interface MyApplicationDelegate : NSObject <NSApplicationDelegate>
//{
//    QApplication *qtApplication;
//}
//@end

//@implementation MyApplicationDelegate

//- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
//    return YES;
//}
//@end

// code below before version 1.37.5

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
        qWarning("kColorSyncDeviceClass failed");
        return true;
    }
    if (!CFEqual(str, kColorSyncDisplayDeviceClass))
    {
        return true;
    }
    if (!CFDictionaryGetValueIfPresent(dict, kColorSyncDeviceID, (const void**)&uuid))
    {
        qWarning("kColorSyncDeviceID failed");
        return true;
    }
    if (!CFEqual(uuid, iterData->dispuuid))
    {
        return true;
    }
    if (!CFDictionaryGetValueIfPresent(dict, kColorSyncDeviceProfileIsCurrent, (const void**)&iscur))
    {
        qWarning("kColorSyncDeviceProfileIsCurrent failed");
        return true;
    }
    if (!CFBooleanGetValue(iscur))
    {
        return true;
    }
    if (!CFDictionaryGetValueIfPresent(dict, kColorSyncDeviceProfileURL, (const void**)&(iterData->url)))
    {
        qWarning("Could not get current profile URL");
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
    }
    CFRelease(data.url);
//    qDebug() << "Mac::getDisplayProfileURL" << QString::fromCFString(urlstr);
    return QString::fromCFString(urlstr);
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

//void Mac::spaces()
//{
//    QStringList sL;
//    CFArrayRef spaces = CGSCopySpaces(CGSDefaultConnection, kCGSSpaceAll);

//    qDebug() << "spaces: " << spaces;

//    int i;

//    for(i = 0; i < CFArrayGetCount(spaces); i++) {
//        CGSSpace space = (CGSSpace)CFArrayGetValueAtIndex(spaces, i);

//        CFShow(CGSSpaceCopyName(CGSDefaultConnection, space));
//        printf("i: %i", i);
//        printf("Space ID: %lld\n", space);
//        printf("Absolute level: %d\n", CGSSpaceGetAbsoluteLevel(CGSDefaultConnection, space));
//        printf("Compat ID: %d\n", CGSSpaceGetCompatID(CGSDefaultConnection, space));
//        printf("Type: %d\n", CGSSpaceGetType(CGSDefaultConnection, space));
//        printf("\n");

//        sL.push_back(QString::number(space));
//    }

//}
