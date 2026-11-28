#ifndef DEVPREVIEWPOLICY_H
#define DEVPREVIEWPOLICY_H

#include <QString>
#include <QList>
#include <QtGlobal>
#include <algorithm>
#include <cstdlib>

#include "Main/catalogscope.h"
#include "Utilities/volumeinfo.h"

/*
    WHERE THE BACKGROUND devPreview BUILD IS ALLOWED TO RUN. One rule, in one place,
    because it is the only thing standing between a camera card and hours of sensor
    decoding whose entire output is discarded at ingest. See
    MW::queueBackgroundDevPreviewBuild for the full reasoning and
    notes/Documentation.txt "Original and Developed Previews".

    Its own header rather than a lambda inside the builder so the rule can be tested
    without a MainWindow -- it is pure, and the two conditions it composes are the two
    that are easy to get subtly wrong (prefix matching on folder paths, and what counts
    as removable media).

    THIS DOES NOT GOVERN Develop > Build Developed Previews. Asking for a build
    explicitly, on a card, means it.
*/
namespace DevPreviewPolicy
{
    /*  scope is the user's catalog scope table (MW::catalogScope).

        WITH A SCOPE TABLE the answer is "did the user nominate this folder", which is the
        same statement as "is this part of my library". It is a PREFIX test, so a
        subfolder created a minute ago under a recursive include row qualifies at once --
        no catalog scan has to run first.

        WITHOUT ONE the question has not been asked yet (promptForCatalogScope asks it
        once, and can be dismissed), so fall back to the volume: build on fixed local
        drives, never on removable or network media. That keeps the preference useful out
        of the box without touching a card. */
    inline bool folderEligible(const CatalogScope &scope, const QString &folder)
    {
        if (folder.isEmpty()) return false;
        if (scope.isEmpty()) return VolumeInfo::isLocalFixed(folder);
        return catalogScopeIncludes(scope, folder) &&
               !catalogScopeExcludes(scope, folder);
    }

    /*  WHICH IMAGES ARE WORTH A SPECULATIVE devPreview (autoDevPreviews, the background
        build, the harvest). Develop > Build Developed Previews is NOT governed by this --
        asking means it. See notes/Documentation.md "autoDevPreviews".

          edited             always: without one, Browse shows the image unedited
          small-preview raw  the embedded JPEG is too small for the loupe (Sony ARWs carry
                             1616 px), so Browse shows a soft, upscaled picture; a raw
                             with a good embedded preview is NOT built -- Browse already
                             shows it at JPEG speed, and a default render would only
                             change the look and spend the cache
          slow non-raw       the loupe decode measured slow (a 16-bit LZW TIFF takes over
                             a second): HARVESTED from that decode, never rendered

        Thresholds are named here so they are tested in one place. */

    /* An embedded preview under this fraction of what the loupe needs counts as small:
       an OM-1's 3200 px against a 3840 px screen (0.83) is fine, a 1616 px ARW is not. */
    inline constexpr double kSmallPreviewFraction = 0.75;
    /* A decode at least this long, AND this many times what a JPEG of the same pixel
       count would take, is slow enough to cache. */
    inline constexpr int kSlowDecodeMs = 400;
    inline constexpr double kSlowDecodeRatio = 3.0;
    /* libjpeg-turbo on one core decodes roughly 125 MP/s: ~8 ms per megapixel. */
    inline constexpr double kJpegMsPerMP = 8.0;
    /* Speculative builds stop, never evict, from here: see nearlyFull. */
    inline constexpr double kNearlyFullFraction = 0.90;
    /* How long the user must leave the selection and the scroll alone before an
       autoDevPreviews pass starts. Riffling restarts the wait, so it never competes. */
    inline constexpr int kAutoDelayMs = 1500;
    /* An auto step cancelled for performance this many times is skipped for the rest of
       the session: whatever it does (a slow mask on the GUI thread, a huge file), it
       keeps costing the user, and retrying it every pass would waste a decode each
       time. */
    inline constexpr int kAutoPerfMissLimit = 2;
    /* The event loop stalling this long while an auto step runs means it is costing the
       user, and the step is abandoned. */
    inline constexpr int kAutoLagMs = 250;

