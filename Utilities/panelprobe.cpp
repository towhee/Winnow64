#include "Utilities/panelprobe.h"
#include "Main/global.h"
#include "Utilities/utilities.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QResizeEvent>
#include <QMoveEvent>
#include <QTimer>

/*
    See panelprobe.h for what this records and why the existing logging could not answer
    the question.
*/

PanelProbe &PanelProbe::Instance()
{
    static PanelProbe probe;
    return probe;
}

/* ---------------------------------------------------------------- arm / reset ---- */

void PanelProbe::Arm(bool on)
{
    if (on) {
        Reset();
        everArmed = true;
        clock.start();
        armed.store(true, std::memory_order_relaxed);
        if (!filtersInstalled) {
            for (const Watched &pw : watched)
                if (pw.w) pw.w->installEventFilter(this);
            filtersInstalled = true;
        }
        /*  A baseline: everything the panels are before the first watched event, so a
            later resize has something to be a change FROM. */
        SnapAll("armed");
        RunAuditor("armed");
    }
    else {
        armed.store(false, std::memory_order_relaxed);
        if (filtersInstalled) {
            for (const Watched &pw : watched)
                if (pw.w) pw.w->removeEventFilter(this);
            filtersInstalled = false;
        }
        /*  The buffers are KEPT: a probe is read after the fact. */
    }
}

void PanelProbe::Reset()
{
    evs.clear();
    suspects.clear();
    dropped = 0;
    lastAudit.clear();
    settledPending = false;
    clock.start();
}

void PanelProbe::Watch(QWidget *w, const QString &name)
{
    if (!w) return;
    for (const Watched &pw : watched)
        if (pw.w == w) return;
    watched.append({QPointer<QWidget>(w), name});
    if (filtersInstalled) w->installEventFilter(this);
}

/* ------------------------------------------------------------------ recording ---- */

void PanelProbe::Add(Kind kind, const QString &who, const QString &text,
                     const QString &suspect)
{
    if (evs.size() >= maxEvs) {
        /*  Drop the oldest half rather than stopping: in a session left running it is
            the RECENT gesture that is being explained, and the startup sequence is only
            a few hundred events, so it survives anything short of a marathon. */
        const int cut = maxEvs / 2;
        evs.remove(0, cut);
        dropped += cut;
        QVector<int> kept;
        for (int i : suspects) if (i >= cut) kept.append(i - cut);
        suspects = kept;
    }
    Ev e;
    e.atMs = clock.isValid() ? clock.elapsed() : 0;
    e.kind = kind;
    e.who = who;
    e.text = text;
    e.suspect = suspect;
    evs.append(e);
    if (!suspect.isEmpty()) suspects.append(evs.size() - 1);
}

void PanelProbe::SnapOne(const Watched &pw, const char *why)
{
    QWidget *w = pw.w;
    if (!w) return;

    const QSize sz = w->size();
    const QSize hint = w->sizeHint();
    const QSize minHint = w->minimumSizeHint();
    const auto *dock = qobject_cast<QDockWidget*>(w);
    const bool floating = dock && dock->isFloating();

    QString t;
    QTextStream s(&t);
    s << sz.width() << "x" << sz.height()
      << "  minmax " << w->minimumWidth() << ".." << w->maximumWidth()
      << " / " << w->minimumHeight() << ".." << w->maximumHeight()
      << "  hint " << hint.width() << "x" << hint.height()
      << "  minHint " << minHint.width() << "x" << minHint.height()
      << "  " << (w->isVisible() ? "vis" : "hidden");
    if (dock) {
        s << (floating ? " floating" : " docked") << " " << AreaName(w);
        if (dock->widget())
            s << "  body " << dock->widget()->size().width()
              << "x" << dock->widget()->size().height();
    }
    if (why && *why) s << "   (" << why << ")";

    /*  THE NARROW-PANEL TEST. A docked, visible panel allocated less than the width its
        own content says it needs. minimumSizeHint is the right yardstick rather than
        sizeHint: sizeHint is a preference the layout may fairly ignore, minimumSizeHint
        is not, so falling below it means the allocation came from somewhere that did
        not consult the widget at all. Floating and hidden panels are exempt -- neither
        is laid out by QDockAreaLayout. */
    QString suspect;
    if (w->isVisible() && !floating && minHint.width() > 0
        && sz.width() < minHint.width()) {
        suspect = QString("NARROW: width %1 < content minimum %2")
                      .arg(sz.width()).arg(minHint.width());
    }
    else if (w->isVisible() && !floating && minHint.height() > 0
             && sz.height() < minHint.height()) {
        suspect = QString("SHORT: height %1 < content minimum %2")
                      .arg(sz.height()).arg(minHint.height());
    }

    Add(Snap, pw.name, t, suspect);
}

