#include "Utilities/waitcursor.h"
#include <QColorSpace>
#include <QImage>
#include <QPixmap>

QCursor WaitCursor::Get(Qt::CursorShape shape)
{
#ifdef Q_OS_MAC
    Q_UNUSED(shape)
    /* Built once and never freed: a QCursor holding a pixmap must not be destroyed
       after QGuiApplication, which a function static would be. */
    static const QCursor *cursor = [] {
        QImage img(":/qt-project.org/mac/cursors/images/spincursor.png");
        // the native arrow needs no image conversion, so it cannot hit the crash
        if (img.isNull()) return new QCursor(Qt::ArrowCursor);
        img.setColorSpace(QColorSpace());
        return new QCursor(QPixmap::fromImage(img));    // hotspot -1,-1 = centre
    }();
    return *cursor;
#else
    return QCursor(shape);
#endif
}
