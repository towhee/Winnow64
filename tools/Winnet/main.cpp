/*
    Winnet — the Winnow Embellish/FocusStack droplet "master".

    A tiny Qt Core-only forwarder that acts like a Photoshop droplet. Winnow's
    EmbelProperties::syncWinnets() copies this executable once per Embellish
    template into <AppData>/Winnow/Winnets/, renaming each copy
    "Embellish<TemplateName>" (e.g. EmbellishZen2048). Dropping or exporting
    images onto one of those copies (e.g. a Lightroom export post-process action)
    runs it with the image paths as arguments. It then launches Winnow, telling
    it which template to use (derived from its own filename) and which files to
    embellish.

    Built as a CMake target inside the Winnow project (tools/Winnet) so it always
    links the SAME Qt as Winnow. The droplets load a shared QtCore from a sibling
    Frameworks/ via rpath; if that QtCore is older than the Qt the droplet was
    built against, it crashes in dyld before main() runs (a stale 6.4.0 QtCore
    did exactly that, producing no log output and a silent failure).

    Arguments forwarded to Winnow (the receiving side joins argv[1..] with '\n'
    and splits again — see Main/main.cpp and MW::handleStartupArgs):
        arg[0]  = this droplet's name   e.g. "EmbellishZen2048"  (srcProgram)
        arg[1+] = image path(s) to embellish

    The module ("Embellish") and template name ("Zen2048") are not sent — Winnow
    derives them from the droplet name (arg[0]) in MW::handleStartupArgs.
*/

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    /* Winnow's per-user data folder. We must NOT use AppDataLocation here:
       QStandardPaths derives that from the executable's own name, and this
       droplet is renamed per template (e.g. "EmbellishZen2048"), so it would
       resolve to ".../Application Support/EmbellishZen2048" instead of the
       shared ".../Application Support/Winnow". GenericDataLocation has no
       app-name suffix, so appending "Winnow" gives the same folder Winnow uses
       (mac: ~/Library/Application Support/Winnow; win: AppData/Roaming/Winnow). */
    QString appData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                      + "/Winnow";
    QString logPath = appData + "/Log/WinnowLog.txt";
    QDir().mkpath(QFileInfo(logPath).path());

    /* Append one line to Winnow's shared log in the same format as
       Utilities::log ("timestamp  function  msg"). We can't reuse Utilities::log
       itself: it lives in the Utilities class (which #includes <QtWidgets> and
       the rest of the app), so linking it would pull QtWidgets back into this
       Core-only droplet; and it writes to AppDataLocation, which resolves to the
       wrong per-droplet folder once this executable is renamed. */
    auto log = [&](const QString &function, const QString &msg) {
        QFile f(logPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QString t = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            f.write(QString("%1  %2  %3\n").arg(t, function, msg).toUtf8());
            f.close();
        }
    };

    log("Winnet::main", "started");

    // This droplet's own name carries the template: "Embellish<Template>".
    QString winnetName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    #ifdef Q_OS_WIN
    if (winnetName.endsWith(".exe", Qt::CaseInsensitive))
        winnetName.chop(4);
    #endif

    /* Arguments to forward to Winnow: just the droplet name + the image paths.
       The module/template are derived by Winnow from arg[0]. */
    QStringList winnowArgs;
    winnowArgs << winnetName;        // arg[0] srcProgram ("Embellish<Template>")
    for (int i = 1; i < argc; ++i)   // arg[1+] image path(s)
        winnowArgs << QString::fromLocal8Bit(argv[i]);

    /* Winnow executable path from the shared settings.ini "appPath", read from
       the Winnow data folder resolved above. Winnow writes appPath via
       qApp->applicationFilePath(), so it is the full path to the executable
       (incl. name and, on Windows, ".exe") — use it directly, do not append. */
    QSettings settings(appData + "/settings.ini", QSettings::IniFormat);
    QString winnowPath = settings.value("appPath").toString();

    // Debug log — inspected to confirm the droplet actually ran.
    log("Winnet::main", "name=" + winnetName
                        + " files=" + QString::number(argc - 1)
                        + " winnowPath=" + winnowPath);

    QProcess process;
    process.setProgram(winnowPath);
    process.setArguments(winnowArgs);
    process.startDetached();
    return 0;
}