void PanelProbe::SnapAll(const char *why)
{
    for (const Watched &pw : watched) SnapOne(pw, why);
}

void PanelProbe::RunAuditor(const char *why)
{
    if (!auditor) return;
    const QString verdict = auditor();
    if (verdict.isEmpty() || verdict == lastAudit) return;
    lastAudit = verdict;
    /*  The auditor reports its own suspicion by prefixing "!": it knows what a bad
        answer is, the probe only knows it changed. */
    const bool bad = verdict.startsWith('!');
    Add(Audit, "audit", QString("%1   (%2)").arg(bad ? verdict.mid(1) : verdict, why),
        bad ? verdict.mid(1) : QString());
}

void PanelProbe::Mark(const QString &phase)
{
    if (!IsArmed()) return;
    Add(Mrk, "--", phase);
    SnapAll("mark");
    RunAuditor("mark");
}

void PanelProbe::ScheduleSettled(const QString &phase)
{
    if (settledPending) return;
    settledPending = true;
    settledPhase = phase;
    QTimer::singleShot(0, this, [this]() {
        settledPending = false;
        if (!IsArmed()) return;
        Add(Mrk, "--", "settled: " + settledPhase);
        SnapAll("settled");
        RunAuditor("settled");
    });
}

void PanelProbe::MarkSettled(const QString &phase)
{
    if (!IsArmed()) return;
    ScheduleSettled(phase);
}

void PanelProbe::NoteRequest(const QString &who, const QString &what, int value)
{
    if (!IsArmed()) return;
    Add(Request, who, QString("%1(%2)").arg(what).arg(value));
    ScheduleSettled(who + " " + what);
}

void PanelProbe::NoteConstraint(const QString &who, const QString &what, int lo, int hi)
{
    if (!IsArmed()) return;
    Add(Constraint, who, QString("%1 %2..%3").arg(what).arg(lo).arg(hi));
    ScheduleSettled(who + " " + what);
}

void PanelProbe::NoteThumbFit(int cellH, int viewportH, int dockH, int minH, int maxH,
                              const QString &src)
{
    if (!IsArmed()) return;
    const int clippedBy = cellH - viewportH;
    QString t = QString("cell %1  viewport %2  dock %3  thumbView minmax %4..%5  [%6]")
                    .arg(cellH).arg(viewportH).arg(dockH).arg(minH).arg(maxH).arg(src);
    /*  THE HALF-HIDDEN TEST. One pixel short is a rounding difference nobody sees; the
        complaint is an icon cut through the middle, so the threshold is a visible
        fraction of a cell. */
    QString suspect;
    if (clippedBy > 2)
        suspect = QString("CLIPPED: thumb cell %1 needs %2 more px than the viewport's %3")
                      .arg(cellH).arg(clippedBy).arg(viewportH);
    Add(Fit, "thumbView", t, suspect);
}

/* --------------------------------------------------------------- event filter ---- */

bool PanelProbe::eventFilter(QObject *obj, QEvent *event)
{
    if (!IsArmed()) return QObject::eventFilter(obj, event);

    const QEvent::Type type = event->type();
    if (type != QEvent::Resize && type != QEvent::Move && type != QEvent::Show
        && type != QEvent::Hide)
        return QObject::eventFilter(obj, event);

    QString name;
    for (const Watched &pw : watched)
        if (pw.w == obj) { name = pw.name; break; }
    if (name.isEmpty()) return QObject::eventFilter(obj, event);

    auto *w = static_cast<QWidget*>(obj);
    switch (type) {
    case QEvent::Resize: {
        auto *re = static_cast<QResizeEvent*>(event);
        Add(Resize, name, QString("%1x%2 -> %3x%4")
                              .arg(re->oldSize().width()).arg(re->oldSize().height())
                              .arg(re->size().width()).arg(re->size().height()));
        ScheduleSettled(name + " resize");
        break;
    }
    case QEvent::Move: {
        auto *me = static_cast<QMoveEvent*>(event);
        Add(Move, name, QString("(%1,%2) -> (%3,%4)")
                            .arg(me->oldPos().x()).arg(me->oldPos().y())
                            .arg(me->pos().x()).arg(me->pos().y()));
        ScheduleSettled(name + " move");
        break;
    }
    case QEvent::Show:
        Add(Shown, name, QString("%1x%2 %3")
                             .arg(w->size().width()).arg(w->size().height())
                             .arg(AreaName(w)));
        ScheduleSettled(name + " show");
        break;
    default:
        Add(Hidden, name, QString("%1x%2").arg(w->size().width()).arg(w->size().height()));
        break;
    }

    return QObject::eventFilter(obj, event);
}

