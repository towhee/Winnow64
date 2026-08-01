#ifndef DEVELOPHISTORY_H
#define DEVELOPHISTORY_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include "Develop/editstack.h"

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

    SESSION SCOPED: entries are built in memory as the user edits and are discarded on
    quit (nothing is written to the sidecar). Snapshots are not free -- a scope's brush /
    object mask paramsJson can run to tens of KB -- so the store is capped two ways:
    kMaxEntries per image (oldest edits fall off, the baseline is relabelled but kept) and
    kMaxImages images retained, evicting the least recently touched.

    REVERT MODEL (Lightroom): pos is the entry currently applied. Recording a new action
    while pos is not the last entry DISCARDS everything after pos -- editing from an
    earlier state truncates, exactly like an undo stack.

    COALESCING: a continuous gesture (a slider drag, a colour-wheel drag) commits many
    times. A non-empty mergeKey that matches the top entry's REPLACES that entry instead
    of appending, so a whole drag reads as one "Global: Exposure  +0.35" step.
*/

struct HistoryEntry {
    QString   scope;      // display prefix: "Global", "Subject Mask", ...
    QString   action;     // "Exposure", "Add Subject Mask", "Crop", ...
    QString   value;      // "+0.35", "-18"; empty when the action has no value
    QString   mergeKey;   // gesture identity for coalescing; empty = never merge
    EditStack stack;      // full snapshot AFTER the action
};

class DevelopHistory : public QObject
{
    Q_OBJECT
public:
    explicit DevelopHistory(QObject *parent = nullptr) : QObject(parent) {}

    static constexpr int kMaxEntries = 100;   // steps kept per image
    static constexpr int kMaxImages  = 40;    // images kept before LRU eviction

    /* First touch of an image: lay down the baseline entry the user can always return
       to. "Original" for an untouched image, "Saved settings" when the sidecar already
       carried edits. Idempotent -- re-visiting an image must not reset its history. */
    void seed(const QString &path, const EditStack &s) {
        if (path.isEmpty()) return;
        touch(path);
        if (byImage.contains(path) && !byImage[path].isEmpty()) return;
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
                const QString &value, const QString &mergeKey, const EditStack &after)
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
        e.mergeKey = mergeKey; e.stack = after;

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

    int count(const QString &path) const { return byImage.value(path).size(); }
    int pos(const QString &path) const { return posByImage.value(path, -1); }

    const HistoryEntry *at(const QString &path, int i) const {
        auto it = byImage.constFind(path);
        if (it == byImage.constEnd() || i < 0 || i >= it->size()) return nullptr;
        return &(*it)[i];
    }

    void setPos(const QString &path, int i) {
        if (!byImage.contains(path)) return;
        const int n = byImage.value(path).size();
        if (i < 0 || i >= n) return;
        posByImage[path] = i;
        emit changed(path);
    }

    void forget(const QString &path) {
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
