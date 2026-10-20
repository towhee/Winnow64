#include "Views/keywordtags.h"

#include "Datamodel/keywordvocab.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"
#include "Utilities/flowlayout.h"
#include "Utilities/gradientheader.h"
#include "Utilities/popup.h"
#include "Utilities/visualaudit.h"

#include <QApplication>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

/*
    See keywordtags.h. The mime type is shared with Views/keywordtree.cpp.
*/
const char *kVocabNodeMime = "application/x-winnow-vocab-node";
/*  The other direction: a TAG dragged up onto the Keyword list, carrying its path. */
const char *kKeywordTagMime = "application/x-winnow-keyword-tag";

namespace {

/*  One keyword on the selection. A widget rather than a styled QPushButton because it has
    two hit targets that mean different things -- the label (show it in the list, or for
    a red tag, replace it) and the x (remove from all) -- and one button cannot offer
    both. */
class Tag : public QFrame
{
public:
    Tag(const QString &path, const QString &label, bool partial, bool unfiled,
        QWidget *parent)
        : QFrame(parent), path(path), unfiled(unfiled)
    {
        setObjectName("KeywordTag");
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::PointingHandCursor);

        QHBoxLayout *l = new QHBoxLayout(this);
        l->setContentsMargins(6, 1, 2, 1);
        l->setSpacing(4);

        QString text = label;
        if (partial) text += " *";
        QLabel *name = new QLabel(text, this);
        /*  A stylesheet, not the palette: the app stylesheet (widgetcss.cpp) sets a
            colour on every QWidget, and a stylesheet beats a palette silently. */
        if (unfiled)
            name->setStyleSheet("color:" + G::unfiledKeywordColor.name() + ";");
        /*  The full path in the tooltip is how two tags showing the same leaf are told
            apart -- and they will, because that is the case path identity exists for. */
        name->setToolTip(unfiled ? path + "\n\nNot in your keyword list. Click to pick "
                                   "a similar keyword from your list to replace it, "
                                   "drop a keyword from the list on it, or drag it "
                                   "onto the list to file it there."
                                 : path);
        l->addWidget(name);

        QToolButton *close = new QToolButton(this);
        close->setText("×");
        close->setAutoRaise(true);
        close->setToolTip("Remove from the selected images");
        close->setCursor(Qt::ArrowCursor);
        l->addWidget(close);

        QObject::connect(close, &QToolButton::clicked, this, [this]{
            if (onRemove) onRemove(this->path);
        });
    }

    std::function<void(const QString &)> onRemove;
    std::function<void(const QString &)> onClick;
    /*  Right-click on a partial ("*") tag: add it to every selected image. */
    std::function<void(const QString &)> onPromote;

    /*  The drop target while a vocabulary keyword is dragged over an unfiled tag: an
        outline, so "this one gets replaced" is visible before the mouse is released. */
    void setDropTarget(bool on)
    {
        setStyleSheet(on ? "QFrame#KeywordTag { border: 1px solid "
                           + G::unfiledKeywordColor.name() + "; }"
                         : QString());
    }

    const QString path;
    const bool unfiled;

protected:
    /*  A RED tag is DRAGGABLE onto the Keyword list, which files it there (see
        KeywordTree::dropEvent). Only red: a filed tag is already in the list, and
        dragging it would re-file images behind the user's back. The click still means
        what it did; a press that turns into a drag is not also a click. */
    void mousePressEvent(QMouseEvent *event) override
    {
        pressPos = event->position().toPoint();
        dragged = false;
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!unfiled || !(event->buttons() & Qt::LeftButton) || dragged) return;
        if ((event->position().toPoint() - pressPos).manhattanLength()
            < QApplication::startDragDistance()) return;
        dragged = true;

