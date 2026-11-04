#include "Dialogs/catalogrootsdlg.h"
#include "Main/catalogenumerate.h"
#include "Main/catalogscanner.h"
#include "Main/global.h"
#include "Utilities/collapsiblesections.h"
#include "Utilities/htmlwindow.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLocale>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

namespace {

/* Column order is the order the rule reads in: what this row does, to which folder, how
   far down. */
enum Column { ColMode = 0, ColPath = 1, ColRecurse = 2, ColImages = 3, ColCount = 4 };

}  // namespace

CatalogRootsDlg::CatalogRootsDlg(QWidget *parent)
    : QDialog(parent)
{
    if (G::isLogger) G::log("CatalogRootsDlg::CatalogRootsDlg");

    setWindowTitle("Manage Catalog");
    /* A tool window, not an application-modal dialog: a scan runs for minutes and this is
       where its state is shown. See the header. */
    setWindowFlag(Qt::Tool);
    /* 850 so the Folder path column shows a real library path, and the info table
       below keeps each value on one line. */
    setMinimumWidth(850);
    resize(850, height());

    table = new QTableWidget(0, ColCount);
    table->setHorizontalHeaderLabels(
        QStringList() << "Action" << "Folder path" << "Include Subfolders"
                      << "Images");
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(ColMode, QHeaderView::ResizeToContents);
    /* The path is the column that varies in length and the one worth reading, so it takes
       whatever the other two do not. */
    table->horizontalHeader()->setSectionResizeMode(ColPath, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(ColRecurse, QHeaderView::ResizeToContents);
    /* Sized once for the widest number it will ever hold rather than to its contents:
       ResizeToContents makes the column jump every time a count lands, which drags the
       path column with it and makes a settled table look like it is still working. */
    {
        QFontMetrics fm(table->font());
        const int wide = fm.horizontalAdvance(QLocale().toString(9999999)) + 28;
        const int header = fm.horizontalAdvance("Images") + 28;
        table->horizontalHeader()->setSectionResizeMode(ColImages, QHeaderView::Fixed);
        table->setColumnWidth(ColImages, qMax(wide, header));
    }
    /*  THE TABLE IS AS TALL AS ITS ROWS, and the window follows it (fitTableHeight,
        fitToContents). A fixed minimum left a band of empty table under two rows that
        pushed the info section out of the window. */
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    noteLabel = new QLabel;
    noteLabel->setWordWrap(true);
    statusLabel = new QLabel;
    /*  NOT word-wrapped. It holds an HTML table whose lines are laid out already, and a
        word-wrapped rich-text QLabel under-reports its height -- the last line (the
        disk-space folder) was clipped. Unwrapped, its size hint is exact. */
    statusLabel->setWordWrap(false);
    /* MW composes this one, and colours the clause that says a scan is owed. */
    statusLabel->setTextFormat(Qt::RichText);
    /* Its one link, "See which", opens the Catalog Diagnostics report, not a URL. */
    statusLabel->setOpenExternalLinks(false);
    statusLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(statusLabel, &QLabel::linkActivated, this,
            [this]{ emit diagnosticsRequested(); });

    /*  THE SCAN'S PROGRESS LIVES HERE AS WELL AS ON THE STATUS BAR. A library scan is
        hours, and this is the window someone opens to ask "how long". The line also says
        the window can be closed -- the scan is a background job and does not need it. */
    scanBar = new QProgressBar;
    scanBar->setTextVisible(false);
    scanBar->setMaximumHeight(8);
    scanLabel = new QLabel;
    scanLabel->setWordWrap(true);
    scanLabel->setTextFormat(Qt::RichText);

    helpBtn = new QPushButton("Help");
    helpBtn->setStyleSheet("background-color: " + G::helpColor.name() + ";");
    addBtn = new QPushButton("Append");
    removeBtn = new QPushButton("Remove");
    scanBtn = new QPushButton("Scan");
    closeBtn = new QPushButton("Close");
    removeBtn->setEnabled(false);

    /* Help on the left, set apart from Append and Remove, which edit the table. */
    QHBoxLayout *btns = new QHBoxLayout;
    btns->setContentsMargins(0, 0, 0, 0);
    btns->addWidget(helpBtn);
    btns->addSpacing(24);
    btns->addWidget(addBtn);
    btns->addWidget(removeBtn);
    btns->addStretch(1);
    btns->addWidget(scanBtn);
    btns->addWidget(closeBtn);

    /*  TWO COLLAPSIBLE SECTIONS -- the scope (editing) and the information (reporting) --
        then the scan progress and the buttons, which belong to neither and stay visible
        whichever is closed. */
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    /* The sections in a layout of their own at the default spacing, as ExportDlg's are,
       so the gap between the boxes matches that dialog. */
    QVBoxLayout *sectionsLay = new QVBoxLayout;
    sectionsLay->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(sectionsLay);

    QVBoxLayout *scopeLay = nullptr;
    QGroupBox *scopeBox = makeSection("Catalog Scope", scopeLay);
    scopeLay->addWidget(table);
    scopeLay->addWidget(noteLabel);
    sectionsLay->addWidget(scopeBox);

    QVBoxLayout *infoLay = nullptr;
    QGroupBox *infoBox = makeSection("Catalog Information", infoLay);
    infoLay->addWidget(statusLabel);
    sectionsLay->addWidget(infoBox);

    /*  The scan's bar and line are ONE block, shown and hidden together, with their gap
        from the section above inside it -- so an idle window has no stray spacing above
        the buttons. */
    scanBlock = new QWidget;
    QVBoxLayout *scanLayout = new QVBoxLayout(scanBlock);
    scanLayout->setContentsMargins(0, 4, 0, 0);
    scanLayout->setSpacing(8);
    scanLayout->addWidget(scanBar);
    scanLayout->addWidget(scanLabel);
    scanBlock->hide();
    layout->addWidget(scanBlock);
    layout->addSpacing(14);
    layout->addLayout(btns);

    /*  OPEN BY DEFAULT, unlike ExportDlg and IngestDlg. CollapsibleSections reads a key
        with nothing saved as collapsed, which suits a form too tall for a small screen;
        this window is short, and the scope table is the reason to open it. So a first
        open seeds both keys as open, and after that each opens as the user left it. */
    const QString group = "CatalogRootsDlg/collapsed";
    if (G::settings)
        for (const QString &key : {QString("Scope"), QString("Information")})
            if (!G::settings->contains(group + "/" + key))
                G::settings->setValue(group + "/" + key, false);
    sections = new CollapsibleSections(this, group);
    sections->add(scopeBox, sectionsLay, "Scope", [this]{ return scopeSummary(); });
    sections->add(infoBox, sectionsLay, "Information", [this]{ return infoSummary(); });
    sections->onChanged = [this]{ fitToContents(); };
    sections->restore();

    /* The only editable item state is the Include Subfolders check, so every itemChanged
       is a user toggling one -- the guard is populating, not the column. */
    connect(table, &QTableWidget::itemChanged, this, [this]{ noteEdit(); });

    connect(table, &QTableWidget::itemSelectionChanged, this, [this]{
        removeBtn->setEnabled(!scanning && !table->selectedItems().isEmpty());
    });

    connect(addBtn, &QPushButton::clicked, this, [this]{
        /* Start the browser at the first included folder: a new row is nearly always
           either a sibling of it or a branch inside it. */
        QString start;
        for (int r = 0; r < table->rowCount() && start.isEmpty(); ++r) {
            auto *mode = qobject_cast<QComboBox*>(table->cellWidget(r, ColMode));
            if (mode && mode->currentIndex() == 0)
                start = table->item(r, ColPath)->text();
        }
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Choose a folder to include in or exclude from the catalog", start);
        if (dir.isEmpty()) return;
        /* The same folder twice would be two rules about one folder, which is either a
           duplicate or a contradiction; neither is worth a table row. */
        for (int r = 0; r < table->rowCount(); ++r)
            if (table->item(r, ColPath)->text() == catalogScopeNormalize(dir)) {
                table->selectRow(r);
                noteLabel->setText("\"" + QDir(dir).dirName() + "\" is already in the "
                                   "table -- change that row instead.");
                return;
            }
        CatalogScopeEntry e;
        e.path = catalogScopeNormalize(dir);
        addRow(e);
        table->selectRow(table->rowCount() - 1);
        noteEdit();
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]{
        /* Rows removed from the bottom up, so each index is still valid when it is
           reached. */
        QList<int> rows;
        const auto ranges = table->selectedRanges();
        for (const QTableWidgetSelectionRange &range : ranges)
            for (int r = range.topRow(); r <= range.bottomRow(); ++r) rows << r;
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int r : rows) table->removeRow(r);
        fitTableHeight();
        /*  REMOVING A ROW UN-CATALOGUES WHAT IT CONTRIBUTED, and this is the ordinary
            scopeChanged that says so: the table states what the catalog HOLDS, not
            merely what a scan visits, so no separate signal or confirmation belongs
            here. MW counts what the removal disowns, asks, and puts the rows back if the
            user declines -- which is why this does not need to know whether the deletion
            was accepted. Removing an exclusion still indexes nothing by itself: the next
            scan picks that folder up like any other. */
        noteEdit();
    });

    connect(scanBtn, &QPushButton::clicked, this, [this]{
        if (scanning) emit stopScanRequested();
        else emit scanRequested();
    });

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(helpBtn, &QPushButton::clicked, this, [this]{
        QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
        new HtmlWindow("Winnow - Manage Catalog", ":/Docs/managecataloghelp.html",
                       QSize(700, 600), r, this);
    });

    updateNote();
}

