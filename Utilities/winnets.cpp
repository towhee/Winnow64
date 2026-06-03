#include "Utilities/winnets.h"
#include "Main/global.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

void Winnets::sync(const QStringList &embellishDropletNames)
{
    if (G::isLogger) G::log("Winnets::sync");

    // Winnow app directory (where the master + staged Frameworks live)
    QString executableDirPath = qApp->applicationDirPath();
    /* Per-user Winnets folder. AppDataLocation is correct here because the
       running executable is Winnow itself; the droplets must NOT use it (they
       are renamed, so it would resolve to a per-droplet folder). */
    QString appDataWinnetsPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Winnets";
    QDir dir(appDataWinnetsPath);
    if (!dir.exists()) dir.mkpath(appDataWinnetsPath);
    QString ext = "";
    QString winnetPath = "";

    // Ensure the shared Qt runtime is present and matches the master's Qt
    {
    #ifdef Q_OS_WIN
        ext = ".exe";
        winnetPath = executableDirPath + "/Winnet.exe";
        // make sure Qt6Core.dll exists next to the droplets
        QString dllPath = appDataWinnetsPath + "/Qt6Core.dll";
        QString dllSourcePath = executableDirPath + "/Qt6Core.dll";
        QFileInfo srcDll(dllSourcePath);
        QFileInfo dstDll(dllPath);
        bool refreshDll = !dstDll.exists()
                          || srcDll.size() != dstDll.size()
                          || srcDll.lastModified() > dstDll.lastModified();
        if (refreshDll && srcDll.exists()) {
            QFile::remove(dllPath);
            QFile::copy(dllSourcePath, dllPath);
        }
    #endif
    #ifdef Q_OS_MAC
        ext = "";
        // The master is a bare executable staged into the app bundle by the
        // Winnet CMake target.
        winnetPath = executableDirPath + "/Winnets/Winnet";
        // make sure Frameworks folder exists
        dir.setPath(appDataWinnetsPath + "/Frameworks");
        if (!dir.exists())
            dir.mkdir(appDataWinnetsPath + "/Frameworks");
        /* Make sure QtCore.framework exists AND matches the version the master
           droplet was built against. The bundled framework is staged fresh at
           build time; if Qt was upgraded the copy in AppData must be refreshed
           or every droplet crashes in dyld before main() runs. */
        QString srcPath = executableDirPath + "/Winnets/Frameworks/QtCore.framework";
        QString dstPath = appDataWinnetsPath + "/Frameworks/QtCore.framework";
        QFileInfo srcCore(srcPath + "/Versions/A/QtCore");
        QFileInfo dstCore(dstPath + "/Versions/A/QtCore");
        bool refresh = !dstCore.exists()
                       || srcCore.size() != dstCore.size()
                       || srcCore.lastModified() > dstCore.lastModified();
        if (refresh && srcCore.exists()) {
            QProcess::execute("rm", {"-rf", dstPath});
            QProcess copy;
            copy.start("cp", {"-R", srcPath, dstPath});
            copy.waitForFinished();
        }
    #endif
    }

    // list of all existing droplet executables in the Winnets folder
    dir.setPath(appDataWinnetsPath);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    QStringList existingExecutables;
    const QStringList entries = dir.entryList();
    for (const QString &name : entries) {
        QFileInfo fi(dir.filePath(name));
        #ifdef Q_OS_WIN
        // On Windows, real executables end with .exe
        if (name.endsWith(".exe", Qt::CaseInsensitive))
            existingExecutables << name;
        #else
        // On macOS/Linux: check UNIX executable permission
        if (fi.isExecutable())
            existingExecutables << name;
        #endif
    }
    // strip ".exe" (no-op on mac where ext == "") to compare to droplet names
    existingExecutables.replaceInStrings(ext, "");

    /* Droplets to create/refresh: one per Embellish template, plus the fixed
       "FocusStack" droplet (a copy of the same master that sends srcProgram
       "FocusStack", handled by the FocusStack branch in MW::handleStartupArgs).
       Each droplet is a byte copy of the master, so when the master is rebuilt
       (e.g. after a Winnow/Qt upgrade) the existing droplets are stale copies of
       the old master and must be re-copied — otherwise they crash against the
       refreshed QtCore. */
    QStringList dropletList = embellishDropletNames;
    dropletList << "FocusStack";
    QFileInfo srcInfo(winnetPath);
    for (const QString &name : dropletList) {
        QString newWinnetPath = appDataWinnetsPath + "/" + name + ext;
        QFileInfo dstInfo(newWinnetPath);

        // (re)copy if missing or the master is newer than this droplet
        bool needCopy = !dstInfo.exists()
                        || srcInfo.lastModified() > dstInfo.lastModified();
        if (!needCopy) continue;

        // validate source
        if (!srcInfo.exists() || !srcInfo.isFile()) {
            qDebug() << "ERROR: Source Winnet missing:" << winnetPath;
            continue;
        }

        // destination folder exists
        QDir().mkpath(dstInfo.path());

        // remove old destination if needed
        if (dstInfo.exists()) {
            QFile::remove(newWinnetPath);
        }

        // copy
        bool ok = QFile::copy(winnetPath, newWinnetPath);
        if (!ok) {
            qWarning() << "Copy error: src=" << winnetPath
                       << "dst=" << newWinnetPath;
            continue;
        }

        // ensure the droplet is executable (QFile::copy can drop the +x bits)
        QFile::setPermissions(newWinnetPath,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
            QFileDevice::ReadGroup | QFileDevice::ExeGroup |
            QFileDevice::ReadOther | QFileDevice::ExeOther);
    }

    // remove orphaned Embellish* droplets (no longer in the template list)
    for (int i = existingExecutables.length() - 1; i >= 0; i--) {
        if (!embellishDropletNames.contains(existingExecutables.at(i))) {
            QString name = existingExecutables.at(i);
            // only remove Embellish* droplets (keep FocusStack and anything else)
            if (!name.startsWith("Embellish")) continue;
            QString fPath = appDataWinnetsPath + "/" + name + ext;
            QFile::remove(fPath);
        }
    }

    /* Remove obsolete *.app bundles. The current scheme uses bare executables
       only; any .app in the Winnets folder is a leftover from the old
       bundle-based winnets and is no longer used. */
    QDir appsDir(appDataWinnetsPath);
    appsDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    appsDir.setNameFilters(QStringList() << "*.app");
    const QStringList appBundles = appsDir.entryList();
    for (const QString &appName : appBundles) {
        QDir(appsDir.filePath(appName)).removeRecursively();
    }
}