        QMimeData *mime = new QMimeData;
        mime->setData(kKeywordTagMime, path.toUtf8());
        mime->setText(path);
        /*  Parented on the tag zone, not this tag: the drop rebuilds the tags (queued,
            see MW's tagFiled connection), and a drag owned by a deleted widget is a
            use-after-free waiting for exec() to return. */
        QDrag *drag = new QDrag(parentWidget());
        drag->setMimeData(mime);
        drag->setPixmap(grab());
        drag->exec(Qt::CopyAction);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (dragged) { dragged = false; return; }
        if (event->button() != Qt::LeftButton) return;
        if (onClick) onClick(path);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        if (!onPromote) return;
        QMenu menu(this);
        QAction *all = menu.addAction("Add to all selected images");
        if (menu.exec(event->globalPos()) == all) onPromote(path);
    }

private:
    QPoint pressPos;
    bool dragged = false;
public:

};

/*  One suggested keyword: "+ Heron", click to add, x to say "not this keyword". Not a
    Tag -- it is not on the image yet, and a chip that looked like the tags above it
    would read as already applied. */
class SuggestChip : public QFrame
{
public:
    SuggestChip(const QString &path, const QString &tip, QWidget *parent)
        : QFrame(parent), path(path)
    {
        setObjectName("KeywordSuggestion");
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::PointingHandCursor);
        QHBoxLayout *l = new QHBoxLayout(this);
        l->setContentsMargins(6, 1, 2, 1);
        l->setSpacing(4);
        QLabel *name = new QLabel("+ " + keywordLeafOf(path), this);
        name->setEnabled(false);            // dimmed: a proposal, not a tag
        l->addWidget(name);
        setToolTip(tip);
        QToolButton *no = new QToolButton(this);
        no->setText("×");
        no->setAutoRaise(true);
        no->setToolTip("Not this keyword -- don't suggest it for this image again");
        no->setCursor(Qt::ArrowCursor);
        l->addWidget(no);
        QObject::connect(no, &QToolButton::clicked, this, [this] {
            if (onDismiss) onDismiss(this->path);
        });
    }
    std::function<void(const QString &)> onAccept, onDismiss;
    const QString path;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && onAccept) onAccept(path);
    }
};

/*  The completer popup: leaf in the normal weight, its branch dimmed after it. Two
    keywords sharing a leaf are two rows that differ only in the dimmed half, which is
    exactly the information needed to pick the right one. */
class CompletionDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        QStyledItemDelegate::paint(painter, opt, index);

        const QString leaf = index.data(Qt::DisplayRole).toString();
        const QString parent = index.data(Qt::UserRole + 2).toString();

        painter->save();
        QRect r = opt.rect.adjusted(4, 0, -4, 0);
        painter->setPen(opt.palette.color(QPalette::Text));
        const int leafW = opt.fontMetrics.horizontalAdvance(leaf);
        painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeft, leaf);
        if (!parent.isEmpty()) {
            QColor dim = opt.palette.color(QPalette::Text);
            dim.setAlpha(120);
            painter->setPen(dim);
            QRect pr = r.adjusted(leafW + 10, 0, 0, 0);
            const QString elided =
                opt.fontMetrics.elidedText(parent, Qt::ElideLeft, pr.width());
            painter->drawText(pr, Qt::AlignVCenter | Qt::AlignLeft, elided);
        }
        painter->restore();
    }
};

}   // namespace