QGroupBox *CatalogRootsDlg::makeSection(const QString &title, QVBoxLayout *&inner)
{
/*
    The same box ExportDlg::addSection builds: the title two points up on the body text,
    an inner body widget that resets the font so the controls do not inherit the title
    size, and clearance under the title scaled with its font.
*/
    QGroupBox *gb = new QGroupBox(title, this);
    QFont tf = gb->font();
    tf.setPointSize(font().pointSize() + 2);
    gb->setFont(tf);
    const int topGap = tf.pointSize() + 6;

    QVBoxLayout *outer = new QVBoxLayout(gb);
    outer->setContentsMargins(10, topGap, 10, 10);
    QWidget *body = new QWidget(gb);
    body->setFont(font());
    inner = new QVBoxLayout(body);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(8);
    outer->addWidget(body);
    return gb;
}

QString CatalogRootsDlg::scopeSummary() const
{
    const CatalogScope s = scope();
    if (s.isEmpty()) return "No folders";
    int inc = 0;
    for (const CatalogScopeEntry &e : s) if (e.include) ++inc;
    const int exc = int(s.size()) - inc;
    QString text = QString("%1 folder%2 included").arg(inc).arg(inc == 1 ? "" : "s");
    if (exc > 0) text += QString(", %1 excluded").arg(exc);
    return text;
}

