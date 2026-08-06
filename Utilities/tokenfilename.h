#ifndef TOKENFILENAME_H
#define TOKENFILENAME_H

#include <QFileInfo>
#include <QMap>
#include <QString>
#include <QStringList>
#include "Metadata/imagemetadata.h"

/*
    Winnow's filename TOKEN language, in one place.

    A token template is plain text with {TOKEN} placeholders that expand from an image's
    metadata -- "{YYYY}-{MM}-{DD}_{ORIGINAL FILENAME}" becomes "2026-08-05_DSC01234". The
    same templates are shared by Ingest, Rename and Export (they all read the one
    MW::filenameTemplates map and are all edited by the one token editor), so the parser
    that expands them belongs here rather than in any one dialog.

    This was extracted from IngestDlg::parseTokenString / RenameFile::parseTokenString,
    which held two copies of it; both now delegate here so a token added below appears
    everywhere at once.

    SEQUENCE TOKENS. {XX} .. {XXXXXXX} expand to seqNum zero-padded to the token's width.
    The caller owns the numbering (Ingest counts across the whole ingest, Export across
    the batch), so seqNum is passed in rather than tracked here.
*/
namespace TokenFileName {

/* Token name -> the example value shown in the token editor. This map IS the set of
   valid tokens: isToken() accepts a {...} only if its name is a key here, so adding a
   token means adding it here and handling it in parse(). */
const QMap<QString, QString> &exampleMap();

/* Token names in the order the editor lists them. */
QStringList tokens();

/*
    True if position pos in tokenString falls inside a valid {TOKEN}. On success, token is
    the token name and start/end are the positions of its '{' and one past its '}', which
    is what lets parse() copy non-token text through verbatim.
*/
bool isToken(const QString &tokenString, int pos,
             QString &token, int &start, int &end);

/*
    Expand tokenString for one image. m supplies the metadata (created date, title, camera
    ...) and info the file name; seqNum feeds the {XX...} sequence tokens. Text outside a
    valid token is copied through unchanged, so a template with no tokens returns itself.
*/
QString parse(const ImageMetadata &m, const QFileInfo &info,
              const QString &tokenString, int seqNum = 0);

} // namespace TokenFileName

#endif // TOKENFILENAME_H
