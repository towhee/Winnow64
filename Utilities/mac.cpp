#include "mac.h"

void Mac::availableMemory()
{
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__);
    ColorSyncIteratorData data;
    data.dispuuid = CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID());
    data.url = nullptr;//NULL;
    ColorSyncIterateDeviceProfiles(colorSyncIterateCallback, (void *)&data);
    CFRelease(data.dispuuid);
    CFStringRef urlstr = CFURLCopyFileSystemPath(data.url, kCFURLPOSIXPathStyle);
    CFRelease(data.url);
    qDebug() << __FUNCTION__ << QString::fromCFString(urlstr);
    return QString::fromCFString(urlstr);
}