QString CatalogRootsDlg::infoSummary() const
{
/*
    The Catalogued and Status values from MW's table, as plain text: enough to say
    whether a scan is owed without opening the section. A status that is not a table
    (the catalog is unavailable) is shown whole.
*/
    const QString html = statusLabel->text();
    auto plain = [](QString t) {
        t.remove(QRegularExpression("<a [^>]*>.*?</a>"));
        t.remove(QRegularExpression("<[^>]*>"));
        t.replace("&mdash;", QString(QChar(0x2014)));
        t.replace("&hellip;", QString(QChar(0x2026)));
        t.replace("&nbsp;", " ");
        return t.simplified();
    };
    static const QRegularExpression rowRe(
        "<td[^>]*>([^<]*)</td>\\s*<td[^>]*>(.*?)</td>");
    QStringList parts;
    auto it = rowRe.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString label = m.captured(1).trimmed();
        if (label == "Catalogued" || label == "Status") parts << plain(m.captured(2));
    }
    if (parts.isEmpty()) return plain(html);
    return parts.join(QString("  %1  ").arg(QChar(0x00B7)));
}

void CatalogRootsDlg::addRow(const CatalogScopeEntry &e)
{
    const int r = table->rowCount();
    table->insertRow(r);

    QComboBox *mode = new QComboBox;
    mode->addItem("Include");
    mode->addItem("Exclude");
    mode->setCurrentIndex(e.include ? 0 : 1);
    table->setCellWidget(r, ColMode, mode);
    connect(mode, &QComboBox::currentIndexChanged, this, [this]{ noteEdit(); });

    /* Normalised on the way in, not on the way out: every rule here is a prefix test,
       and one trailing slash makes them all quietly false (see catalogScopeNormalize). */
    const QString folder = catalogScopeNormalize(e.path);
    QTableWidgetItem *path = new QTableWidgetItem(folder);
    path->setToolTip(folder);
    table->setItem(r, ColPath, path);

    /* A checkable item rather than a QCheckBox widget: the row must stay selectable by
       clicking anywhere across it, and a widget in the cell swallows that click. */
    QTableWidgetItem *rec = new QTableWidgetItem;
    rec->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    rec->setCheckState(e.recurse ? Qt::Checked : Qt::Unchecked);
    rec->setTextAlignment(Qt::AlignCenter);
    rec->setToolTip("On: this row applies to the folder and everything under it.\n"
                    "Off: it applies to this folder alone.");
    table->setItem(r, ColRecurse, rec);

    QTableWidgetItem *images = new QTableWidgetItem(QString(QChar(0x2014)));
    images->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    images->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    images->setToolTip("Images in this folder on disk now. The Library matches it after "
                       "a scan.");
    table->setItem(r, ColImages, images);
    fitTableHeight();
}