KeywordTags::KeywordTags(KeywordVocab *vocab, QWidget *parent)
    : QWidget(parent), vocab(vocab)
{
    setAcceptDrops(true);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(3);

    /*  The band names WHOSE keywords these are, which is the one thing a tag cannot say
        for itself: the zone follows the SELECTION, so the same tag means something
        different with one image selected than with forty. rebuild() keeps the count in
        it. */
    header = new GradientHeader(headerText(), this);
    outer->addWidget(header);

    /*  THE ADD FIELD HEADS THE ZONE, above the tags rather than under them. The zone now
        sits on top of the dock, so a field at its foot would land against the tree's
        "Find keyword" box with only the dim legend between them -- two text fields in a
        row, doing opposite things. */
    addEdit = new QLineEdit(this);
    addEdit->setPlaceholderText("Add keyword from your list");
    addEdit->setToolTip("Type to pick a keyword from your keyword list and add it to the "
                        "selected images.\nNew keywords are created in the Keyword list "
                        "below.");
    addEdit->setClearButtonEnabled(true);
    outer->addWidget(addEdit);

    /*  Scrolled, because an image can carry twenty keywords and the dock is not tall.
        widgetResizable so the flow layout is asked for its height at the real width. */
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    tagArea = new QWidget(scroll);
    flow = new FlowLayout(tagArea, 2, 4, 3);
    tagArea->setLayout(flow);
    scroll->setWidget(tagArea);
    outer->addWidget(scroll, 1);

    /*  SUGGESTED: keywords for the current image from Audit keywords' model and the
        image's session (KeywordSuggester). Its own band, because these are NOT on the
        image -- they are a question -- and below the tags, because what the image
        carries is read first. Always there, with a status line, so the feature can be
        found before it has anything to say. */
    suggestHeader = new GradientHeader(tr("Suggested"), this);
    suggestHeader->setToolTip(tr("Keywords this image may be missing: ones it looks like "
                                 "your images carrying them, and ones most images from "
                                 "the same session carry. Click to add; x to dismiss."));
    outer->addWidget(suggestHeader);
    suggestArea = new QWidget(this);
    suggestFlow = new FlowLayout(suggestArea, 2, 4, 3);
    suggestArea->setLayout(suggestFlow);
    outer->addWidget(suggestArea);
    suggestStatus = new QLabel(this);
    suggestStatus->setEnabled(false);
    suggestStatus->setWordWrap(true);
    suggestStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    suggestStatus->setTextFormat(Qt::RichText);
    connect(suggestStatus, &QLabel::linkActivated, this, [this] { emit auditRequested(); });
    outer->addWidget(suggestStatus);

    legend = new QLabel(this);
    legend->setEnabled(false);
    /*  THE LEGEND MUST NEVER WIDEN THE DOCK. An unwrapped QLabel's minimum width is its
        whole text, and the legend's lines ("... (click it, or drop a keyword on it, to
        replace it)", "Replacing <path> ...") are long -- so the layout forced the dock
        wider whenever one appeared. Wrapped, and Ignored horizontally, it takes the
        width the dock has and grows DOWN instead. */
    legend->setWordWrap(true);
    legend->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    outer->addWidget(legend);

    completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setModel(new QStandardItemModel(completer));
    completer->popup()->setItemDelegate(new CompletionDelegate(completer));
    addEdit->setCompleter(completer);

    /*  The model is rebuilt per keystroke from KeywordVocab::completions rather than
        holding the whole vocabulary: completions already sorts shallowest-first and
        searches synonyms, and rebuilding a few dozen rows is nothing next to keeping
        thousands sorted by a rule QCompleter does not know. */
    connect(addEdit, &QLineEdit::textEdited, this, [this](const QString &t) {
        /*  Emptying the field is how a replace is cancelled. */
        if (t.trimmed().isEmpty()) endReplace();
        fillCompletions(t.trimmed().isEmpty() ? QList<const VocabNode *>()
                                              : this->vocab->completions(t.trimmed()));
    });

    connect(completer, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, [this](const QModelIndex &idx) {
        const QString path = idx.data(Qt::UserRole + 1).toString();
        if (path.isEmpty()) return;
        const QString red = replacing;
        endReplace();
        addEdit->clear();
        if (!red.isEmpty()) emit replaceRequested(red, {path});
        else emit addRequested(path);
    });

    /*  Return with nothing chosen from the popup resolves the text against the keyword
        list -- see commitTyped. It never CREATES a keyword: that is the Keyword list's
        job, so every keyword this field puts on a photograph is one the list holds. */
    connect(addEdit, &QLineEdit::returnPressed, this, &KeywordTags::commitTyped);

    rebuild();
}

QString KeywordTags::headerText() const
{
    return selectionSize == 1 ? tr("Selected image tags")
                              : tr("Selected images tags");
}

