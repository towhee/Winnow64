#include "Main/mainwindow.h"
#include "Dialogs/keywordretagdlg.h"
#include "Dialogs/keywordtidydlg.h"
#include "Metadata/keywordpaths.h"

#include <QApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>

#include <functional>

namespace {

/*
    THE TWO PROPERTIES A SET OF PATHS IS WRITTEN AS.

    dc:subject gets the LEAF of every path plus any depth-1 path (which is a leaf
    already); lr:hierarchicalSubject gets the paths of depth 2 or more, whole. A depth-1
    path is deliberately NOT written as a one-element hierarchical entry: it carries no
    hierarchy, it is fewer bytes, and it is what an application that has never heard of
    lr: writes.

    READING THIS BACK IS IDEMPOTENT, which is the property that matters.
    keywordEffectivePaths consumes exactly the dc:subject entries that name a node of one
    of the paths -- which is precisely the set emitted here -- leaving the depth-1 entries
    as roots. So the input list is reconstructed exactly, and a second write of an
    unchanged image is a no-op rather than a slow drift.

    dc:subject IS DE-DUPLICATED, because two keywords can share a leaf: the two Vancouvers
    would otherwise put "Vancouver" into dc:subject twice, which no other application does
    and which reads as a duplicate tag anywhere this file is opened. It costs nothing on
    read-back -- one entry is consumed by both paths, and both paths survive in
    lr:hierarchicalSubject.

    SHARED BY ALL THREE WRITERS -- the dock, a retag after a rename, and the flat-keyword
    tidy -- because an image tagged from one and rewritten by another must end up with the
    same shape, and three copies of this is three chances for one of them to drift.
*/
void composeKeywordWrite(const QStringList &paths, QStringList &subject,
                         QStringList &hierarchical)
{
    QSet<QString> subjectSeen;
    for (const QString &p : paths) {
        const QString leaf = keywordLeafOf(p);
        const QString leafFold = keywordFold(leaf);
        if (!leafFold.isEmpty() && !subjectSeen.contains(leafFold)) {
            subjectSeen.insert(leafFold);
            subject << leaf;
        }
        if (p.contains('|')) hierarchical << p;
    }
}

}  // namespace

/*
    APPLYING KEYWORDS TO A SELECTION.

    Modelled line for line on MW::setRating (Main/sortandfilter.cpp), because a metadata
    edit has a fixed shape in this app and departing from it is how the fiddly parts get
    missed: snapshot the selection before touching the model, mirror the edit onto the
    hidden half of a raw+jpg pair, keep the catalog in step per row, and only then rebuild
    the filters and let the proxy re-run.

    ONE THING IS DELIBERATELY THE OPPOSITE WAY ROUND, and it is the trap in this file.
    setRating writes the MODEL first and the file second. Keywords write the FILE first
    and the model second. The reason is that every other edited field carries a shadow
    COLUMN in the datamodel -- G::_RatingColumn beside G::RatingColumn -- and keywords
    deliberately do not, because two more QStringList columns at 250k rows is real memory
    spent to re-derive what the model already holds. DataModel::imMetadata therefore takes
    the shadow from the model's own value, so a model updated FIRST would make the shadow
    equal to the new value and Metadata::writeXMP would conclude nothing had changed and
    write nothing at all. Silently.

    Anyone tidying this to match setRating will break keyword writing without breaking a
    build or a test that does not exist yet. That is why it is written down twice -- here
    and at the shadow assignment in DataModel::imMetadata.
*/

QMap<QString, int> MW::keywordsInSelection() const
{
/*
    Every keyword path carried by the selection, with how many of the selected images
    carry it. The tag zone shows a tag per key and marks the ones whose count is below
    the selection size as partial.

    READ FROM THE LITERAL AND HIERARCHICAL COLUMNS, not from G::KeywordsAllColumn. That
    column holds the prefix EXPANSION -- every ancestor of every path -- which is what
    gets filtered on and is emphatically not what the user assigned. Showing it would put
    a tag for "Fauna" on an image the user only ever tagged "Fauna|Bird|Heron", and
    removing that tag would then have to mean something.
*/
    QMap<QString, int> out;
    if (!dm || !sel) return out;

    /*  dm->selectionModel->selectedRows(), NOT Selection::selectedRows. That member
        exists, compiles, and is NEVER ASSIGNED -- the only code that filled it is inside
        a commented-out "old code" block in Selection::save. Reading it returns an empty
        list forever, so the tag zone reported "No images selected" with an image
        selected, and applyKeywordsToSelection quietly did nothing at all. The live
        selection is the QItemSelectionModel's, which is what MW::setRating uses. */
    const QModelIndexList selection = dm->selectionModel->selectedRows();
    for (const QModelIndex &sfIdx : selection) {
        const int dmRow = dm->modelRowFromProxyRow(sfIdx.row());
        if (dmRow < 0) continue;
        const QStringList literal =
            dm->index(dmRow, G::KeywordsColumn).data().toStringList();
        const QStringList paths =
            dm->index(dmRow, G::KeywordPathsColumn).data().toStringList();
        for (const QString &p : keywordEffectivePaths(literal, paths)) out[p]++;
    }
    return out;
}

