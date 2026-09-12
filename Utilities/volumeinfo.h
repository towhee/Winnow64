#ifndef VOLUMEINFO_H
#define VOLUMEINFO_H

#include <QString>

/*
    WHAT KIND OF VOLUME A PATH LIVES ON: internal, fixed external, removable, or a
    network share. One question, asked where the answer changes what Winnow does with
    somebody's files -- currently the background devPreview build, which must not spend
    hours decoding a camera card whose contents are about to be ingested to a different
    path and then erased.

    WHY NOT UsbUtil::isRemovable. That function answers a different question and answers
    it coarsely: on macOS it is "does the path start with /Volumes/", which is true of a
    permanently attached external library drive as well as of an SD card. It is right
    enough for deciding whether to offer Eject -- you can eject both -- and wrong for
    deciding whether a volume is worth building a cache against. Left alone rather than
    tightened, because the Eject menu item depends on its present reading.

    THE REAL SIGNALS ARE PER-PLATFORM AND BOTH ARE NEEDED.

      macOS     statfs() on the path -- one syscall, and it reports the flags of the
                mount that CONTAINS the path, so no mount-table walk and no prefix
                matching. MNT_REMOVABLE marks the card; MNT_LOCAL distinguishes a real
                filesystem from a network mount. An exfat card reads local=1 removable=1
                and an APFS data volume reads local=1 removable=0, so neither flag alone
                separates them.
      Windows   GetDriveTypeW on the volume root, which names all three cases directly.

    ANYWHERE ELSE, AND ON ANY FAILURE, THE ANSWER IS Unknown AND Unknown COUNTS AS LOCAL
    FIXED. The callers use this to WITHHOLD work, so an unrecognised platform or a failed
    syscall must fall back to the behaviour that existed before this file did. A wrong
    "skip" is a feature that silently stopped working; a wrong "build" is a cache fill
    nobody asked for. The first is worse.

    NO CACHING HERE, deliberately -- the same discipline MountSnapshot follows (see
    Cache/mountsnapshot.h). A verdict about removable media is only true while the media
    is still there, and a stale cached answer about an ejected card is exactly the
    dangerous one. A caller that asks per file memoises the answer for the life of one
    pass and then throws it away.
*/
namespace VolumeInfo
{
    enum class Kind {
        Internal,       // the boot volume, or another fixed filesystem on it
        FixedExternal,  // permanently attached external drive
        Removable,      // card, USB stick, optical
        Network,        // SMB, NFS, AFP, WebDAV
        Unknown         // unrecognised platform, or the query failed
    };

    Kind kindOf(const QString &path);

    /*  The question every caller actually has: is this somewhere worth spending
        background work on? Unknown is included -- see the fallback rule above. */
    bool isLocalFixed(const QString &path);

    /* For log lines and diagnostics. */
    QString kindName(Kind kind);
}

#endif // VOLUMEINFO_H
