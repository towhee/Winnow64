#include "Main/mainwindow.h"
#include "Undo/undomanager.h"

/*
    EDIT > UNDO / REDO, ROUTED BY MODE.

    Cmd+Z (Ctrl+Z) and Cmd+Shift+Z (Ctrl+Y / Ctrl+Shift+Z) mean "take back what I just
    did" -- and what the user just did depends on the mode:

      Develop   1. a Brush or Object mask being painted takes back its last stroke
                   (ImageView::undoMaskStroke), as the tool always has;
                2. otherwise the current image's History steps one entry back or
                   forward (DevelopProperties::stepHistory) -- the same revert a click
                   on the History row makes, linked multi-image steps included.
      Preview   the app-wide undoManager (Undo/undomanager.h): picks, ratings,
                keywords, file operations ... as each operation is wired in.

    A focused text field keeps its own undo: QLineEdit / QTextEdit claim the keys through
    ShortcutOverride before the window-level actions see them.

    The action's text names the step ("Undo Exposure", "Undo Rating") and, when there is
    nothing to undo, it is disabled with a reason (disabledReason), like every gated item.
    See "Undo" in notes/Documentation.md.
*/

static bool inDevelopMode()
{
    return G::operationMode == G::OperationMode::Develop;
}

void MW::undoEdit()
{
    if (G::isLogger) G::log("MW::undoEdit");
    if (inDevelopMode()) {
        if (imageView && imageView->undoMaskStroke()) return;
        if (developProperties) developProperties->stepHistory(-1);
    }
    else if (undoManager) {
        undoManager->undo();
    }
    refreshUndoActions();
}

void MW::redoEdit()
{
    if (G::isLogger) G::log("MW::redoEdit");
    if (inDevelopMode()) {
        if (developProperties) developProperties->stepHistory(+1);
    }
    else if (undoManager) {
        undoManager->redo();
    }
    refreshUndoActions();
}

void MW::refreshUndoActions()
{
    if (!undoAction || !redoAction) return;

    bool canUndo = false, canRedo = false;
    /* The *Why strings are lowercase fragments: MW::actionDisabledReason shows one as
       "Undo — <reason>." when the disabled shortcut is pressed. */
    QString undoWhat, redoWhat, undoWhy, redoWhy;
    if (inDevelopMode()) {
        if (imageView && imageView->hasMaskStrokeToUndo()) {
            canUndo = true;
            undoWhat = tr("Stroke");
        }
        else if (developProperties && developProperties->canStepHistory(-1)) {
            canUndo = true;
            undoWhat = developProperties->historyStepText(-1);
        }
        if (developProperties && developProperties->canStepHistory(+1)) {
            canRedo = true;
            redoWhat = developProperties->historyStepText(+1);
        }
        undoWhy = tr("this image's History is at its first step");
        redoWhy = tr("this image's History is at its latest step");
    }
    else if (undoManager) {
        canUndo = undoManager->canUndo();
        canRedo = undoManager->canRedo();
        undoWhat = undoManager->undoText();
        redoWhat = undoManager->redoText();
        undoWhy = tr("nothing undoable has been done this session");
        redoWhy = tr("nothing has been undone");
    }

    undoAction->setText(undoWhat.isEmpty() ? tr("Undo") : tr("Undo %1").arg(undoWhat));
    redoAction->setText(redoWhat.isEmpty() ? tr("Redo") : tr("Redo %1").arg(redoWhat));
    /* The reason first: setEnabled emits QAction::changed, which readers of the
       reason listen to (see enableSelectionDependentMenus). */
    undoAction->setProperty("disabledReason", canUndo ? QString() : undoWhy);
    redoAction->setProperty("disabledReason", canRedo ? QString() : redoWhy);
    undoAction->setEnabled(canUndo);
    redoAction->setEnabled(canRedo);
}
