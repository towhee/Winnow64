/* Unit tests for Log/issue.cpp: the issue log's one-line layout, the key
   MW::prepareReport collapses repeats on, and the privacy pass over WinnowReport.txt
   (Issue::redactReport). See Documentation.md 22.7. */

#include <QtTest>
#include "Log/issue.h"

class TstIssueLog : public QObject
{
    Q_OBJECT
private slots:
    void longMessageKeepsColumns();
    void rowAndObjFollowMessage();
    void newlinesFolded();
    void collapseKeyIgnoresRowAndPath();
    void collapseKeyOldFormat();
    void homeBecomesTilde();
    void homeWholeFolderOnly();
    void homeWindowsAnyCaseAndSeparator();
    void deviceIdsHidden();
    void hideFoldersPaths();
    void hideFoldersTwoPathsOnALine();
    void hideFoldersLeavesUrls();
    void keepFoldersWhenNotHiding();
};

static QString line(Issue::Type type, const QString &msg, const QString &src,
                    int row = -1, const QString &path = QString())
{
    Issue i;
    i.type = type;
    i.msg = msg;
    i.src = src;
    i.sfRow = row;
    i.fPath = path;
    i.timeStamp = "2026-11-23 17:11:01 ";
    return i.toString();
}

static const int msgCol = Issue::TimeWidth + Issue::TypeWidth + Issue::SrcWidth;

void TstIssueLog::longMessageKeepsColumns()
{
    const QString shortMsg = line(Issue::Warning, "Box type free is unknown", "Heic::getHeifBox");
    const QString longMsg = line(Issue::Warning,
        "Catalog write failed: NOT NULL constraint failed: image.focusx Unable to fetch row",
        "Catalog::commit", -1, "/x/a.ARW");
    QCOMPARE(shortMsg.mid(msgCol, 3), QString("Box"));
    QCOMPARE(longMsg.mid(msgCol, 7), QString("Catalog"));
    QCOMPARE(longMsg.mid(Issue::TimeWidth + Issue::TypeWidth, 15), QString("Catalog::commit"));
}

void TstIssueLog::rowAndObjFollowMessage()
{
    const QString l = line(Issue::Error, "JPG does not start with 0xFFD8.", "Jpeg::parse",
                           7, "/x/a.jpg");
    QVERIFY(l.endsWith("JPG does not start with 0xFFD8.  Row: 7  Obj: /x/a.jpg"));
    const QString noRow = line(Issue::Warning, "Invalid index.", "Selection::setCurrentIndex");
    QVERIFY(noRow.endsWith("Invalid index."));
}

void TstIssueLog::newlinesFolded()
{
    const QString l = line(Issue::Warning, "first\nsecond\r\nthird", "X::y");
    QVERIFY(!l.contains('\n'));
    QVERIFY(l.endsWith("first | second | third"));
}

void TstIssueLog::collapseKeyIgnoresRowAndPath()
{
    const QString a = line(Issue::Warning, "Box type free is unknown", "Heic::getHeifBox", 3, "/x/a.HEIC");
    const QString b = line(Issue::Warning, "Box type free is unknown", "Heic::getHeifBox", 9, "/y/b.HEIC");
    const QString c = line(Issue::Warning, "Box type auxC is unknown", "Heic::getHeifBox", 3, "/x/a.HEIC");
    QCOMPARE(Issue::collapseKey(a), Issue::collapseKey(b));
    QVERIFY(Issue::collapseKey(a) != Issue::collapseKey(c));
    QVERIFY(Issue::collapseKey(a).endsWith("Box type free is unknown"));
}

void TstIssueLog::collapseKeyOldFormat()
{
    // Written before 3_beta7: message at col 29 padded to 60, then "Src: "
    const QString old =
        "2026-11-23 09:47:21 Warning: Box type auxC is unknown                                    "
        "Src: Heic::getHeifBox                   Row:   n/a   Obj: /x/a.HEIC";
    QCOMPARE(Issue::collapseKey(old), old.mid(20, 109));
}

void TstIssueLog::homeBecomesTilde()
{
    QCOMPARE(Issue::redactReport("Obj: /Users/al/Photos/a.jpg", "/Users/al", false),
             QString("Obj: ~/Photos/a.jpg"));
}

