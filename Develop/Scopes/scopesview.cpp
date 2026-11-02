#include "Develop/Scopes/scopesview.h"
#include "Main/global.h"
#include "Main/dockwidget.h"       // BarBtn
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QLinearGradient>
#include <QLabel>
#include <QIcon>
#include <QPixmap>

/* Inset for the strip on ALL FOUR edges, matching the CURVES PLOT below it: that plot is
   a spanned row two indent levels in (2 x the tree's 10px indentation) whose widget is
   re-centred in the panel (see DevelopProperties::updateEditorGeometries), so it clears
   each edge by 10px. The scopes are the other full-width graphic in this dock and sat 2px
   off the edges, which read as a different panel.

   setFixedHeight (updateHeight) is raised by the extra top + bottom margin, so the scopes keep the
   160px of drawing height they had -- the strip grows, the graphs do not shrink. */
static const int kScopeMargin = 10;

ScopesView::ScopesView(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("ScopesView::ScopesView");

    /* The bottom margin reserves the separator rule drawn in paintEvent. */
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, G::panelBorderHeight);
    outer->setSpacing(0);

    /* Header band (collapse arrow + "Histogram" + menu), built like RawPanel's: a
       property-tree row high, transparent so paintEvent draws the gradient behind it, the
       arrow in the tree's gutter and the caption G::decorationTitleGap clear of it. */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    headerBand->setCursor(Qt::PointingHandCursor);
    headerBand->installEventFilter(this);        // a header click toggles collapse
    headerBand->setFixedHeight(G::propertyRowHeight());
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(G::headerLeftInset, 0, 6, G::headerCaptionTrim);
    hb->setSpacing(0);
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip(tr("Hide or show the histogram and vectorscope"));
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(9, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0; background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleExpanded(); });
    QLabel *titleLabel = new QLabel(tr("Histogram"), headerBand);
    titleLabel->setStyleSheet(G::labelCss(G::header2Color, G::strFontSize.toInt()));
    /* The menu button opens the same scopes-layout menu as a right-click on the strip. */
    menuBtn = new BarBtn();
    menuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    menuBtn->setToolTip(tr("Choose which scopes are shown"));
    connect(menuBtn, &BarBtn::clicked, this, [this]{
        QMenu menu(this);
        emit menuRequested(&menu, menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });
    hb->addWidget(collapseBtn);
    hb->addSpacing(G::decorationTitleGap);
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(menuBtn);
    outer->addWidget(headerBand);

    /* Body: the scopes, hidden while collapsed. Translucent so the content background
       painted in paintEvent shows through. */
    body = new QWidget(this);
    body->setAttribute(Qt::WA_TranslucentBackground);
    outer->addWidget(body);

    histogram = new HistogramView(body);
    vectorscope = new VectorscopeView(body);
    tone = new ToneRegionSlider(body);
    tone->setToolTip("Split the tonal range into blacks | shadows | highlights | whites. "
                     "These are the same handles as under the Curves panel's "
                     "parametric curve.");

    /* Left column: histogram with the tone-region slider tucked directly under it (sharing the
       same x-axis); vectorscope on the right. */
    leftCol = new QVBoxLayout;
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(1);
    leftCol->addWidget(histogram, 1);
    leftCol->addWidget(tone);           // fixed height (sizeHint)

    rowLay = new QHBoxLayout(body);
    rowLay->setContentsMargins(kScopeMargin, kScopeMargin, kScopeMargin, kScopeMargin);
    rowLay->setSpacing(2);
    rowLay->addLayout(leftCol, 3);      // histogram column left, wider
    rowLay->addWidget(vectorscope, 2);  // vectorscope right

    /* Each scope builds its own context-menu items and hands the part-built menu up;
       relay it to MW, which appends the shared scopes-layout section and shows it. */
    connect(histogram, &HistogramView::menuRequested, this, &ScopesView::menuRequested);
    connect(vectorscope, &VectorscopeView::menuRequested, this, &ScopesView::menuRequested);
    /* Re-emit the vectorscope's menu choices so MW can persist them. */
    connect(vectorscope, &VectorscopeView::zoomChanged,
            this, &ScopesView::vectorscopeZoomChanged);
    connect(vectorscope, &VectorscopeView::skinLineChanged,
            this, &ScopesView::vectorscopeSkinLineChanged);

    /* Fixed strip at the top of the dock; the property tree below takes the stretch.
       updateHeight sizes it for the header alone or the header + scopes. */
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    updateCollapseIcon();
    updateHeight();
}

