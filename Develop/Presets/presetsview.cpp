#include "Develop/Presets/presetsview.h"
#include "Develop/Presets/developpresets.h"
#include "Main/global.h"

#include <QFontMetrics>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <climits>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QTimer>

/*
    PresetsView / PresetDelegate (see presetsview.h): the Develop Presets dock's list of
    user presets, grouped under header categories. Hover previews a preset on the loupe,
    a click applies it -- the same interaction as the History dock, because they are the
    same kind of list. Clicking a header folds it; dragging reorders presets and
    categories.
*/

/* ------------------------------------------------------------------------------------
   PresetDelegate
   ------------------------------------------------------------------------------------ */

namespace {
constexpr int kHeaderIndent = 6;    // header arrow from the left edge
constexpr int kPresetIndent = 22;   // preset text, lined up under the header name
}

PresetDelegate::PresetDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void PresetDelegate::setHoveredRow(int row)
{
    hoveredRow = row;
}

void PresetDelegate::setDropHeaderRow(int row)
{
    dropHeaderRow = row;
}

void PresetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QString text = index.data(Qt::DisplayRole).toString();
    const int kind = index.data(KindRole).toInt();
    const QRect rect = option.rect;

    if (kind == Header) {
        /* A band fill so the categories read as sections, a disclosure arrow, the name
           in bold and the preset count dimmed at the right. */
        const int b = G::backgroundShade + 10;
        painter->fillRect(rect, QColor(b, b, b));
        if (index.row() == dropHeaderRow) {
            painter->setPen(QPen(G::textColor, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5));
        }

        const bool collapsed = index.data(CollapsedRole).toBool();
        const QPointF c(rect.left() + kHeaderIndent + 4, rect.center().y() + 0.5);
        QPolygonF arrow;
        if (collapsed) arrow << c + QPointF(-2, -4) << c + QPointF(-2, 4) << c + QPointF(3, 0);
        else           arrow << c + QPointF(-4, -2) << c + QPointF(4, -2) << c + QPointF(0, 3);
        painter->setPen(Qt::NoPen);
        painter->setBrush(G::textColor);
        painter->drawPolygon(arrow);

        QFont f = option.font;
        f.setBold(true);
        const QFontMetrics fm(f);
        const QString count = QString::number(index.data(CountRole).toInt());
        const QFontMetrics cfm(option.font);
        const int cw = cfm.horizontalAdvance(count);
        const QRect cr(rect.right() - 8 - cw, rect.top(), cw, rect.height());
        painter->setFont(option.font);
        painter->setPen(G::disabledColor);
        painter->drawText(cr, Qt::AlignRight | Qt::AlignVCenter, count);

        const QRect nameRect = rect.adjusted(kPresetIndent - 6, 0, -(cw + 16), 0);
        painter->setFont(f);
        painter->setPen(G::textColor);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(text, Qt::ElideRight, nameRect.width()));
        painter->restore();
        return;
    }

    const bool isPreset = (kind == Preset);
    /* A hovered row is lightened because hovering is live -- the loupe is showing that
       preset applied to the image. */
    if (isPreset && index.row() == hoveredRow) {
        const int b = G::backgroundShade + 20;
        painter->fillRect(rect, QColor(b, b, b));
    }

    const QFontMetrics fm(option.font);
    const QRect r = rect.adjusted(kPresetIndent, 0, -8, 0);
    painter->setPen(isPreset ? G::textColor : G::disabledColor);
    painter->drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(text, Qt::ElideRight, r.width()));

    painter->restore();
}

QSize PresetDelegate::sizeHint(const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(QFontMetrics(option.font).height() + 8);
    return s;
}

/* ------------------------------------------------------------------------------------
   PresetsView
   ------------------------------------------------------------------------------------ */

PresetsView::PresetsView(QWidget *parent) : QListWidget(parent)
{
    if (G::isLogger) G::log("PresetsView::PresetsView");

    delegate = new PresetDelegate(this);
    setItemDelegate(delegate);
    setSelectionMode(QAbstractItemView::NoSelection);   // clicking APPLIES, not selects
    setFocusPolicy(Qt::NoFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setUniformItemSizes(true);
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &PresetsView::showMenu);

    /* Debounce the hover: a fast sweep down the list must not queue one develop render
       per row. Only a settled hover previews. */
    hoverTimer = new QTimer(this);
    hoverTimer->setSingleShot(true);
    connect(hoverTimer, &QTimer::timeout, this, [this]{
        if (dragging) return;
        const QString name = nameForRow(hoverRow);
        if (name.isEmpty()) return;
        hoverEmitted = true;
        emit presetHovered(name);
    });
}

