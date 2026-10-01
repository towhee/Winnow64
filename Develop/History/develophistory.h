#ifndef DEVELOPHISTORY_H
#define DEVELOPHISTORY_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include "Develop/History/historyentry.h"

/*
    Lightroom-style Develop edit history (the History dock's model).

    THE SIDECAR DOES NOT RECORD ACTIONS. The image's XMP sidecar holds ONE base64 blob
    (winnow:Develop) carrying the current EditStack -- the RESULT state, not a log of how
    it got there (see DevelopProperties::flushImage). So History keeps its own timeline:
    an ordered list of labelled EditStack SNAPSHOTS, one per committed action, per image.

    It stays in sync with the sidecar by sharing the same object. Every entry IS an
    EditStack, so restoring one is just writing it back into DevelopProperties::stackCache
    and marking the image dirty -- the normal debounced flush then persists it. The
    sidecar remains the single source of truth for the CURRENT state; History is the
    road there.

    PERSISTED IN THE LOCAL INDEX, NOT THE SIDECAR (2026-09-30). It used to be session
    scoped -- gone on quit, so reopening an image showed one "Saved settings" step. Now
    DevelopProperties saves an image's history to index.db (HistoryStore, table
    develop_history) every time it writes the sidecar, and seed() restores it through
    the `loader` hook on the image's first touch in a session -- but ONLY while the
    sidecar still holds the recipe it was saved with (HistoryEntry::recipeStamp). A
    history whose recipe has moved on (edited on another machine, reset elsewhere, a
    sidecar restored from a backup) is ignored and the baseline is laid down as before:
    a history that does not lead to the image on screen would revert to states the user
    never saw. The sidecar stays the single source of truth for the CURRENT state.

    Snapshots are not free -- a scope's brush / object mask paramsJson can run to tens of
    KB -- so memory is capped two ways: kMaxEntries per image (oldest edits fall off, the
    baseline is relabelled but kept) and kMaxImages images retained, evicting the least
    recently touched. An evicted image's history is still in the store.

    REVERT MODEL (Lightroom): pos is the entry currently applied. Recording a new action
    while pos is not the last entry DISCARDS everything after pos -- editing from an
    earlier state truncates, exactly like an undo stack.

    COALESCING: a continuous gesture (a slider drag, a colour-wheel drag) commits many
    times. A non-empty mergeKey that matches the top entry's REPLACES that entry instead
    of appending, so a whole drag reads as one "Global: Exposure  +0.35" step.

    LINKED STEPS (multi-image edits). History stays per image, but one edit made with
    several images selected writes a step into EVERY image it lands on, and all of those
    steps carry the same syncId. That is what lets a revert on the current image move the
    rest of the selection to the matching point in THEIR histories (syncedPos), undo and
    redo alike, without touching each image's own unrelated steps.
*/

class DevelopHistory : public QObject
{
    Q_OBJECT
public:
    explicit DevelopHistory(QObject *parent = nullptr) : QObject(parent) {}

    static constexpr int kMaxEntries = 100;   // steps kept per image
    /* Images kept before LRU eviction. 200, not the original 40: every image a
       multi-image edit lands on now gets history (so it can be undone), and a 40-image
       store would evict the very images a large selection just edited. A snapshot with
       no brush/object masks is a few KB. */
    static constexpr int kMaxImages  = 200;

    /* First touch of an image: lay down the baseline entry the user can always return
       to. "Original" for an untouched image, "Saved settings" when the sidecar already
       carried edits. Idempotent -- re-visiting an image must not reset its history. */
    void seed(const QString &path, const EditStack &s) {
        if (path.isEmpty()) return;
        touch(path);
        if (byImage.contains(path) && !byImage[path].isEmpty()) return;
        /* A saved history from an earlier session, if the sidecar still holds the recipe
           it was saved with. */
        QVector<HistoryEntry> saved;
        int savedPos = -1;
        QString stamp;
        if (loader && loader(path, saved, savedPos, stamp) &&
            savedPos >= 0 && savedPos < saved.size() &&
            stamp == HistoryEntry::recipeStamp(s)) {
            byImage[path] = saved;
            posByImage[path] = savedPos;
            emit changed(path);
            return;
        }
        HistoryEntry e;
        e.action = s.isIdentity() ? QStringLiteral("Original")
                                  : QStringLiteral("Saved settings");
        e.stack  = s;
        byImage[path] = QVector<HistoryEntry>{e};
        posByImage[path] = 0;
        emit changed(path);
    }

