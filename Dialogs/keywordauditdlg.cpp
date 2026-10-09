#include "Dialogs/keywordauditdlg.h"
#include "Cache/thumbcache.h"
#include "Datamodel/keywordauditstore.h"
#include "Metadata/keywordpaths.h"

#include <QAbstractListModel>
#include <QCache>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

using VisualAudit::Finding;

namespace {

constexpr int kRowHeight = 92;
constexpr int kListThumb = 80;
constexpr int kBigThumb = 320;
constexpr int kExThumb = 96;

enum Role { FindingRole = Qt::UserRole + 1 };

}   // namespace

/* ---------------------------------------------------------------- list model */
/*  The findings the filters let through, as indices into the outcome. Painting is the
    delegate's; the model only says which finding a row is. */
class FindingModel : public QAbstractListModel
{
public:
    using QAbstractListModel::QAbstractListModel;
    QVector<int> rows;

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : rows.size();
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= rows.size()) return {};
        if (role == FindingRole) return rows[index.row()];
        return {};
    }
    void setRows(const QVector<int> &r)
    {
        beginResetModel();
        rows = r;
        endResetModel();
    }
    void touch(int row)
    {
        const QModelIndex i = index(row);
        emit dataChanged(i, i);
    }
};

/* ---------------------------------------------------------------- delegate */
class FindingDelegate : public QStyledItemDelegate
{
public:
    std::function<void(QPainter *, const QStyleOptionViewItem &, int)> paintFn;
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {300, kRowHeight};
    }
    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, index);
        o.text.clear();
        o.widget->style()->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
        if (paintFn) paintFn(p, opt, index.data(FindingRole).toInt());
    }
};

/* ---------------------------------------------------------------- dialog */
KeywordAuditDlg::KeywordAuditDlg(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Audit Keywords"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    qRegisterMetaType<KeywordAuditDlg::Fix>("KeywordAuditDlg::Fix");
    buildUi();
    resize(1180, 780);
}

KeywordAuditDlg::~KeywordAuditDlg() = default;

QString KeywordAuditDlg::leaf(const QString &path) const
{
    return keywordLeafOf(path);
}