void PresetsView::bindPresets(DevelopPresets *p)
{
    presets = p;
    if (!presets) return;
    connect(presets, &DevelopPresets::changed, this, &PresetsView::refresh);
    refresh();
}

void PresetsView::refresh()
{
/*
    Rebuild from the store's layout: one header row per category, then its presets,
    hidden while the header is folded. The scroll position is kept -- this runs after
    every drag, and jumping to the top under the user's cursor would be disorienting.
*/
    const int scroll = verticalScrollBar()->value();
    clear();
    hoverRow = -1;
    delegate->setHoveredRow(-1);
    if (!presets) return;

    const QVector<PresetCategory> cats = presets->layout();
    bool any = false;
    for (const PresetCategory &c : cats) any = any || !c.presets.isEmpty();

    for (const PresetCategory &c : cats) {
        QListWidgetItem *h = new QListWidgetItem(c.name, this);
        h->setData(PresetDelegate::KindRole, PresetDelegate::Header);
        h->setData(PresetDelegate::CategoryRole, c.name);
        h->setData(PresetDelegate::CollapsedRole, c.collapsed);
        h->setData(PresetDelegate::CountRole, int(c.presets.size()));
        h->setToolTip(tr("Click to fold or unfold \"%1\". Drag to reorder categories; "
                         "right-click to rename or delete.").arg(c.name));

        /* Say how to make one rather than showing an empty box -- the [+] button in
           the band is otherwise easy to miss. Only when there are no presets at all,
           under the permanent category. */
        if (!any && c.name == DevelopPresets::kWinnowCategory) {
            QListWidgetItem *item =
                new QListWidgetItem(tr("No presets yet - click + to create one"), this);
            item->setData(PresetDelegate::KindRole, PresetDelegate::Placeholder);
            item->setData(PresetDelegate::CategoryRole, c.name);
            setRowHidden(row(item), c.collapsed);
        }

        for (const QString &name : c.presets) {
            QListWidgetItem *item = new QListWidgetItem(name, this);
            item->setData(PresetDelegate::KindRole, PresetDelegate::Preset);
            item->setData(PresetDelegate::NameRole, name);
            item->setData(PresetDelegate::CategoryRole, c.name);
            const QString scope = presets->read(name).sourceScope;
            item->setToolTip(scope.isEmpty()
                ? tr("Apply \"%1\" to this image. Drag to reorder.").arg(name)
                : tr("Apply \"%1\" to this image (saved from %2). Drag to reorder.")
                      .arg(name, scope));
            setRowHidden(row(item), c.collapsed);
        }
    }
    verticalScrollBar()->setValue(scroll);
}

int PresetsView::kindForRow(int row) const
{
    const QListWidgetItem *it = item(row);
    return it ? it->data(PresetDelegate::KindRole).toInt() : -1;
}

QString PresetsView::nameForRow(int row) const
{
    const QListWidgetItem *it = item(row);
    if (!it || it->data(PresetDelegate::KindRole).toInt() != PresetDelegate::Preset)
        return QString();
    return it->data(PresetDelegate::NameRole).toString();
}

QString PresetsView::categoryForRow(int row) const
{
    const QListWidgetItem *it = item(row);
    return it ? it->data(PresetDelegate::CategoryRole).toString() : QString();
}

int PresetsView::lastVisibleRowOf(const QString &category) const
{
    int last = -1;
    for (int r = 0; r < count(); ++r)
        if (categoryForRow(r) == category && !isRowHidden(r)) last = r;
    return last;
}

int PresetsView::positionInCategory(int row) const
{
    int pos = 0;
    for (int r = row - 1; r >= 0 && kindForRow(r) != PresetDelegate::Header; --r)
        if (kindForRow(r) == PresetDelegate::Preset) ++pos;
    return pos;
}

void PresetsView::toggleCollapsed(int headerRow)
{
/*
    Fold / unfold in place (no rebuild): hide the rows under the header, then tell the
    store so the state survives a restart.
*/
    QListWidgetItem *h = item(headerRow);
    if (!h) return;
    const bool collapsed = !h->data(PresetDelegate::CollapsedRole).toBool();
    h->setData(PresetDelegate::CollapsedRole, collapsed);
    for (int r = headerRow + 1; r < count() && kindForRow(r) != PresetDelegate::Header; ++r)
        setRowHidden(r, collapsed);
    if (presets) presets->setCategoryCollapsed(categoryForRow(headerRow), collapsed);
}