void TstIssueLog::homeWholeFolderOnly()
{
    QCOMPARE(Issue::redactReport("/Users/alice/a.jpg", "/Users/al", false),
             QString("/Users/alice/a.jpg"));
}

void TstIssueLog::homeWindowsAnyCaseAndSeparator()
{
    const QString home = "C:/Users/Al";
    QCOMPARE(Issue::redactReport(R"(c:\users\al\Pictures\a.jpg)", home, false),
             QString(R"(~\Pictures\a.jpg)"));
    QCOMPARE(Issue::redactReport("C:/Users/Al/Pictures/a.jpg", home, false),
             QString("~/Pictures/a.jpg"));
}

void TstIssueLog::deviceIdsHidden()
{
    const QString ips =
        R"({"crashReporterKey" : "870841C9-0A4F-F9F5-A80F-CD30AA6675B8",)"
        R"("deviceIdentifierForVendor":"D1F870D3-49A6-5638-A6A5-1EA08FADF3DF","userID" : 501})";
    const QString r = Issue::redactReport(ips, "/Users/al", false);
    QVERIFY(!r.contains("870841C9"));
    QVERIFY(!r.contains("D1F870D3"));
    QVERIFY(r.contains(R"("crashReporterKey" : "hidden")"));
    QVERIFY(r.contains(R"("userID" : 501)"));
    QCOMPARE(Issue::redactReport("MachineID=ABC123\nAppName=Winnow", "", false),
             QString("MachineID=hidden\nAppName=Winnow"));
}

void TstIssueLog::hideFoldersPaths()
{
    const QString e = QString(QChar(0x2026));
    auto hide = [](const QString &s) { return Issue::redactReport(s, "/Users/al", true); };
    QCOMPARE(hide("Obj: /Users/al/Photos/2021 √/Glen Oaks/0866.HEIC"),
             "Obj: ~/" + e + "/0866.HEIC");
    QCOMPARE(hide("Obj: /Volumes/Card/DCIM/100/A.ARW"), "Obj: /Volumes/" + e + "/A.ARW");
    QCOMPARE(hide(R"(Obj: C:\Users\bo\Pictures\A.JPG)"), R"(Obj: C:\)" + e + R"(\A.JPG)");
    QCOMPARE(hide(R"(Obj: \\nas\photos\2021\A.JPG)"), R"(Obj: \\)" + e + R"(\A.JPG)");
    QCOMPARE(hide("Obj: //nas/photos/A.JPG"), "Obj: //" + e + "/A.JPG");
    QCOMPARE(hide("Obj: /run/media/bo/CARD/A.JPG"), "Obj: /run/" + e + "/A.JPG");
    // A file directly in home has no folder to hide
    QCOMPARE(hide("Obj: /Users/al/a.jpg"), QString("Obj: ~/a.jpg"));
}

void TstIssueLog::hideFoldersTwoPathsOnALine()
{
    const QString e = QString(QChar(0x2026));
    QCOMPARE(Issue::redactReport("Could not move ~/a/b.jpg to ~/c/d.jpg", "", true),
             "Could not move ~/" + e + "/b.jpg to ~/" + e + "/d.jpg");
    QCOMPARE(Issue::redactReport(R"(copy "~/a b/c.jpg" failed  Obj: ~/x/y.jpg)", "", true),
             R"(copy "~/)" + e + R"(/c.jpg" failed  Obj: ~/)" + e + "/y.jpg");
}

void TstIssueLog::hideFoldersLeavesUrls()
{
    const QString url = "Update check: https://api.github.com/repos/towhee/Winnow64/releases";
    QCOMPARE(Issue::redactReport(url, "/Users/al", true), url);
}

void TstIssueLog::keepFoldersWhenNotHiding()
{
    QCOMPARE(Issue::redactReport("Obj: /Volumes/Card/DCIM/A.ARW", "/Users/al", false),
             QString("Obj: /Volumes/Card/DCIM/A.ARW"));
}

QTEST_MAIN(TstIssueLog)
#include "tst_issuelog.moc"
