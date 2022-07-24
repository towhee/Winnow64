#include "Cache/cachedata.h"

ImageCacheData::ImageCacheData(QObject *) {}

IconCacheData::IconCacheData(QObject *) {}

void IconCacheData::add(int dmRow)
{
    rowsWithIcon.append(dmRow);
}

void IconCacheData::remove(int dmRow)
{
    rowsWithIcon.remove(dmRow);
}

int IconCacheData::size()
{
    return rowsWithIcon.size();
}
