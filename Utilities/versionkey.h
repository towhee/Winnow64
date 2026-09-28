#ifndef VERSIONKEY_H
#define VERSIONKEY_H

#include <QString>

/*
    VERSION KEYS -- the identity of a DataModel row once an image can have versions
    (Lightroom's "virtual copies": a second Develop recipe for the same source file).

    A row's KEY (G::KeyRole, fPathRow, IconStore, ImageCache, stackCache, ...) is:

      master   the file path, byte for byte what it has always been
      version  path + "/#v" + id,  e.g.  /Photos/IMG_1.CR3/#v2

    and its SOURCE PATH (G::SourcePathRole) is always the file on disk. Every read or
    write of a real file must use the source path; everything that asks "which row"
    uses the key. With no versions the two are the same string, so a folder without
    versions behaves exactly as before.

    WHY "/#v"

    The key must fail LOUDLY if it ever reaches file I/O by mistake, because the
    silent alternative is a version's rating or recipe written into some other file.
    IMG_1.CR3 is a regular file, so nothing can exist beneath it: open/stat of
    "IMG_1.CR3/#v2" fails with ENOTDIR on macOS and Windows alike, and so does any
    sidecar derived from it. Rejected separators:
      o "|" and "::" are legal in APFS names, and FileOps::sidecarPath's legacy stem
        logic would turn "IMG_1.CR3|v2" into IMG_1.xmp -- the master's sidecar.
      o "\0" truncates back to the master path in toLocal8Bit.
    Belt and braces, the file I/O choke points also refuse a version key outright
    (isVersion) and raise G::issue.

    Ids are positive and never reused within one source file (see Metadata/versions.h).
    Id 0 means "the master" and has no suffix.
*/
namespace VersionKey {

inline const QString kSep = QStringLiteral("/#v");
inline constexpr int kSepLen = 3;

// Index of the separator in key, or -1 for a master key / plain path.
inline int sepIndex(const QString &key)
{
    const int i = key.lastIndexOf(kSep);
    if (i <= 0) return -1;
    const int digits = key.size() - i - kSepLen;
    if (digits <= 0) return -1;
    for (int j = i + kSepLen; j < key.size(); ++j)
        if (!key.at(j).isDigit()) return -1;
    return i;
}

inline bool isVersion(const QString &key) { return sepIndex(key) > 0; }

// The file on disk behind a row key. A master key is returned unchanged.
inline QString sourceOf(const QString &key)
{
    const int i = sepIndex(key);
    return i < 0 ? key : key.left(i);
}

// 0 for a master key, else the version id.
inline int idOf(const QString &key)
{
    const int i = sepIndex(key);
    return i < 0 ? 0 : key.mid(i + kSepLen).toInt();
}

inline QString make(const QString &sourcePath, int id)
{
    if (id <= 0) return sourcePath;
    return sourcePath + kSep + QString::number(id);
}

/*
    Appended to a row's sort key so a version sorts immediately after its master and
    before any other file. The raw key would not do: "img_1.cr3-edit.jpg" sorts
    between "img_1.cr3" and "img_1.cr3/#v2" because '-' (0x2D) < '/' (0x2F). U+0001
    sorts below every character a file name can hold. Empty for a master.
*/
inline QString sortSuffix(int id)
{
    if (id <= 0) return QString();
    return QChar(0x01) + QString::number(id).rightJustified(6, u'0');
}

} // namespace VersionKey

#endif // VERSIONKEY_H
