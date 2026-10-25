#ifndef APPUPDATE_H
#define APPUPDATE_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVersionNumber>

/*
    AppUpdate -- the pure half of "Check for updates" (no network, no widgets), so it is
    unit-tested by tst_appupdate.

    Releases are GitHub Releases on GITHUB_REPO.  The deploy scripts (menu G) tag each
    release "v<WINNOW_VERSION>" (e.g. "v3_beta", "v3.1") and attach one asset per
    platform: Winnow<ver>.dmg (macOS), WinnowSetup-<ver>.exe (Windows) and
    Winnow-<ver>-<arch>.AppImage (Linux, arch = x86_64 or aarch64).  The update
    check reads https://api.github.com/repos/<GITHUB_REPO>/releases/latest, which skips
    drafts and pre-releases -- so a release is offered to users the moment it is
    published as a full release, and never before.
*/
namespace AppUpdate {

inline const char *GITHUB_REPO = "towhee/Winnow64";

inline QString latestReleaseApiUrl()
{
    return QString("https://api.github.com/repos/%1/releases/latest").arg(GITHUB_REPO);
}

/* One release by tag -- unlike /releases/latest this also returns a pre-release, which
   is how a test build is checked (WINNOW_UPDATE_TAG, see MW::checkForUpdate) without
   offering it to anyone else.  Drafts are not reachable without authentication. */
inline QString releaseByTagApiUrl(const QString &tag)
{
    return QString("https://api.github.com/repos/%1/releases/tags/%2").arg(GITHUB_REPO, tag);
}

/*
    Version labels are WINNOW_VERSION_LABEL strings: a numeric part, optionally followed
    by a pre-release suffix ("3_beta", "3.1-rc2").  A leading "v" (the git tag) is
    ignored.  Ordering:
      - numeric parts first, trailing zeros insignificant (3 == 3.0 == 3.0.0)
      - equal numbers: a pre-release sorts BEFORE the release (3_beta < 3.0)
      - two pre-releases: the alphabetic stage (alpha < beta < rc), then its trailing
        number (beta < beta2 < beta10)
    Returns <0, 0 or >0 like strcmp.
*/
struct VersionLabel {
    QVersionNumber number;
    QString stage;          // lower-case alphabetic part of the suffix, "" = release
    int stageNumber = 0;
};

inline VersionLabel parseVersion(QString s)
{
    VersionLabel v;
    s = s.trimmed();
    if (s.startsWith('v') || s.startsWith('V')) s.remove(0, 1);

    int i = 0;
    while (i < s.size() && (s[i].isDigit() || s[i] == '.')) ++i;
    v.number = QVersionNumber::fromString(s.left(i)).normalized();

    QString suffix = s.mid(i);
    while (!suffix.isEmpty() && !suffix[0].isLetterOrNumber()) suffix.remove(0, 1);
    int j = 0;
    while (j < suffix.size() && !suffix[j].isDigit()) ++j;
    v.stage = suffix.left(j).toLower();
    while (v.stage.endsWith('_') || v.stage.endsWith('-') || v.stage.endsWith('.'))
        v.stage.chop(1);
    v.stageNumber = suffix.mid(j).toInt();
    if (v.stage.isEmpty() && j < suffix.size())
        v.stage = "pre";    // a bare numeric suffix ("3_2") still marks a pre-release
    return v;
}

inline int compareVersions(const QString &a, const QString &b)
{
    const VersionLabel va = parseVersion(a);
    const VersionLabel vb = parseVersion(b);
    if (int c = QVersionNumber::compare(va.number, vb.number)) return c;
    if (va.stage.isEmpty() != vb.stage.isEmpty()) return va.stage.isEmpty() ? 1 : -1;
    if (int c = va.stage.compare(vb.stage)) return c;
    return va.stageNumber - vb.stageNumber;
}

/* The asset this platform installs, chosen by file name.  macOS ships two DMGs per
   release: Winnow<ver>.dmg (Apple Silicon) and Winnow<ver>_Intel.dmg (x86_64, built by
   `WINNOW_ARCH=x86_64 deploy.command`).  Each build takes only its own architecture's,
   so an Intel Mac never installs the arm64 app (which cannot run there). */
inline bool isPlatformAsset(const QString &name, bool intelMac)
{
    if (!name.endsWith(".dmg", Qt::CaseInsensitive)) return false;
    return name.contains("_Intel", Qt::CaseInsensitive) == intelMac;
}

/* Linux ships one AppImage per architecture, Winnow-<ver>-x86_64.AppImage and
   Winnow-<ver>-aarch64.AppImage (packaging/linux/appimage.sh); each build takes only its
   own. */
inline bool isLinuxAsset(const QString &name, bool x86_64)
{
    if (!name.endsWith(".AppImage", Qt::CaseInsensitive)) return false;
    return name.endsWith(x86_64 ? "-x86_64.AppImage" : "-aarch64.AppImage",
                         Qt::CaseInsensitive);
}

inline bool isPlatformAsset(const QString &name)
{
#if defined(Q_OS_MAC) && defined(Q_PROCESSOR_X86_64)
    return isPlatformAsset(name, true);
#elif defined(Q_OS_MAC)
    return isPlatformAsset(name, false);
#elif defined(Q_OS_WIN)
    return name.startsWith("WinnowSetup", Qt::CaseInsensitive)
        && name.endsWith(".exe", Qt::CaseInsensitive);
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_64)
    return isLinuxAsset(name, true);
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM_64)
    return isLinuxAsset(name, false);
