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

void MW::applyInfoField(const QString &field, const PathValues &values)
{
/*
    Edit > Undo / Redo of a Metadata panel text edit (InfoView::dataChanged): write each
    file's value back the way the edit wrote it -- model (edit and tooltip roles), then
    the sidecar or file (writeXMP), then the file's versions -- and refresh what reads
    the field. A file no longer loaded takes the value straight into its sidecar.
*/
    if (G::isLogger) G::log("MW::applyInfoField", field);
    if (!dm || !metadata) return;
    struct Field { int column; QByteArray xmpItem; };
    static const QHash<QString, Field> fields = {
        {"Title*",     {G::TitleColumn,     "title"}},
        {"Creator*",   {G::CreatorColumn,   "creator"}},
        {"Copyright*", {G::CopyrightColumn, "rights"}},
        {"Email*",     {G::EmailColumn,     "email"}},
        {"Url*",       {G::UrlColumn,       "url"}}};
    if (!fields.contains(field)) return;
    const Field f = fields.value(field);
    const QString src = "MW::applyInfoField";

    for (const auto &pv : values) {
        const QString fPath = pv.first;
        const QString s = pv.second.toString();
        const int dmRow = dm->rowFromKey(fPath);
        if (dmRow < 0) {
            metadata->writeItemsToSidecar(fPath, {{f.xmpItem, s.toUtf8()}},
                                          /*allowEmbed*/ true, src);
            continue;
        }
        emit setValDm(dmRow, f.column, s, dm->instance, src, Qt::EditRole);
        emit setValDm(dmRow, f.column, s, dm->instance, src, Qt::ToolTipRole);
        dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
        metadata->writeXMP(fPath, src);
        dm->syncSharedToVersions(fPath);
    }

    // the Title and Creator filter categories read these (see InfoView::dataChanged)
    if (f.column == G::TitleColumn || f.column == G::CreatorColumn) {
        dm->sf->suspend(true, src);
        // runSync: filterChange follows -- see BuildFilters::updateCategory
        buildFilters->updateCategory(f.column == G::TitleColumn
                                         ? BuildFilters::TitleEdit
                                         : BuildFilters::CreatorEdit,
                                     BuildFilters::NoAfterAction, /*runSync*/ true);
        filterChange(src);
    }
    if (G::useInfoView && infoView) {
        if (dm->currentSfRow >= 0) infoView->updateInfo(dm->currentSfRow);
        emit infoView->dataEdited();        // Embel texts and the loupe's info overlay
    }
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