void CatalogRootsDlg::fitTableHeight()
{
/*
    Header plus every row, up to kMaxVisibleRows; beyond that the table scrolls rather
    than pushing the buttons off a laptop screen. An empty table keeps one row's height
    so it still reads as a table.
*/
    constexpr int kMaxVisibleRows = 12;
    int rowsH = 0;
    const int n = table->rowCount();
    for (int r = 0; r < qMin(n, kMaxVisibleRows); ++r) rowsH += table->rowHeight(r);
    if (n == 0) rowsH = table->verticalHeader()->defaultSectionSize();
    /* sizeHint, not height(): before the window is first shown the header has no
       geometry yet and reports 0. */
    table->setFixedHeight(table->horizontalHeader()->sizeHint().height() + rowsH
                          + 2 * table->frameWidth());
    fitToContents();
}

void CatalogRootsDlg::fitToContents()
{
/*
    Take the height the contents need, no more and no less, keeping the user's width.
    Deferred a tick so every change made in one pass (rows, counts, status, progress)
    is laid out once, and so a label's new text has been measured.
*/
    if (fitPending) return;
    fitPending = true;
    QTimer::singleShot(0, this, [this]{
        fitPending = false;
        layout()->activate();
        const int h = layout()->hasHeightForWidth()
                          ? layout()->totalHeightForWidth(width())
                          : layout()->totalSizeHint().height();
        if (h > 0 && h != height()) resize(width(), h);
    });
}

void CatalogRootsDlg::setRowCounts(const QVector<int> &counts)
{
/*
    Written with populating held, because these are items in the same table whose
    itemChanged is how a user edit is noticed -- without it, showing a count would look
    like an edit, which would ask MW for the counts again, forever.
*/
    const bool wasPopulating = populating;
    populating = true;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem *item = table->item(r, ColImages);
        if (!item) continue;
        const int n = r < counts.size() ? counts.at(r) : -1;
        /* -2 counting, -1 unknown (the folder is not there, or unmounted), 0 or more the
           answer. A stale number left in place would be read as a fresh one. */
        if (n == -2) item->setText("...");
        else if (n < 0) item->setText(QString(QChar(0x2014)));
        else item->setText(QLocale().toString(n));
    }
    populating = wasPopulating;
}

void CatalogRootsDlg::setScope(const CatalogScope &scope)
{
    populating = true;                  // a restore is not a user edit
    table->setRowCount(0);
    for (const CatalogScopeEntry &e : scope) addRow(e);
    populating = false;
    fitTableHeight();
    removeBtn->setEnabled(false);
    updateNote();
    sections->updateSummaries();
}

CatalogScope CatalogRootsDlg::scope() const
{
    CatalogScope out;
    for (int r = 0; r < table->rowCount(); ++r) {
        const QTableWidgetItem *path = table->item(r, ColPath);
        const QTableWidgetItem *rec = table->item(r, ColRecurse);
        auto *mode = qobject_cast<QComboBox*>(table->cellWidget(r, ColMode));
        if (!path || !rec || !mode) continue;
        CatalogScopeEntry e;
        e.path = path->text();
        e.include = mode->currentIndex() == 0;
        e.recurse = rec->checkState() == Qt::Checked;
        out << e;
    }
    return out;
}

