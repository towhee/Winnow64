#ifndef TIFFHANDLER_H
#define TIFFHANDLER_H

#include <QtWidgets>
extern "C" {
#include "tiffio.h"
}

class TiffHandler
{
public:
    TiffHandler();
    void test(const char* fPath);
};

#endif // TIFFHANDLER_H
