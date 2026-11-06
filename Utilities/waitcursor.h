#ifndef WAITCURSOR_H
#define WAITCURSOR_H

#include <QCursor>

/*
    Use WaitCursor::Get() INSTEAD of Qt::WaitCursor / Qt::BusyCursor in
    setOverrideCursor(). On macOS Qt draws both from a PNG in its own resources that
    carries an ICC profile; QImage::toCGImage() turns that profile into a CGColorSpace
    and hands it to CGImageCreate WITHOUT a null check, so whenever CoreGraphics
    declines the profile the app dies in CGColorSpaceGetType (EXC_BREAKPOINT inside
    QGuiApplication::setOverrideCursor -- seen from Auto tone, filterChange and
    loadEntireMetadataCache). The cursor built here is the same image with its colour
    space cleared, which sends toCGImage down its plain sRGB branch. Elsewhere it is
    just the Qt shape.
*/
namespace WaitCursor {
QCursor Get(Qt::CursorShape shape = Qt::WaitCursor);
}

#endif // WAITCURSOR_H
