#include "Develop/Properties/layerheader.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QLinearGradient>
#include <QMouseEvent>

LayerHeader::LayerHeader(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("LayerHeader::LayerHeader");

    /* Checkmark for the active layer, plus a same-size transparent spacer so the other layers'
       captions line up with it in the popup. */
    const int iconPx = 12;
    QPixmap cm(":/images/checkmark.png");
    if (!cm.isNull())
        checkIcon = QIcon(cm.scaled(iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QPixmap blank(iconPx, iconPx);
    blank.fill(Qt::transparent);
    blankIcon = QIcon(blank);

    /* Collapse arrow: hides/shows the selected layer's rows in the tree below. Matches
       the tree's branch arrows (PropertyDelegate draws the same pixmap at 9x9, full
       opacity in the gutter). The button is one indentation wide (10px, the tree's
       gutter) with no padding, so the 9x9 icon sits in the gutter and the "Layer" label
       after it lands at x=10 -- aligned with the tree's section-header captions. */
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show this layer's settings");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(10, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0; background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleCollapsed(); });
    updateCollapseIcon();

    layerLabel = new QLabel(tr("Layer"), this);
    /* Clicking the label toggles collapse as if the arrow was clicked. */
    layerLabel->setCursor(Qt::PointingHandCursor);
    layerLabel->installEventFilter(this);
    /* Match the tree's section-header captions (PropertyDelegate): same point size
       (G::strFontSize) and colour (G::header2Color), regular weight (headers are not
       bold -- the delegate only sets the point size). Both via the stylesheet so they
       don't fight a setFont() call. */
    layerLabel->setStyleSheet(QString("color: %1; font-size: %2pt; background: transparent;")
                                  .arg(G::header2Color.name()).arg(G::strFontSize.toInt()));

    combo = new QComboBox(this);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->setIconSize(QSize(iconPx, iconPx));
    combo->setToolTip("The layer whose settings are shown below (this image's layers), plus layer actions");
    connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx){
        const int role = combo->itemData(idx).toInt();
        if (role >= 0) {                    // a layer row: role is its index
            emit layerSelected(combo->itemText(idx));
            return;
        }
        /* An action row: fire it and revert the box to the active layer, so the box never shows the
           action caption. */
        {
            const QSignalBlocker block(combo);
            combo->setCurrentIndex(activeIndex);
        }
        switch (role) {
        case ActAddLayer: emit addLayerRequested();   break;
        case ActAddMask:  emit addMaskRequested();     break;
        case ActReset:    emit resetLayerRequested();  break;
        case ActRemove:   emit removeLayerRequested(); break;
        case ActRename:   emit renameRequested();      break;
        }
    });

    previewBtn = new BarBtn();
    previewBtn->setToolTip("Preview: show or ignore this whole layer");
    connect(previewBtn, &BarBtn::clicked, this, [this]{
        previewShown = !previewShown;
        updatePreviewIcon();
        emit previewToggled(previewShown);
    });
    updatePreviewIcon();

    QHBoxLayout *row = new QHBoxLayout(this);
    /* margin.left 0 + a gutter-width (10px) arrow button + spacing 0 puts the arrow in
       the tree's gutter and the "Layer" label at one indentation (x=10), aligning it with
       the tree's section-header captions. Tweak if the tree frame/indentation changes. */
    row->setContentsMargins(0, 3, 6, 3);
    row->setSpacing(0);
    row->addWidget(collapseBtn);
    row->addWidget(layerLabel);
    row->addSpacing(6);
    row->addWidget(combo, 1);
    row->addSpacing(6);
    row->addWidget(previewBtn);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void LayerHeader::paintEvent(QPaintEvent *)
{
    /* The same top-to-bottom gradient the property-tree headers use (see PropertyDelegate::paint and
       TransformPanel's header: backgroundShade +5 -> -15). */
    QPainter p(this);
    const int a = G::backgroundShade + 5;
    const int b = G::backgroundShade - 15;
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0, QColor(a, a, a));
    g.setColorAt(1, QColor(b, b, b));
    p.fillRect(rect(), g);
}

void LayerHeader::setLayers(const QStringList &names, int currentIndex)
{
    const QSignalBlocker block(combo);
    combo->clear();

    layerCount  = names.size();
    activeIndex = (currentIndex >= 0 && currentIndex < layerCount) ? currentIndex : 0;
    baseActive  = (activeIndex == 0);

    /* Layers first; the active one carries the checkmark, the rest a blank spacer. Each layer row's
       item data is its own index (>= 0), which the activated handler uses to tell layers from
       actions. */
    for (int i = 0; i < layerCount; ++i) {
        combo->addItem(i == activeIndex ? checkIcon : blankIcon, names.at(i));
        combo->setItemData(i, i);
    }

    /* Separator, then "Add new layer" (always available). */
    combo->insertSeparator(combo->count());
    combo->addItem(tr("Add new layer"));
    combo->setItemData(combo->count() - 1, ActAddLayer);

    /* Per-layer actions, captioned with the active layer's name. Omitted for Base (index 0), which
       applies globally and cannot be reset/removed/renamed here. */
    if (!baseActive) {
        const QString nm = names.at(activeIndex);
        combo->insertSeparator(combo->count());
        combo->addItem(tr("Add mask to %1").arg(nm));
        combo->setItemData(combo->count() - 1, ActAddMask);
        combo->addItem(tr("Reset %1").arg(nm));
        combo->setItemData(combo->count() - 1, ActReset);
        combo->addItem(tr("Remove %1").arg(nm));
        combo->setItemData(combo->count() - 1, ActRemove);
        combo->addItem(tr("Rename %1").arg(nm));
        combo->setItemData(combo->count() - 1, ActRename);
    }

    combo->setCurrentIndex(activeIndex);
}

QString LayerHeader::currentLayerName() const
{
    return combo ? combo->itemText(activeIndex) : QString();
}

void LayerHeader::setCollapsed(bool c)
{
    collapsed = c;
    updateCollapseIcon();
}

void LayerHeader::setPreviewShown(bool shown)
{
    previewShown = shown;
    updatePreviewIcon();
}

void LayerHeader::setBaseActive(bool isBase)
{
    /* Base (index 0) applies globally: the dropdown omits the per-layer action group for it. */
    baseActive = isBase;
}

void LayerHeader::toggleCollapsed()
{
    collapsed = !collapsed;
    updateCollapseIcon();
    emit collapseToggled(collapsed);
}

bool LayerHeader::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == layerLabel && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && layerLabel->rect().contains(me->pos())) {
            toggleCollapsed();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LayerHeader::updatePreviewIcon()
{
    if (!previewBtn) return;
    previewBtn->setIcon(previewShown ? ":/images/icon16/eye.png"
                                     : ":/images/icon16/eye_off.png", G::iconOpacity);
}

void LayerHeader::updateCollapseIcon()
{
    if (!collapseBtn) return;
    /* Open branch (down) when settings show; closed branch (right) when hidden. Full
       opacity + 9x9 (setIconSize in the ctor) to match the tree's branch arrows;
       BarBtn::setIcon(path, opacity) would dim it (G::iconOpacity) at native 11x11, so
       set a plain full-opacity icon instead. */
    const QString path = collapsed ? ":/images/branch-closed-winnow.png"
                                    : ":/images/branch-open-winnow.png";
    collapseBtn->setIcon(QIcon(QPixmap(path)));
}
