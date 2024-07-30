#include "htmlwindow.h"

HtmlWindow::HtmlWindow(const QString &title,
                       const QString &htmlPath,
                       const QSize &windowSize,
                       const QRect mwRect,
                       QObject */*parent*/) : QScrollArea()
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
    text = new QTextBrowser;
    QFile f(htmlPath);
    f.open(QIODevice::ReadOnly);
    text->setHtml(f.readAll());
    f.close();
    text->setMinimumWidth(windowSize.width());
    text->setMinimumHeight(windowSize.height());
    text->setContentsMargins(9,9,9,9);
    text->setStyleSheet(G::css);
    setLayout(new QHBoxLayout);
    layout()->setContentsMargins(0,0,0,0);
    layout()->addWidget(text);

    // Calculate the new position to center w over MW
    // if (mwRect.isEmpty()) mwRect = screenRect;
    int x = mwRect.x() + (mwRect.width() - windowSize.width()) / 2;
    int y = mwRect.y() + (mwRect.height() - windowSize.height()) / 2;

    // Get the available geometry of the screen where MW is located
    QRect screenRect = QGuiApplication::screenAt(mwRect.center())->availableGeometry();

    // Adjust if w extends outside the current monitor
    if (x + windowSize.width() > screenRect.right()) {
        x = screenRect.right() - windowSize.width();
    }
    if (x < screenRect.left()) {
        x = screenRect.left();
    }
    if (y + windowSize.height() > screenRect.bottom()) {
        y = screenRect.bottom() - windowSize.height();
    }
    if (y < screenRect.top()) {
        y = screenRect.top();
    }

    move(x, y);

    show();
}

HtmlWindow::~HtmlWindow()
{
    delete text;
}