void MW::applyKeywordsToSelection(const QStringList &add, const QStringList &remove)
{
/*
    Add and/or remove keyword paths across the selection.

    ADD AND REMOVE IN ONE PASS rather than two calls. Re-parenting a keyword is a remove
    and an add of the same image, and doing them as two passes would write every sidecar
    twice and fire two filter rebuilds for one user action.

    REMOVING A PATH REMOVES ITS SUBTREE. Taking "Fauna|Bird" off an image cannot sensibly
    leave "Fauna|Bird|Heron" on it -- the path would name a parent the image no longer
    claims. keywordIsDescendant is the same test the vocabulary tree uses to reject a drop
    onto a node's own descendant, shared so the two cannot disagree about what "beneath"
    means.
*/
    if (G::isLogger) G::log("MW::applyKeywordsToSelection");
    if (!dm || !sel || !metadata) return;
    if (add.isEmpty() && remove.isEmpty()) return;

    /*  dm->selectionModel->selectedRows(), NOT Selection::selectedRows. That member
        exists, compiles, and is NEVER ASSIGNED -- the only code that filled it is inside
        a commented-out "old code" block in Selection::save. Reading it returns an empty
        list forever, so the tag zone reported "No images selected" with an image
        selected, and applyKeywordsToSelection quietly did nothing at all. The live
        selection is the QItemSelectionModel's, which is what MW::setRating uses. */
    const QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    /*  Snapshot the rows FIRST. The proxy re-filters as the model changes, so iterating
        the selection while editing it walks off the end of a list that is moving. Same
        reason, same fix, as MW::setRating. */
    QList<int> rows;
    rows.reserve(selection.size());
    for (const QModelIndex &sfIdx : selection) {
        const int dmRow = dm->modelRowFromProxyRow(sfIdx.row());
        if (dmRow >= 0) rows.append(dmRow);
    }
    if (rows.isEmpty()) return;

    QStringList removeFolded;
    for (const QString &r : remove) removeFolded << keywordFold(r);

    const QString src = "MW::applyKeywordsToSelection";
    int written = 0;

    for (int i = 0; i < rows.size(); ++i) {
        const int dmRow = rows.at(i);
        const QString fPath =
            dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        if (fPath.isEmpty()) continue;

        /*  What the image carries now, as PATHS: the literal dc:subject list and the
            hierarchical list resolved through leaf consumption, so the tag Lightroom
            wrote twice is one entry rather than two. */
        const QStringList literal =
            dm->index(dmRow, G::KeywordsColumn).data().toStringList();
        const QStringList paths =
            dm->index(dmRow, G::KeywordPathsColumn).data().toStringList();

        QStringList have = keywordEffectivePaths(literal, paths);

        QStringList next;
        QSet<QString> seen;
        for (const QString &p : have) {
            const QString fold = keywordFold(p);
            bool drop = false;
            for (const QString &r : removeFolded)
                if (keywordIsDescendant(fold, r)) { drop = true; break; }
            if (drop) continue;
            if (seen.contains(fold)) continue;
            seen.insert(fold);
            next << p;
        }
        for (const QString &a : add) {
            const QString trimmed = keywordNodes(a).join('|');
            if (trimmed.isEmpty()) continue;
            const QString fold = keywordFold(trimmed);
            if (seen.contains(fold)) continue;
            seen.insert(fold);
            next << trimmed;
        }

        next.sort(Qt::CaseInsensitive);

        /*  NOTHING CHANGED FOR THIS ROW, which is the ordinary case rather than an
            error: removing a keyword from a selection where only some images carry it,
            or adding one several already have. Skipping here means the file is not
            rewritten and its ModifyDate is not touched. writeXMP would decline the write
            anyway (its change detection compares the same two sets), but it would also
            log a warning per row, and a hundred warnings for a working operation is
            noise that hides the one that matters. */
        auto folded = [](QStringList v) {
            for (QString &s : v) s = keywordFold(s);
            v.sort();
            return v;
        };
        if (folded(next) == folded(have)) {
            G::popup->setProgress(i + 1);
            continue;
        }

        /*  WHAT GOES INTO THE FILE -- see composeKeywordWrite at the top of this file
            for the shape and for why reading it back is idempotent. */
        QStringList subject, hierarchical;
        composeKeywordWrite(next, subject, hierarchical);

        /*  THE FILE FIRST. See the note at the top of this file: with no shadow column,
            updating the model before this would hide the edit from writeXMP. */
        dm->imMetadata(fPath, true);        // true = load metadata->m AND its shadows
        metadata->setKeywords(subject, hierarchical);
        if (metadata->writeXMP(fPath, src)) ++written;

        /*  THEN THE MODEL, all three columns. The two source columns hold what the file
            now holds; the third is the prefix expansion the filters and the catalog read,
            and it is derived here exactly as DataModel::addMetadataForItem derives it so
            an edited row and a freshly read one cannot disagree. */
        const QStringList expanded =
            keywordPrefixExpand(keywordEffectivePaths(subject, hierarchical));
        emit setValDm(dmRow, G::KeywordsColumn, subject, dm->instance, src, Qt::EditRole);
        emit setValDm(dmRow, G::KeywordPathsColumn, hierarchical, dm->instance, src,
                      Qt::EditRole);
        emit setValDm(dmRow, G::KeywordsAllColumn, expanded, dm->instance, src,
                      Qt::EditRole);

        /*  A raw+jpg pair is ONE picture with two files, and the hidden half has to carry
            the same keywords or a keyword filter would hide one of them. Its sidecar is
            written too -- the hidden row is a real file the user may later see on its
            own. Mirrors what MW::setRating does for the rating. */
        if (combineRawJpg) {
            const int rowDup = dm->isDupJpg(dmRow) ? dm->dupOtherRow(dmRow) : -1;
            if (rowDup >= 0) {
                const QString dupPath =
                    dm->index(rowDup, G::PathColumn).data(G::PathRole).toString();
                if (!dupPath.isEmpty()) {
                    dm->imMetadata(dupPath, true);
                    metadata->setKeywords(subject, hierarchical);
                    metadata->writeXMP(dupPath, src);
                }
                emit setValDm(rowDup, G::KeywordsColumn, subject, dm->instance, src,
                              Qt::EditRole);
                emit setValDm(rowDup, G::KeywordPathsColumn, hierarchical, dm->instance,
                              src, Qt::EditRole);
                emit setValDm(rowDup, G::KeywordsAllColumn, expanded, dm->instance, src,
                              Qt::EditRole);
                updateCatalogForRow(rowDup);
            }
        }

        /*  And the index, so a keyword set in Catalog scope is searchable without
            reopening the folder. Scope-gated inside updateCatalogForRow: an EDIT must not
            be what puts an out-of-scope image into the catalog. */
        updateCatalogForRow(dmRow);
        G::popup->setProgress(i + 1);
    }

    if (G::isLogger)
        G::log("MW::applyKeywordsToSelection", QString("wrote %1 of %2 rows")
                                                   .arg(written).arg(rows.size()));

    /*  MUST EXECUTE IN THIS ORDER: suspend the proxy, rebuild the category, then let
        filterChange lift the suspension and re-run.

        runSync = TRUE, which is where this departs from MW::setRating rather than
        copying it. BuildFilters::updateCategory documents the rule: a caller that
        immediately follows it with MW::filterChange must run the rebuild inline, because
        otherwise filterChange un-suspends and re-runs filterAcceptsRow while the WORKER
        thread is still mutating the category tree items -- a use-after-free (see
        MW::togglePick, which passes runSync exactly when a filterChange follows).
        setRating and setColorClass do not, which looks like a latent instance of the same
        hazard; that is not this file's to fix, but it is not a pattern to copy either.
        Keywords have more reason to obey it than any other category: this rebuild DELETES
        and recreates the whole tree (Filters::updateKeywordItems), so there is strictly
        more for a live filter pass to walk into. */
    dm->sf->suspend(true, src);
    buildFilters->updateCategory(BuildFilters::KeywordEdit,
                                 BuildFilters::NoAfterAction, /*runSync*/ true);
    filterChange(src);

    thumbView->refreshIcons(src);
    gridView->refreshIcons(src);
}

