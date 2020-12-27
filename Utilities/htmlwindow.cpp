#include "htmlwindow.h"

HtmlWindow::HtmlWindow(const QString &title,
                       const QString &htmlPath,
                       const QSize &size, QObject *parent) : QScrollArea()
{
/*
    This opens a modeless window containing the html document.  The window will have a minimum
    size of size.  The html and any other resources such as images need to be in Winnow
    resources.  Example usage:

    HtmlWindow *w = new HtmlWindow("Winnow - Embel Container and Coordinate System",
                                   ":/Docs/embelcoordsystem.html",
                                   QSize(939,736));

    and the associated file embelcoordsystem.html:

    <!DOCTYPE html>
    <html>
    <body>
    <img src="qrc:/Docs/images/EmbelCoordSystem.png">
    </body>
    </html>

    Note the path syntax within the html to EmbelCoordSystem.png
*/
    setWindowTitle(title);
    QFile f(htmlPath);
    f.open(QIODevice::ReadOnly);
    text = new QTextBrowser;
    text->setMinimumWidth(size.width());
    text->setMinimumHeight(size.height());
    text->setContentsMargins(9,9,9,9);
    text->setHtml(f.readAll());
    text->setStyleSheet(G::css);
    setLayout(new QHBoxLayout);
    layout()->setContentsMargins(0,0,0,0);
    layout()->addWidget(text);
    show();
}

HtmlWindow::~HtmlWindow()
{
    delete text;
}
