#include "reportdialog.h"
#include <QAction>
#include <QKeySequence>
#include <QEvent>
#include "Main/global.h"

ReportDialog::ReportDialog(QWidget *parent) : QDialog(parent)
{
    // Setup UI Layout manually (No .ui file needed)
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    textBrowser = new QTextBrowser(this);
    layout->addWidget(textBrowser);

    // Apply Winnow Styling
    setStyleSheet(G::css);
    textBrowser->setStyleSheet(G::css);
    textBrowser->setFontFamily("Monaco");
    textBrowser->setWordWrapMode(QTextOption::NoWrap);

    // FIX: Enable Cmd+C (Mac) / Ctrl+C (Win)
    QAction *copyAction = new QAction(this);
    copyAction->setShortcut(QKeySequence::Copy);
    this->addAction(copyAction);
    connect(copyAction, &QAction::triggered, textBrowser, &QTextBrowser::copy);

    // Handle Windows Title Bar
#ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
#endif

    setAttribute(Qt::WA_DeleteOnClose);
}

void ReportDialog::setReport(const QString &title, const QString &content)
{
    setWindowTitle(title);
    textBrowser->setText(content);
}

bool ReportDialog::event(QEvent *e)
{
    // When Qt asks if a shortcut should be overridden by standard key presses...
    if (e->type() == QEvent::ShortcutOverride) {
        e->accept(); // Accept it, meaning "treat this as a normal key press here"
        return true; // Stop it from propagating to the main window
    }

    // For all other events, do the normal QDialog behavior
    return QDialog::event(e);
}
