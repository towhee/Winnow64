#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H

#include <QObject>
#include <QString>
#include <QUndoCommand>

class QUndoStack;

/*
    APP-WIDE UNDO (Edit > Undo / Redo outside Develop).

    Develop keeps its own per-image History (Develop/History/develophistory.h); MW routes
    Cmd+Z there while in Develop mode. Everything else -- picks, ratings, keywords,
    collections, file operations -- records here. See "Undo" in notes/Documentation.md.

    RECORD AFTER THE FACT. Winnow's operations run first (they have their own dialogs,
    progress and error handling) and then push a command describing what they did. So a
    command's FIRST redo() -- the one QUndoStack::push makes -- does nothing; only a real
    Redo re-applies it. UndoCommand handles that; subclasses implement doUndo / doRedo.

    NO RE-RECORDING. Undo and redo usually re-run the same setters the user's action ran
    (setRating, writeXMP, ...), and those setters record. While a command is being applied
    isApplying() is true and push() discards anything offered, so recording sites need
    no guard of their own (they may still test isApplying() to skip building a command).

    KEY BY PATH, NOT ROW. Rows move with sort, filter and folder changes; a command must
    find its images by file path and cope with an image that is no longer loaded.

    Session only: the stack is not persisted. Capped at kUndoLimit steps.
*/

class UndoCommand : public QUndoCommand
{
public:
    explicit UndoCommand(const QString &text, QUndoCommand *parent = nullptr)
        : QUndoCommand(text, parent) {}

    /* final: the skip-first-redo rule must hold for every command. */
    void redo() final;
    void undo() final;

protected:
    virtual void doRedo() {}
    virtual void doUndo() {}

private:
    friend class UndoManager;
    bool done = false;      // the action ran before push(); the first redo() is a no-op
};

class UndoManager : public QObject
{
    Q_OBJECT
public:
    explicit UndoManager(QObject *parent = nullptr);

    static constexpr int kUndoLimit = 200;

    /* Takes ownership. Discarded (deleted) while a command is being applied. */
    void push(UndoCommand *cmd);

    /* Group several pushes into one step ("Rate 12 images"). Nestable; the outermost
       text names the step. Not QUndoStack's own macros: those push a step even when
       nothing was recorded, which would leave a Cmd+Z that does nothing. Here an empty
       group records nothing and a group of one is pushed as that command. */
    void beginMacro(const QString &text);
    void endMacro();

    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;

    void undo();
    void redo();
    void clear();

    bool isApplying() const { return applying; }

signals:
    /* The stack changed: a push, an undo/redo or a clear. MW refreshes Edit > Undo. */
    void changed();

private:
    QUndoStack *stack;
    bool applying = false;
    int macroDepth = 0;
    QString macroText;
    QList<UndoCommand *> macroCmds;     // collected while macroDepth > 0
};

/* RAII macro: UndoMacro m(undoManager, "Rate 12 images"); every push until m goes out
   of scope becomes one step. A null manager is allowed (does nothing). */
class UndoMacro
{
public:
    UndoMacro(UndoManager *m, const QString &text) : mgr(m) {
        if (mgr) mgr->beginMacro(text);
    }
    ~UndoMacro() { if (mgr) mgr->endMacro(); }
    UndoMacro(const UndoMacro &) = delete;
    UndoMacro &operator=(const UndoMacro &) = delete;
private:
    UndoManager *mgr;
};

#endif // UNDOMANAGER_H
