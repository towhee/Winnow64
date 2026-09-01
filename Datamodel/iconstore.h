#ifndef ICONSTORE_H
#define ICONSTORE_H

#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QString>

/*
    THUMBNAILS, KEYED BY PATH.

    Icons used to live in the datamodel as QIcon on Qt::DecorationRole of column
    0. Moving them here is the same move ImageCache's bookkeeping and the row
    store already made, for the same three reasons:

    ADDRESSABILITY. A thumbnail belongs to an IMAGE, not to a row. Rows shift
    under sorting and filtering and are rebuilt on every folder change, so an
    icon held by row has to be found again every time the proxy moves, while an
    icon held by path is simply still there. icd->imCache is keyed by path for
    exactly this reason, and now the two agree.

    IT IS WHAT THE CATALOG NEEDS. Scrolling a 250,000-image catalog cannot open
    a file per row, so the next layer is a thumbnail cached in the index, keyed
    by pathkey. "Memory store, then index, then decode" only reads as one lookup
    chain if the memory layer is keyed the same way as the index layer.

    IT TAKES THE THUMBNAIL OFF THE ROW. A QStandardItem cell holding a QIcon
    carries the item, its role list and a pointer slot on top of the pixmap;
    more to the point, an icon on the row is an icon the row store cannot
    replace, and the row store is what makes a whole catalog fit in memory.

    WHAT THIS IS NOT: a change of eviction POLICY. What gets loaded and what
    gets dropped is still decided exactly where it was -- the icon chunk range
    (DataModel::resolveIconChunkSize, refineIconChunkSize,
    applyIconCachePressure, clearIconsOutsideChunkRange). Those call setData
    with an empty QVariant on DecorationRole, which now removes from here. Only
    the storage moved; the policy is untouched, deliberately, so that if
    thumbnails start behaving differently it is this file's fault and not a
    policy change hiding inside a storage change.

    THREADING. GUI thread only, like the QStandardItems it replaces. Icons
    arrive from Reader/FrameDecoder threads through the existing queued
    setIcon/setIconFromVideoFrame signals and are stored on the GUI thread.
*/
class IconStore
{
public:
    bool contains(const QString &path) const { return mIcons.contains(path); }

    QIcon icon(const QString &path) const { return mIcons.value(path); }

    void insert(const QString &path, const QIcon &icon)
    {
        if (path.isEmpty()) return;
        auto it = mIcons.find(path);
        if (it != mIcons.end()) mBytes -= iconBytes(it.value());
        mIcons.insert(path, icon);
        mBytes += iconBytes(icon);
    }

    void remove(const QString &path)
    {
        auto it = mIcons.find(path);
        if (it == mIcons.end()) return;
        mBytes -= iconBytes(it.value());
        mIcons.erase(it);
    }

    void clear() { mIcons.clear(); mBytes = 0; }

    int count() const { return mIcons.size(); }
    qint64 bytes() const { return mBytes; }

private:
    /*  A QIcon may hold several sizes; Winnow stores exactly one pixmap per
        icon (see DataModel::setIcon1), so the first available size is the
        whole of it. 4 bytes per pixel is the ARGB32 the thumbnails are in. */
    static qint64 iconBytes(const QIcon &icon)
    {
        const QList<QSize> sizes = icon.availableSizes();
        if (sizes.isEmpty()) return 0;
        const QSize s = sizes.first();
        return qint64(s.width()) * s.height() * 4;
    }

    QHash<QString, QIcon> mIcons;
    qint64 mBytes = 0;
};

#endif // ICONSTORE_H
