#ifndef APPINSTALLER_H
#define APPINSTALLER_H

#include <QString>

/*
    AppInstaller -- installs a downloaded Winnow update over the running installation
    and relaunches it.  The platform half of "Check for updates"; the GitHub release
    lookup and version ordering are in Utilities/appupdate.h.

    The install is two steps, so that everything that can fail runs while Winnow is
    still open and can report it:

      prepare()        WORKER THREAD.  macOS: mount the DMG, verify the new bundle's
                       code signature (Winnow's identifier + our Developer ID team),
                       copy it beside the running bundle as a hidden staging bundle,
                       unmount.  Windows: nothing to do.  Linux: copy the AppImage
                       beside the running one and make it executable.
      launchFinisher() GUI THREAD.  Starts a detached helper that waits for THIS
                       process to exit, puts the new version in place and relaunches
                       it.  The caller then closes the main window.

    macOS helper: a /bin/sh script.  The DMG's bundle is version-named
    (Winnow3.1.app), so the new bundle goes in BESIDE the running one and the old one
    is moved to the Trash -- or, if the Trash cannot be reached, left where it is.
    Nothing is ever deleted.  If the swap fails the old version is relaunched and the
    DMG opened for a manual install.  The script logs to <cache>/Update/install.log.

    Windows helper: the Inno Setup installer itself, run /SILENT with /WAITPID=<pid>
    (it waits for Winnow to exit) and /RELAUNCH=1 (its [Run] entry restarts Winnow as
    the original, non-elevated user).  See packaging/windows/winnow.iss.

    Linux helper: a /bin/sh script, like macOS. The update is the new AppImage, staged
    beside the running one (APPIMAGE) as a hidden file; after Winnow exits it takes the
    old file's path and is relaunched, and the old file goes to the Trash (or is kept as
    <name>.previous). Logs to <cache>/Update/install.log.

    If the user declines the quit (unsaved picks, a background ingest), the helper keeps
    waiting and the update is installed whenever Winnow does quit.
*/
namespace AppInstaller {

/* Where downloads for an in-place install go (and the macOS install log). */
QString updateDir();

/*
    True when an update can be installed over this copy of Winnow.  When false, *reason
    says why and the caller falls back to downloading the installer and opening it
    (the user installs by hand).  macOS: the bundle must be in an Applications folder
    the user can write to, and not run from a disk image or App Translocation.
    Windows: Winnow must have been installed by the Inno Setup installer.
    Linux: Winnow must be running as an AppImage the user can replace.
*/
bool canInstallInPlace(QString *reason = nullptr);

bool prepare(const QString &installerPath, QString &error);
bool launchFinisher(const QString &installerPath, QString &error);

} // namespace AppInstaller

#endif // APPINSTALLER_H
