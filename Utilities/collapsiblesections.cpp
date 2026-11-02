#include "Utilities/collapsiblesections.h"
#include "Main/global.h"

#include <QtWidgets>

CollapsibleSections::CollapsibleSections(QWidget *owner, const QString &settingsGroup)
    : QObject(owner), owner(owner), settingsGroup(settingsGroup)
{
}

int CollapsibleSections::add(QGroupBox *box, QBoxLayout *lay, const QString &key,
                             Summary summary)
{
    Section s;
    s.key = key;
    s.title = box->title();
    s.box = box;
    s.summary = std::move(summary);

    /* The box shows a pointing hand over its title. Its children get an arrow of their
       own, because the box gets no Leave when the mouse moves on into a child, and a
       child with no cursor of its own would keep showing the hand. */
    box->setMouseTracking(true);
    box->installEventFilter(this);
    const QList<QWidget *> kids = box->findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
    for (QWidget *c : kids)
        if (!c->testAttribute(Qt::WA_SetCursor)) c->setCursor(Qt::ArrowCursor);

    s.closedRow = new QWidget(owner);
    QHBoxLayout *row = new QHBoxLayout(s.closedRow);
    row->setContentsMargins(0, 2, 10, 2);
    s.closedBtn = new QToolButton(s.closedRow);
    s.closedBtn->setFont(box->font());
    s.closedBtn->setAutoRaise(true);
    s.closedBtn->setStyleSheet("QToolButton { border: none; padding: 0px; }");
    s.closedBtn->setCursor(Qt::PointingHandCursor);
    s.closedBtn->setToolTip(tr("Click to expand.\nRight click to expand all / collapse all."));
    s.closedBtn->setText(QString(QChar(0x25B8)) + " " + s.title);
    /* The button keeps its own width (a QToolButton centres its text, whatever the
       sheet says) at the left of a cell restore() widens to the longest title. */
    s.titleCell = new QWidget(s.closedRow);
    QHBoxLayout *cell = new QHBoxLayout(s.titleCell);
    cell->setContentsMargins(0, 0, 0, 0);
    cell->addWidget(s.closedBtn);
    cell->addStretch(1);
    s.summaryLab = new QLabel(s.closedRow);
    s.summaryLab->setStyleSheet("QLabel { color:" + G::disabledColor.name() + "; }");
    /* A long summary (a folder path) is clipped, not allowed to widen the dialog. */
    s.summaryLab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    row->addWidget(s.titleCell);
    row->addSpacing(12);
    row->addWidget(s.summaryLab, 1);
    s.closedRow->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(s.closedRow, &QWidget::customContextMenuRequested, this,
            [this, w = s.closedRow](const QPoint &p) { menu(w->mapToGlobal(p)); });

    const int at = lay->indexOf(box);
    lay->insertWidget(at < 0 ? lay->count() : at + 1, s.closedRow);
    s.closedRow->hide();

    const int i = int(sections.size());
    connect(s.closedBtn, &QToolButton::clicked, this, [this, i]() { setOpen(i, true); });
    sections.append(s);
    apply(i);
    return i;
}

void CollapsibleSections::restore()
{
    int w = 0;
    for (int i = 0; i < sections.size(); ++i) {
        Section &s = sections[i];
        s.open = G::settings
            ? !G::settings->value(settingsGroup + "/" + s.key, true).toBool()
            : false;
        apply(i);
        s.closedBtn->ensurePolished();
        w = qMax(w, s.closedBtn->sizeHint().width());
    }
    for (const Section &s : std::as_const(sections)) s.titleCell->setFixedWidth(w);
    updateSummaries();
}

int CollapsibleSections::indexOf(const QString &key) const
{
    for (int i = 0; i < sections.size(); ++i)
        if (sections.at(i).key == key) return i;
    return -1;
}

bool CollapsibleSections::isOpen(int i) const
{
    return i >= 0 && i < sections.size() && sections.at(i).open;
}

