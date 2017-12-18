#include "macos/macscale.h"
#include "macos/macscale_p.h"
#import <AppKit/AppKit.h>

namespace QtMac {
QSizeF macBackingScaleFactor() {
    NSScreen *screen = [NSScreen mainScreen];
    NSDictionary *description = [screen deviceDescription];
    NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
    return QSizeF(displayPixelSize.width, displayPixelSize.height);

//    return [[NSScreen mainScreen] backingScaleFactor];
}
}  // namespace QtMac
