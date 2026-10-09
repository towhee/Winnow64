#ifndef KEYWORDAUDITPROBE_H
#define KEYWORDAUDITPROBE_H

#include <QString>

/*
    --keywordaudit [keyword filter]: run Audit Keywords against the REAL index, headless,
    and print what it found -- the numbers the thresholds in Utilities/visualaudit.h are
    re-measured with, without opening the window.

    Runs exactly what the dialog's Run does (KeywordAuditJob::run): vectors for any
    keyworded image that lacks one are computed and STORED in index.db, as the dialog
    would store them, then everything is scored. Derived data only -- no image, sidecar
    or userdata.db row is written. Like --catalogprobe it does not enter QStandardPaths
    test mode, so it reads the user's real index and keyword list.

    The filter is a case-insensitive substring of the keyword path; it limits the
    findings PRINTED, not what is scored.

    Results go to stderr and the process exits: 0 when the audit ran, 2 when it could not.
*/
namespace KeywordAuditProbe {

int run(const QString &keywordFilter);

}   // namespace KeywordAuditProbe

#endif // KEYWORDAUDITPROBE_H