bool CollapsibleSections::anyOpen() const
{
    for (const Section &s : sections) if (s.open) return true;
    return false;
}

bool CollapsibleSections::anyClosed() const
{
    for (const Section &s : sections) if (!s.open) return true;
    return false;
}

void CollapsibleSections::apply(int i)
{
    /* The chevrons as code points (U+25BE open, U+25B8 closed), not literals: a
       non-ASCII narrow literal depends on the compiler's execution charset. */
    Section &s = sections[i];
    s.box->setTitle(QString(QChar(0x25BE)) + " " + s.title);
    s.box->setVisible(s.open);
    s.closedRow->setVisible(!s.open);
}

void CollapsibleSections::setOpen(int i, bool open, bool save)
{
    if (i < 0 || i >= sections.size()) return;
    sections[i].open = open;
    apply(i);
    if (!save) return;
    if (G::settings) G::settings->setValue(settingsGroup + "/" + sections.at(i).key, !open);
    if (onChanged) onChanged();
}

void CollapsibleSections::setAllOpen(bool open)
{
    for (int i = 0; i < sections.size(); ++i) {
        sections[i].open = open;
        apply(i);
        if (G::settings)
            G::settings->setValue(settingsGroup + "/" + sections.at(i).key, !open);
    }
    if (onChanged) onChanged();
}

void CollapsibleSections::updateSummaries()
{
    for (const Section &s : std::as_const(sections))
        if (s.summary) s.summaryLab->setText(s.summary());
}

void CollapsibleSections::menu(const QPoint &globalPos)
{
    QMenu m(owner);
    m.addAction(tr("Expand all"), this, [this]() { setAllOpen(true); });
    m.addAction(tr("Collapse all"), this, [this]() { setAllOpen(false); });
    m.exec(globalPos);
}

bool CollapsibleSections::onTitle(QGroupBox *box, const QPoint &pos)
{
/*
    The band above the box's content: its layout's top margin, or for a box whose
    children are placed by hand (IngestDlg's .ui), the top of the highest child.
*/
    int top = box->height();
    if (box->layout()) top = box->layout()->contentsMargins().top();
    else {
        const QList<QWidget *> kids =
            box->findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
        for (QWidget *c : kids) if (c->isVisible()) top = qMin(top, c->y());
    }
    const QFontMetrics fm(box->font());
    return pos.y() < top && pos.x() < fm.horizontalAdvance(box->title()) + 2 * fm.height();
}

bool CollapsibleSections::eventFilter(QObject *obj, QEvent *event)
{
    for (int i = 0; i < sections.size(); ++i) {
        QGroupBox *box = sections.at(i).box;
        if (obj != box) continue;
        switch (event->type()) {
        case QEvent::MouseMove: {
            const QPoint p = static_cast<QMouseEvent *>(event)->position().toPoint();
            if (onTitle(box, p)) box->setCursor(Qt::PointingHandCursor);
            else box->unsetCursor();
            break;
        }
        case QEvent::Leave:
            box->unsetCursor();
            break;
        case QEvent::MouseButtonRelease: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && onTitle(box, me->position().toPoint())) {
                setOpen(i, false);
                return true;
            }
            break;
        }
        case QEvent::ContextMenu: {
            auto *ce = static_cast<QContextMenuEvent *>(event);
            if (onTitle(box, ce->pos())) {
                menu(ce->globalPos());
                return true;
            }
            break;
        }
        case QEvent::ToolTip: {
            auto *he = static_cast<QHelpEvent *>(event);
            if (onTitle(box, he->pos()))
                QToolTip::showText(he->globalPos(),
                                   tr("Click to collapse.\n"
                                      "Right click to expand all / collapse all."), box);
            else
                QToolTip::hideText();
            return true;
        }
        default:
            break;
        }
        break;
    }
    return QObject::eventFilter(obj, event);
}
