#include "aboutdlg.h"
#include "ui_aboutdlg.h"
#include "Main/global.h"

AboutDlg::AboutDlg(QWidget *parent,
                   QString &version,
                   QString &qtVersion,
                   QString &compileDate) :
    QDialog(parent),
    ui(new Ui::About)
{
    ui->setupUi(this);
    ui->qtVersion->setText(qtVersion);
    ui->version->setText(version);
    ui->compileDate->setText(compileDate);
    QFont titleFont = ui->title->font();
    titleFont.setPixelSize(static_cast<int>(G::strFontSize.toInt() * G::ptToPx));
    ui->title->setFont(titleFont);
    adjustSize();
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

AboutDlg::~AboutDlg()
{
    delete ui;
}

void AboutDlg::on_okButton_clicked()
{
    accept();
}

void AboutDlg::on_email_linkActivated(const QString &link)
{
    QString preventcompilerWarning = link;
    QDesktopServices::openUrl(QUrl("mailto:winnowimageviewer@outlook.com"));
}

void AboutDlg::on_webSite_linkActivated(const QString &link)
{
    QString preventcompilerWarning = link;
    QDesktopServices::openUrl(QUrl("http://winnow.ca/"));
//    QDesktopServices::openUrl(QUrl("http://165.227.46.158/winnow/winnow.html?subject=Winnow&body=Hi Rory"));
}

void AboutDlg::on_license_linkActivated(const QString &link)
{
/*
    Shows THIRD_PARTY_NOTICES.md and the license texts it links to, all compiled in from
    licenses/licenses.qrc (:/licenses/...). Relative links in the notices ("LICENSE",
    "licenses/zlib.txt") open the text in place; web links go to the browser.
*/
    Q_UNUSED(link)
    auto read = [](const QString &path) {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    };
    const QString notices = read(":/licenses/THIRD_PARTY_NOTICES.md");

    QDialog dlg(this);
    dlg.setWindowTitle("Third-party licenses");
    dlg.resize(820, 700);
    auto *browser = new QTextBrowser(&dlg);
    browser->setOpenLinks(false);
    browser->setMarkdown(notices);
    auto *backBtn = new QPushButton("Back to notices", &dlg);
    backBtn->setEnabled(false);
    auto *closeBtn = new QPushButton("Close", &dlg);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(backBtn);
    buttons->addStretch();
    buttons->addWidget(closeBtn);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(browser);
    layout->addLayout(buttons);

    auto showNotices = [=] {
        browser->setMarkdown(notices);
        backBtn->setEnabled(false);
    };
    connect(backBtn, &QPushButton::clicked, &dlg, showNotices);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(browser, &QTextBrowser::anchorClicked, &dlg, [=](const QUrl &url) {
        if (!url.scheme().isEmpty()) {
            QDesktopServices::openUrl(url);
            return;
        }
        // "LICENSE" or "licenses/<file>"; the bare folder link returns to the notices
        const QString name = QFileInfo(url.path()).fileName();
        const QString text = read(":/licenses/" + name);
        if (name == "licenses" || text.isEmpty()) {
            showNotices();
            return;
        }
        if (name.endsWith(".md")) browser->setMarkdown(text);
        else browser->setPlainText(text);
        backBtn->setEnabled(true);
    });

    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg.winId(), G::backgroundColor);
    #endif
    dlg.exec();
}
