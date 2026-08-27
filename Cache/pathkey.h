#ifndef CACHEPATHKEY_H
#define CACHEPATHKEY_H

#include <QDir>
#include <QString>

/*
    The lookup key for anything in the local index database that is keyed by an image path
    (Cache/cachedb.h). Shared, because the key has to be computed identically by the code
    that stores a row, the code that looks one up, and the schema migration that backfills
    the column -- and because the keyword tables to come will key on the same thing.

    WHY A KEY AND NOT THE PATH

    The same file reaches Winnow spelled several different ways, and a byte-exact index
    treats each spelling as a different image:

      o DataModel stores QFileInfo::filePath(), which is the path AS CONSTRUCTED -- not
        cleaned, not absolutised.
      o Drop and paste build destinations by concatenation (dropDir + "/" + name), so a
        source folder that already ends in a separator yields a doubled slash.
      o Finder and Explorer drags arrive through QUrl::toLocalFile().
      o Windows mixes '\' and '/' and varies the drive letter's case.
      o The same folder can be reached through a symlink, through a share mounted under
        another name, or as /Volumes/X and /System/Volumes/Data/X.

    A mismatch never showed the wrong picture, but it cost real things: put() under a
    second spelling INSERTS A SECOND ROW, so the same image is cached twice at several MB
    each and the byte cap evicts sooner; get() under a second spelling silently re-decodes
    the raw; and the FileOps move/delete hooks quietly found nothing to update.

    CASE IS FOLDED. Both supported platforms are case-insensitive by default, and
    DevPreviewCache::isCachePath and DataModel already compare paths that way, so this
    follows the decision the codebase has already made rather than making a new one.

    The edge case that leaves is a case-SENSITIVE volume (APFS can be formatted that way,
    and Linux-hosted network shares usually are) holding two files whose names differ only
    in case. They fold to one key. That cannot show the wrong picture -- the source stamp
    recorded with every row disagrees and the entry is dropped, see DevPreviewCache's
    STALENESS -- so the two images take turns missing rather than lying. Wasteful in a
    situation that is close to hypothetical; a wrong photograph would not be acceptable at
    any frequency.

    Unicode is normalised to NFC on macOS, where the filesystem hands back decomposed
    forms and QUrl does not: e.g. an accented filename read from a directory listing and
    the same name arriving from a Finder drag are different QStrings for the same file.
*/
inline QString cachePathKey(const QString &path)
{
    if (path.isEmpty()) return QString();
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_MACOS
    key = key.normalized(QString::NormalizationForm_C);
#endif
    /* toCaseFolded rather than toLower: case folding is the Unicode-correct operation and
       is locale-independent, where toLower is neither. */
    return key.toCaseFolded();
}

#endif // CACHEPATHKEY_H
