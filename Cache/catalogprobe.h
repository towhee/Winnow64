#ifndef CATALOGPROBE_H
#define CATALOGPROBE_H

#include <QString>

/*
    WHAT DOES IT COST TO BROWSE THE WHOLE CATALOG?

    The plan to drop the search result limit and let Catalog scope behave exactly like a
    folder -- every row in the model, "Pos: 1 of 50,000", icons and verification chunked
    behind the scenes -- rests on three numbers that had only ever been estimated:

      1. the SQL pass that produces the rows,
      2. the STAT sweep that decides which of them are stale,
      3. the FILE re-read that a stale row (or a row with no index entry) costs.

    Everything in the plan follows from the ratio between 2 and 3. If a stat is a hundred
    times cheaper than a read, verification can happen as rows scroll into view and be
    invisible; if it is not, the whole verify-lazily design is wrong and the index has to
    be trusted outright. That is a measurement, and static reasoning has picked the wrong
    culprit here before.

    IT RUNS AGAINST THE REAL INDEX, deliberately. A synthetic database would answer a
    question nobody asked: the costs here are dominated by how many rows the user actually
    has, how they are spread across volumes, and whether those volumes are local. So this
    is not a unit test -- it opens the user's own catalog, reads, and writes nothing.

      Winnow --catalogprobe                  every volume, defaults below
      Winnow --catalogprobe /Volumes/NAS     only rows whose path contains that text

    The filter argument is how local and network are compared: run it twice.

      WINNOW_PROBE_ROWS    rows to fetch and stat        (default 50000)
      WINNOW_PROBE_SAMPLE  rows to fully re-read         (default 500)
      WINNOW_PROBE_THREADS re-read concurrency, 0 = auto (default 1, serial)

    THE RE-READ IS SAMPLED because it is the expensive one -- at the ~20 ms per raw that
    Documentation.txt records, ten thousand rows would be several minutes. The sample's
    per-row mean is reported, and the extrapolation to the full row count is printed as an
    extrapolation rather than as a measurement.

    Results go to stderr and the process exits; no window is ever shown.
*/
namespace CatalogProbe {

/*  Run the probe and return a process exit code: 0 when it produced numbers, 2 when the
    catalog could not be opened or holds nothing to measure. pathFilter is a plain
    substring match against the indexed path, empty for all rows. */
int run(const QString &pathFilter);

}   // namespace CatalogProbe

#endif // CATALOGPROBE_H
