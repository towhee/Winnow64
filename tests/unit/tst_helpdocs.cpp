// Unit tests for Utilities/helpassembler.h and the general help document
// (Docs/winnowhelp.html): every help page is in its table of contents, and the
// assembled document carries each one as a chapter.

#include <QtTest>
#include "Utilities/helpassembler.h"

class TstHelpDocs : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void everyPageIsInContents();
    void contentsTargetsExistAndAreInQrc();
    void assemblesChaptersInOrder();
    void pageWithoutLinksIsUnchanged();
private:
    QString docs;
    QString toc;
    QStringList pages;      // TOC targets that are files
};

/* Docs/ files that are deliberately NOT in the general help: the page template, the
   general help itself, and the orphans listed in Documentation.md 20.6. */
static const QStringList excluded = {"_template", "winnowhelp", "versions"};

void TstHelpDocs::initTestCase()
{
    docs = QString(WINNOW_SOURCE_DIR) + "/Docs";
    QFile f(docs + "/winnowhelp.html");
    QVERIFY(f.open(QIODevice::ReadOnly));
    toc = QString::fromUtf8(f.readAll());
    for (const QString &name : HelpAssembler::linkedNames(toc))
        if (QFileInfo::exists(docs + "/" + name + ".html")) pages << name;
    QVERIFY(pages.size() > 20);
}

void TstHelpDocs::everyPageIsInContents()
{
    const QStringList files = QDir(docs).entryList({"*.html"}, QDir::Files);
    for (const QString &file : files) {
        const QString name = QFileInfo(file).completeBaseName();
        if (excluded.contains(name)) continue;
        QVERIFY2(pages.contains(name),
                 qPrintable(file + " is not in winnowhelp.html's contents: add "
                            "<li><a href=\"#" + name + "\">...</a></li>"));
    }
}

void TstHelpDocs::contentsTargetsExistAndAreInQrc()
{
    QFile q(QString(WINNOW_SOURCE_DIR) + "/winnow.qrc");
    QVERIFY(q.open(QIODevice::ReadOnly));
    const QString qrc = QString::fromUtf8(q.readAll());
    QVERIFY(qrc.contains("<file>Docs/winnowhelp.html</file>"));
    for (const QString &name : pages)
        QVERIFY2(qrc.contains("<file>Docs/" + name + ".html</file>"),
                 qPrintable(name + ".html is not listed in winnow.qrc"));

    // every in-page anchor the contents names is defined
    for (const QString &name : HelpAssembler::linkedNames(toc))
        if (!pages.contains(name))
            QVERIFY2(toc.contains("<a name=\"" + name + "\""),
                     qPrintable("#" + name + " is neither a page nor an anchor"));
}

void TstHelpDocs::assemblesChaptersInOrder()
{
    const QString html = HelpAssembler::assemble(toc, docs);
    qsizetype last = html.indexOf("<a name=\"overview\"");
    QVERIFY(last >= 0);
    for (const QString &name : pages) {
        const QString anchor = "<a name=\"" + name + "\"";
        QCOMPARE(html.count(anchor), 1);
        const qsizetype at = html.indexOf(anchor);
        QVERIFY2(at > last, qPrintable(name + " is out of contents order"));
        last = at;
    }
    // ingestautopath's per-page <style> is carried into the combined <head>
    const qsizetype head = html.indexOf("</head>");
    QVERIFY(html.indexOf(".tmpl") >= 0 && html.indexOf(".tmpl") < head);
    // exactly one document
    QCOMPARE(html.count("<body"), 1);
    QCOMPARE(html.count("</body>"), 1);
}

void TstHelpDocs::pageWithoutLinksIsUnchanged()
{
    QFile f(docs + "/pictureshelp.html");
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString page = QString::fromUtf8(f.readAll());
    QCOMPARE(HelpAssembler::assemble(page, docs), page);
}

QTEST_GUILESS_MAIN(TstHelpDocs)
#include "tst_helpdocs.moc"