void KeywordTags::commitTyped()
{
/*
    Add what was typed -- but ONLY AS A KEYWORD ALREADY IN THE LIST. Keywords are created
    in the Keyword list and nowhere else; an earlier version took unmatched text literally
    and put a new root keyword on the images, which is how stray unfiled (red) keywords
    were born.

    Resolved in order: a full path ("Fauna|Bird|Heron"), then a name or synonym that
    names exactly ONE node. Two nodes sharing a name (the two Vancouvers) are ambiguous
    and the completer popup, which shows each one's branch, is how to choose. Anything
    else is refused with a message saying where keywords are made; the text stays so it
    can be corrected.
*/
    const QString typed = keywordNodes(addEdit->text()).join('|');
    if (typed.isEmpty()) return;

    QString path;
    const QModelIndex exact = vocab->indexForPath(typed);
    if (exact.isValid()) {
        path = exact.data(KeywordVocab::PathRole).toString();
    }
    else {
        const QString fold = keywordFold(typed);
        QStringList matches;
        for (const VocabNode *n : vocab->completions(typed, 0)) {
            bool hit = keywordFold(n->name) == fold;
            for (const QString &syn : n->synonyms)
                if (keywordFold(syn) == fold) hit = true;
            if (hit) matches << n->path;
        }
        if (matches.size() > 1) {
            G::popup->showPopup(QString("\"%1\" is in your keyword list %2 times. Pick "
                                        "the one you mean from the suggestions.")
                                    .arg(typed).arg(matches.size()), 3000);
            return;
        }
        if (matches.size() == 1) path = matches.first();
    }

    if (path.isEmpty()) {
        G::popup->showPopup(QString("\"%1\" is not in your keyword list. Create it in "
                                    "the Keyword list first, then add it here.")
                                .arg(typed), 3500);
        return;
    }
    const QString red = replacing;
    endReplace();
    addEdit->clear();
    if (!red.isEmpty()) emit replaceRequested(red, {path});
    else emit addRequested(path);
}

void KeywordTags::fillCompletions(const QList<const VocabNode *> &nodes)
{
    QStandardItemModel *m = qobject_cast<QStandardItemModel *>(completer->model());
    if (!m) return;
    m->clear();
    for (const VocabNode *n : nodes) {
        QStandardItem *item = new QStandardItem(n->name);
        item->setData(n->path, Qt::UserRole + 1);
        item->setData(keywordParentPath(n->path), Qt::UserRole + 2);
        m->appendRow(item);
    }
}

QList<const VocabNode *> KeywordTags::similarNodes(const QString &text) const
{
    QList<const VocabNode *> equal, nearMatch;
    const QString t = keywordFold(text);
    if (t.isEmpty()) return {};
    /*  The whole list, not KeywordVocab::completions: that matches PREFIXES, and a stray
        like "BackyardBirds" should still offer "Backyard". Short names (under 3) only
        count when equal, or "a" would resemble everything. */
    auto resembles = [&](const QString &name) {
        const QString f = keywordFold(name);
        if (f.isEmpty()) return 0;
        if (f == t) return 2;
        if (f.size() >= 3 && t.size() >= 3 && (f.contains(t) || t.contains(f))) return 1;
        return 0;
    };
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        for (int i = 0; i < vocab->rowCount(parent); ++i) {
            const QModelIndex idx = vocab->index(i, 0, parent);
            const QString path = idx.data(KeywordVocab::PathRole).toString();
            const VocabNode *n = KeywordVocab::nodeOf(idx);
            if (n && !path.isEmpty()) {
                int score = resembles(n->name);
                for (const QString &syn : n->synonyms)
                    score = qMax(score, resembles(syn));
                if (score == 2) equal << n;
                else if (score == 1) nearMatch << n;
            }
            walk(idx);
        }
    };
    walk(QModelIndex());
    auto byDepth = [](const VocabNode *a, const VocabNode *b) {
        const int da = a->path.count('|'), db = b->path.count('|');
        if (da != db) return da < db;
        return a->path.compare(b->path, Qt::CaseInsensitive) < 0;
    };
    std::stable_sort(equal.begin(), equal.end(), byDepth);
    std::stable_sort(nearMatch.begin(), nearMatch.end(), byDepth);
    return (equal + nearMatch).mid(0, 40);
}

