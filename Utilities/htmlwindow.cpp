#include "htmlwindow.h"
#include "Main/global.h"

HtmlWindow::HtmlWindow(const QString &title,
                       const QString &htmlPath,
                       const QSize &windowSize,
                       const QRect mwRect,
                       QWidget *parent) : QScrollArea(parent)
{
    // parented so MW (or owning dialog) closing destroys this window too;
    // WA_DeleteOnClose frees memory when the user closes it.
    //
    // Qt::Dialog (not Qt::Window) is deliberate: when opened from a dialog run
    // with exec() (e.g. IngestDlg), macOS runs a native NSApp modal session that
    // blocks every other window at the OS level — Qt's transient-parent exemption
    // does not apply to it. A Qt::Dialog top-level is backed by an NSPanel, whose
    // -worksWhenModal returns YES for a transient child of the modal window, so
    // this help window stays interactive (focus/scroll/close) alongside the
    // still-usable dialog. Qt::Window would create a plain NSWindow that the
    // modal session blocks until the dialog closes. Do not change back to
    // Qt::Window.
    setWindowFlag(Qt::Dialog);
    setAttribute(Qt::WA_DeleteOnClose);

/*
    This opens a modeless window containing the html document. The window will have
    a minimum size of size. The html and any other resources such as images need to
    be in Winnow resources. Example usage:

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
    text->setOpenExternalLinks(true);
    QFile f(htmlPath);
    f.open(QIODevice::ReadOnly);
    // shared stylesheet for all help pages; per-page <style> blocks override
    QFile cssFile(":/Docs/help.css");
    if (cssFile.open(QIODevice::ReadOnly)) {
        text->document()->setDefaultStyleSheet(cssFile.readAll());
        cssFile.close();
    }
    // base URL lets <img src="images/foo.png"> resolve relative to the html file
    text->document()->setBaseUrl(QUrl("qrc" + QFileInfo(htmlPath).path() + "/"));
    text->setHtml(f.readAll());
    f.close();
    text->setMinimumWidth(windowSize.width());
    text->setMinimumHeight(windowSize.height());
    text->setContentsMargins(9,9,9,9);
    text->setStyleSheet(G::css);
    setLayout(new QHBoxLayout);
    layout()->setContentsMargins(0,0,0,0);
    layout()->addWidget(text);

    // Enable Cmd+C (Mac) / Ctrl+C (Win) — right-click Copy works without this,
    // but the standard shortcut is otherwise swallowed by the main window.
    QAction *copyAction = new QAction(this);
    copyAction->setShortcut(QKeySequence::Copy);
    addAction(copyAction);
    connect(copyAction, &QAction::triggered, text, &QTextBrowser::copy);

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
    // Bring it in front of the owning dialog and give it focus; show() alone
    // leaves it stacked behind the active (modal) dialog.
    raise();
    activateWindow();
}

HtmlWindow::~HtmlWindow() = default;

bool HtmlWindow::event(QEvent *e)
{
    // Accept ShortcutOverride so Cmd+C / Ctrl+C is delivered as a key press here
    // instead of being routed to MW's global shortcuts.
    if (e->type() == QEvent::ShortcutOverride) {
        e->accept();
        return true;
    }
    return QScrollArea::event(e);
}
