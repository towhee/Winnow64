#include "tiffhandler.h"

TiffHandler::TiffHandler()
{

}

void TiffHandler::test(const char* fPath)
{
    TIFF* tif = TIFFOpen(fPath, "r");
    if (tif) {
        int dircount = 0;
        do {
            dircount++;
        } while (TIFFReadDirectory(tif));
        printf("%d directories in %s\n", dircount, fPath);
        TIFFClose(tif);
    }
}