void CatalogRootsDlg::noteEdit()
{
    if (populating) return;
    updateNote();
    sections->updateSummaries();
    emit scopeChanged(scope());
}

void CatalogRootsDlg::updateNote()
{
/*
    The note carries whatever the table cannot say for itself, and it says it HERE rather
    than in a popup after the fact: a rule that quietly achieves nothing is exactly the
    kind of thing a user only discovers hours later, when a search comes back short.
*/
    fitToContents();        // the note's length, and so its wrapped height, changes
    if (scanning) {
        noteLabel->setText("The table cannot be changed while a scan is running.");
        return;
    }

    const CatalogScope s = scope();
    bool anyInclude = false;
    for (const CatalogScopeEntry &e : s) if (e.include) anyInclude = true;
    if (!anyInclude) {
        noteLabel->setText(s.isEmpty()
            ? "Add a folder to have it indexed in the background."
            : "Nothing is included, so a scan would index nothing.");
        return;
    }

    /* An exclude only means something inside an included tree. */
    for (const CatalogScopeEntry &e : s) {
        if (e.include || e.path.isEmpty()) continue;
        if (!catalogScopeIncludes(s, e.path)) {
            noteLabel->setText("\"" + QDir(e.path).dirName() + "\" is not inside any "
                               "included folder, so excluding it changes nothing.");
            return;
        }
    }

    noteLabel->setText("These folders are the catalog: removing a row removes its "
                       "images from the catalog. The image files are not affected.");
}

void CatalogRootsDlg::setScanning(bool on)
{
/*
    While a scan runs the button becomes Stop and the table is frozen: editing the scope
    underneath a running scan would leave the user unsure whether the folder they just
    added is being scanned or not.
*/
    scanning = on;
    scanBtn->setText(on ? "Stop" : "Scan");
    table->setEnabled(!on);
    addBtn->setEnabled(!on);
    removeBtn->setEnabled(!on && !table->selectedItems().isEmpty());
    if (!on) {
        scanBlock->hide();
        fitToContents();
    }
    else if (scanBlock->isHidden()) {
        scanBar->setRange(0, 0);            // busy until the first report
        scanLabel->setText("Starting the scan...");
        scanBlock->show();
        fitToContents();
    }
    updateNote();
}

void CatalogRootsDlg::setScanProgress(const CatalogScanProgress &p)
{
    if (!scanning) return;
    const QLocale loc;
    scanBar->setRange(0, qMax(1, p.total));
    scanBar->setValue(qMin(p.done, qMax(1, p.total)));

    /*  TWO LINES: where the scan is and how long is left, then -- in grey -- that the
        window is not needed. One sentence carrying both was the crowding. */
    QString text;
    if (p.phase == CatalogScanProgress::Checking) {
        text = QString("<b>Checking folders</b> for new and changed images: %1 of %2")
                   .arg(loc.toString(p.done), loc.toString(p.total));
    }
    else {
        text = QString("<b>Indexing</b> %1 of %2 images").arg(loc.toString(p.done),
                                                              loc.toString(p.total));
        if (p.filesPerSec > 0) {
            const int decimals = p.filesPerSec < 10 ? 1 : 0;
            text += QString(" &nbsp;&middot;&nbsp; %1 per second")
                        .arg(loc.toString(p.filesPerSec, 'f', decimals));
        }
    }
    text += " &nbsp;&middot;&nbsp; " + catalogFormatEta(p.etaSecs)
            + (p.etaSecs < 0 ? "" : " left")
            + (p.phase == CatalogScanProgress::Checking && p.etaSecs >= 0
                   ? " to check" : "");
    if (p.paused)
        text = "<b>Paused</b> while a folder loads &mdash; resumes by itself.<br>" + text;
    text += "<br>" + G::secondaryHtml("You can close this window: the scan "
            "continues in the background, and resumes at the next launch if Winnow "
            "quits first.");
    const bool wasHidden = scanBlock->isHidden();
    const int oldLines = scanLabel->text().count("<br>");
    scanLabel->setText(text);
    scanBlock->show();
    if (wasHidden || oldLines != text.count("<br>")) fitToContents();
}

void CatalogRootsDlg::setCatalogStatus(const QString &text)
{
    statusLabel->setText(text);
    sections->updateSummaries();
    fitToContents();
}