void MW::keywordPathChanged(const QString &oldPath, const QString &newPath)
{
/*
    A vocabulary node was renamed or re-parented. The NAME has already changed; this is
    only about the photographs that carry the old path.

    ASKED, NOT ASSUMED, and the counts are gathered BEFORE anything is written so the
    number on the button is the number the operation will touch. Lightroom retags
    silently, which turns one keystroke into thousands of file writes; never retagging
    would leave the vocabulary and the library permanently disagreeing. So the user is
    told what each choice costs.

    DECLINING IS A REAL ANSWER, not a deferral to be remembered somewhere. Those images
    keep the old path, which now matches no vocabulary node, and the dock shows them as
    unfiled -- visible in the panel the user is already looking at rather than recorded in
    a table they cannot see.
*/
    if (G::isLogger) G::log("MW::keywordPathChanged");
    if (oldPath.isEmpty() || newPath.isEmpty() || oldPath == newPath) return;

    Catalog &cat = Catalog::instance();
    if (!cat.isAvailable()) return;

    /*  The current folder, for the narrower scope. Empty when nothing is loaded, in which
        case only the catalog-wide choice can do anything. */
    const QString folder = dm && dm->rowCount() > 0
        ? dm->index(0, G::FolderNameColumn).data().toString() : QString();

    const int catalogCount = cat.imagesUnderKeyword(oldPath);
    const int folderCount = folder.isEmpty() ? 0 : cat.imagesUnderKeyword(oldPath, folder);
    if (catalogCount == 0) return;      // nothing carries it; nothing to ask

    KeywordRetagDlg dlg(oldPath, newPath, folderCount, catalogCount, this);
    dlg.exec();
    if (dlg.choice() == KeywordRetagDlg::NotNow) return;

    retagKeywordPath(oldPath, newPath,
                     dlg.choice() == KeywordRetagDlg::ThisFolder ? folder : QString());
}