QPixmap KeywordAuditDlg::thumb(const QString &path, int side)
{
/*
    The cached browsing thumbnail -- the picture the audit looked at -- scaled for the
    dialog and memoised, because the list repaints far more often than it scrolls. A
    miss (an image never browsed) draws a grey square rather than reading the file on
    the GUI thread.
*/
    static QCache<QString, QPixmap> cache(800);
    const QString key = path + '#' + QString::number(side);
    if (QPixmap *p = cache.object(key)) return *p;
    QPixmap pm;
    const QImage img = ThumbCache::instance().getImage(path, false);
    if (!img.isNull())
        pm = QPixmap::fromImage(img.scaled(side, side, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
    else {
        pm = QPixmap(side, side);
        pm.fill(QColor(70, 70, 70));
    }
    cache.insert(key, new QPixmap(pm));
    return pm;
}

void KeywordAuditDlg::buildUi()
{
    auto *root = new QVBoxLayout(this);

    /* Run row: what is happening, and the one button that starts or stops it. */
    auto *runRow = new QHBoxLayout;
    stageLabel = new QLabel(tr("Compares every keyworded image with your other images "
                               "carrying the same keywords, and lists the ones that "
                               "do not fit."));
    stageLabel->setWordWrap(true);
    progressBar = new QProgressBar;
    progressBar->setMaximumWidth(260);
    progressBar->setVisible(false);
    runBtn = new QPushButton(tr("Run Audit"));
    runRow->addWidget(stageLabel, 1);
    runRow->addWidget(progressBar);
    runRow->addWidget(runBtn);
    root->addLayout(runRow);
    connect(runBtn, &QPushButton::clicked, this, [this] {
        if (running) emit stopRequested();
        else emit runRequested();
    });

    tabs = new QTabWidget;
    root->addWidget(tabs, 1);

    /* ------------------------------------------------ Findings tab */
    auto *findingsPage = new QWidget;
    auto *fl = new QVBoxLayout(findingsPage);
    auto *filterRow = new QHBoxLayout;
    kindCombo = new QComboBox;
    kindCombo->addItem(tr("All findings"), -1);
    kindCombo->addItem(tr("Suspect keywords"), int(Finding::Suspect));
    kindCombo->addItem(tr("Missing keywords"), int(Finding::Missing));
    keywordFilter = new QLineEdit;
    keywordFilter->setPlaceholderText(tr("Filter by keyword"));
    keywordFilter->setClearButtonEnabled(true);
    hideDecided = new QCheckBox(tr("Hide decided"));
    filterRow->addWidget(kindCombo);
    filterRow->addWidget(keywordFilter, 1);
    filterRow->addWidget(hideDecided);
    fl->addLayout(filterRow);
    connect(kindCombo, &QComboBox::currentIndexChanged, this, &KeywordAuditDlg::refilter);
    connect(keywordFilter, &QLineEdit::textChanged, this, &KeywordAuditDlg::refilter);
    connect(hideDecided, &QCheckBox::toggled, this, &KeywordAuditDlg::refilter);

    auto *split = new QSplitter;
    fl->addWidget(split, 1);

    list = new QListView;
    list->setUniformItemSizes(true);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    model = new FindingModel(this);
    list->setModel(model);
    auto *delegate = new FindingDelegate(list);
    delegate->paintFn = [this](QPainter *p, const QStyleOptionViewItem &opt, int fi) {
        const Finding &f = outcome.result.findings[fi];
        const KeywordAuditImage &im = outcome.images[f.members.first()];
        const QRect r = opt.rect.adjusted(6, 6, -6, -6);
        const QPixmap pm = thumb(im.path, kListThumb);
        p->drawPixmap(r.left() + (kListThumb - pm.width()) / 2,
                      r.top() + (kListThumb - pm.height()) / 2, pm);

        const QRect text = r.adjusted(kListThumb + 10, 0, 0, 0);
        const bool sus = f.kind == Finding::Suspect;
        QString claim = sus ? tr("Tagged %1").arg(leaf(f.keyword))
                            : tr("Not tagged %1").arg(leaf(f.keyword));
        if (sus && !f.alternative.isEmpty())
            claim += tr("  →  looks like %1").arg(leaf(f.alternative));
        QFont bold = opt.font;
        bold.setBold(true);
        p->setFont(bold);
        p->setPen(opt.palette.color(QPalette::Text));
        const int lh = opt.fontMetrics.height() + 2;
        p->drawText(text.adjusted(0, 0, -110, 0), Qt::AlignLeft | Qt::AlignTop, claim);
        p->setFont(opt.font);

        QString line2 = QFileInfo(im.path).fileName();
        if (f.members.size() > 1) line2 += tr("  ·  burst of %1").arg(f.members.size());
        p->drawText(text.adjusted(0, lh, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                    opt.fontMetrics.elidedText(line2, Qt::ElideMiddle, text.width()));
        p->setPen(opt.palette.color(QPalette::PlaceholderText));
        p->drawText(text.adjusted(0, 2 * lh, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                    opt.fontMetrics.elidedText(QFileInfo(im.path).absolutePath(),
                                               Qt::ElideMiddle, text.width()));

        static const char *names[] = {"", "Replace", "Remove", "Add", "Keywords right",
                                      "Applied"};
        const int d = decision[fi];
        QString right = d == Undecided
                            ? (sus ? tr("Suspect") : tr("Missing"))
                            : tr(names[d]);
        QColor c = d == Undecided ? (sus ? QColor(220, 110, 100) : QColor(110, 160, 220))
                 : d == Correct   ? QColor(140, 140, 140)
                                  : QColor(110, 190, 120);
        p->setPen(c);
        p->drawText(text, Qt::AlignRight | Qt::AlignTop, right);
    };
    list->setItemDelegate(delegate);
    split->addWidget(list);
    connect(list->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &KeywordAuditDlg::showCurrent);
    connect(list, &QListView::doubleClicked, this, [this] {
        if (showBtn->isEnabled()) showBtn->click();
    });

    /* Detail: the image, the claim, and what the user's own pictures look like. */
    auto *detail = new QWidget;
    auto *dl = new QVBoxLayout(detail);
    bigImage = new QLabel;
    bigImage->setFixedSize(kBigThumb, kBigThumb);
    bigImage->setAlignment(Qt::AlignCenter);
    claimLabel = new QLabel;
    claimLabel->setWordWrap(true);
    claimLabel->setTextFormat(Qt::RichText);
    dl->addWidget(bigImage, 0, Qt::AlignHCenter);
    dl->addWidget(claimLabel);

    auto exRow = [&](QLabel *&caption, QLabel **cells) {
        caption = new QLabel;
        dl->addWidget(caption);
        auto *row = new QHBoxLayout;
        for (int i = 0; i < 3; ++i) {
            cells[i] = new QLabel;
            cells[i]->setFixedSize(kExThumb, kExThumb);
            cells[i]->setAlignment(Qt::AlignCenter);
            row->addWidget(cells[i]);
        }
        row->addStretch();
        dl->addLayout(row);
    };
    exRow(kwExLabel, kwEx);
    exRow(altExLabel, altEx);

    pathLabel = new QLabel;
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tagsLabel = new QLabel;
    tagsLabel->setWordWrap(true);
    dl->addWidget(pathLabel);
    dl->addWidget(tagsLabel);
    dl->addStretch();

    auto *btnGrid = new QHBoxLayout;
    replaceBtn = new QPushButton;
    removeBtn = new QPushButton;
    addBtn = new QPushButton;
    correctBtn = new QPushButton(tr("Keywords Are Right (C)"));
    for (QPushButton *b : {replaceBtn, removeBtn, addBtn, correctBtn}) btnGrid->addWidget(b);
    dl->addLayout(btnGrid);
    auto *btnRow2 = new QHBoxLayout;
    undoBtn = new QPushButton(tr("Undecide (U)"));
    showBtn = new QPushButton(tr("Show in Winnow (S)"));
    skipBtn = new QPushButton;
    btnRow2->addWidget(undoBtn);
    btnRow2->addWidget(showBtn);
    btnRow2->addWidget(skipBtn);
    dl->addLayout(btnRow2);
    split->addWidget(detail);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);

    connect(replaceBtn, &QPushButton::clicked, this, [this] { decide(Replace); });
    connect(removeBtn, &QPushButton::clicked, this, [this] { decide(Remove); });
    connect(addBtn, &QPushButton::clicked, this, [this] { decide(Add); });
    connect(correctBtn, &QPushButton::clicked, this, [this] { decide(Correct); });
    connect(undoBtn, &QPushButton::clicked, this, [this] { decide(Undecided); });
    connect(showBtn, &QPushButton::clicked, this, [this] {
        const int fi = list->currentIndex().data(FindingRole).toInt();
        if (list->currentIndex().isValid())
            emit showImageRequested(outcome.images[outcome.result.findings[fi]
                                                       .members.first()].path);
    });
    connect(skipBtn, &QPushButton::clicked, this, [this] {
        if (!list->currentIndex().isValid()) return;
        const int fi = list->currentIndex().data(FindingRole).toInt();
        const QString kw = outcome.result.findings[fi].keyword;
        KeywordAuditStore::setSkipped(kw, true);
        skippedKeywords.insert(keywordFold(kw), kw);
        fillKeywordTable();
        refilter();
    });
    tabs->addTab(findingsPage, tr("Findings"));

    /* ------------------------------------------------ Keywords tab */
    auto *kwPage = new QWidget;
    auto *kl = new QVBoxLayout(kwPage);
    auto *kwNote = new QLabel(tr(
        "Accuracy is how well your own images predict each keyword on images they were "
        "not shown. Keywords below 80% cannot be judged from the picture (people across "
        "shoots, ratings, projects) and are not audited. Tick \"Don't audit\" to leave a "
        "keyword out of the next run."));
    kwNote->setWordWrap(true);
    kl->addWidget(kwNote);
    kwTable = new QTableWidget(0, 7);
    kwTable->setHorizontalHeaderLabels({tr("Keyword"), tr("Images"), tr("Accuracy"),
                                        tr("Status"), tr("Suspect"), tr("Missing"),
                                        tr("Don't audit")});
    kwTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    kwTable->verticalHeader()->setVisible(false);
    kwTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    kwTable->setSortingEnabled(true);
    kl->addWidget(kwTable, 1);
    connect(kwTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
        if (it->column() != 6) return;
        const QString kw = kwTable->item(it->row(), 0)->data(Qt::UserRole).toString();
        const bool on = it->checkState() == Qt::Checked;
        KeywordAuditStore::setSkipped(kw, on);
        if (on) skippedKeywords.insert(keywordFold(kw), kw);
        else skippedKeywords.remove(keywordFold(kw));
        refilter();
    });
    tabs->addTab(kwPage, tr("Keywords"));

    /* ------------------------------------------------ bottom */
    auto *bottom = new QHBoxLayout;
    summaryLabel = new QLabel;
    applyBtn = new QPushButton(tr("Apply Changes"));
    auto *closeBtn = new QPushButton(tr("Close"));
    bottom->addWidget(summaryLabel, 1);
    bottom->addWidget(applyBtn);
    bottom->addWidget(closeBtn);
    root->addLayout(bottom);
    connect(applyBtn, &QPushButton::clicked, this, &KeywordAuditDlg::apply);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    skippedKeywords = KeywordAuditStore::skipped();
    setOutcome({});
}

void KeywordAuditDlg::setUnavailableReason(const QString &reason)
{
    unavailable = reason;
    runBtn->setEnabled(reason.isEmpty());
    runBtn->setToolTip(reason);
    if (!reason.isEmpty() && !running) stageLabel->setText(reason);
}

void KeywordAuditDlg::setRunning(bool on)
{
    running = on;
    runBtn->setText(on ? tr("Stop") : tr("Run Audit"));
    runBtn->setEnabled(on || unavailable.isEmpty());
    progressBar->setVisible(on);
    if (on) {
        progressBar->setRange(0, 0);
        stageLabel->setText(tr("Starting..."));
    }
}

void KeywordAuditDlg::setProgress(const QString &stage, int done, int total)
{
    if (total > 0) {
        progressBar->setRange(0, total);
        progressBar->setValue(done);
        stageLabel->setText(tr("%1: %2 of %3").arg(stage, QLocale().toString(done),
                                                   QLocale().toString(total)));
    }
    else if (done > 0) {
        progressBar->setRange(0, 100);
        progressBar->setValue(done);
        stageLabel->setText(stage);
    }
    else {
        progressBar->setRange(0, 0);
        stageLabel->setText(stage);
    }
}

void KeywordAuditDlg::setOutcome(const KeywordAuditOutcome &o)
{
    outcome = o;
    decision = QVector<int>(o.result.findings.size(), Undecided);
    pendingApply.clear();
    exemplars.clear();
    for (const VisualAudit::KeywordStat &s : o.result.stats)
        exemplars.insert(s.keyword, s.exemplars);

    if (!o.error.isEmpty()) stageLabel->setText(o.error);
    else if (o.cancelled) stageLabel->setText(tr("Stopped. Run again to continue -- "
                                                 "images already read are kept."));
    else if (o.ok) {
        int sus = 0;
        for (const Finding &f : o.result.findings) sus += f.kind == Finding::Suspect;
        stageLabel->setText(
            tr("%1 images checked in %2 s: %3 suspect keywords, %4 possibly missing. "
               "Most confident first.")
                .arg(QLocale().toString(o.images.size()))
                .arg(o.ms / 1000)
                .arg(QLocale().toString(sus))
                .arg(QLocale().toString(o.result.findings.size() - sus)));
    }
    fillKeywordTable();
    refilter();
}

void KeywordAuditDlg::fillKeywordTable()
{
    const QSignalBlocker block(kwTable);
    kwTable->setSortingEnabled(false);
    kwTable->setRowCount(0);
    kwTable->setRowCount(outcome.result.stats.size());
    int r = 0;
    for (const VisualAudit::KeywordStat &s : outcome.result.stats) {
        auto num = [](double v) {
            auto *it = new QTableWidgetItem;
            it->setData(Qt::DisplayRole, v);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return it;
        };
        auto *name = new QTableWidgetItem(s.keyword);
        name->setData(Qt::UserRole, s.keyword);
        kwTable->setItem(r, 0, name);
        kwTable->setItem(r, 1, num(s.images));
        auto *acc = num(qRound(100 * std::min(s.aucKnn, s.aucRidge)));
        acc->setText(QString("%1%").arg(qRound(100 * std::min(s.aucKnn, s.aucRidge))));
        kwTable->setItem(r, 2, acc);
        const bool skipped = skippedKeywords.contains(keywordFold(s.keyword));
        kwTable->setItem(r, 3, new QTableWidgetItem(
            skipped ? tr("Excluded") : s.visual ? tr("Audited") : tr("Not visual")));
        kwTable->setItem(r, 4, num(s.suspects));
        kwTable->setItem(r, 5, num(s.missing));
        auto *skip = new QTableWidgetItem;
        skip->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        skip->setCheckState(skipped ? Qt::Checked : Qt::Unchecked);
        kwTable->setItem(r, 6, skip);
        ++r;
    }
    kwTable->setSortingEnabled(true);
}

void KeywordAuditDlg::refilter()
{
    const int kind = kindCombo->currentData().toInt();
    const QString text = keywordFilter->text().trimmed();
    const bool hide = hideDecided->isChecked();
    const int keep = list->currentIndex().isValid()
                         ? list->currentIndex().data(FindingRole).toInt() : -1;
    QVector<int> rows;
    for (int i = 0; i < outcome.result.findings.size(); ++i) {
        const Finding &f = outcome.result.findings[i];
        if (kind >= 0 && int(f.kind) != kind) continue;
        if (!text.isEmpty() && !f.keyword.contains(text, Qt::CaseInsensitive)
            && !f.alternative.contains(text, Qt::CaseInsensitive))
            continue;
        if (hide && decision[i] != Undecided) continue;
        if (skippedKeywords.contains(keywordFold(f.keyword))) continue;
        rows << i;
    }
    model->setRows(rows);
    const int at = rows.indexOf(keep);
    list->setCurrentIndex(model->index(at >= 0 ? at : 0));
    showCurrent();
    updateSummary();
}

void KeywordAuditDlg::showCurrent()
{
    const QModelIndex cur = list->currentIndex();
    const bool has = cur.isValid() && !outcome.result.findings.isEmpty();
    for (QPushButton *b : {replaceBtn, removeBtn, addBtn, correctBtn, undoBtn, showBtn,
                           skipBtn})
        b->setVisible(has);
    if (!has) {
        bigImage->clear();
        claimLabel->setText(outcome.ok ? tr("No findings match.") : QString());
        for (QLabel *l : {kwExLabel, altExLabel, pathLabel, tagsLabel}) l->clear();
        for (int i = 0; i < 3; ++i) { kwEx[i]->clear(); altEx[i]->clear(); }
        return;
    }

    const int fi = cur.data(FindingRole).toInt();
    const Finding &f = outcome.result.findings[fi];
    const KeywordAuditImage &im = outcome.images[f.members.first()];
    const bool sus = f.kind == Finding::Suspect;
    bigImage->setPixmap(thumb(im.path, kBigThumb));

    const QString kw = f.keyword.toHtmlEscaped(), alt = f.alternative.toHtmlEscaped();
    QString claim;
    if (sus && !f.alternative.isEmpty())
        claim = tr("Tagged <b>%1</b>, but looks like your <b>%2</b> images.").arg(kw, alt);
    else if (sus)
        claim = tr("Tagged <b>%1</b>, but does not look like your other %2 images.")
                    .arg(kw, leaf(f.keyword).toHtmlEscaped());
    else {
        claim = tr("Not tagged <b>%1</b>, but looks like your %2 images.")
                    .arg(kw, leaf(f.keyword).toHtmlEscaped());
        if (!f.siblings.isEmpty())
            claim += tr("<br>It is tagged %1.").arg(f.siblings.join(", ").toHtmlEscaped());
    }
    if (f.members.size() > 1)
        claim += tr("<br>A burst of %1 near-identical frames; a decision applies to all.")
                     .arg(f.members.size());
    claim += tr("<br><span style='color:gray'>Confidence %1%</span>")
                 .arg(qRound(100 * f.score));
    claimLabel->setText(claim);

    auto fillEx = [&](QLabel *caption, QLabel **cells, const QString &keyword) {
        const QVector<int> ex = exemplars.value(keyword);
        caption->setText(keyword.isEmpty() ? QString()
                                           : tr("Your %1 images:").arg(leaf(keyword)));
        for (int i = 0; i < 3; ++i) {
            if (i < ex.size()) cells[i]->setPixmap(thumb(outcome.images[ex[i]].path,
                                                         kExThumb));
            else cells[i]->clear();
        }
    };
    fillEx(kwExLabel, kwEx, f.keyword);
    fillEx(altExLabel, altEx, f.alternative);

    pathLabel->setText(im.path);
    tagsLabel->setText(tr("Keywords: %1").arg(im.explicitPaths.join(", ")));

    replaceBtn->setVisible(sus && !f.alternative.isEmpty());
    replaceBtn->setText(tr("Replace with %1 (R)").arg(leaf(f.alternative)));
    removeBtn->setVisible(sus);
    removeBtn->setText(tr("Remove %1 (D)").arg(leaf(f.keyword)));
    addBtn->setVisible(!sus);
    addBtn->setText(tr("Add %1 (A)").arg(leaf(f.keyword)));
    skipBtn->setText(tr("Don't Audit %1").arg(leaf(f.keyword)));

    const bool done = decision[fi] == Applied;
    for (QPushButton *b : {replaceBtn, removeBtn, addBtn, correctBtn, undoBtn})
        b->setEnabled(!done);
    undoBtn->setEnabled(!done && decision[fi] != Undecided);
}

void KeywordAuditDlg::decide(Decision d)
{
    const QModelIndex cur = list->currentIndex();
    if (!cur.isValid()) return;
    const int fi = cur.data(FindingRole).toInt();
    const Finding &f = outcome.result.findings[fi];
    if (decision[fi] == Applied) return;
    const bool sus = f.kind == Finding::Suspect;
    if ((d == Replace && (!sus || f.alternative.isEmpty())) || (d == Remove && !sus)
        || (d == Add && sus))
        return;

    /* "Keywords are right" is remembered now; anything else forgets it. */
    const QString kind = sus ? "suspect" : "missing";
    if (d == Correct || decision[fi] == Correct)
        for (int m : f.members)
            KeywordAuditStore::setDismissed(outcome.images[m].pathKey, kind, f.keyword,
                                            d == Correct);
    decision[fi] = d;
    model->touch(cur.row());
    updateSummary();
    if (d == Undecided) showCurrent();
    else advance();
}

void KeywordAuditDlg::advance()
{
    if (hideDecided->isChecked()) {
        const int row = list->currentIndex().row();
        refilter();
        list->setCurrentIndex(model->index(std::min(row, model->rowCount() - 1)));
        showCurrent();
        return;
    }
    for (int r = list->currentIndex().row() + 1; r < model->rowCount(); ++r)
        if (decision[model->rows[r]] == Undecided) {
            list->setCurrentIndex(model->index(r));
            return;
        }
    showCurrent();
}

int KeywordAuditDlg::pendingCount() const
{
    int n = 0;
    for (int d : decision) n += d == Replace || d == Remove || d == Add;
    return n;
}

void KeywordAuditDlg::updateSummary()
{
    int decided = 0;
    for (int d : decision) decided += d != Undecided;
    const int pending = pendingCount();
    summaryLabel->setText(tr("%1 findings  ·  %2 decided  ·  %3 changes to apply")
                              .arg(QLocale().toString(decision.size()))
                              .arg(QLocale().toString(decided))
                              .arg(QLocale().toString(pending)));
    applyBtn->setEnabled(pending > 0 && pendingApply.isEmpty());
}

void KeywordAuditDlg::apply()
{
    QVector<Fix> fixes;
    pendingApply.clear();
    for (int fi = 0; fi < decision.size(); ++fi) {
        const int d = decision[fi];
        if (d != Replace && d != Remove && d != Add) continue;
        const Finding &f = outcome.result.findings[fi];
        for (int m : f.members) {
            Fix fx;
            fx.path = outcome.images[m].path;
            if (d == Replace || d == Remove) fx.remove = f.keyword;
            if (d == Replace) fx.add = f.alternative;
            if (d == Add) fx.add = f.keyword;
            fixes << fx;
        }
        pendingApply << fi;
    }
    if (fixes.isEmpty()) return;
    applyBtn->setEnabled(false);
    emit applyRequested(fixes);
}

void KeywordAuditDlg::applied(int written, int failed)
{
    for (int fi : pendingApply) decision[fi] = Applied;
    pendingApply.clear();
    stageLabel->setText(failed
        ? tr("Updated %1 images; %2 could not be written (offline or read-only).")
              .arg(written).arg(failed)
        : tr("Updated %1 images.").arg(written));
    model->setRows(model->rows);
    showCurrent();
    updateSummary();
}

void KeywordAuditDlg::keyPressEvent(QKeyEvent *event)
{
    if (keywordFilter->hasFocus() || tabs->currentIndex() != 0) {
        QDialog::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
        case Qt::Key_R: if (replaceBtn->isVisible()) decide(Replace); return;
        case Qt::Key_D:
        case Qt::Key_Delete:
        case Qt::Key_Backspace: if (removeBtn->isVisible()) decide(Remove); return;
        case Qt::Key_A: if (addBtn->isVisible()) decide(Add); return;
        case Qt::Key_C: decide(Correct); return;
        case Qt::Key_U: decide(Undecided); return;
        case Qt::Key_S: showBtn->click(); return;
        case Qt::Key_Escape: close(); return;
    }
    QDialog::keyPressEvent(event);
}

void KeywordAuditDlg::closeEvent(QCloseEvent *event)
{
    const int pending = pendingCount();
    if (pending > 0) {
        const auto b = QMessageBox::question(
            this, tr("Audit Keywords"),
            tr("%1 keyword changes have not been applied. Apply them before closing?")
                .arg(pending),
            QMessageBox::Apply | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Apply);
        if (b == QMessageBox::Cancel) { event->ignore(); return; }
        if (b == QMessageBox::Apply) apply();
        else for (int &d : decision)
            if (d == Replace || d == Remove || d == Add) d = Undecided;
    }
    QDialog::closeEvent(event);
}