    /*  The loupe's need, in pixels on the long edge: the screen's long edge in device
        pixels, but never more than the sensor (a 12 MP raw on an 8K screen needs only
        its own size). */
    inline int loupeNeed(int sensorW, int sensorH, int screenEdge)
    {
        const int sensorEdge = qMax(sensorW, sensorH);
        if (screenEdge <= 0) return sensorEdge;
        if (sensorEdge <= 0) return screenEdge;
        return qMin(sensorEdge, screenEdge);
    }

    /*  True when a raw's embedded preview is too small for the loupe. A raw with no
        preview size recorded at all counts as small: there is nothing good to show. */
    inline bool smallPreviewRaw(int previewW, int previewH, int sensorW, int sensorH,
                                int screenEdge)
    {
        const int previewEdge = qMax(previewW, previewH);
        if (previewEdge <= 0) return true;
        const int need = loupeNeed(sensorW, sensorH, screenEdge);
        if (need <= 0) return false;
        return previewEdge < need * kSmallPreviewFraction;
    }

    /*  Formats whose devPreview can be HARVESTED from the loupe's own decode: non-raw
        files that can be slow (16-bit or LZW TIFF, 16-bit PNG) AND that the decoder color
        manages (Metadata::iccFormats), because the harvested preview is converted out of
        the file's profile. JPEG is excluded: a JPEG of a JPEG saves nothing. */
    inline bool sourceRenderFormat(const QString &ext)
    {
        return ext == "tif" || ext == "png";
    }

    /*  Is a measured loupe decode slow enough to be worth a cached JPEG? */
    inline bool slowDecode(qint64 ms, int width, int height)
    {
        if (ms < kSlowDecodeMs) return false;
        if (width <= 0 || height <= 0) return true;
        const double mp = double(width) * height / 1.0e6;
        const double jpegMs = qMax(1.0, mp * kJpegMsPerMP);
        return ms >= jpegMs * kSlowDecodeRatio;
    }

    /*  The cache is too full for SPECULATIVE work. An explicit build and the byproduct
        writes of editing still go through the LRU; a speculative one would only evict
        previews the user may want for previews they have not asked for. */
    inline bool nearlyFull(qint64 usedBytes, qint64 capBytes)
    {
        if (capBytes <= 0) return true;
        return usedBytes >= qint64(capBytes * kNearlyFullFraction);
    }

    /*  The cache size to suggest when it is nearly full: the smallest offered size
        (steps, in bytes, ascending) that is larger than the cap and fits inside what is
        used now plus a quarter of the free disk. 0 = nothing sensible to suggest (the
        disk itself is short, or the cap is already the largest step). */
    inline qint64 suggestCap(qint64 usedBytes, qint64 capBytes, qint64 freeBytes,
                             const QList<qint64> &steps)
    {
        const qint64 ceiling = usedBytes + freeBytes / 4;
        for (qint64 s : steps)
            if (s > capBytes && s <= ceiling) return s;
        return 0;
    }

    /*  Rows ordered nearest the current row first, ties going FORWARD (the way the user
        is most likely heading, and the read-ahead's own direction). The current row
        itself is left out: its picture is on screen and must not change under the user.
        Stable for equal distances on the same side. */
    inline QList<int> nearestFirst(QList<int> rows, int currentRow)
    {
        rows.removeAll(currentRow);
        std::stable_sort(rows.begin(), rows.end(), [currentRow](int a, int b) {
            const int da = std::abs(a - currentRow), db = std::abs(b - currentRow);
            if (da != db) return da < db;
            return a > b;           // same distance: the one ahead of the current first
        });
        return rows;
    }
}

#endif // DEVPREVIEWPOLICY_H
