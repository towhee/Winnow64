#include "Develop/Presets/presetsview.h"
#include "Develop/Presets/developpresets.h"
#include "Main/global.h"

#include <QFontMetrics>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

/*
    PresetsView / PresetDelegate (see presetsview.h): the Develop Presets dock's list of
    user presets. Hover previews a preset on the loupe, a click applies it -- the same
    interaction as the History dock, because they are the same kind of list.
*/

/* ------------------------------------------------------------------------------------
   PresetDelegate
   ------------------------------------------------------------------------------------ */

PresetDelegate::PresetDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void PresetDelegate::setHoveredRow(int row)
{
    hoveredRow = row;
}

void PresetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
    painter->save();

    const QString text = index.data(Qt::DisplayRole).toString();
    const bool isPreset = !index.data(NameRole).toString().isEmpty();

    /* A hovered row is lightened because hovering is live -- the loupe is showing that
       preset applied to the image. */
    if (isPreset && index.row() == hoveredRow) {
        const int b = G::backgroundShade + 20;
        painter->fillRect(option.rect, QColor(b, b, b));
    }

    const QFontMetrics fm(option.font);
    const QRect r = option.rect.adjusted(8, 0, -8, 0);
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
    clear();
    if (!presets) return;

    const QStringList names = presets->names();
    if (names.isEmpty()) {
        /* Say how to make one rather than showing an empty box -- the [+] button in the
           dock title bar is otherwise easy to miss. */
        QListWidgetItem *item =
            new QListWidgetItem(tr("No presets yet - click + to create one"), this);
        item->setData(PresetDelegate::NameRole, QString());
        return;
    }
    for (const QString &name : names) {
        QListWidgetItem *item = new QListWidgetItem(name, this);
        item->setData(PresetDelegate::NameRole, name);
        const QString scope = presets->read(name).sourceScope;
        item->setToolTip(scope.isEmpty()
            ? tr("Apply \"%1\" to this image").arg(name)
            : tr("Apply \"%1\" to this image (saved from %2)").arg(name, scope));
    }
    scrollToTop();
}

QString PresetsView::nameForRow(int row) const
{
    const QListWidgetItem *it = item(row);
    if (!it) return QString();
    return it->data(PresetDelegate::NameRole).toString();
}

void PresetsView::setHover(int row)
{
    if (row == hoverRow) return;
    hoverRow = row;
    delegate->setHoveredRow(row);
    viewport()->update();
    if (row < 0) hoverTimer->stop();
    else         hoverTimer->start(kHoverDelayMs);
}

void PresetsView::mouseMoveEvent(QMouseEvent *event)
{
    setHover(indexAt(event->pos()).row());
    QListWidget::mouseMoveEvent(event);
}

void PresetsView::leaveEvent(QEvent *event)
{
    setHover(-1);
    if (hoverEmitted) {
        hoverEmitted = false;
        emit hoverEnded();          // put the real state back on the loupe
    }
    QListWidget::leaveEvent(event);
}

void PresetsView::mouseReleaseEvent(QMouseEvent *event)
{
    const QString name = nameForRow(indexAt(event->pos()).row());
    QListWidget::mouseReleaseEvent(event);
    if (name.isEmpty() || event->button() != Qt::LeftButton) return;
    /* End the hover preview first: applying the preset makes it the real state. Deferred
       a tick because applying rebuilds the Develop tree from inside this handler. */
    hoverEmitted = false;
    QTimer::singleShot(0, this, [this, name]{ emit presetChosen(name); });
}

void PresetsView::showMenu(const QPoint &pos)
{
/*
    Manage presets (Lightroom's preset context menu). The row actions appear only over a
    real preset. The prompts live here so the caller receives a decided action, never a
    "maybe" -- the same division of labour as SaveDevelopPresetDlg, which owns its own
    overwrite confirmation.
*/
    const QString name = nameForRow(indexAt(pos).row());

    /* Hovering the row that opened the menu left a preview on the loupe; the menu grabs
       the mouse, so no leaveEvent will arrive to undo it. */
    setHover(-1);
    if (hoverEmitted) {
        hoverEmitted = false;
        emit hoverEnded();
    }

    QMenu menu(this);
    QAction *aNew    = menu.addAction(tr("New Preset..."));
    QAction *aUpdate = nullptr;
    QAction *aRename = nullptr;
    QAction *aDelete = nullptr;
    if (!name.isEmpty()) {
        menu.addSeparator();
        aUpdate = menu.addAction(tr("Update with Current Settings"));
        aRename = menu.addAction(tr("Rename..."));
        aDelete = menu.addAction(tr("Delete"));
    }

    QAction *chosen = menu.exec(mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == aNew) {
        emit newPresetRequested();
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
