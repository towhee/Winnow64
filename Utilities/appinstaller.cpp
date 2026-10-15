#include "appinstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace AppInstaller {

/* The relaunched Winnow inherits this process's environment (macOS `open` passes it
   on, and so does the Windows installer).  Drop the update-test variables
   (MW::checkForUpdate) first, or the new copy would offer the same test release again
   at startup. */
static void clearTestEnvironment()
{
    qunsetenv("WINNOW_UPDATE_TAG");
    qunsetenv("WINNOW_UPDATE_AS_VERSION");
}

QString updateDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    dir = QDir(dir).filePath("Update");
    QDir().mkpath(dir);
    return dir;
}

#ifdef Q_OS_MAC

/* Designated requirement every update must satisfy: Winnow's bundle id, signed with a
   Developer ID certificate issued to our team.  Checked with codesign before the new
   bundle is staged, so a tampered or foreign DMG is never installed. */
static const char *kRequirement =
    "identifier \"com.winnow.Winnow\" and anchor apple generic"
    " and certificate leaf[subject.OU] = \"2663CS489R\"";

/* Set by prepare() for launchFinisher(), which runs after it on the GUI thread. */
static QString stagedBundle;        // <Applications>/.WinnowUpdate.app
static QString newBundle;           // <Applications>/<name of the bundle in the DMG>

static QString runningBundle()
{
    QDir d(QCoreApplication::applicationDirPath());     // .../Winnow.app/Contents/MacOS
    d.cdUp();
    d.cdUp();
    return d.absolutePath();
}

/* Runs a tool to completion; on failure *error gets its output. */
static bool run(const QString &program, const QStringList &args, int timeoutMs,
                QString &error, QString *output = nullptr)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (!p.waitForStarted(10000)) {
        error = "Could not run " + program;
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        error = QFileInfo(program).fileName() + " timed out.";
        return false;
    }
    const QString out = QString::fromUtf8(p.readAll()).trimmed();
    if (output) *output = out;
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        error = QFileInfo(program).fileName() + " failed: " + out;
        return false;
    }
    return true;
}

bool canInstallInPlace(QString *reason)
{
    auto no = [reason](const QString &why) { if (reason) *reason = why; return false; };

    const QString bundle = runningBundle();
    const QFileInfo bundleInfo(bundle);
    const QFileInfo parentInfo(bundleInfo.absolutePath());
    if (!bundle.endsWith(".app"))
        return no("Winnow is not running from an application bundle.");
    if (bundle.startsWith("/Volumes/") && parentInfo.fileName() != "Applications")
        return no("Winnow is running from a disk image.");
    if (bundle.contains("/AppTranslocation/"))
        return no("macOS is running Winnow from a temporary location "
                  "(move it to the Applications folder).");
    if (parentInfo.fileName() != "Applications")
        return no("Winnow is not in an Applications folder.");
    if (!parentInfo.isWritable() || !bundleInfo.isWritable())
        return no("You do not have permission to change " + parentInfo.absoluteFilePath() + ".");
    return true;
}

bool prepare(const QString &installerPath, QString &error)
{
    stagedBundle.clear();
    newBundle.clear();

    const QString parent = QFileInfo(runningBundle()).absolutePath();
    const QString mount = QDir(QDir::tempPath())
        .filePath(QString("WinnowUpdateMount-%1").arg(QCoreApplication::applicationPid()));
    QDir().mkpath(mount);

    if (!run("/usr/bin/hdiutil", {"attach", "-nobrowse", "-readonly", "-noautoopen",
                                  "-mountpoint", mount, installerPath}, 180000, error))
        return false;

    bool ok = false;
    do {
        const QStringList apps = QDir(mount).entryList({"*.app"}, QDir::Dirs);
        if (apps.isEmpty()) {
            error = "The update disk image does not contain Winnow.";
            break;
        }
        const QString app = QDir(mount).filePath(apps.first());

        if (!run("/usr/bin/codesign", {"--verify", "--deep", "--strict",
                                       QString("-R=") + kRequirement, app}, 300000, error)) {
            error = "The update is not signed by the Winnow developer, so it was not "
                    "installed.\n\n" + error;
            break;
        }

        const QString staged = QDir(parent).filePath(".WinnowUpdate.app");
        if (QFileInfo::exists(staged)) QDir(staged).removeRecursively();
        // ditto keeps the signature, extended attributes and symlinks intact
        if (!run("/usr/bin/ditto", {app, staged}, 600000, error)) {
            QDir(staged).removeRecursively();
            break;
        }
        stagedBundle = staged;
        newBundle = QDir(parent).filePath(apps.first());
        ok = true;
    } while (false);

    QString ignore;
    if (!run("/usr/bin/hdiutil", {"detach", mount}, 60000, ignore))
        run("/usr/bin/hdiutil", {"detach", "-force", mount}, 60000, ignore);
    QDir().rmdir(mount);
    return ok;
}

