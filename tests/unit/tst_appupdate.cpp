// Unit tests for Utilities/appupdate.h -- the GitHub release lookup behind
// "Check for updates": version-label ordering and parsing of /releases/latest.

#include <QtTest>
#include "Utilities/appupdate.h"

class TstAppUpdate : public QObject
{
    Q_OBJECT
private slots:
    void compare_data();
    void compare();
    void parsesRelease();
    void missingPlatformAssetIsNotAnError();
    void macAssetMatchesArchitecture();
    void prereleaseOnlyWhenAllowed();
    void rejectsBadReleases_data();
    void rejectsBadReleases();
};

void TstAppUpdate::compare_data()
{
    QTest::addColumn<QString>("a");
    QTest::addColumn<QString>("b");
    QTest::addColumn<int>("sign");

    QTest::newRow("equal")              << "3.0"     << "3.0"      << 0;
    QTest::newRow("trailing zeros")     << "3"       << "3.0.0"    << 0;
    QTest::newRow("tag prefix")         << "v3_beta" << "3_beta"   << 0;
    QTest::newRow("numeric not string") << "2.10"    << "2.9"      << 1;
    QTest::newRow("minor bump")         << "3.1"     << "3.0"      << 1;
    QTest::newRow("legacy 2.05 < 3")    << "2.05"    << "3_beta"   << -1;
    QTest::newRow("beta < release")     << "3_beta"  << "3.0"      << -1;
    QTest::newRow("beta < release 3")   << "3_beta"  << "3"        << -1;
    QTest::newRow("release > rc")       << "3.1"     << "3.1-rc2"  << 1;
    QTest::newRow("alpha < beta")       << "3_alpha" << "3_beta"   << -1;
    QTest::newRow("beta < rc")          << "3_beta"  << "3_rc1"    << -1;
    QTest::newRow("beta2 < beta10")     << "3_beta2" << "3_beta10" << -1;
    QTest::newRow("beta < beta2")       << "3_beta"  << "3_beta2"  << -1;
    QTest::newRow("case insensitive")   << "3_Beta"  << "3_beta"   << 0;
    QTest::newRow("next beta > release")<< "3.1_beta"<< "3.0"      << 1;
}

void TstAppUpdate::compare()
{
    QFETCH(QString, a);
    QFETCH(QString, b);
    QFETCH(int, sign);
    const int c = AppUpdate::compareVersions(a, b);
    QCOMPARE((c > 0) - (c < 0), sign);
    const int r = AppUpdate::compareVersions(b, a);
    QCOMPARE((r > 0) - (r < 0), -sign);
}

static QByteArray releaseJson(const QString &tag, bool prerelease = false)
{
    return QString(R"({
        "tag_name": "%1",
        "html_url": "https://github.com/towhee/Winnow64/releases/tag/%1",
        "draft": false,
        "prerelease": %2,
        "assets": [
          { "name": "Winnow3.1.dmg", "state": "uploaded", "size": 1234,
            "digest": "sha256:ABCDEF0123",
            "browser_download_url": "https://github.com/dl/Winnow3.1.dmg" },
          { "name": "WinnowSetup-3.1.exe", "state": "uploaded", "size": 5678,
            "digest": "sha256:fedcba9876",
            "browser_download_url": "https://github.com/dl/WinnowSetup-3.1.exe" }
        ]
    })").arg(tag, prerelease ? "true" : "false").toUtf8();
}

void TstAppUpdate::parsesRelease()
{
    AppUpdate::Release r;
    QString err;
    QVERIFY2(AppUpdate::parseLatestRelease(releaseJson("v3.1"), r, &err), qPrintable(err));
    QCOMPARE(r.version, QString("3.1"));
    QCOMPARE(r.notesUrl, QString("https://github.com/towhee/Winnow64/releases/tag/v3.1"));
#if defined(Q_OS_MAC)
    QCOMPARE(r.assetName, QString("Winnow3.1.dmg"));
    QCOMPARE(r.assetSize, qint64(1234));
    QCOMPARE(r.sha256, QByteArray("abcdef0123"));      // lower-cased for hex compare
#elif defined(Q_OS_WIN)
    QCOMPARE(r.assetName, QString("WinnowSetup-3.1.exe"));
    QCOMPARE(r.assetSize, qint64(5678));
    QCOMPARE(r.sha256, QByteArray("fedcba9876"));
#endif
}

void TstAppUpdate::missingPlatformAssetIsNotAnError()
{
    const QByteArray json = R"({ "tag_name": "v3.2", "draft": false, "prerelease": false,
                                 "assets": [ { "name": "notes.txt", "state": "uploaded",
                                 "browser_download_url": "x" } ] })";
    AppUpdate::Release r;
    QVERIFY(AppUpdate::parseLatestRelease(json, r));
    QCOMPARE(r.version, QString("3.2"));
    QVERIFY(r.assetUrl.isEmpty());
}

/* Apple Silicon and Intel DMGs share a release; each build must take only its own,
   whatever order GitHub lists them in. */
void TstAppUpdate::macAssetMatchesArchitecture()
{
    QVERIFY( AppUpdate::isPlatformAsset("Winnow3.1.dmg", false));
    QVERIFY(!AppUpdate::isPlatformAsset("Winnow3.1_Intel.dmg", false));
    QVERIFY( AppUpdate::isPlatformAsset("Winnow3.1_Intel.dmg", true));
    QVERIFY(!AppUpdate::isPlatformAsset("Winnow3.1.dmg", true));
    QVERIFY(!AppUpdate::isPlatformAsset("WinnowSetup-3.1.exe", true));

#if defined(Q_OS_MAC)
    const QByteArray json = R"({ "tag_name": "v3.1", "draft": false, "prerelease": false,
        "assets": [
          { "name": "Winnow3.1_Intel.dmg", "state": "uploaded", "browser_download_url": "i" },
          { "name": "Winnow3.1.dmg", "state": "uploaded", "browser_download_url": "a" } ] })";
    AppUpdate::Release r;
    QVERIFY(AppUpdate::parseLatestRelease(json, r));
#if defined(Q_PROCESSOR_X86_64)
    QCOMPARE(r.assetName, QString("Winnow3.1_Intel.dmg"));
#else
    QCOMPARE(r.assetName, QString("Winnow3.1.dmg"));
#endif
#endif
}

void TstAppUpdate::prereleaseOnlyWhenAllowed()
{
    AppUpdate::Release r;
    QVERIFY(!AppUpdate::parseLatestRelease(releaseJson("v3.1", true), r));
    QVERIFY(AppUpdate::parseLatestRelease(releaseJson("v3.1", true), r, nullptr, true));
    QCOMPARE(r.version, QString("3.1"));
}

void TstAppUpdate::rejectsBadReleases_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::newRow("not json")    << QByteArray("<html>rate limited</html>");
    QTest::newRow("no tag")      << QByteArray(R"({ "assets": [] })");
    QTest::newRow("non-numeric") << QByteArray(R"({ "tag_name": "latest" })");
    QTest::newRow("prerelease")  << releaseJson("v3.1", true);
}

void TstAppUpdate::rejectsBadReleases()
{
    QFETCH(QByteArray, json);
    AppUpdate::Release r;
    QString err;
    QVERIFY(!AppUpdate::parseLatestRelease(json, r, &err));
    QVERIFY(!err.isEmpty());
}

QTEST_GUILESS_MAIN(TstAppUpdate)
#include "tst_appupdate.moc"
