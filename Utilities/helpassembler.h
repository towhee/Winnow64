#ifndef HELPASSEMBLER_H
#define HELPASSEMBLER_H

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

/*
    HelpAssembler -- builds the general help document (Docs/winnowhelp.html) at open
    time, so every help page has exactly one copy of its text. Pure (files and strings,
    no widgets), so it is unit-tested by tst_helpdocs.

    The general page holds a table of contents whose links name other help pages by
    base name: <a href="#developbasichelp">Basic</a>. For each such link, in document
    order, whose page <docsDir>/<name>.html exists, the page's <body> is appended as a
    chapter headed by an "up to Contents" link that also carries <a name="<name>">.
    Any <style> blocks in a page's <head> are carried into the combined <head>. Links
    with no matching file (#overview, #contents) are ordinary in-page anchors and are
    left alone, so a page without such links passes through unchanged.

    Adding a help page = one TOC line in winnowhelp.html; tst_helpdocs fails until it
    is there (or the page is on its exclude list).
*/
namespace HelpAssembler {

// Base names of the pages a document links to as "#name", in order, without repeats.
inline QStringList linkedNames(const QString &html)
{
    // not a raw string: moc misparses R"(...)" holding quotes and loses the namespace
    static const QRegularExpression re("href\\s*=\\s*\"#([A-Za-z0-9_]+)\"");
    QStringList names;
    QSet<QString> seen;
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        if (!seen.contains(name)) {
            seen.insert(name);
            names.append(name);
        }
    }
    return names;
}

// The HTML between <body ...> and </body>; the whole text when there is no <body>.
inline QString bodyOf(const QString &html)
{
    static const QRegularExpression open("<body[^>]*>",
                                         QRegularExpression::CaseInsensitiveOption);
    const auto m = open.match(html);
    if (!m.hasMatch()) return html;
    const qsizetype start = m.capturedEnd();
    qsizetype end = html.indexOf("</body>", start, Qt::CaseInsensitive);
    if (end < 0) end = html.size();
    return html.mid(start, end - start);
}

// Every <style>...</style> block in a page, concatenated.
inline QString stylesOf(const QString &html)
{
    static const QRegularExpression re("<style[^>]*>.*?</style>",
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption);
    QString styles;
    auto it = re.globalMatch(html);
    while (it.hasNext()) styles += it.next().captured(0) + "\n";
    return styles;
}

inline QString assemble(const QString &tocHtml, const QString &docsDir)
{
    QString chapters;
    QString styles;
    for (const QString &name : linkedNames(tocHtml)) {
        QFile f(docsDir + "/" + name + ".html");
        if (!f.open(QIODevice::ReadOnly)) continue;     // an in-page anchor, not a page
        const QString page = QString::fromUtf8(f.readAll());
        styles += stylesOf(page);
        /* The name rides on the link: an empty <a name></a> between blocks (after a
           </ul>, say) is dropped by Qt's HTML importer, so the jump would go nowhere. */
        chapters += "\n<p class=\"toclink\"><a name=\"" + name + "\" href=\"#contents\">"
                    "&uarr; Contents</a></p>\n" + bodyOf(page);
    }
    if (chapters.isEmpty()) return tocHtml;

    QString html = tocHtml;
    if (!styles.isEmpty()) {
        const qsizetype head = html.indexOf("</head>", 0, Qt::CaseInsensitive);
        if (head >= 0) html.insert(head, styles);
        else html.prepend(styles);
    }
    const qsizetype body = html.lastIndexOf("</body>", -1, Qt::CaseInsensitive);
    if (body >= 0) html.insert(body, chapters);
    else html += chapters;
    return html;
}

} // namespace HelpAssembler

#endif // HELPASSEMBLER_H
