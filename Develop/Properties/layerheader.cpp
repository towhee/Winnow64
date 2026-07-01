#include "Develop/Properties/layerheader.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QMenu>
#include <QContextMenuEvent>
#include <QPainter>
#include <QLinearGradient>

LayerHeader::LayerHeader(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("LayerHeader::LayerHeader");

    /* Collapse arrow: hides/shows the selected layer's rows in the tree below. */
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show this layer's settings");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{
        collapsed = !collapsed;
        updateCollapseIcon();
        emit collapseToggled(collapsed);
    });
    updateCollapseIcon();

    combo = new QComboBox(this);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->setToolTip("The layer whose settings are shown below (this image's layers)");
    connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this](int){
        emit layerSelected(combo->currentText());
    });

    /* [?] pops the layer-actions menu (rename / reset / add / remove / add mask); also on right-click. */
    menuBtn = new BarBtn();
    menuBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    menuBtn->setToolTip("Layer actions (rename, reset, add, remove, mask)");
    connect(menuBtn, &BarBtn::clicked, this, [this]{
        showActionsMenu(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
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
    row->addWidget(combo, 1);
    row->addSpacing(4);
    row->addWidget(menuBtn);
    row->addWidget(previewBtn);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void LayerHeader::showActionsMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *aRename = menu.addAction("Rename layer...");
    QAction *aReset  = menu.addAction("Reset layer");
    menu.addSeparator();
    QAction *aAdd    = menu.addAction("Add layer...");
    QAction *aRemove = menu.addAction("Remove layer");
    menu.addSeparator();
    QAction *aMask   = menu.addAction("Add mask...");

    /* Base applies globally: it cannot be renamed, removed or masked. */
    aRename->setEnabled(!baseActive);
    aRemove->setEnabled(!baseActive);
    aMask->setEnabled(!baseActive);

    QAction *chosen = menu.exec(globalPos);
    if      (chosen == aRename) emit renameRequested();
    else if (chosen == aReset)  emit resetLayerRequested();
    else if (chosen == aAdd)    emit addLayerRequested();
    else if (chosen == aRemove) emit removeLayerRequested();
    else if (chosen == aMask)   emit maskMenuRequested();
}

void LayerHeader::contextMenuEvent(QContextMenuEvent *event)
{
    showActionsMenu(event->globalPos());
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
    combo->addItems(names);
    if (currentIndex >= 0 && currentIndex < names.size())
        combo->setCurrentIndex(currentIndex);
    setBaseActive(combo->currentIndex() == 0);
}

QString LayerHeader::currentLayerName() const
{
    return combo ? combo->currentText() : QString();
}

void LayerHeader::setPreviewShown(bool shown)
{
    previewShown = shown;
    updatePreviewIcon();
}

void LayerHeader::setBaseActive(bool isBase)
{
    /* Base (index 0) applies globally: the menu disables Rename / Remove / Add mask for it. */
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