int MW::retagKeywordPath(const QString &oldPath, const QString &newPath,
                         const QString &folder)
{
/*
    Rewrite oldPath to newPath on every image that carries it, or anything beneath it.

    THE IMAGES NEED NOT BE LOADED, and mostly are not: a rename near the root of the
    vocabulary reaches folders the datamodel has never seen. So the work is driven from
    the CATALOG rather than from the model -- Catalog::searchRows on the old path returns
    every affected image with the two verbatim keyword lists it was indexed with, which is
    everything needed to compute its new ones.

    THE QUERY NEEDS NO SUBTREE WALK. Because every image is linked to every ANCESTOR
    PREFIX of its keywords, asking for images with keyword "Location|Canada" already
    returns everything filed beneath it. The prefix range in Catalog::imagesUnderKeyword
    exists to COUNT them; finding them is plain equality.

    A DESCENDANT IS REWRITTEN AT ITS PREFIX, not replaced. Renaming "Fauna|Bird" to
    "Fauna|Birds" must turn "Fauna|Bird|Heron" into "Fauna|Birds|Heron" -- the leaf and
    everything below the renamed node are the user's and must survive. keywordIsDescendant
    decides what is affected, so this and the count the user was shown agree by
    construction.

    A LOADED ROW IS UPDATED IN THE MODEL TOO. The catalog is an index; the datamodel is
    what the user is looking at. Writing only the file would leave the panel showing the
    old keyword until the folder was reopened.
*/
    if (G::isLogger) G::log("MW::retagKeywordPath");
    if (!metadata) return 0;

    Catalog &cat = Catalog::instance();
    if (!cat.isAvailable()) return 0;

    CatalogQuery q;
    q.keywords = {oldPath};
    q.folder = folder;
    const QVector<CatalogRow> rows = cat.searchRows(q, 0);
    if (rows.isEmpty()) return 0;

    const QString oldFold = keywordFold(oldPath);
    int written = 0;

    G::popup->setProgressVisible(true);
    G::popup->setProgress(0);

    for (int i = 0; i < rows.size(); ++i) {
        const CatalogRow &r = rows.at(i);
        if (r.path.isEmpty()) continue;

        const QStringList have =
            keywordEffectivePaths(r.keywordsLiteral, r.keywordPaths);

        QStringList next;
        bool changed = false;
        for (const QString &p : have) {
            if (keywordIsDescendant(keywordFold(p), oldFold)) {
                /*  Rewrite the PREFIX and keep the tail. mid() rather than replace(),
                    because a path can legitimately contain the old text further along
                    ("Fauna|Bird|Bird of prey") and only the prefix is being renamed. */
                next << newPath + p.mid(oldPath.size());
                changed = true;
            }
            else {
                next << p;
            }
        }
        if (!changed) continue;

        next.sort(Qt::CaseInsensitive);

        /*  Composed exactly as applyKeywordsToSelection composes it, because it is the
            same function: an image retagged here and one tagged from the dock must end
            up with the same shape. */
        QStringList subject, hierarchical;
        composeKeywordWrite(next, subject, hierarchical);

        if (metadata->writeKeywordsToSidecar(r.path, subject, hierarchical)) ++written;

        /*  The model if the row is loaded, the index either way -- see
            MW::publishKeywordWrite, which the tidy shares. Writing only the file would
            leave the panel showing the old keyword until the folder was reopened, and
            leave the catalog serving it to searches until the folder was rescanned. */
        publishKeywordWrite(r, subject, hierarchical);

        G::popup->setProgress(i + 1);
    }

    G::popup->setProgressVisible(false);

    if (keywordVocab) keywordVocab->refreshCounts();
    if (buildFilters && filters && filters->filtersBuilt) {
        dm->sf->suspend(true, "MW::retagKeywordPath");
        buildFilters->updateCategory(BuildFilters::KeywordEdit,
                                     BuildFilters::NoAfterAction, /*runSync*/ true);
        filterChange("MW::retagKeywordPath");
    }

    G::popup->showPopup(QString("Updated %1 image%2.")
                            .arg(written).arg(written == 1 ? "" : "s"), 3000);
    return written;
}

