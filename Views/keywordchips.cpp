#include "Views/keywordchips.h"

#include "Datamodel/keywordvocab.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"
#include "Utilities/flowlayout.h"

#include <QCompleter>
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

/*
    See keywordchips.h. The mime type is shared with Views/keywordtree.cpp.
*/
const char *kVocabNodeMime = "application/x-winnow-vocab-node";

namespace {

/*  One keyword on the selection. A widget rather than a styled QPushButton because it has
    two hit targets that mean different things -- the label (promote to all) and the x
    (remove from all) -- and one button cannot offer both. */
class Chip : public QFrame
{
public:
    Chip(const QString &path, const QString &label, bool partial, bool unfiled,
         QWidget *parent)
        : QFrame(parent), path(path)
    {
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::PointingHandCursor);

        QHBoxLayout *l = new QHBoxLayout(this);
        l->setContentsMargins(6, 1, 2, 1);
        l->setSpacing(4);

        QString text = label;
        if (partial) text += " *";
        if (unfiled) text.prepend("? ");
        QLabel *name = new QLabel(text, this);
        /*  The full path in the tooltip is how two chips showing the same leaf are told
            apart -- and they will, because that is the case path identity exists for. */
        name->setToolTip(path);
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

protected:
    void mouseReleaseEvent(QMouseEvent *) override
    {
        if (onClick) onClick(path);
    }

private:
    QString path;
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

KeywordChips::KeywordChips(KeywordVocab *vocab, QWidget *parent)
    : QWidget(parent), vocab(vocab)
{
    setAcceptDrops(true);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(3);

    /*  Scrolled, because an image can carry twenty keywords and the dock is not tall.
        widgetResizable so the flow layout is asked for its height at the real width. */
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    chipArea = new QWidget(scroll);
    flow = new FlowLayout(chipArea, 2, 4, 3);
    chipArea->setLayout(flow);
    scroll->setWidget(chipArea);
    outer->addWidget(scroll, 1);

    addEdit = new QLineEdit(this);
    addEdit->setPlaceholderText("Add keyword");
    addEdit->setClearButtonEnabled(true);
    outer->addWidget(addEdit);

    legend = new QLabel(this);
    legend->setEnabled(false);
    outer->addWidget(legend);

    QCompleter *completer = new QCompleter(this);
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
    connect(addEdit, &QLineEdit::textEdited, this, [this, completer](const QString &t) {
        QStandardItemModel *m = qobject_cast<QStandardItemModel *>(completer->model());
        if (!m) return;
        m->clear();
        if (t.trimmed().isEmpty()) return;
        for (const VocabNode *n : this->vocab->completions(t.trimmed())) {
            QStandardItem *item = new QStandardItem(n->name);
            item->setData(n->path, Qt::UserRole + 1);
            item->setData(keywordParentPath(n->path), Qt::UserRole + 2);
            m->appendRow(item);
        }
    });

    connect(completer, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, [this](const QModelIndex &idx) {
        const QString path = idx.data(Qt::UserRole + 1).toString();
        if (path.isEmpty()) return;
        addEdit->clear();
        emit addRequested(path);
    });

    /*  Return with nothing chosen from the popup takes the text literally, which is how
        a keyword that is not in the vocabulary yet gets added. It becomes a root; the
        user files it from the tree or by dragging. */
    connect(addEdit, &QLineEdit::returnPressed, this, &KeywordChips::commitTyped);

    rebuild();
}

void KeywordChips::commitTyped()
{
    const QString typed = keywordNodes(addEdit->text()).join('|');
    if (typed.isEmpty()) return;
    addEdit->clear();
    emit addRequested(typed);
}

void KeywordChips::setSelection(const QMap<QString, int> &counts, int selectionSize)
{
    /*  NOTHING TO DO IF NOTHING CHANGED, and it usually has not. fileSelectionChange
        fires when the CURRENT image moves, not only when the selection set does, so
        arrowing through a multi-selection delivers the same map over and over -- and
        rebuilding means destroying and recreating every chip widget each time. */
    if (selectionSize == this->selectionSize && counts == this->counts) return;

    this->counts = counts;
    this->selectionSize = selectionSize;
    rebuild();
}

void KeywordChips::rebuild()
{
    QLayoutItem *item;
    while ((item = flow->takeAt(0))) {
        delete item->widget();
        delete item;
    }

    if (selectionSize == 0) {
        legend->setText("No images selected.");
        addEdit->setEnabled(false);
        return;
    }
    addEdit->setEnabled(true);

    /*  A CAP, because the union of a large selection's keywords is unbounded. Filtering
        a real library to one branch and selecting it selects images spanning HUNDREDS of
        distinct keywords -- 578 for one ordinary case -- and a chip apiece is a QFrame, a
        layout, a label and a button each, built and torn down on every selection change.
        That was slow enough to be felt while browsing and again on quit.

        FULL-COVERAGE KEYWORDS COME FIRST because they are the actionable ones: a keyword
        every selected image carries is one you might remove from all of them. A keyword
        on three images out of eight hundred is noise at that scale, and if it is cut off
        by the cap the count in the legend says so. */
    const int kMaxChips = 60;

    QStringList full, partialPaths;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.value() >= selectionSize) full << it.key();
        else partialPaths << it.key();
    }
    const QStringList ordered = full + partialPaths;
    const int shown = qMin(ordered.size(), kMaxChips);

    bool anyPartial = false, anyUnfiled = false;

    for (int i = 0; i < shown; ++i) {
        const QString path = ordered.at(i);
        const bool partial = counts.value(path) < selectionSize;
        const bool unfiled = !vocab->indexForPath(path).isValid();
        anyPartial |= partial;
        anyUnfiled |= unfiled;

        Chip *chip = new Chip(path, keywordLeafOf(path), partial, unfiled, chipArea);
        chip->onRemove = [this](const QString &p) { emit removeRequested(p); };
        chip->onClick = [this, partial, unfiled](const QString &p) {
            /*  A partial chip promotes to the whole selection -- the obvious meaning of
                clicking "some of these have it". An unfiled one offers the tree instead,
                because filing it is the only thing that changes anything. */
            if (partial) emit addRequested(p);
            else if (unfiled) emit fileRequested(p);
        };
        flow->addWidget(chip);
    }

    QStringList notes;
    if (selectionSize > 1)
        notes << QString("%1 images selected").arg(selectionSize);
    if (ordered.size() > shown)
        notes << QString("showing %1 of %2 keywords").arg(shown).arg(ordered.size());
    if (anyPartial) notes << "* on some";
    if (anyUnfiled) notes << "? not in your keyword list";
    legend->setText(notes.join("   ·   "));
}

void KeywordChips::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(kVocabNodeMime) && selectionSize > 0)
        event->acceptProposedAction();
}

void KeywordChips::dropEvent(QDropEvent *event)
{
    const QByteArray data = event->mimeData()->data(kVocabNodeMime);
    if (data.isEmpty()) return;
    for (const QByteArray &p : data.split('\n')) {
        const QString path = QString::fromUtf8(p);
        if (!path.isEmpty()) emit addRequested(path);
    }
    event->acceptProposedAction();
}
