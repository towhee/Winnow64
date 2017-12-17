#include "macos/macscale.h"
#include "macos/macscale_p.h"
#import <AppKit/AppKit.h>

namespace QtMac {
float macBackingScaleFactor() {
        return [[NSScreen mainScreen] backingScaleFactor];
}
}  // namespace QtMac
