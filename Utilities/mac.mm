#include "mac.h"
#import <Foundation/Foundation.h>
#import <Social/Social.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/NSSharingService.h>
#import <Cocoa/Cocoa.h>
#import <sys/mount.h>
#import <IOKit/IOBSD.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOBlockStorageDevice.h>

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
    vm_size_t page_size = 0;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm_stats{};
    kern_return_t kr1 = host_page_size(mach_host_self(), &page_size);
    kern_return_t kr2 = host_statistics64(mach_host_self(),
                                          HOST_VM_INFO64,
                                          (host_info64_t)&vm_stats,
                                          &count);
    if (kr1 == KERN_SUCCESS && kr2 == KERN_SUCCESS) {
        // free + inactive (+ speculative if available in this SDK)
        uint64_t freePages = vm_stats.free_count + vm_stats.inactive_count;
#ifdef __MAC_10_9
        freePages += vm_stats.speculative_count;
#endif
        uint64_t freeBytes = (uint64_t)freePages * (uint64_t)page_size;
        G::availableMemoryMB = static_cast<quint32>(freeBytes / (1024ull * 1024ull));
    }
    // if (G::isLogger)
    //   G::log("Mac::availableMemory");
    // vm_size_t page_size;
    // mach_port_t mach_port;
    // mach_msg_type_number_t count;
    // vm_statistics64_data_t vm_stats;

    // mach_port = mach_host_self();
    // count = sizeof(vm_stats) / sizeof(natural_t);
    // if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
    //     KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
    //     (host_info64_t)&vm_stats, &count))
    // {
    //     long long free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
    //     G::availableMemoryMB = static_cast<quint32>(free_memory / (1024 * 1024));
    // }
}

qint64 Mac::getResidentMemoryUsageBytes()
{
  if (G::isLogger)
    G::log("Mac::getResidentMemoryUsageBytes");
  struct task_basic_info info;
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(),
                                 TASK_BASIC_INFO,
                                 (task_info_t)&info,
                                 &size);
    if (kr != KERN_SUCCESS) return -1;
    return static_cast<qint64>(info.resident_size);
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

// Helper function to check if a device is a USB storage device
bool isUsbDevice(const QString& bsdName) {
    bool isUsb = false;
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOMediaClass);
    if (!matchingDict) return false;

    io_iterator_t iterator;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iterator) == KERN_SUCCESS) {
        io_registry_entry_t media;
        while ((media = IOIteratorNext(iterator))) {
            CFTypeRef bsdNameRef = IORegistryEntryCreateCFProperty(media, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
            if (bsdNameRef && CFGetTypeID(bsdNameRef) == CFStringGetTypeID()) {
                QString mediaBsdName = QString::fromCFString(static_cast<CFStringRef>(bsdNameRef));
                CFRelease(bsdNameRef);

                if (mediaBsdName == bsdName) {
                    io_registry_entry_t parent;
                    if (IORegistryEntryGetParentEntry(media, kIOServicePlane, &parent) == KERN_SUCCESS) {
                        if (IOObjectConformsTo(parent, kIOUSBDeviceClassName)) {
                            isUsb = true;
                        }
                        IOObjectRelease(parent);
                    }
                }
            }
            if (bsdNameRef) CFRelease(bsdNameRef);
            IOObjectRelease(media);
            if (isUsb) break;
        }
        IOObjectRelease(iterator);
    }
    return isUsb;
}


QStringList Mac::listMountedVolumes()
{
    // if (G::isLogger)
        G::log("Mac::listMountedVolumes");
    QStringList usbVolumes;
    struct statfs *mnts;
    int mountCount = getmntinfo(&mnts, MNT_NOWAIT);

    if (mountCount == 0) {
        return usbVolumes; // No mounted volumes
    }

    for (int i = 0; i < mountCount; i++) {
        QString devicePath = QString::fromUtf8(mnts[i].f_mntfromname);
        QString mountPoint = QString::fromUtf8(mnts[i].f_mntonname);

        // Extract BSD name from device path (e.g., "/dev/disk2s1" -> "disk2s1")
        if (devicePath.startsWith("/dev/disk")) {
            QString bsdName = devicePath.mid(5); // Remove "/dev/"

            // Check if it's a USB device
            if (isUsbDevice(bsdName)) {
                QString volumeName = mountPoint.section('/', -1); // Extract last part of path
                usbVolumes.append(volumeName + " (" + mountPoint + ")");
            }
        }
    }
    return usbVolumes;

  /*
    if (G::isLogger) G::log("Mac::listMountedVolumes");

    QStringList usbList;
    CFMutableDictionaryRef matchingDict;
    io_iterator_t iter;
    io_service_t service;

    // Match all USB devices
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    if (!matchingDict) {
        G::issue("Error", "IOServiceMatching failed for USB devices", "Mac::listUsbDevices");
        return usbList;
    }

    // Get iterator of matching devices
    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iter);
    if (kr != KERN_SUCCESS) {
        G::issue("Error", "IOServiceGetMatchingServices failed", "Mac::listMountedVolumes");
        return usbList;
    }

    // Iterate through devices
    while ((service = IOIteratorNext(iter))) {
        CFStringRef cfVendor, cfProduct;
        uint16_t vendorID, productID;
        io_name_t deviceName;

        // Get vendor name
        cfVendor = (CFStringRef)IORegistryEntryCreateCFProperty(service, CFSTR(kUSBVendorString), kCFAllocatorDefault, 0);
        cfProduct = (CFStringRef)IORegistryEntryCreateCFProperty(service, CFSTR(kUSBProductString), kCFAllocatorDefault, 0);

        IORegistryEntryGetName(service, deviceName);

        // Get vendor ID and product ID
        CFNumberRef vendorIDRef = (CFNumberRef)IORegistryEntryCreateCFProperty(service, CFSTR(kUSBVendorID), kCFAllocatorDefault, 0);
        CFNumberRef productIDRef = (CFNumberRef)IORegistryEntryCreateCFProperty(service, CFSTR(kUSBProductID), kCFAllocatorDefault, 0);
        if (vendorIDRef) {
            CFNumberGetValue(vendorIDRef, kCFNumberSInt16Type, &vendorID);
            CFRelease(vendorIDRef);
        }
        if (productIDRef) {
            CFNumberGetValue(productIDRef, kCFNumberSInt16Type, &productID);
            CFRelease(productIDRef);
        }

        // Convert to QString
        QString vendor = cfVendor ? QString::fromCFString(cfVendor) : "Unknown Vendor";
        QString product = cfProduct ? QString::fromCFString(cfProduct) : "Unknown Product";
        QString name = QString(deviceName);
        QString deviceInfo = QString("%1 (%2) - VID: %3, PID: %4")
            .arg(vendor, product)
            .arg(vendorID, 4, 16, QChar('0'))
            .arg(productID, 4, 16, QChar('0'));

        usbList.append(deviceInfo);

        if (cfVendor) CFRelease(cfVendor);
        if (cfProduct) CFRelease(cfProduct);
        IOObjectRelease(service);
    }

    IOObjectRelease(iter);
    return usbList;
*/
}
