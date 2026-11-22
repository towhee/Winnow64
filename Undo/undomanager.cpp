#include "Undo/undomanager.h"

#include <QUndoStack>

void UndoCommand::redo()
{
    if (!done) { done = true; return; }     // the push: the action already ran
    doRedo();
}

void UndoCommand::undo()
{
    doUndo();
}

namespace {

/* One step made of several recorded commands (UndoManager::beginMacro). They had all
   run, and been marked done, before the group was pushed. Undo runs them newest first,
   redo oldest first. */
class GroupCommand : public UndoCommand
{
public:
    GroupCommand(const QString &text, const QList<UndoCommand *> &cmds)
        : UndoCommand(text), cmds(cmds) {}
    ~GroupCommand() override { qDeleteAll(cmds); }

protected:
    void doRedo() override {
        for (UndoCommand *c : cmds) c->redo();
    }
    void doUndo() override {
        for (auto it = cmds.crbegin(); it != cmds.crend(); ++it) (*it)->undo();
    }

private:
    QList<UndoCommand *> cmds;
};

/* Sets a flag for the life of a scope, restoring the old value (re-entrant safe). */
struct FlagGuard {
    bool &flag;
    bool old;
    explicit FlagGuard(bool &f) : flag(f), old(f) { flag = true; }
    ~FlagGuard() { flag = old; }
};

} // namespace

UndoManager::UndoManager(QObject *parent)
    : QObject(parent),
      stack(new QUndoStack(this))
{
    stack->setUndoLimit(kUndoLimit);
    connect(stack, &QUndoStack::indexChanged, this, &UndoManager::changed);
    connect(stack, &QUndoStack::cleanChanged, this, &UndoManager::changed);
}

void UndoManager::push(UndoCommand *cmd)
{
    if (!cmd) return;
    if (applying) { delete cmd; return; }
    if (macroDepth > 0) {
        cmd->done = true;       // ran already; the group's redo must re-apply it
        macroCmds.append(cmd);
        return;
    }
    stack->push(cmd);
    emit changed();
}

void UndoManager::beginMacro(const QString &text)
{
    if (applying) return;
    if (macroDepth++ == 0) {
        macroText = text;
        macroCmds.clear();
    }
}

void UndoManager::endMacro()
{
    if (applying || macroDepth == 0) return;
    if (--macroDepth > 0) return;
    QList<UndoCommand *> cmds;
    cmds.swap(macroCmds);
    if (cmds.isEmpty()) return;
    UndoCommand *step;
    if (cmds.size() == 1) {
        step = cmds.first();
        step->done = false;         // pushed on its own: skip the push's redo()
        if (!macroText.isEmpty()) step->setText(macroText);
    }
    else {
        step = new GroupCommand(macroText.isEmpty() ? cmds.last()->text() : macroText,
                                cmds);
    }
    stack->push(step);
    emit changed();
}

bool UndoManager::canUndo() const { return stack->canUndo(); }
bool UndoManager::canRedo() const { return stack->canRedo(); }
QString UndoManager::undoText() const { return stack->undoText(); }
QString UndoManager::redoText() const { return stack->redoText(); }

void UndoManager::undo()
{
    if (applying || !stack->canUndo()) return;
    {
        FlagGuard g(applying);
        stack->undo();
    }
    emit changed();
}

void UndoManager::redo()
{
    if (applying || !stack->canRedo()) return;
    {
        FlagGuard g(applying);
        stack->redo();
    }
    emit changed();
}

void UndoManager::clear()
{
    if (applying) return;
    stack->clear();
    emit changed();
}
