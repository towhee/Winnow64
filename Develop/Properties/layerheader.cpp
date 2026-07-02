#include "Develop/Properties/layerheader.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QLinearGradient>

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

    /* Collapse arrow: hides/shows the selected layer's rows in the tree below. */
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show this layer's settings");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{
        collapsed = !collapsed;
        updateCollapseIcon();
        emit collapseToggled(collapsed);
    });
    updateCollapseIcon();

    layerLabel = new QLabel(tr("Layer"), this);

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
    row->setContentsMargins(4, 3, 6, 3);
    row->setSpacing(4);
    row->addWidget(collapseBtn);
    row->addWidget(layerLabel);
    row->addWidget(combo, 1);
    row->addSpacing(4);
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

void LayerHeader::updatePreviewIcon()
{
    if (!previewBtn) return;
    previewBtn->setIcon(previewShown ? ":/images/icon16/eye.png"
                                     : ":/images/icon16/eye_off.png", G::iconOpacity);
}

void LayerHeader::updateCollapseIcon()
{
    if (!collapseBtn) return;
    /* Open branch (down) when the layer's settings are showing; closed branch (right) when hidden. */
    collapseBtn->setIcon(collapsed ? ":/images/branch-closed-winnow.png"
                                   : ":/images/branch-open-winnow.png", G::iconOpacity);
}