void MW::publishKeywordWrite(const CatalogRow &r, const QStringList &subject,
                             const QStringList &hierarchical)
{
/*
    The file has just been written. This is the other two places the same fact lives.

    A LOADED ROW IS UPDATED IN THE MODEL, because the catalog is an index and the
    datamodel is what the user is looking at: writing only the file would leave the panel
    showing the old keyword until the folder was reopened.

    A ROW THAT IS NOT LOADED IS COMMITTED TO THE INDEX from the CatalogRow it came from,
    with only its keywords replaced. commit() replaces the whole row, which is why the
    caller has to have fetched a whole one.

    ALL THREE COLUMNS EITHER WAY. The two source columns hold what the file now holds; the
    third is the prefix expansion the filters and the catalog read, derived here exactly
    as DataModel::addMetadataForItem derives it so an edited row and a freshly read one
    cannot disagree.
*/
    const QStringList expanded =
        keywordPrefixExpand(keywordEffectivePaths(subject, hierarchical));

    const int dmRow = dm ? dm->rowFromPath(r.path) : -1;
    if (dmRow >= 0) {
        const QString src = "MW::publishKeywordWrite";
        emit setValDm(dmRow, G::KeywordsColumn, subject, dm->instance, src, Qt::EditRole);
        emit setValDm(dmRow, G::KeywordPathsColumn, hierarchical, dm->instance, src,
                      Qt::EditRole);
        emit setValDm(dmRow, G::KeywordsAllColumn, expanded, dm->instance, src,
                      Qt::EditRole);
        updateCatalogForRow(dmRow);
        return;
    }

    CatalogRow updated = r;
    updated.keywordsLiteral = subject;
    updated.keywordPaths = hierarchical;
    updated.keywords = expanded;

    /*  RE-STAMPED, OR THE COMMIT IS SILENTLY DISCARDED. Catalog::commit skips a row whose
        (srcSize, srcMtime, sidecarMtime) still match the index -- the skip that makes
        revisiting a folder free -- and r carries the stamps it was INDEXED with. The
        write above has just moved the sidecar's mtime, so committing r unchanged offers
        the index a row that looks identical to the one it holds, and the new keywords go
        nowhere: searching in Catalog scope would keep returning the old ones until the
        folder was next opened. Two stats against a file write already paid for. */
    const QFileInfo fi(r.path);
    if (fi.exists()) {
        updated.srcSize = fi.size();
        updated.srcMtime = fi.lastModified().toSecsSinceEpoch();
    }
    const QFileInfo si(metadata->sidecarPath(r.path));
    updated.sidecarMtime = si.exists() ? si.lastModified().toSecsSinceEpoch() : 0;

    Catalog::instance().commit({updated});
}