void KeywordTags::startReplace(const QString &unfiledPath)
{
/*
    A red tag was clicked. Keywords are only CREATED in the Keyword list, so the click no
    longer files the stray into it; instead it offers the list keywords that look like it,
    in the Add field's own popup, and picking one replaces the red keyword on the
    selected images that carry it (the same swap as dropping a list keyword on the tag).

    Resemblance is judged on the LEAF -- "Location|Backyard" and a stray "BAckyard" are
    the same idea filed differently -- and, failing that, on each node of the stray's
    path from the deepest up, so "Places|Backyrd|Feeder" can still find "Feeder".
*/
    const QStringList nodes = keywordNodes(unfiledPath);
    if (nodes.isEmpty()) return;

    QList<const VocabNode *> found;
    QString shown = nodes.last();
    for (int i = nodes.size() - 1; i >= 0 && found.isEmpty(); --i) {
        found = similarNodes(nodes.at(i));
        if (!found.isEmpty()) shown = nodes.at(i);
    }
    if (found.isEmpty()) {
        G::popup->showPopup(QString("Nothing in your keyword list resembles \"%1\". "
                                    "Drop a keyword from the Keyword list on the red "
                                    "tag, or create one there first.")
                                .arg(unfiledPath), 4000);
        return;
    }

    replacing = unfiledPath;
    updateLegend();
    addEdit->setText(shown);
    addEdit->setFocus();
    fillCompletions(found);
    /*  An empty prefix shows EVERY row: the list was chosen by resemblance, not by what
        the field happens to say, and MatchContains on the leaf would hide "Backyard"
        from the text "BAckyardBirds". */
    completer->setCompletionPrefix(QString());
    completer->complete();
}

void KeywordTags::endReplace()
{
    if (replacing.isEmpty()) return;
    replacing.clear();
    updateLegend();
}

void KeywordTags::updateLegend()
{
    if (replacing.isEmpty()) { legend->setText(legendBase); return; }
    legend->setText(QString("Replacing <span style='color:%1'>%2</span> -- pick a "
                            "keyword from your list (clear the box to cancel)")
                        .arg(G::unfiledKeywordColor.name(), replacing.toHtmlEscaped()));
}

