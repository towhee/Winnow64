#ifndef DRAGINTENT_H
#define DRAGINTENT_H

#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QStringList>

/*
    THUMBNAIL DRAG RULES (IconView::startDrag). A drag out of the thumbnails never lets
    another application MOVE the files: outside Winnow it is always a copy, of the images
    alone unless the drag started with Shift. Inside Winnow no modifier moves and Opt/Alt
    copies, and the file targets (FSTree, BookMarks) carry each image's sidecars
    themselves through FileOps.

    WHO DELETES THE ORIGINALS. The source does, but only the files a Winnow target says
    it copied for a move (reportMoved). The old rule -- "the drag returned MoveAction, so
    trash everything dragged" -- trashed originals the target had skipped ("already in
    destination folder"), and when Finder took the Move it trashed the sidecars of
    images Finder had just moved.

    HOW A TARGET KNOWS IT IS A MOVE (isMove). On macOS Mac::installDragMaskHook lets the
    drag offer Copy+Move inside Winnow and Copy alone outside, so the drop action is
    real (nativeMove). Elsewhere, or if that hook could not be installed, the drag offers
    Copy only, and a Winnow thumbnail drag (kIntentMime) without Alt held at the drop is
    a move.

    All of this runs on the GUI thread: a drag and its drops are one nested event loop.
*/
namespace DragIntent {

inline const char *kIntentMime = "application/x-winnow-thumb-drag";

// true when the drag offers a real MoveAction (macOS with the mask hook installed)
inline bool nativeMove = false;

/* True while a thumbnail drag runs (IconView::startDrag sets it around exec). The live
   drag message owns the status bar then; a target must not write its own status. */
inline bool thumbDragActive = false;

inline QStringList &movedList()
{
    static QStringList moved;
    return moved;
}

inline bool isWinnowThumbDrag(const QMimeData *m)
{
    return m && m->hasFormat(kIntentMime);
}

inline bool isMove(const QDropEvent *event)
{
    if (!event || !event->source() || !isWinnowThumbDrag(event->mimeData())) return false;
    if (nativeMove) return event->dropAction() == Qt::MoveAction;
    return !(QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier);
}

// A Winnow file target: these sources were copied for a move; the source trashes them.
inline void reportMoved(const QStringList &srcs) { movedList() << srcs; }

inline QStringList takeMoved()
{
    QStringList m = movedList();
    movedList().clear();
    return m;
}

}  // namespace DragIntent

#endif // DRAGINTENT_H
