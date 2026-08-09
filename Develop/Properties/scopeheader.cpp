#include "Develop/Properties/scopeheader.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QLinearGradient>
#include <QMouseEvent>

ScopeHeader::ScopeHeader(QWidget *parent) : ScopeHeaderBase(parent)
{
    if (G::isLogger) G::log("ScopeHeader::ScopeHeader");

    /* Checkmark for the active scope, plus a same-size transparent spacer so the other scopes'
       captions line up with it in the popup. */
    const int iconPx = 12;
    QPixmap cm(":/images/checkmark.png");
    if (!cm.isNull())
        checkIcon = QIcon(cm.scaled(iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QPixmap blank(iconPx, iconPx);
    blank.fill(Qt::transparent);
    blankIcon = QIcon(blank);

    /* Collapse arrow: hides/shows the selected scope's rows in the tree below. Matches
       the tree's branch arrows (PropertyDelegate draws the same pixmap at 9x9, full
       opacity in the gutter). The button is exactly icon-wide with no padding, so the
       arrow sits in the tree's gutter and the "Scope" label, G::decorationTitleGap after
       it, lands where the tree's section-header captions do. */
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show this scope's settings");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(9, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0; background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleCollapsed(); });
    updateCollapseIcon();

    scopeLabel = new QLabel(tr("Global and Masks"), this);
    /* Clicking the label toggles collapse as if the arrow was clicked. */
    scopeLabel->setCursor(Qt::PointingHandCursor);
    scopeLabel->installEventFilter(this);
    /* Match the tree's section-header captions (PropertyDelegate): same point size
       (G::strFontSize) and colour (G::header2Color), regular weight (headers are not
       bold -- the delegate only sets the point size). Both via the stylesheet so they
       don't fight a setFont() call. */
    scopeLabel->setStyleSheet(QString("color: %1; font-size: %2pt; background: transparent;")
                                  .arg(G::header2Color.name()).arg(G::strFontSize.toInt()));

    combo = new QComboBox(this);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->setIconSize(QSize(iconPx, iconPx));
    combo->setToolTip("The scope whose settings are shown below:\n"
                      "Global, or one of this image's masks");
    /* The combo lists only scopes, so every activated row is a scope selection. */
    connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx){
        emit scopeSelected(combo->itemText(idx));
    });

    /* Scope-actions menu button (ellipsis): pops up the Add / Reset / Remove / Rename
       menu, like EmbelProperties::effectNewBtn triggers effectContextMenu. */
    scopeMenuBtn = new BarBtn();
    scopeMenuBtn->setToolTip("Scope actions (new mask, reset, remove, rename)");
    scopeMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    connect(scopeMenuBtn, &BarBtn::clicked, this, [this]{ showScopeMenu(); });

    previewBtn = new BarBtn();
    previewBtn->setToolTip("Preview: show or ignore this whole scope");
    connect(previewBtn, &BarBtn::clicked, this, [this]{
        previewShown = !previewShown;
        updatePreviewIcon();
        emit previewToggled(previewShown);
    });
    updatePreviewIcon();

    QHBoxLayout *row = new QHBoxLayout(this);
    /* margin.left 0 + an icon-wide arrow button + G::decorationTitleGap puts the arrow in
       the tree's gutter and the "Scope" label just clear of it, aligning it with the
       tree's section-header captions. Tweak if the tree frame/indentation changes. */
    row->setContentsMargins(0, 3, 6, 3);
    row->setSpacing(0);
    row->addWidget(collapseBtn);
    row->addSpacing(G::decorationTitleGap);
    row->addWidget(scopeLabel);
    row->addSpacing(6);
    row->addWidget(combo, 1);
    row->addSpacing(6);
    row->addWidget(scopeMenuBtn);
    row->addSpacing(6);
    row->addWidget(previewBtn);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ScopeHeader::paintEvent(QPaintEvent *)
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

void ScopeHeader::setScopes(const QStringList &names, int currentIndex)
{
    const QSignalBlocker block(combo);
    combo->clear();

    scopeCount  = names.size();
    activeIndex = (currentIndex >= 0 && currentIndex < scopeCount) ? currentIndex : 0;
    globalActive  = (activeIndex == 0);

    /* Scopes only; the active one carries the checkmark, the rest a blank spacer. The
       scope actions now live on the scopeMenuBtn menu (see showScopeMenu). */
    for (int i = 0; i < scopeCount; ++i)
        combo->addItem(i == activeIndex ? checkIcon : blankIcon, names.at(i));

    combo->setCurrentIndex(activeIndex);
}

void ScopeHeader::showScopeMenu()
{
    if (G::isLogger) G::log("ScopeHeader::showScopeMenu");

    QMenu menu(this);

    /* "Add new scope" is always available. The tab'd key hints are the Develop mode local
       shortcuts (MW::loadDevelopShortcuts); these are plain menu actions with no sequence
       of their own, so the hint has to be part of the text. */
    connect(menu.addAction(tr("New mask\tN")), &QAction::triggered,
            this, [this]{ emit addScopeRequested(); });

    /* Per-scope actions, captioned with the active scope's name. Omitted for Global
       (index 0), which applies globally and cannot be reset/removed/renamed here. */
    if (!globalActive) {
        const QString nm = currentScopeName();
        menu.addSeparator();
        connect(menu.addAction(tr("Tweak %1 mask\tM").arg(nm)), &QAction::triggered,
                this, [this]{ emit addMaskRequested(); });
        /* Show/hide the mask coverage tint on the loupe. Only meaningful while a submask
           is being defined (its overlay is being edited), so the row is omitted if not.
           Same action as the "O" Develop shortcut (MW::toggleMaskOverlay). */
        if (maskOverlayAvailable) {
            QAction *ov = menu.addAction(tr("Show mask overlay\tO"));
            ov->setCheckable(true);
            ov->setChecked(maskOverlayShown);
            connect(ov, &QAction::triggered, this, [this]{ emit maskOverlayToggled(); });
        }
        connect(menu.addAction(tr("Reset %1").arg(nm)), &QAction::triggered,
                this, [this]{ emit resetScopeRequested(); });
        connect(menu.addAction(tr("Remove %1").arg(nm)), &QAction::triggered,
                this, [this]{ emit removeScopeRequested(); });
        connect(menu.addAction(tr("Rename %1").arg(nm)), &QAction::triggered,
                this, [this]{ emit renameRequested(); });
    }

    menu.exec(QCursor::pos());
}

QString ScopeHeader::currentScopeName() const
{
    return combo ? combo->itemText(activeIndex) : QString();
}

void ScopeHeader::setCollapsed(bool c)
{
    collapsed = c;
    updateCollapseIcon();
}

void ScopeHeader::setPreviewShown(bool shown)
{
    previewShown = shown;
    updatePreviewIcon();
}

void ScopeHeader::setMaskOverlayAvailable(bool available)
{
    maskOverlayAvailable = available;
}

void ScopeHeader::setMaskOverlayShown(bool shown)
{
    maskOverlayShown = shown;
}

void ScopeHeader::setGlobalActive(bool isGlobal)
{
    /* Global (index 0) applies globally: the dropdown omits the per-scope action group for it. */
    globalActive = isGlobal;
}

void ScopeHeader::toggleCollapsed()
{
    collapsed = !collapsed;
    updateCollapseIcon();
    emit collapseToggled(collapsed);
}

bool ScopeHeader::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == scopeLabel && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && scopeLabel->rect().contains(me->pos())) {
            toggleCollapsed();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ScopeHeader::updatePreviewIcon()
{
    if (!previewBtn) return;
    previewBtn->setIcon(previewShown ? ":/images/icon16/eye.png"
                                     : ":/images/icon16/eye_off.png", G::iconOpacity);
}

void ScopeHeader::updateCollapseIcon()
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