void KeywordTags::setSuggestions(const QVector<VisualAudit::Suggestion> &suggestions,
                                 const QString &status)
{
    QLayoutItem *item;
    while ((item = suggestFlow->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    const QString target = selectionSize > 1
        ? tr("Click to add to the %1 selected images.").arg(selectionSize)
        : tr("Click to add to this image.");
    for (const VisualAudit::Suggestion &s : suggestions) {
        SuggestChip *chip = new SuggestChip(
            s.keyword, s.keyword + "\n" + s.reason() + "\n\n" + target, suggestArea);
        chip->onAccept = [this](const QString &p) { emit suggestionAccepted(p); };
        chip->onDismiss = [this](const QString &p) { emit suggestionDismissed(p); };
        suggestFlow->addWidget(chip);
    }
    /*  The audit link is offered in the status whatever it says: it is how the visual
        half is learned the first time and re-learned once the keywords have moved on. */
    suggestStatus->setText(status.isEmpty() ? QString()
        : status.toHtmlEscaped() + QString("  <a href='audit'>%1</a>").arg(tr("Audit "
                                                                            "keywords...")));
}

void KeywordTags::setSelection(const QMap<QString, int> &counts, int selectionSize)
{
    /*  NOTHING TO DO IF NOTHING CHANGED, and it usually has not. fileSelectionChange
        fires when the CURRENT image moves, not only when the selection set does, so
        arrowing through a multi-selection delivers the same map over and over -- and
        rebuilding means destroying and recreating every tag widget each time. */
    if (selectionSize == this->selectionSize && counts == this->counts) return;

    /*  A replace belongs to the red tag it was started from, and survives a selection
        change only while the new selection still carries it -- filtering on the red
        keyword (a red tag click) usually trims the selection, and must not cancel the
        replace that same click started. */
    if (!replacing.isEmpty() && !counts.contains(replacing)) {
        replacing.clear();
        addEdit->clear();
    }
    this->counts = counts;
    this->selectionSize = selectionSize;
    rebuild();
}

void KeywordTags::rebuild()
{
    header->setText(headerText());

    QLayoutItem *item;
    while ((item = flow->takeAt(0))) {
        delete item->widget();
        delete item;
    }

    if (selectionSize == 0) {
        legendBase = "No images selected.";
        updateLegend();
        addEdit->setEnabled(false);
        return;
    }
    addEdit->setEnabled(true);

    /*  A CAP, because the union of a large selection's keywords is unbounded. Filtering
        a real library to one branch and selecting it selects images spanning HUNDREDS of
        distinct keywords -- 578 for one ordinary case -- and a tag apiece is a QFrame, a
        layout, a label and a button each, built and torn down on every selection change.
        That was slow enough to be felt while browsing and again on quit.

        FULL-COVERAGE KEYWORDS COME FIRST because they are the actionable ones: a keyword
        every selected image carries is one you might remove from all of them. A keyword
        on three images out of eight hundred is noise at that scale, and if it is cut off
        by the cap the count in the legend says so. */
    const int kMaxTags = 60;

    QStringList full, partialPaths;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.value() >= selectionSize) full << it.key();
        else partialPaths << it.key();
    }
    const QStringList ordered = full + partialPaths;
    const int shown = qMin(ordered.size(), kMaxTags);

    /*  WHAT A TAG IS CALLED, and the rule is about STABILITY before brevity.

        AN UNFILED TAG SHOWS ITS WHOLE PATH. A red (unfiled) tag is one the user is
        deciding about -- it matches nothing in their keyword list -- and its identity is
        the whole question, so hiding all but its last word is hiding the thing they came
        here to look at. "Buoy|Thing" drawn as "Thing" is not a short label, it is a
        different keyword.

        A FILED TAG SHOWS ITS LEAF, because the user filed it there and already knows
        where it lives; "Category|Aspect|16x10" is "16x10" to the person who put it under
        Aspect, and a panel of full paths would be unreadable at depth for no gain.

        THE LABEL DEPENDS ONLY ON THE TAG, NOT ON ITS NEIGHBOURS, and that is the point.
        An earlier version showed the full path only where two tags' leaves COLLIDED,
        which meant deleting "Dinghy|Thing" silently relabelled "Buoy|Thing" from
        "Buoy|Thing" back to "Thing" -- the user went looking for a chip that was on
        screen the whole time under a different name. A label that moves because of what
        else happens to be selected is worse than one that is merely short.

        THE ONE EXCEPTION IS TWO FILED TAGS THAT SHARE A LEAF -- the two Vancouvers, of
        which a real vocabulary has dozens. There the collision rule survives, instability
        and all, because the alternative is two identical chips each with a destructive x:
        a label that varies is a nuisance, and removing the wrong keyword from a thousand
        photographs is not. Unfiled tags cannot reach this case; they are already full. */
    QHash<QString, int> leafUses;
    for (int i = 0; i < shown; ++i)
        leafUses[keywordFold(keywordLeafOf(ordered.at(i)))]++;

    bool anyPartial = false, anyUnfiled = false;

    for (int i = 0; i < shown; ++i) {
        const QString path = ordered.at(i);
        const QString leaf = keywordLeafOf(path);
        const bool partial = counts.value(path) < selectionSize;
        const bool unfiled = !vocab->indexForPath(path).isValid();
        const bool collides = leafUses.value(keywordFold(leaf)) > 1;
        const QString label = (unfiled || collides) ? path : leaf;
        anyPartial |= partial;
        anyUnfiled |= unfiled;

        Tag *tag = new Tag(path, label, partial, unfiled, tagArea);
        tag->onRemove = [this](const QString &p) { emit removeRequested(p); };
        tag->onClick = [this, unfiled](const QString &p) {
            /*  A white (filed) tag SHOWS ITSELF in the Keyword list -- where it lives is
                the question a leaf-only label leaves open. A red one offers similar
                keywords from the list to REPLACE it with (via MW, which filters on it
                first); it is never added to the list from here, because keywords are
                created in the Keyword list only. Both filter the images on the tag. */
            if (unfiled) emit redTagClicked(p);
            else emit showRequested(p);
        };
        /*  Promoting a partial tag to the whole selection WRITES FILES, so it is a menu
            choice rather than the click -- a click that only meant "where is this?"
            must not rewrite a hundred sidecars. */
        if (partial)
            tag->onPromote = [this](const QString &p) { emit addRequested(p); };
        flow->addWidget(tag);
    }

    QStringList notes;
    if (selectionSize > 1)
        notes << QString("%1 images selected").arg(selectionSize);
    if (ordered.size() > shown)
        notes << QString("showing %1 of %2 keywords").arg(shown).arg(ordered.size());
    if (anyPartial) notes << "* on some (right-click to add to all)";
    if (anyUnfiled) {
        notes << QString("<span style='color:%1'>red</span> not in your keyword list "
                         "(click it, or drop a keyword on it, to replace it)")
                 .arg(G::unfiledKeywordColor.name());
    }
    legendBase = notes.join("   ·   ");
    updateLegend();
}