void MW::tidyFlatKeywords()
{
/*
    FILE THE FLAT KEYWORDS INTO THE TREE -- the whole legacy vocabulary in one review.

    WHY IT IS ONE OPERATION AND NOT A THOUSAND RENAMES. Every flat keyword could be filed
    by hand: drag the node into place in the tree and accept the retag. On a library that
    predates hierarchical keywords there are around a thousand of them, and each hand
    filing is a dialog, a catalog query and a pass over the files it touches. Doing them
    together means ONE pass over the images, so an image carrying six flat keywords is
    written once rather than six times -- which is the difference between a tidy and a
    day of watching sidecars being rewritten.

    THE VOCABULARY IS THE MATCHER. A flat keyword is filed by finding a vocabulary node
    whose LEAF is that name; the user's Lightroom import is what makes that work, and it
    is also why the ambiguity is real -- 59 names in that export appear in more than one
    branch. The dialog shows what each row matched and pre-selects the deepest.

    NOTHING IS WRITTEN UNTIL IT IS CONFIRMED TWICE: the review list, and then a count of
    the files about to change. This rewrites metadata in thousands of photographs, and it
    is not undoable from inside Winnow.
*/
    if (G::isLogger) G::log("MW::tidyFlatKeywords");

    Catalog &cat = Catalog::instance();
    if (!cat.isAvailable()) {
        G::popup->showPopup("The catalog is not available.", 2000);
        return;
    }

    /*  The vocabulary has to be loaded, and not only because the dialog needs its paths:
        the dock may never have been opened this session, and an empty tree would report
        that every flat keyword matched nothing and offer to delete the lot. */
    ensureKeywordVocabLoaded();

    QStringList vocabPaths;
    if (keywordVocab) {
        /*  From the AUTHORED tree, not the observed keyword table. The observed table
            holds every path any file ever carried, including the malformed ones this is
            here to clean up, so matching against it would offer to file "Nanaimo" under
            a branch that is itself a stray. */
        std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
            const int n = keywordVocab->rowCount(parent);
            for (int i = 0; i < n; ++i) {
                const QModelIndex idx = keywordVocab->index(i, 0, parent);
                vocabPaths << idx.data(KeywordVocab::PathRole).toString();
                walk(idx);
            }
        };
        walk(QModelIndex());
    }

    if (vocabPaths.isEmpty()) {
        QMessageBox::information(this, "Tidy Flat Keywords",
            "Your keyword list is empty, so there is nothing to file these keywords "
            "into.\n\nImport a keyword list, or build one from the catalog, from the "
            "Keywords panel first.");
        return;
    }

    G::popup->showPopup("Looking for flat keywords...", 0, true, 0.75);
    qApp->processEvents();
    const QList<CatalogKeyword> flat = cat.flatKeywords();
    G::popup->reset();

    if (flat.isEmpty()) {
        QMessageBox::information(this, "Tidy Flat Keywords",
            "Every keyword in the catalog is already filed in your keyword list.");
        return;
    }

    /*  The counts the review rows are decided on, from the OBSERVED table: how many
        images each vocabulary path already holds. Keyed folded, as everything that
        compares keyword paths is. */
    QHash<QString, int> pathCounts;
    for (const CatalogKeyword &k : cat.keywords())
        pathCounts.insert(keywordFold(k.path), k.count);

    KeywordTidyDlg dlg(flat, vocabPaths, pathCounts, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QList<KeywordTidyAction> plan = dlg.plan();
    if (plan.isEmpty()) return;

    /*  THE SECOND CONFIRMATION, and the only place a real file count appears: the dialog
        can only add up its rows, and an image carrying six flat keywords is one file. */
    const int images = cat.flatKeywordRows().size();
    const QString ask =
        QString("%1 keyword%2 will be changed in up to %3 image%4.\n\n"
                "Winnow writes the change into each image's XMP sidecar or file. This "
                "cannot be undone from inside Winnow.\n\nGo ahead?")
            .arg(plan.size()).arg(plan.size() == 1 ? "" : "s")
            .arg(QLocale().toString(images)).arg(images == 1 ? "" : "s");
    if (QMessageBox::question(this, "Tidy Flat Keywords", ask,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) return;

    const int written = applyKeywordTidyPlan(plan);

    /*  THE VOCABULARY AND THE INDEX ARE TIDIED LAST, in that order, because both are
        answering "is anything still using this?" and the answer only becomes true once
        the images have been rewritten.

        The observed keyword rows go first-class through pruneUnusedKeywords: a flat
        keyword that has just been moved off every image it was on is a name nothing in
        the library says any more, and leaving it would keep offering a filter that
        matches nothing. The AUTHORED nodes are removed only where the user had one for a
        flat keyword and it is now empty -- see KeywordVocab::removeUnusedRoots. */
    QStringList tidied;
    for (const KeywordTidyAction &a : plan) tidied << a.flat;
    cat.pruneUnusedKeywords();
    int vocabGone = 0;
    if (keywordVocab) {
        keywordVocab->refreshCounts();
        vocabGone = keywordVocab->removeUnusedRoots(tidied);
    }
    refreshKeywordsDock();

    QMessageBox::information(this, "Tidy Flat Keywords",
        QString("Filed %1 keyword%2 and updated %3 image%4.%5")
            .arg(plan.size()).arg(plan.size() == 1 ? "" : "s")
            .arg(QLocale().toString(written)).arg(written == 1 ? "" : "s")
            .arg(vocabGone > 0
                     ? QString("\n\n%1 empty keyword%2 removed from your keyword list.")
                           .arg(vocabGone).arg(vocabGone == 1 ? " was" : "s were")
                     : QString()));
}

int MW::applyKeywordTidyPlan(const QList<KeywordTidyAction> &plan)
{
/*
    ONE PASS OVER THE IMAGES, WITH THE WHOLE PLAN IN HAND. That is the reason this is not
    a loop of retagKeywordPath calls: images overlap heavily between keywords -- the same
    photograph carries "Nanaimo" and "Heron" and "16x9" -- so a call per keyword would
    open, rewrite and re-index the same sidecar once per keyword it happens to carry, and
    would touch every file's ModifyDate that many times.

    THE SET IT WALKS IS Catalog::flatKeywordRows, the same scan and the same test that
    produced the counts the user was shown, so a row cannot be affected here that the
    dialog did not know about.

    REDUNDANT ANCESTORS ARE PRUNED, and this is the operation that creates them: moving a
    flat "Canada" onto an image that already carries Location|Canada|BC would otherwise
    leave both, and the second says nothing the first does not (see
    keywordPruneAncestors). It is scoped to the rows this rewrites -- nothing goes looking
    for redundancy in images the tidy is not already touching.

    A REMOVAL IS NOT A DELETION OF THE KEYWORD, it is the removal of a bare name from the
    images carrying it. "Location" on its own, beside Location|Canada|BC, is the same fact
    without its parents; prefix expansion still answers a search for Location.
*/
    if (G::isLogger) G::log("MW::applyKeywordTidyPlan");
    if (!metadata || plan.isEmpty()) return 0;

    Catalog &cat = Catalog::instance();
    if (!cat.isAvailable()) return 0;

    QHash<QString, QString> moveTo;     // folded flat keyword -> the path it becomes
    QSet<QString> removeFold;
    for (const KeywordTidyAction &a : plan) {
        const QString fold = keywordFold(a.flat);
        if (fold.isEmpty()) continue;
        if (a.remove) removeFold.insert(fold);
        else if (!a.target.isEmpty()) moveTo.insert(fold, a.target);
    }
    if (moveTo.isEmpty() && removeFold.isEmpty()) return 0;

    const QVector<CatalogRow> rows = cat.flatKeywordRows();
    if (rows.isEmpty()) return 0;

    int written = 0;
    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(rows.size());
    G::popup->setProgress(0);
    G::popup->showPopup(QString("Tidying keywords in %1 images...")
                            .arg(QLocale().toString(rows.size())), 0, true, 0.75);

    /*  THE POPUP IS SHOWN BY A QUEUED SINGLE-SHOT, so it does not appear until the event
        loop runs -- and this function then holds the GUI thread for minutes. Without this
        pump the popup was still QUEUED when the loop finished, reset() hid a window that
        had never been shown, and the queued show finally fired inside the completion
        QMessageBox's own event loop: a "Tidying keywords" popup appeared AFTER the work
        was over, with msDuration 0 (no hide timer) and nothing left to hide it. It sat on
        screen until the app was restarted, reporting work that had finished hours before.
        Reported from use, and the reason for both pumps in this function. */
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    for (int i = 0; i < rows.size(); ++i) {
        const CatalogRow &r = rows.at(i);
        if (r.path.isEmpty()) continue;

        const QStringList have =
            keywordEffectivePaths(r.keywordsLiteral, r.keywordPaths);

        QStringList next;
        QSet<QString> seen;
        bool changed = false;
        for (const QString &p : have) {
            QString use = p;
            /*  ONLY DEPTH-1 PATHS ARE IN THE PLAN. A hierarchical path is not touched
                even when its ROOT is a flat keyword being filed: "Canada|BC|Nanaimo" is
                a hierarchy the user has, moving it is a re-parent, and it is offered in
                the vocabulary tree where the counts and the confirmation are about that.
                Doing it here would rewrite thousands of paths the dialog never named. */
            if (!use.contains('|')) {
                const QString fold = keywordFold(use);
                if (removeFold.contains(fold)) { changed = true; continue; }
                const auto it = moveTo.constFind(fold);
                if (it != moveTo.constEnd()) { use = it.value(); changed = true; }
            }
            const QString useFold = keywordFold(use);
            /*  A move can land on a path the image ALREADY carries -- the ordinary case
                for a Lightroom library where some files were tagged the new way and some
                the old. That is a merge, not a duplicate. */
            if (seen.contains(useFold)) { changed = true; continue; }
            seen.insert(useFold);
            next << use;
        }
        if (!changed) continue;

        const QStringList pruned = keywordPruneAncestors(next);
        if (pruned.size() != next.size()) next = pruned;
        next.sort(Qt::CaseInsensitive);

        QStringList subject, hierarchical;
        composeKeywordWrite(next, subject, hierarchical);

        if (metadata->writeKeywordsToSidecar(r.path, subject, hierarchical)) ++written;
        publishKeywordWrite(r, subject, hierarchical);

        G::popup->setProgress(i + 1);

        /*  AND THE BAR HAS TO BE ALLOWED TO PAINT. This loop owns the GUI thread for the
            whole run -- thousands of sidecar writes -- so without a pump the progress bar
            is a still picture and the only honest answer to "how far has it got?" is to
            watch the file system. Every 25 rows keeps the cost negligible against a file
            write. USER INPUT STAYS EXCLUDED: the model, the filters and the catalog are
            all mid-rewrite, and a click that started a folder change here would reenter
            everything this is walking. */
        if ((i % 25) == 0) qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    G::popup->setProgressVisible(false);
    G::popup->reset();

    if (G::isLogger)
        G::log("MW::applyKeywordTidyPlan", QString("wrote %1 of %2 rows")
                                               .arg(written).arg(rows.size()));

    /*  Same order and same reason as MW::applyKeywordsToSelection: suspend the proxy,
        rebuild the keyword category INLINE, then let filterChange lift the suspension and
        re-run. runSync is not optional when a filterChange follows -- the rebuild deletes
        and recreates the whole keyword tree. */
    if (buildFilters && filters && filters->filtersBuilt) {
        dm->sf->suspend(true, "MW::applyKeywordTidyPlan");
        buildFilters->updateCategory(BuildFilters::KeywordEdit,
                                     BuildFilters::NoAfterAction, /*runSync*/ true);
        filterChange("MW::applyKeywordTidyPlan");
    }

    thumbView->refreshIcons("MW::applyKeywordTidyPlan");
    gridView->refreshIcons("MW::applyKeywordTidyPlan");
    return written;
}


void MW::ensureKeywordVocabLoaded()
{
/*
    Load the vocabulary if it is not loaded yet.

    THERE ARE FOUR WAYS THE DOCK CAN BECOME VISIBLE and only one of them used to load
    anything: Window > Keywords Panel (showKeywordsDock), a layout RESTORED from settings
    with the dock already on, a workspace being applied, and the dock being raised as a
    tab. The vocabulary was loaded only on the first, so a restart with the panel left
    open showed an empty tree -- which looks exactly like "the keywords are gone".

    IT RETRIES RATHER THAN LATCHING. Emptiness is the test, so a call that arrives before
    CacheDb has a path (which can happen during startup) simply does nothing and the next
    route to visibility tries again. A flag saying "we have loaded" would remember the
    failure instead of recovering from it.

    NOT RELOADED WHEN ALREADY POPULATED, because reload() resets the model and the user's
    expansion state goes with it -- and being raised as a tab is not a reason to collapse
    the branch someone was reading. Counts are refreshed instead, which is what actually
    goes stale.
*/
    if (!keywordVocab || !keywordsDock) return;

    if (keywordVocab->rowCount(QModelIndex()) > 0) {
        keywordVocab->refreshCounts();
        return;
    }
    /*  No database yet: not an error and not an empty vocabulary, just too early. */
    if (!Catalog::instance().isAvailable()) return;
    keywordVocab->reload();
}

void MW::keywordsDockVisibilityChange(bool visible)
{
    if (!visible) return;
    ensureKeywordVocabLoaded();
    refreshKeywordsDock();
}

void MW::refreshKeywordsDock()
{
/*
    Repaint the dock from the current selection: the tags above and the dots in the tree.

    GUARDED ON VISIBILITY. The dock ships off, and walking a vocabulary on every arrow key
    for a panel nobody has open is work for nothing.

    THE TAGS AND THE DOTS ANSWER DIFFERENT QUESTIONS and are fed differently. The tags
    describe the SELECTION (with a count per keyword, so "on some" can be marked); the
    dots describe the CURRENT image alone, because a dot is a yes/no mark and there is no
    honest way to draw "sort of".
*/
    if (!keywordsDock || !keywordsDock->isVisible()) return;
    if (!keywordVocab || !keywordTags || !dm || !sel) return;

    keywordTags->setSelection(keywordsInSelection(),
                               dm->selectionModel->selectedRows().size());

    const int dmRow = dm->currentSfRow >= 0
        ? dm->modelRowFromProxyRow(dm->currentSfRow) : -1;
    if (dmRow >= 0) {
        keywordVocab->setAppliedPaths(keywordEffectivePaths(
            dm->index(dmRow, G::KeywordsColumn).data().toStringList(),
            dm->index(dmRow, G::KeywordPathsColumn).data().toStringList()));
    }
}

void MW::applyKeywordToPaths(const QString &keywordPath, const QStringList &imagePaths)
{
/*
    Images were dropped onto a keyword in the tree. Tag exactly those, which are NOT
    necessarily the selection -- dragging a thumbnail that is not selected must tag the
    one that was dragged, not whatever happened to be highlighted.

    IMPLEMENTED BY SELECTING THEM, which looks indirect and is deliberate. The whole
    edit path -- sidecar write, raw+jpg mirroring, model update, catalog update, filter
    rebuild -- lives in applyKeywordsToSelection, and a second implementation of it that
    took a path list would be a second place for the raw+jpg pairing and the ordering rule
    to be got wrong. The selection is restored afterwards so the drop does not silently
    move the user somewhere else.
*/
    if (G::isLogger) G::log("MW::applyKeywordToPaths");
    if (keywordPath.isEmpty() || imagePaths.isEmpty()) return;
    if (!dm || !sel) return;

    const QModelIndexList wasSelected = dm->selectionModel->selectedRows();
    const QModelIndex wasCurrent = dm->sf->index(dm->currentSfRow, 0);

    QItemSelection toSelect;
    for (const QString &p : imagePaths) {
        const int dmRow = dm->rowFromPath(p);
        if (dmRow < 0) continue;                    // dropped from outside this folder
        const QModelIndex sfIdx = dm->sf->mapFromSource(dm->index(dmRow, 0));
        if (sfIdx.isValid()) toSelect.select(sfIdx, sfIdx);
    }
    if (toSelect.isEmpty()) return;

    dm->selectionModel->select(toSelect, QItemSelectionModel::ClearAndSelect
                                             | QItemSelectionModel::Rows);
    applyKeywordsToSelection({keywordPath}, {});

    // put the user back where they were
    QItemSelection restore;
    for (const QModelIndex &idx : wasSelected) restore.select(idx, idx);
    if (!restore.isEmpty()) {
        dm->selectionModel->select(restore, QItemSelectionModel::ClearAndSelect
                                                 | QItemSelectionModel::Rows);
        if (wasCurrent.isValid())
            dm->selectionModel->setCurrentIndex(wasCurrent,
                                                QItemSelectionModel::NoUpdate);
    }
    refreshKeywordsDock();
}
