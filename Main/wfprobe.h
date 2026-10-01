#ifndef WFPROBE_H
#define WFPROBE_H

/*  WFPROBE -- TEMPORARY instrumentation for the Browse <-> Develop workflow switch.
    Remove (grep WFPROBE) once these are understood:

      1. the sort reverting to File Name on a switch,
      2. video thumbnails going blank on a switch,
      3. the delay leaving Develop for Browse.

    Always on, to the console (qDebug), like BMPROBE.  Every line starts "WFPROBE" so the
    Application Output pane can be filtered on it.  WfProbe::begin restarts the clock at a
    switch (D, or E / G / T / C from Develop); WfProbe::mark prints milliseconds since
    then.  WfProbe::videoCensus counts video rows and how many have lost their icon --
    it walks the whole model, so it only runs at the switch checkpoints, never per row. */

#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <functional>
#include "Datamodel/datamodel.h"
#include "Main/global.h"

namespace WfProbe {

inline QElapsedTimer &clock()
{
    static QElapsedTimer t;
    return t;
}

inline void begin(const QString &what)
{
    clock().start();
    qDebug().noquote() << "WFPROBE ======" << what;
}

inline void mark(const QString &what)
{
    const qint64 ms = clock().isValid() ? clock().elapsed() : -1;
    qDebug().noquote() << QString("WFPROBE %1 ms ").arg(ms, 6) << what;
}

inline QString videoCensus(DataModel *dm)
{
    if (!dm) return "dm=null";
    int videos = 0, videoNoIcon = 0, videoNotLoaded = 0, stillNoIcon = 0;
    QStringList blank;
    for (int row = 0; row < dm->rowCount(); ++row) {
        const bool isVideo = dm->index(row, G::VideoColumn).data().toBool();
        const bool noIcon = dm->index(row, 0).data(Qt::DecorationRole).isNull();
        if (!isVideo) {
            if (noIcon) ++stillNoIcon;
            continue;
        }
        ++videos;
        if (noIcon) {
            ++videoNoIcon;
            if (blank.size() < 5)
                blank << QString("%1:%2").arg(row)
                             .arg(dm->index(row, 0).data(G::KeyRole).toString()
                                      .section('/', -1));
        }
        if (!dm->index(row, G::IconLoadedColumn).data().toBool()) ++videoNotLoaded;
    }
    return QString("rows=%1 videos=%2 videoNoIcon=%3 videoIconLoadedFalse=%4 "
                   "stillsNoIcon=%5 %6")
        .arg(dm->rowCount()).arg(videos).arg(videoNoIcon).arg(videoNotLoaded)
        .arg(stillNoIcon).arg(blank.isEmpty() ? QString() : "blank:" + blank.join(' '));
}

/*  One line per icon dropped from a VIDEO row: who dropped it.  Video rows are exempt
    from the icon loader's "missing" counts (DataModel::countIconChunkMissing,
    MetaRead re-arm), so a cleared video icon may never be re-read -- this says which
    path clears them.  Silent until the first switch (the clock starts there), so a
    folder load or scroll before any D / E does not flood the console. */
inline void videoIconCleared(DataModel *dm, int dmRow, const char *by)
{
    if (!clock().isValid()) return;
    if (!dm || !dm->index(dmRow, G::VideoColumn).data().toBool()) return;
    mark(QString("VIDEO ICON CLEARED by %1 dmRow=%2 %3").arg(by).arg(dmRow)
             .arg(dm->index(dmRow, 0).data(G::KeyRole).toString().section('/', -1)));
}

/*  The current image three ways: the key, the proxy row the model claims for it, and the
    key actually AT that proxy row.  If the last two disagree, currentSfRow is stale --
    something re-ordered the proxy without updating it -- and any caller that selects
    sf->index(currentSfRow) moves the selection to a neighbour. */
inline QString cur(DataModel *dm)
{
    if (!dm || !dm->sf) return "cur=?";
    const QString key = dm->currentKey.section('/', -1);
    const QString at = dm->sf->index(dm->currentSfRow, 0).data(G::KeyRole).toString()
                           .section('/', -1);
    const int realRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    return QString("[cur=%1 currentSfRow=%2 atThatRow=%3 currentDmIdx->sfRow=%4%5]")
        .arg(key).arg(dm->currentSfRow).arg(at).arg(realRow)
        .arg(realRow != dm->currentSfRow ? " STALE" : "");
}

/*  Every proxy re-order while the probe clock runs.  A sort with ties (Created time
    shared by scans) can swap neighbours whenever the proxy re-sorts itself -- dynamic
    sort on a dataChanged -- and that is invisible to sortChange's probe. */
inline void watchProxy(DataModel *dm)
{
    static bool wired = false;
    if (wired || !dm || !dm->sf) return;
    wired = true;
    QObject::connect(dm->sf, &QAbstractItemModel::layoutChanged, dm->sf, [dm] {
        if (clock().isValid()) mark("PROXY layoutChanged (re-sort/re-order) " + cur(dm));
    });
    QObject::connect(dm->sf, &QAbstractItemModel::modelReset, dm->sf, [dm] {
        if (clock().isValid()) mark("PROXY modelReset " + cur(dm));
    });
}

/*  The switch's tail: the posted repaint, the icon loader and any deferred work land
    after the switch function returns, so the state is sampled again as the event loop
    turns and at 0.5, 2 and 5 s.  `state` is built by the caller (MW members are
    private). */
inline void followUps(QObject *ctx, const QString &label, std::function<QString()> state)
{
    for (int ms : {0, 500, 2000, 5000})
        QTimer::singleShot(ms, ctx, [label, state, ms] {
            mark(QString("%1 +%2 ms after: %3").arg(label).arg(ms).arg(state()));
        });
}

}   // namespace WfProbe

#endif // WFPROBE_H