void PresetsView::endHoverPreview()
{
    if (!hoverEmitted) return;
    hoverEmitted = false;
    emit hoverEnded();              // put the real state back on the loupe
}

void PresetsView::setHover(int row)
{
    if (row == hoverRow) return;
    hoverRow = row;
    delegate->setHoveredRow(row);
    viewport()->update();

    /* Only a preset row previews. Landing anywhere else -- a header, the blank space
       below the last preset, or the empty-list placeholder -- must take the preview off
       the loupe here: that space is still inside the list, so no leaveEvent is coming. */
    if (nameForRow(row).isEmpty()) {
        hoverTimer->stop();
        endHoverPreview();
    }
    else {
        hoverTimer->start(kHoverDelayMs);
    }
}

/* ---- Drag to reorder ---------------------------------------------------------------- */

PresetsView::DropTarget PresetsView::dropTargetAt(const QPoint &pos) const
{
/*
    Translate a cursor position into where the dragged row would land.

    Dragging a PRESET:
      over a header           -> top of that category (the header is outlined)
      upper / lower half of a preset -> before / after it, in its category
      over the placeholder    -> top of its category
      below the last row      -> end of the last category
    Dragging a HEADER (a whole category):
      upper half of a header  -> before that category
      anything else           -> after the category the row belongs to
*/
    DropTarget t;
    if (pressRow < 0 || count() == 0 || !viewport()->rect().contains(pos)) return t;
    const bool draggingHeader = (kindForRow(pressRow) == PresetDelegate::Header);
    const QStringList cats = presets ? presets->categoryNames() : QStringList();

    int r = indexAt(pos).row();
    bool below = false;
    if (r < 0) {                    // blank space under the last row
        for (int i = count() - 1; i >= 0; --i)
            if (!isRowHidden(i)) { r = i; break; }
        if (r < 0) return t;
        below = true;
    }
    const QRect rr = visualRect(model()->index(r, 0));
    const bool upper = !below && pos.y() < rr.center().y();
    const int kind = kindForRow(r);
    const QString cat = categoryForRow(r);

    if (draggingHeader) {
        if (kind == PresetDelegate::Header && upper) {
            t.catIndex = cats.indexOf(cat);
            t.lineY = rr.top();
        }
        else {
            t.catIndex = cats.indexOf(cat) + 1;
            t.lineY = visualRect(model()->index(lastVisibleRowOf(cat), 0)).bottom() + 1;
        }
        t.category = categoryForRow(pressRow);
        t.valid = t.catIndex >= 0;
        return t;
    }

    t.category = cat;
    if (below) {
        t.index = presets ? int(presets->layout().value(cats.indexOf(cat)).presets.size()) : 0;
        t.lineY = rr.bottom() + 1;
    }
    else if (kind == PresetDelegate::Header || kind == PresetDelegate::Placeholder) {
        t.index = 0;
        if (kind == PresetDelegate::Header) t.headerRow = r;
        else t.lineY = rr.top();
    }
    else {
        t.index = positionInCategory(r) + (upper ? 0 : 1);
        t.lineY = upper ? rr.top() : rr.bottom() + 1;
    }
    t.valid = true;
    return t;
}

void PresetsView::performDrop(const DropTarget &t)
{
    if (!presets || !t.valid) return;
    /* Deferred: the move emits changed(), which rebuilds this list from inside its own
       mouse handler. */
    if (kindForRow(pressRow) == PresetDelegate::Header) {
        const QString cat = t.category;
        const int to = t.catIndex;
        QTimer::singleShot(0, this, [this, cat, to]{ presets->moveCategory(cat, to); });
    }
    else {
        const QString name = nameForRow(pressRow);
        if (name.isEmpty()) return;
        const QString cat = t.category;
        const int to = t.index;
        QTimer::singleShot(0, this, [this, name, cat, to]{
            presets->movePreset(name, cat, to);
        });
    }
}

void PresetsView::endDrag()
{
    dragging = false;
    pressRow = -1;
    drop = DropTarget();
    delegate->setDropHeaderRow(-1);
    unsetCursor();
    viewport()->update();
}