    /* Commit one action. Truncates anything after the current position, then merges into
       the top entry (same non-empty mergeKey) or appends. */
    void record(const QString &path, const QString &scope, const QString &action,
                const QString &value, const QString &mergeKey, const EditStack &after,
                quint64 syncId = 0)
    {
        if (path.isEmpty()) return;
        touch(path);
        QVector<HistoryEntry> &v = byImage[path];
        int &pos = posByImage[path];
        if (v.isEmpty()) {                 // no seed (can't happen) -- synthesize one
            HistoryEntry base;
            base.action = QStringLiteral("Original");
            v.append(base);
            pos = 0;
        }
        /* Editing from an earlier state discards the steps after it. */
        if (pos < v.size() - 1) v.resize(pos + 1);

        HistoryEntry e;
        e.scope = scope; e.action = action; e.value = value;
        e.mergeKey = mergeKey; e.stack = after; e.syncId = syncId;

        const bool merge = !mergeKey.isEmpty() && v.size() > 1 &&
                           v.last().mergeKey == mergeKey;
        if (merge) v.last() = e;
        else       v.append(e);

        /* Cap: drop the oldest EDITS, never the baseline (index 0), so "Original" stays
           reachable. The baseline keeps its label but adopts the state it now stands
           for -- otherwise reverting to it would resurrect a state the user cannot see
           any of the steps for. */
        while (v.size() > kMaxEntries) {
            v[0].stack = v[1].stack;       // baseline advances to the dropped step
            v.remove(1);
        }
        pos = v.size() - 1;
        emit changed(path);
    }

    /* Persistence hooks, set by DevelopProperties (the model stays free of the database).
       loader: fill entries + pos with path's saved history, and the stamp of the recipe
       it was saved with; false when there is none.
       remover: drop path's saved history (forget -- a reset or a deleted version). */
    std::function<bool(const QString &, QVector<HistoryEntry> &, int &, QString &)> loader;
    std::function<void(const QString &)> remover;

    /* path's whole history, for the store. */
    QVector<HistoryEntry> entries(const QString &path) const { return byImage.value(path); }

    int count(const QString &path) const { return byImage.value(path).size(); }
    int pos(const QString &path) const { return posByImage.value(path, -1); }

    const HistoryEntry *at(const QString &path, int i) const {
        auto it = byImage.constFind(path);
        if (it == byImage.constEnd() || i < 0 || i >= it->size()) return nullptr;
        return &(*it)[i];
    }

    /* Link the step just recorded on path (its newest, current entry) to a multi-image
       edit. The source image records BEFORE the batch exists, so it is stamped after. */
    void stampTop(const QString &path, quint64 syncId) {
        auto it = byImage.find(path);
        if (it == byImage.end() || it->size() < 2) return;   // never the baseline
        if (posByImage.value(path) != it->size() - 1) return;
        it->last().syncId = syncId;
    }

    /* The multi-image edits path's history links to: every non-zero syncId in it, and
       the subset applied at entry i (indices 1..i). */
    void syncIds(const QString &path, int i, QSet<quint64> &known,
                 QSet<quint64> &applied) const {
        const QVector<HistoryEntry> v = byImage.value(path);
        for (int k = 1; k < v.size(); ++k) {
            if (!v[k].syncId) continue;
            known.insert(v[k].syncId);
            if (k <= i) applied.insert(v[k].syncId);
        }
    }

    /* Where path's history should stand when another image reverted to a point where
       exactly `applied` of the `known` linked edits are in force: just before its first
       step from a known edit that is NOT applied, else its newest step. -1 when path
       shares no linked edit with it (leave it alone). Its own unlinked steps after that
       point go with it, as they would on any revert. */
    int syncedPos(const QString &path, const QSet<quint64> &known,
                  const QSet<quint64> &applied) const {
        const QVector<HistoryEntry> v = byImage.value(path);
        bool linked = false;
        for (int k = 1; k < v.size(); ++k) {
            if (!v[k].syncId || !known.contains(v[k].syncId)) continue;
            linked = true;
            if (!applied.contains(v[k].syncId)) return k - 1;
        }
        return linked ? int(v.size()) - 1 : -1;
    }

    void setPos(const QString &path, int i) {
        if (!byImage.contains(path)) return;
        const int n = byImage.value(path).size();
        if (i < 0 || i >= n) return;
        posByImage[path] = i;
        emit changed(path);
    }

    /* Apply f to EVERY snapshot of path's history. For a change that re-expresses the
       recipe rather than editing it -- an image rotation moves every stored coordinate
       into the new frame (Develop/editrotate.h) -- so that stepping back through the
       history does not restore masks and crops measured in the old one. Not a step of
       its own: the rotation is not a develop edit, and undoing it here would leave the
       image turned and its recipe not. */
    void transformAll(const QString &path, const std::function<void(EditStack &)> &f) {
        auto it = byImage.find(path);
        if (it == byImage.end()) return;
        for (HistoryEntry &e : *it) f(e.stack);
        emit changed(path);
    }

    void forget(const QString &path) {
        if (remover) remover(path);
        byImage.remove(path);
        posByImage.remove(path);
        lru.removeAll(path);
        emit changed(path);
    }

    void clear() {
        byImage.clear();
        posByImage.clear();
        lru.clear();
        emit changed(QString());
    }

signals:
    void changed(const QString &path);

private:
    /* Mark an image most-recently-used and evict the coldest once over kMaxImages. */
    void touch(const QString &path) {
        lru.removeAll(path);
        lru.append(path);
        while (lru.size() > kMaxImages) {
            const QString old = lru.takeFirst();
            byImage.remove(old);
            posByImage.remove(old);
        }
    }

    QHash<QString, QVector<HistoryEntry>> byImage;
    QHash<QString, int> posByImage;    // index of the entry currently applied
    QStringList lru;                   // least-recently-touched first
};

#endif // DEVELOPHISTORY_H