/*
    Args: $1 pid to wait for, $2 staged bundle, $3 new bundle path, $4 running (old)
    bundle, $5 DMG.  set_aside moves a bundle to the Trash (its new path in $aside);
    failing that (the Trash is privacy-protected on some systems) it returns non-zero
    and the bundle stays put.
*/
static const char *kFinisherScript = R"SH(#!/bin/sh
pid="$1"; staged="$2"; new="$3"; old="$4"; dmg="$5"
echo "=== $(date) install $new (replacing $old)"
while kill -0 "$pid" 2>/dev/null; do sleep 0.5; done

set_aside() {
    aside="$HOME/.Trash/$(basename "$1" .app) $(date +%Y-%m-%d_%H%M%S).app"
    mv "$1" "$aside"
}

fail() {
    echo "FAILED: $1"
    rm -rf "$staged"
    [ -d "$old" ] && open "$old"
    open "$dmg"
    exit 1
}

if [ "$new" = "$old" ]; then
    set_aside "$old" || fail "could not move $old aside"
    if ! mv "$staged" "$new"; then
        mv "$aside" "$old"
        fail "could not move the new version into place"
    fi
else
    if [ -e "$new" ]; then set_aside "$new" || fail "could not replace $new"; fi
    mv "$staged" "$new" || fail "could not move the new version into place"
    set_aside "$old" || echo "kept the previous version at $old"
fi

echo "installed; launching"
open "$new"
rm -f "$dmg"
exit 0
)SH";

bool launchFinisher(const QString &installerPath, QString &error)
{
    if (stagedBundle.isEmpty() || newBundle.isEmpty()) {
        error = "The update was not prepared.";
        return false;
    }
    const QString scriptPath = QDir(updateDir()).filePath("install.sh");
    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = "Could not write " + scriptPath;
        return false;
    }
    f.write(kFinisherScript);
    f.close();

    clearTestEnvironment();
    const QString log = QDir(updateDir()).filePath("install.log");
    const QString cmd = QString("/bin/sh \"$0\" \"$@\" >>\"%1\" 2>&1").arg(log);
    const QStringList args = {"-c", cmd, scriptPath,
                              QString::number(QCoreApplication::applicationPid()),
                              stagedBundle, newBundle, runningBundle(), installerPath};
    if (!QProcess::startDetached("/bin/sh", args)) {
        error = "Could not start the update installer.";
        return false;
    }
    return true;
}

#elif defined(Q_OS_WIN)

bool canInstallInPlace(QString *reason)
{
    // the Inno Setup uninstaller sits beside Winnow.exe in an installed copy
    if (QFileInfo::exists(QDir(QCoreApplication::applicationDirPath()).filePath("unins000.exe")))
        return true;
    if (reason) *reason = "This copy of Winnow was not installed by the Winnow installer.";
    return false;
}

bool prepare(const QString &, QString &)
{
    return true;
}

bool launchFinisher(const QString &installerPath, QString &error)
{
    clearTestEnvironment();

    /* ShellExecute rather than QProcess::startDetached (CreateProcess): the installer
       must elevate to write Program Files, and only ShellExecute raises the UAC prompt
       for an executable that asks for it. */
    const QString params = "/SILENT /SUPPRESSMSGBOXES /NORESTART /RELAUNCH=1 /WAITPID="
                         + QString::number(QCoreApplication::applicationPid());
    const QString file = QDir::toNativeSeparators(installerPath);
    const auto rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(file.utf16()),
                      reinterpret_cast<LPCWSTR>(params.utf16()), nullptr, SW_SHOWNORMAL));
    if (rc <= 32) {
        error = QString("Could not start the update installer (error %1).").arg(rc);
        return false;
    }
    return true;
}

#else

bool canInstallInPlace(QString *reason)
{
    if (reason) *reason = "Automatic install is not supported on this platform.";
    return false;
}
bool prepare(const QString &, QString &error) { error = "Not supported."; return false; }
bool launchFinisher(const QString &, QString &error) { error = "Not supported."; return false; }

#endif

} // namespace AppInstaller
