#ifndef DEVPREVIEWPOLICY_H
#define DEVPREVIEWPOLICY_H

#include <QString>

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
}

#endif // DEVPREVIEWPOLICY_H