void ScopesView::setExpanded(bool isExpanded)
{
    expanded = isExpanded;
    body->setVisible(expanded);
    updateCollapseIcon();
    updateHeight();
}

void ScopesView::toggleExpanded()
{
    if (G::isLogger) G::log("ScopesView::toggleExpanded");
    setExpanded(!expanded);
    emit expandToggled(expanded);
}

void ScopesView::updateCollapseIcon()
{
    /* Match the tree's branch arrows (and RawPanel's): open (down) when shown, closed
       (right) when hidden, full opacity at 9x9. */
    const QString path = expanded ? ":/images/branch-open-winnow.png"
                                  : ":/images/branch-closed-winnow.png";
    collapseBtn->setIcon(QIcon(QPixmap(path)));
}

void ScopesView::updateHeight()
{
    /* The margins and the separator rule are added on top of the 160px of scopes, not
       taken out of them. */
    const int scopesH = expanded ? 160 + 2 * kScopeMargin : 0;
    setFixedHeight(G::propertyRowHeight() + scopesH + G::panelBorderHeight);
}

bool ScopesView::eventFilter(QObject *watched, QEvent *event)
{
    /* A left click anywhere on the header band toggles collapse; the arrow and menu
       buttons consume their own clicks. MouseButtonDblClick is included because Qt sends
       it INSTEAD of the second press (see RawPanel::eventFilter). */
    if (watched == headerBand && (event->type() == QEvent::MouseButtonPress ||
                                  event->type() == QEvent::MouseButtonDblClick)) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) { toggleExpanded(); return true; }
    }
    return QWidget::eventFilter(watched, event);
}

void ScopesView::setScopeLayout(ScopeLayout layout)
{
/*
    Switch the strip between both scopes, histogram only and vectorscope only. Hidden
    widgets are empty layout items, so the survivor already grows to fill the strip; the
    stretch factors are re-set as well so the fill does not depend on that behaviour. The
    tone-region slider shares the histogram's x-axis, so it follows the histogram.
*/
    if (G::isLogger) G::log("ScopesView::setScopeLayout");
    curLayout = layout;
    const bool showHist = (layout != VectorscopeOnly);
    const bool showVec  = (layout != HistogramOnly);
    histogram->setVisible(showHist);
    tone->setVisible(showHist);
    vectorscope->setVisible(showVec);
    rowLay->setStretch(0, showHist ? (showVec ? 3 : 1) : 0);   // histogram column
    rowLay->setStretch(1, showVec ? (showHist ? 2 : 1) : 0);   // vectorscope
}

void ScopesView::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    /* Subpanel content background (G::panelContentBg) below the header band, then the
       header gradient. The scopes themselves paint over the content background. */
    const QRect r = headerBand->geometry();
    p.fillRect(0, r.bottom() + 1, width(), height() - r.bottom() - 1, G::panelContentBg);
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, G::headerGradientTop);
    g.setColorAt(1, G::headerGradientBottom);
    p.fillRect(r, g);
    /* Separator rule across the bottom edge (space reserved by the layout margin). */
    p.fillRect(0, height() - G::panelBorderHeight, width(), G::panelBorderHeight,
               G::panelSeparatorColor);
}

void ScopesView::setData(const ScopeData &d)
{
    histogram->setData(d);
    vectorscope->setData(d);
}

void ScopesView::clear()
{
    histogram->clear();
    vectorscope->clear();
    clearMarker();      // drop any stale cursor readout when the image goes away
}

void ScopesView::setMarker(int r, int g, int b)
{
    histogram->setMarker(r, g, b);
    vectorscope->setMarker(r, g, b);
}

void ScopesView::clearMarker()
{
    histogram->clearMarker();
    vectorscope->clearMarker();
}

void ScopesView::setVectorscopeZoom(double z)
{
    vectorscope->setZoom(z);
}

void ScopesView::setVectorscopeSkinLine(bool on)
{
    vectorscope->setSkinLine(on);
}

void ScopesView::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();   // consume so the dock does not treat it as un/redock
}

void ScopesView::contextMenuEvent(QContextMenuEvent *event)
{
/*
    A right-click that no scope handled (the tone-region slider, or the strip's margins):
    the same menu with no scope-specific items, i.e. just the scopes-layout section.
*/
    if (G::isLogger) G::log("ScopesView::contextMenuEvent");
    QMenu menu(this);
    event->accept();
    emit menuRequested(&menu, event->globalPos());
}