#else
    Q_UNUSED(name)
    return false;
#endif
}

struct Release {
    QString version;        // tag without the leading "v"
    QString notesUrl;       // the release's GitHub page (html_url)
    QString assetName;
    QString assetUrl;       // browser_download_url (redirects to GitHub's CDN)
    qint64 assetSize = 0;
    QByteArray sha256;      // lower-case hex from the asset's "digest", empty if absent
};

/*
    Parses a /releases/latest (or /releases/tags/<tag>) response.  A pre-release is
    rejected unless allowPrerelease (the WINNOW_UPDATE_TAG test path).  On failure returns false and sets *error to a
    user-facing reason.  A release without an asset for this platform (the other
    platform's machine has not run menu G yet) is NOT an error: assetUrl is left empty
    and the caller treats the release as not yet available here.
*/
inline bool parseLatestRelease(const QByteArray &json, Release &r, QString *error = nullptr,
                               bool allowPrerelease = false)
{
    auto fail = [error](const QString &msg) { if (error) *error = msg; return false; };

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail("The release information could not be read.");
    const QJsonObject obj = doc.object();
    if (obj.value("draft").toBool() || (obj.value("prerelease").toBool() && !allowPrerelease))
        return fail("The latest release is not a final release.");

    QString tag = obj.value("tag_name").toString().trimmed();
    if (tag.startsWith('v') || tag.startsWith('V')) tag.remove(0, 1);
    if (tag.isEmpty() || parseVersion(tag).number.isNull())
        return fail("The latest release has no version number.");

    r = Release();
    r.version = tag;
    r.notesUrl = obj.value("html_url").toString();

    const QJsonArray assets = obj.value("assets").toArray();
    for (const QJsonValue &av : assets) {
        const QJsonObject a = av.toObject();
        const QString name = a.value("name").toString();
        if (!isPlatformAsset(name)) continue;
        if (a.value("state").toString("uploaded") != "uploaded") continue;
        r.assetName = name;
        r.assetUrl = a.value("browser_download_url").toString();
        r.assetSize = a.value("size").toInteger();
        const QString digest = a.value("digest").toString();
        if (digest.startsWith("sha256:", Qt::CaseInsensitive))
            r.sha256 = digest.mid(7).toLower().toLatin1();
        break;
    }
    return true;
}

} // namespace AppUpdate

#endif // APPUPDATE_H
