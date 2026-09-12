#include "Utilities/volumeinfo.h"

#include <QDir>
#include <QFile>
#include <QStorageInfo>

#if defined(Q_OS_MAC)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace VolumeInfo {

VolumeInfo::Kind kindOf(const QString &path)
{
    if (path.isEmpty()) return Kind::Unknown;

#if defined(Q_OS_MAC)
    /*  statfs reports the mount that contains path, so an image file and its folder give
        the same answer and neither needs the mount table walked. QFile::encodeName is
        what turns a QString path into the filesystem's own bytes. */
    struct statfs st;
    if (statfs(QFile::encodeName(path).constData(), &st) != 0) return Kind::Unknown;

    if (!(st.f_flags & MNT_LOCAL)) return Kind::Network;
    if (st.f_flags & MNT_REMOVABLE) return Kind::Removable;

    /*  Local and not removable. Everything mounted under /Volumes is a second volume
        rather than the boot filesystem; the distinction is cosmetic here (both are
        "fixed") but it is what makes a log line readable. */
    const QString mount = QString::fromLocal8Bit(st.f_mntonname);
    return mount.startsWith("/Volumes/") ? Kind::FixedExternal : Kind::Internal;

#elif defined(Q_OS_WIN)
    const QString root = QStorageInfo(path).rootPath();
    if (root.isEmpty()) return Kind::Unknown;
    const QString native = QDir::toNativeSeparators(
        root.endsWith('\\') || root.endsWith('/') ? root : root + '/');
    switch (GetDriveTypeW(reinterpret_cast<const wchar_t *>(native.utf16()))) {
    case DRIVE_FIXED:       return Kind::FixedExternal;
    case DRIVE_REMOVABLE:
    case DRIVE_CDROM:       return Kind::Removable;
    case DRIVE_REMOTE:      return Kind::Network;
    case DRIVE_RAMDISK:     return Kind::Internal;
    default:                return Kind::Unknown;
    }

#else
    Q_UNUSED(path)
    return Kind::Unknown;
#endif
}

bool isLocalFixed(const QString &path)
{
    const Kind k = kindOf(path);
    /*  Unknown counts as local fixed on purpose: a failed query must not silently
        withhold work that used to happen. See the header. */
    return k == Kind::Internal || k == Kind::FixedExternal || k == Kind::Unknown;
}

QString kindName(Kind kind)
{
    switch (kind) {
    case Kind::Internal:      return "internal";
    case Kind::FixedExternal: return "external";
    case Kind::Removable:     return "removable";
    case Kind::Network:       return "network";
    case Kind::Unknown:       break;
    }
    return "unknown";
}

}  // namespace VolumeInfo
