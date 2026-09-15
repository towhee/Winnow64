#ifndef PANELPROBE_H
#define PANELPROBE_H

#include <QtCore>
#include <QWidget>
#include <QElapsedTimer>
#include <functional>
#include <atomic>

/*
    WHY A PANEL COMES UP THE WRONG SIZE.

    Two complaints, both intermittent, both about geometry Winnow never asked for:

      NARROW PANELS AT STARTUP   the left dock group opens a fraction of its saved
                                 width, so the folder tree is a sliver.
      THUMBVIEW HALF HIDDEN      after the window is resized or a dock is dragged, the
                                 thumbDock is shorter than one icon cell, so the
                                 thumbnails are cut through the middle.

    Neither can be read off the code, because the final geometry is not set in one
    place. It is negotiated: restoreGeometry, restoreWindowState, placeDocksAddedSince,
    applyDockCollapseState, invokeWorkflowWorkspace, resizeDocks calls in
    setThumbDockFeatures and builtInDefaultWorkspace, the min/max pin in
    DockWidget::setCollapsed, and QDockAreaLayout's own redistribution all write to the
    same numbers within the first few event-loop turns. A qDebug in any one of them
    shows a plausible value that something later overwrites.

    So this records the NEGOTIATION rather than any one step:

      MARK        a labelled milestone in the startup or workspace sequence, with a
                  snapshot of every watched panel taken at that instant.
      SNAP        one panel's geometry AND its constraints -- min/max, sizeHint,
                  minimumSizeHint -- because a panel pinned narrow and a panel merely
                  allocated narrow are different bugs with the same appearance.
      RESIZE      every resize/move/show/hide the layout actually delivers, old -> new,
                  which is the only record of WHICH step overwrote a good value.
      REQUEST     an explicit sizing call Winnow made (resizeDocks, setMinimumHeight),
                  so a request that the layout silently refused is visible as a request
                  with no matching resize.
      AUDIT       a caller-supplied verdict line (MW installs one that answers "does the
                  thumbView viewport still hold a whole cell?"), re-evaluated at every
                  mark and after every settled resize.

    SUSPECTS are flagged as they are recorded -- a docked panel narrower than its own
    minimumSizeHint, a thumb viewport shorter than a cell -- and listed first in the
    report, so the timeline below them is read for the cause rather than searched for
    the symptom.

    ARMING. The startup glitch happens before any menu can be reached, so the armed
    state is PERSISTED (QSettings "PanelProbe") as well as settable by --panelprobe:
    ticking it in Help > Diagnostics arms the NEXT launch too. Arming clears the
    buffers; disarming keeps them so the report can still be read.

    THE COST WHEN DISARMED IS ONE RELAXED ATOMIC LOAD per hook site, and no event filter
    is installed until Arm(true) -- Watch() only remembers the widget.

    THREADING. GUI thread only. Every hook is a layout event or a call from MW.
*/

class PanelProbe : public QObject
{
    Q_OBJECT

public:
    static PanelProbe &Instance();

    void Arm(bool on);
    bool IsArmed() const { return armed.load(std::memory_order_relaxed); }
    void Reset();

    /*  Remember a panel to watch. Safe to call when disarmed (and normal: the docks are
        registered once at startup, whether or not the probe is ever armed). The event
        filter is installed by Arm(true) and removed by Arm(false). */
    void Watch(QWidget *w, const QString &name);

    /*  A labelled milestone, with a snapshot of every watched panel. */
    void Mark(const QString &phase);
    /*  The same, deferred to the next event-loop turn, for the value AFTER the layout
        has settled -- which for a dock is never the value at the end of the call that
        asked for it. */
    void MarkSettled(const QString &phase);

    /*  An explicit sizing call Winnow made. `what` is the call ("resizeDocks",
        "setMaximumHeight"), `value` what was asked for. A REQUEST with no RESIZE after
        it is a refused request. */
    void NoteRequest(const QString &who, const QString &what, int value);
    /*  A min/max pin, which is the other way a panel is forced to a size. */
    void NoteConstraint(const QString &who, const QString &what, int lo, int hi);

    /*  Does the thumbView viewport still hold a whole icon cell? Only MW can compute
        the cell height, so it is passed in. clippedBy > 0 is the "half hidden" bug. */
    void NoteThumbFit(int cellH, int viewportH, int dockH, int minH, int maxH,
                      const QString &src);

    /*  A verdict line the owner supplies, re-read at every mark and after every settled
        resize. MW installs one that reports the thumb fit, so the timeline carries the
        answer at every step rather than only where a hook was placed. */
    void SetAuditor(std::function<QString()> fn) { auditor = std::move(fn); }

    QString Report() const;
    /*  The report to stderr on the way out, so a --panelprobe session leaves it in
        console.txt. Silent when the probe was never armed. */
    void DumpReport() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    PanelProbe() = default;
    Q_DISABLE_COPY_MOVE(PanelProbe)

    enum Kind { Mrk, Snap, Resize, Move, Shown, Hidden, Request, Constraint, Audit, Fit };

    struct Ev {
        qint64 atMs = 0;
        Kind kind = Mrk;
        QString who;
        QString text;
        QString suspect;        // non-empty => flagged
    };

    struct Watched {
        QPointer<QWidget> w;
        QString name;
    };

    void Add(Kind kind, const QString &who, const QString &text,
             const QString &suspect = QString());
    /*  One panel's geometry and constraints, with the suspect test applied. */
    void SnapOne(const Watched &pw, const char *why);
    void SnapAll(const char *why);
    void RunAuditor(const char *why);
    /*  Coalesce the burst of resizes a single drag or restore produces into one
        settled snapshot on the next turn. */
    void ScheduleSettled(const QString &phase);
    static QString KindName(Kind k);
    static QString AreaName(const QWidget *w);

    std::atomic<bool> armed{false};
    bool everArmed = false;
    bool filtersInstalled = false;

    QElapsedTimer clock;
    QVector<Ev> evs;
    QVector<Watched> watched;
    QVector<int> suspects;      // indexes into evs
    int dropped = 0;
    bool settledPending = false;
    QString settledPhase;

    std::function<QString()> auditor;
    QString lastAudit;          // only a CHANGED verdict is recorded

    /*  Generous: a startup sequence plus a few dock drags is a few hundred events, and
        the buffer is only compacted (oldest half dropped) in a session left running. */
    static constexpr int maxEvs = 6000;
};

#endif // PANELPROBE_H