void KeywordTags::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(kVocabNodeMime) && selectionSize > 0)
        event->acceptProposedAction();
}

/*  The unfiled tag under pos (this widget's coordinates), or null. childAt finds the
    innermost widget -- the tag's label or its x -- so walk up to the tag itself. */
static Tag *unfiledTagAt(QWidget *tagArea, QWidget *from, const QPoint &pos)
{
    QWidget *w = tagArea->childAt(tagArea->mapFrom(from, pos));
    while (w && w != tagArea) {
        if (Tag *t = dynamic_cast<Tag *>(w)) return t->unfiled ? t : nullptr;
        w = w->parentWidget();
    }
    return nullptr;
}

void KeywordTags::setDropTarget(QWidget *tag)
{
    if (dropTarget == tag) return;
    if (dropTarget) static_cast<Tag *>(dropTarget.data())->setDropTarget(false);
    dropTarget = tag;
    if (dropTarget) static_cast<Tag *>(dropTarget.data())->setDropTarget(true);
}

void KeywordTags::dragMoveEvent(QDragMoveEvent *event)
{
    if (!event->mimeData()->hasFormat(kVocabNodeMime) || selectionSize == 0) return;
    setDropTarget(unfiledTagAt(tagArea, this, event->position().toPoint()));
    event->acceptProposedAction();
}

void KeywordTags::dragLeaveEvent(QDragLeaveEvent *)
{
    setDropTarget(nullptr);
}

void KeywordTags::dropEvent(QDropEvent *event)
{
    setDropTarget(nullptr);
    const QByteArray data = event->mimeData()->data(kVocabNodeMime);
    if (data.isEmpty()) return;

    /*  ONE SIGNAL FOR THE WHOLE DROP -- see addManyRequested. */
    QStringList paths;
    for (const QByteArray &p : data.split('\n')) {
        const QString path = QString::fromUtf8(p);
        if (!path.isEmpty()) paths << path;
    }
    if (paths.isEmpty()) return;

    /*  DROPPED ON AN UNFILED (RED) TAG: the dragged keyword is the correct one, so it
        REPLACES the red one -- on the images that carry it, not the whole selection. */
    if (Tag *t = unfiledTagAt(tagArea, this, event->position().toPoint()))
        emit replaceRequested(t->path, paths);
    else emit addManyRequested(paths);
    event->acceptProposedAction();
}