void PresetsView::paintEvent(QPaintEvent *event)
{
    QListWidget::paintEvent(event);
    if (!dragging || !drop.valid || drop.headerRow >= 0) return;
    /* The insertion line, inset to the level it lands at: category gaps span the full
       width, preset gaps start under the header name. */
    QPainter p(viewport());
    const bool header = (kindForRow(pressRow) == PresetDelegate::Header);
    const int x0 = header ? 0 : kPresetIndent - 4;
    const int y = qBound(1, drop.lineY, viewport()->height() - 2);
    p.setPen(QPen(G::textColor, 2));
    p.drawLine(x0, y, viewport()->width() - 4, y);
}

void PresetsView::mousePressEvent(QMouseEvent *event)
{
    QListWidget::mousePressEvent(event);
    if (event->button() != Qt::LeftButton) return;
    const int r = indexAt(event->pos()).row();
    const int kind = kindForRow(r);
    pressRow = (kind == PresetDelegate::Header || kind == PresetDelegate::Preset) ? r : -1;
    pressPos = event->pos();
    dragging = false;
}

void PresetsView::mouseMoveEvent(QMouseEvent *event)
{
    if (pressRow >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!dragging &&
            (event->pos() - pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            /* A drag is starting: take any hover preview off the loupe -- the user is
               organising, not trying presets on. */
            dragging = true;
            hoverTimer->stop();
            setHover(-1);
            endHoverPreview();
            setCursor(Qt::ClosedHandCursor);
        }
        if (dragging) {
            /* Nudge the list when the cursor nears an edge, so a long list can be
               crossed without letting go. */
            const int edge = 16;
            QScrollBar *sb = verticalScrollBar();
            if (event->pos().y() < edge) sb->setValue(sb->value() - sb->singleStep());
            else if (event->pos().y() > viewport()->height() - edge)
                sb->setValue(sb->value() + sb->singleStep());
            drop = dropTargetAt(event->pos());
            delegate->setDropHeaderRow(drop.valid ? drop.headerRow : -1);
            viewport()->update();
            return;
        }
    }
    setHover(indexAt(event->pos()).row());
    QListWidget::mouseMoveEvent(event);
}

void PresetsView::leaveEvent(QEvent *event)
{
    setHover(-1);                   // ends any live preview
    QListWidget::leaveEvent(event);
}

void PresetsView::mouseReleaseEvent(QMouseEvent *event)
{
    const int r = indexAt(event->pos()).row();
    QListWidget::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton) return;

    if (dragging) {
        /* Released outside the list = cancelled (dropTargetAt is invalid there). */
        performDrop(dropTargetAt(event->pos()));
        endDrag();
        return;
    }
    const int pressed = pressRow;
    pressRow = -1;
    if (r < 0 || r != pressed) return;

    if (kindForRow(r) == PresetDelegate::Header) {
        toggleCollapsed(r);
        return;
    }
    const QString name = nameForRow(r);
    if (name.isEmpty()) return;
    /* End the hover preview first: applying the preset makes it the real state. Deferred
       a tick because applying rebuilds the Develop tree from inside this handler. */
    hoverEmitted = false;
    QTimer::singleShot(0, this, [this, name]{ emit presetChosen(name); });
}

/* ---- Menus -------------------------------------------------------------------------- */

void PresetsView::showMenu(const QPoint &pos)
{
/*
    Manage presets and categories (Lightroom's preset context menu). The row actions
    appear only over a real preset or header.
*/
    runMenu(mapToGlobal(pos), indexAt(pos).row(), false);
}

void PresetsView::showSectionMenu(const QPoint &globalPos)
{
/*
    The same menu, raised from the Presets band's [:] rather than from a row, so it acts
    on the SELECTED row -- there is no row under the cursor to name one.
*/
    runMenu(globalPos, currentRow(), true);
}

QString PresetsView::askCategoryName(const QString &title, const QString &current)
{
/*
    Prompt for a category name and validate it against the existing ones. Returns empty
    when cancelled or refused (the refusal is explained).
*/
    bool ok = false;
    const QString name = QInputDialog::getText(this, title, tr("Category name:"),
                                               QLineEdit::Normal, current, &ok).trimmed();
    if (!ok || name.isEmpty() || name == current) return QString();
    if (presets && presets->hasCategory(name) &&
        name.compare(current, Qt::CaseInsensitive) != 0) {
        QMessageBox::information(this, title,
            tr("A category named \"%1\" already exists.").arg(name));
        return QString();
    }
    return name;
}

