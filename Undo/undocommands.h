#ifndef UNDOCOMMANDS_H
#define UNDOCOMMANDS_H

#include <QList>
#include <QPair>
#include <QString>
#include <QVariant>
#include <functional>
#include "Undo/undomanager.h"

/*
    The commands MW records (see Undo/undomanager.h for the rules every command follows).

    PathValues is the state of some field per image, keyed by FILE PATH (or version key)
    -- never by row, which moves with sort, filter and folder changes. A list rather than
    a hash so undo and redo walk the images in the order the edit did.
*/
using PathValues = QList<QPair<QString, QVariant>>;

/*
    The common shape: a field had `before` on these images and the edit left `after`.
    apply writes a PathValues back through the same path the edit took (MW::applyRatings,
    MW::applyPickStates, ...), so the file, the model, the catalog and the filters all
    follow, and handles images the datamodel no longer holds itself.
*/
class PathValuesCommand : public UndoCommand
{
public:
    using Apply = std::function<void(const PathValues &)>;
    PathValuesCommand(const QString &text, const PathValues &before,
                      const PathValues &after, Apply apply)
        : UndoCommand(text), before(before), after(after), apply(std::move(apply)) {}

protected:
    void doUndo() override { if (apply) apply(before); }
    void doRedo() override { if (apply) apply(after); }

private:
    PathValues before;
    PathValues after;
    Apply apply;
};

/*
    An edit whose undo is an action rather than a stored value: a rotation is undone by
    the opposite rotation. undoFn and redoFn each run the whole change.
*/
class ActionCommand : public UndoCommand
{
public:
    ActionCommand(const QString &text, std::function<void()> undoFn,
                  std::function<void()> redoFn)
        : UndoCommand(text), undoFn(std::move(undoFn)), redoFn(std::move(redoFn)) {}

protected:
    void doUndo() override { if (undoFn) undoFn(); }
    void doRedo() override { if (redoFn) redoFn(); }

private:
    std::function<void()> undoFn;
    std::function<void()> redoFn;
};

/*
    An ActionCommand whose undo or redo can fail and must then leave the step where it
    was: file operations, which wait for a load, need their files where they were left,
    and a destination that is still free. Each function returns false, having said why,
    to refuse (UndoCommand::refuse).
*/
class CheckedCommand : public UndoCommand
{
public:
    CheckedCommand(const QString &text, std::function<bool()> undoFn,
                   std::function<bool()> redoFn)
        : UndoCommand(text), undoFn(std::move(undoFn)), redoFn(std::move(redoFn)) {}

protected:
    void doUndo() override { if (undoFn && !undoFn()) refuse(); }
    void doRedo() override { if (redoFn && !redoFn()) refuse(); }

private:
    std::function<bool()> undoFn;
    std::function<bool()> redoFn;
};

/* Drop the images whose value the edit did not change (before and after are parallel),
   so a step never claims, or rewrites on undo, an image it did not touch. */
inline void pruneUnchanged(PathValues &before, PathValues &after)
{
    PathValues b, a;
    for (int i = 0; i < before.size() && i < after.size(); ++i) {
        if (before.at(i).second == after.at(i).second) continue;
        b.append(before.at(i));
        a.append(after.at(i));
    }
    before.swap(b);
    after.swap(a);
}

/* The images' count for a step's name: "Rating (12 images)". */
inline QString undoStepText(const QString &what, int images)
{
    return images == 1 ? what : QString("%1 (%2 images)").arg(what).arg(images);
}

#endif // UNDOCOMMANDS_H
