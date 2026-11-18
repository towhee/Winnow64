#ifndef DUPLICATECLASSIFY_H
#define DUPLICATECLASSIFY_H

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>
#include <algorithm>
#include "Cache/catalog.h"          // DuplicateRow

/*
    THE LIBRARY DUPLICATES RULE, as a pure function so it can be tested without a
    database, a decoder or a GUI (tests/unit/tst_duplicates.cpp). See Cache/duplicates.h
    for where the rows come from.

    A GROUP is rows with the same capture time, file type and fingerprint -- the pixel
    hash of an image, the duration of a video. Rows without a usable fingerprint (not
    computed yet, or one that could not be taken) are in no group.

    Within a group, an OKAYED image (Datamodel/duplicateok.h) does not count:
      Duplicate  not okayed, with at least one twin that is not okayed either.
      Okayed     in a group, but not a Duplicate -- okayed itself, or every one of its
                 twins is okayed.
    So okaying either image of a pair clears both (both purple), a triple with one
    okayed keeps the other two red, and a copy that arrives later is red against any
    twin that is not okayed.

    GROUP IDS (DuplicateInfo::group) say WHICH images are twins -- a duplicated focus
    stack is thirty red pairs that look alike. Numbered from 1 in capture-time order,
    then by the group's first path, so the same Library gets the same numbers on every
    rebuild; an id only means "same group as", and is renumbered when groups come or go.
*/
namespace DuplicateStatus {
constexpr int None = 0;
constexpr int Duplicate = 1;
constexpr int Okayed = 2;
/* Shorter than any real hash: the fingerprint of a file that could not be decoded, so
   it is not retried on every check and is in no group. */
inline QByteArray failedHash() { return QByteArray(1, '\0'); }
}

struct DuplicateInfo
{
    int status = DuplicateStatus::None;
    int twins = 0;              // other images in the group
    int group = 0;              // >= 1, shared by every image in the group
    bool video = false;         // matched on duration rather than pixels
};

struct DuplicateCounts
{
    int found = 0;              // images with status Duplicate
    int okayed = 0;             // images with status Okayed
    int groups = 0;             // groups holding at least one Duplicate
};

/* path -> what the thumbnail, the column and the filter show. Paths not in a group are
   absent. */
inline QHash<QString, DuplicateInfo> classifyDuplicates(const QVector<DuplicateRow> &rows,
                                                        const QSet<QString> &okPaths,
                                                        DuplicateCounts *counts = nullptr)
{
    QHash<QByteArray, QVector<int>> groups;
    for (int i = 0; i < rows.size(); ++i) {
        const DuplicateRow &r = rows.at(i);
        QByteArray fp;
        if (r.pixelHash.size() >= 8) fp = "p" + r.pixelHash;
        else if (r.durationMs >= 0) fp = "d" + QByteArray::number(r.durationMs);
        else continue;
        const QByteArray key = QByteArray::number(r.captured) + '|' + r.ext.toUtf8()
                               + '|' + fp;
        groups[key].append(i);
    }

    /* The groups in a stable order, for their ids (see GROUP IDS above). */
    struct Ordered { qint64 captured; QString first; const QVector<int> *members; };
    QVector<Ordered> ordered;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        if (it.value().size() < 2) continue;
        QString first = rows.at(it.value().first()).path;
        for (int i : it.value()) first = std::min(first, rows.at(i).path);
        ordered.append({rows.at(it.value().first()).captured, first, &it.value()});
    }
    std::sort(ordered.begin(), ordered.end(), [](const Ordered &a, const Ordered &b) {
        return a.captured != b.captured ? a.captured < b.captured : a.first < b.first;
    });

    QHash<QString, DuplicateInfo> out;
    DuplicateCounts c;
    for (int g = 0; g < ordered.size(); ++g) {
        const QVector<int> &members = *ordered.at(g).members;
        int notOk = 0;
        for (int i : members) if (!okPaths.contains(rows.at(i).path)) ++notOk;
        bool anyRed = false;
        for (int i : members) {
            const DuplicateRow &r = rows.at(i);
            const bool ok = okPaths.contains(r.path);
            const int otherNotOk = notOk - (ok ? 0 : 1);
            DuplicateInfo info;
            info.status = (!ok && otherNotOk >= 1) ? DuplicateStatus::Duplicate
                                                   : DuplicateStatus::Okayed;
            info.twins = int(members.size()) - 1;
            info.group = g + 1;
            info.video = r.pixelHash.size() < 8;
            if (info.status == DuplicateStatus::Duplicate) { ++c.found; anyRed = true; }
            else ++c.okayed;
            out.insert(r.path, info);
        }
        if (anyRed) ++c.groups;
    }
    if (counts) *counts = c;
    return out;
}

#endif // DUPLICATECLASSIFY_H
