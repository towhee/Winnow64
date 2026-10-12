#ifndef ROWTRAIL_H
#define ROWTRAIL_H

#include <QString>
#include <QStringList>

/*
    ROW TRAIL -- an always-on flight recorder of what happened to each image's row.

    Built for the intermittent "blank, unselectable thumbnail at startup": the icon was
    seen to appear for an instant and vanish, so something REMOVED it (or reset the row)
    and nothing put it back. By the time anyone looks, the model only says "no icon"; the
    question is who took it away and why the loader never came back for it. Every event
    that adds or removes a row's icon, moves its metadata status, or dispatches/returns a
    MetaRead reader for it is recorded here with a timestamp, the thread, and the caller's
    tag. MW::checkBlankRows prints the trail for any row it finds blank.

    COST. One mutex and one slot write per event, only on TRANSITIONS (not on every
    setData). The ring holds the last kCapacity events -- a 43k-row catalog load is ~4
    events a row -- and the slots are allocated on first use.

    TAGS. setData cannot know who called it, so callers that remove icons or reset rows
    wrap the work in a RowTrail::Tag; untagged events print the thread instead.
*/

namespace RowTrail {

/*  event must be a string literal (it is stored by pointer). */
void note(const QString &fPath, int dmRow, const char *event,
          const QString &detail = QString());

/*  Every recorded event for fPath, oldest first, formatted one per line. */
QStringList history(const QString &fPath);

/*  Scoped caller tag for the events recorded on this thread. src must be a literal. */
class Tag
{
public:
    explicit Tag(const char *src);
    ~Tag();
    Tag(const Tag &) = delete;
    Tag &operator=(const Tag &) = delete;
private:
    const char *prev;
};

} // namespace RowTrail

#endif // ROWTRAIL_H
