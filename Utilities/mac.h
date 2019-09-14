#ifndef MAC_H
#define MAC_H

#include <sys/types.h>
#include <sys/sysctl.h>

#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>

#include "Main/global.h"                // req'd by availableMemory

class Mac
{
public:
    static void availableMemory();
};

#endif // MAC_H
