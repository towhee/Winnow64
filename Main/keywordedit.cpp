#include "Main/mainwindow.h"
#include "Dialogs/keywordretagdlg.h"
#include "Metadata/keywordpaths.h"

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
    carry it. The chip zone shows a chip per key and marks the ones whose count is below
    the selection size as partial.

    READ FROM THE LITERAL AND HIERARCHICAL COLUMNS, not from G::KeywordsAllColumn. That
    column holds the prefix EXPANSION -- every ancestor of every path -- which is what
    gets filtered on and is emphatically not what the user assigned. Showing it would put
    a chip for "Fauna" on an image the user only ever tagged "Fauna|Bird|Heron", and
    removing that chip would then have to mean something.
*/
    QMap<QString, int> out;
    if (!dm || !sel) return out;

    const QModelIndexList selection = sel->selectedRows;
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

    const QModelIndexList selection = sel->selectedRows;
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

        /*  WHAT GOES INTO THE FILE. dc:subject gets the LEAF of every path plus any
            depth-1 path (which is a leaf already); lr:hierarchicalSubject gets the paths
            of depth 2 or more, whole. A depth-1 path is deliberately NOT written as a
            one-element hierarchical entry: it carries no hierarchy, it is fewer bytes,
            and it is what an application that has never heard of lr: writes.

            READING THIS BACK IS IDEMPOTENT, which is the property that matters.
            keywordEffectivePaths consumes exactly the dc:subject leaves that match a
            path's leaf -- which is precisely the set emitted here -- leaving the depth-1
            entries as roots. So `next` is reconstructed exactly, and a second write of an
            unchanged image is a no-op rather than a slow drift. */
        QStringList subject, hierarchical;
        QSet<QString> subjectSeen;
        for (const QString &p : next) {
            /*  DE-DUPLICATED, because two keywords can share a leaf: the two Vancouvers
                would otherwise put "Vancouver" into dc:subject twice, which no other
                application does and which reads as a duplicate tag anywhere this file
                is opened. It costs nothing on read-back -- one entry is consumed by
                both paths, and both paths survive in lr:hierarchicalSubject. */
            const QString leaf = keywordLeafOf(p);
            const QString leafFold = keywordFold(leaf);
            if (!leafFold.isEmpty() && !subjectSeen.contains(leafFold)) {
                subjectSeen.insert(leafFold);
                subject << leaf;
            }
            if (p.contains('|')) hierarchical << p;
        }

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

    /*  RETAGGING IS NOT YET WIRED to the file writer for images outside the selection.
        applyKeywordsToSelection is exactly the right machinery -- it already removes a
        subtree and adds a path in one pass per row -- but it works on the SELECTION, and
        this needs to work on a query result that may span folders that are not loaded.
        Loading them to edit them is the honest way to do it and it is the Phase 6 job;
        promising it here and doing nothing would be worse than saying so. */
    G::popup->showPopup(
        QString("Retagging images is not implemented yet. Your keyword list is updated; "
                "the %1 image%2 still carrying \"%3\" will show as unfiled.")
            .arg(dlg.choice() == KeywordRetagDlg::ThisFolder ? folderCount : catalogCount)
            .arg((dlg.choice() == KeywordRetagDlg::ThisFolder ? folderCount : catalogCount)
                     == 1 ? "" : "s")
            .arg(oldPath),
        6000);
}