void PresetsView::runMenu(const QPoint &globalPos, int row, bool includeHelp)
{
/*
    The prompts live here so the caller receives a decided action, never a "maybe" -- the
    same division of labour as SaveDevelopPresetDlg, which owns its own overwrite
    confirmation. Category actions go straight to the store (see the class comment).
*/
    /* Hovering the row that opened the menu left a preview on the loupe; the menu grabs
       the mouse, so no leaveEvent will arrive to undo it. */
    setHover(-1);

    const int kind = kindForRow(row);
    const QString name = nameForRow(row);
    const QString category = categoryForRow(row);
    const bool isHeader = (kind == PresetDelegate::Header);
    const bool isWinnow = (category == DevelopPresets::kWinnowCategory);

    QMenu menu(this);
    QAction *aNew    = menu.addAction(isHeader ? tr("New Preset in \"%1\"...").arg(category)
                                               : tr("New Preset..."));
    QAction *aNewCat = menu.addAction(tr("New Category..."));
    QAction *aUpdate = nullptr;
    QAction *aRename = nullptr;
    QAction *aDelete = nullptr;
    QAction *aRenCat = nullptr;
    QAction *aDelCat = nullptr;
    QMenu   *moveMenu = nullptr;
    if (!name.isEmpty()) {
        menu.addSeparator();
        aUpdate = menu.addAction(tr("Update with Current Settings"));
        aRename = menu.addAction(tr("Rename..."));
        /* Drag does the same; the submenu makes it discoverable. */
        moveMenu = menu.addMenu(tr("Move to Category"));
        if (presets) {
            for (const QString &c : presets->categoryNames()) {
                QAction *a = moveMenu->addAction(c);
                a->setData(c);
                a->setEnabled(c != category);
            }
        }
        aDelete = menu.addAction(tr("Delete"));
    }
    else if (isHeader) {
        menu.addSeparator();
        /* Winnow is permanent: say so in the item rather than hiding it. */
        aRenCat = menu.addAction(isWinnow ? tr("Rename Category (Winnow is permanent)")
                                          : tr("Rename Category..."));
        aDelCat = menu.addAction(isWinnow ? tr("Delete Category (Winnow is permanent)")
                                          : tr("Delete Category..."));
        aRenCat->setEnabled(!isWinnow);
        aDelCat->setEnabled(!isWinnow);
    }

    /* Help is always the last item, as it is on every section band in the Develop dock. */
    QAction *aHelp = nullptr;
    if (includeHelp) {
        menu.addSeparator();
        aHelp = menu.addAction(tr("Presets help"));
    }

    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == aHelp) {
        emit helpRequested();
        return;
    }

    if (chosen == aNew) {
        emit newPresetRequested(isHeader ? category : QString());
    }
    else if (chosen == aNewCat) {
        const QString c = askCategoryName(tr("New category"), QString());
        if (!c.isEmpty() && presets) presets->addCategory(c);
    }
    else if (chosen == aRenCat) {
        const QString c = askCategoryName(tr("Rename category"), category);
        if (!c.isEmpty() && presets) presets->renameCategory(category, c);
    }
    else if (chosen == aDelCat) {
        const int n = item(row) ? item(row)->data(PresetDelegate::CountRole).toInt() : 0;
        if (n > 0) {
            const auto ret = QMessageBox::question(this, tr("Delete category"),
                tr("Delete the category \"%1\"? Its %2 preset(s) move to Winnow; "
                   "no preset is deleted.").arg(category).arg(n),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret != QMessageBox::Yes) return;
        }
        if (presets) presets->removeCategory(category);
    }
    else if (moveMenu && chosen->parent() == moveMenu) {
        if (presets) presets->movePreset(name, chosen->data().toString(), INT_MAX);
    }
    else if (chosen == aUpdate) {
        const auto ret = QMessageBox::question(this, tr("Update preset"),
            tr("Replace the values in \"%1\" with this image's settings?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) emit updateRequested(name);
    }
    else if (chosen == aRename) {
        bool ok = false;
        const QString to = QInputDialog::getText(this, tr("Rename preset"),
            tr("Preset name:"), QLineEdit::Normal, name, &ok).trimmed();
        if (ok && !to.isEmpty() && to != name) emit renameRequested(name, to);
    }
    else if (chosen == aDelete) {
        const auto ret = QMessageBox::question(this, tr("Delete preset"),
            tr("Delete the preset \"%1\"?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) emit deleteRequested(name);
    }
}