/* --------------------------------------------------------------------- report ---- */

QString PanelProbe::KindName(Kind k)
{
    switch (k) {
    case Mrk:        return "MARK";
    case Snap:       return "SNAP";
    case Resize:     return "RESIZE";
    case Move:       return "MOVE";
    case Shown:      return "SHOW";
    case Hidden:     return "HIDE";
    case Request:    return "REQUEST";
    case Constraint: return "PIN";
    case Audit:      return "AUDIT";
    case Fit:        return "FIT";
    }
    return "?";
}

QString PanelProbe::AreaName(const QWidget *w)
{
    const auto *dock = qobject_cast<const QDockWidget*>(w);
    if (!dock) return QString();
    const auto *mw = qobject_cast<const QMainWindow*>(dock->parentWidget());
    if (!mw) return "no-mainwindow";
    switch (mw->dockWidgetArea(const_cast<QDockWidget*>(dock))) {
    case Qt::LeftDockWidgetArea:   return "left";
    case Qt::RightDockWidgetArea:  return "right";
    case Qt::TopDockWidgetArea:    return "top";
    case Qt::BottomDockWidgetArea: return "bottom";
    default:                       return "no-area";
    }
}

QString PanelProbe::Report() const
{
    QString reportString;
    QTextStream rpt(&reportString);
    rpt << Utilities::centeredRptHdr('=', "Panel Probe Diagnostics");
    rpt << "\n\n";

    if (!everArmed) {
        rpt << "The panel probe has never been armed in this session, so there is "
               "nothing to report.\n\n"
               "Arm it at Help > Diagnostics > \"Panel probe (record panel sizing)\", "
               "which also arms it for the NEXT launch -- the startup sizing glitch "
               "happens before any menu can be reached.  Winnow --panelprobe does the "
               "same for one run.\n";
        return reportString;
    }

    rpt << "Armed        = " << (IsArmed() ? "yes (recording)" : "no (buffers kept)");
    rpt << "\n" << "Duration     = " << (clock.isValid() ? clock.elapsed() : 0) << " ms";
    rpt << "\n" << "Events       = " << evs.size();
    if (dropped) rpt << " (+" << dropped << " older events dropped)";
    rpt << "\n" << "Watched      = " << watched.size() << " panels";
    rpt << "\n\n";

    /*  SUSPECTS FIRST. The timeline is read for the cause; it should not have to be
        searched for the symptom. */
    rpt << "SUSPECTS (" << suspects.size() << ")\n";
    if (suspects.isEmpty()) {
        rpt << "  none -- every panel stayed at or above its content minimum, and the "
               "thumb viewport always held a whole cell.\n";
    }
    else {
        for (int i : suspects) {
            const Ev &e = evs.at(i);
            rpt << "  " << QString("t=%1").arg(e.atMs, 6) << "  "
                << QString("%1").arg(e.who, -14) << "  " << e.suspect << "\n";
        }
    }
    rpt << "\n";

    rpt << "PANELS NOW\n";
    for (const Watched &pw : watched) {
        if (!pw.w) { rpt << "  " << QString("%1").arg(pw.name, -14) << "  (deleted)\n"; continue; }
        const QWidget *w = pw.w;
        rpt << "  " << QString("%1").arg(pw.name, -14) << "  "
            << w->size().width() << "x" << w->size().height()
            << "  minmax " << w->minimumWidth() << ".." << w->maximumWidth()
            << " / " << w->minimumHeight() << ".." << w->maximumHeight()
            << "  hint " << w->sizeHint().width() << "x" << w->sizeHint().height()
            << "  minHint " << w->minimumSizeHint().width() << "x"
            << w->minimumSizeHint().height()
            << "  " << (w->isVisible() ? "vis" : "hidden") << " " << AreaName(w) << "\n";
    }
    rpt << "\n";

    rpt << "TIMELINE\n";
    for (const Ev &e : evs) {
        rpt << QString("t=%1").arg(e.atMs, 6) << "  "
            << QString("%1").arg(KindName(e.kind), -8)
            << QString("%1").arg(e.who, -14) << "  " << e.text;
        if (!e.suspect.isEmpty()) rpt << "   <-- " << e.suspect;
        rpt << "\n";
    }
    rpt << "\n";

    return reportString;
}

void PanelProbe::DumpReport() const
{
    if (!everArmed) return;
    qDebug().noquote() << "\n" << Report();
}
