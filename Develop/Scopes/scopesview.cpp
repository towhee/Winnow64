#include "Develop/Scopes/scopesview.h"
#include "Main/global.h"
#include "Main/dockwidget.h"       // BarBtn
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QResizeEvent>

ScopesView::ScopesView(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("ScopesView::ScopesView");

    histogram = new HistogramView(this);
    vectorscope = new VectorscopeView(this);
    tone = new ToneRegionSlider(this);

    /* Left column: histogram with the tone-region slider tucked directly under it (sharing the
       same x-axis); vectorscope on the right. */
    leftCol = new QVBoxLayout;
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(1);
    leftCol->addWidget(histogram, 1);
    leftCol->addWidget(tone);           // fixed height (sizeHint)

    rowLay = new QHBoxLayout(this);
    rowLay->setContentsMargins(2, 2, 2, 2);
    rowLay->setSpacing(2);
    rowLay->addLayout(leftCol, 3);      // histogram column left, wider
    rowLay->addWidget(vectorscope, 2);  // vectorscope right

    /* Re-emit the vectorscope's menu choices so MW can persist them. */
    connect(vectorscope, &VectorscopeView::zoomChanged,
            this, &ScopesView::vectorscopeZoomChanged);
    connect(vectorscope, &VectorscopeView::skinLineChanged,
            this, &ScopesView::vectorscopeSkinLineChanged);
    /* One close button for the whole strip, floated over the top right corner rather
       than placed in the layout (the scopes fill it; a layout slot would steal width
       from whichever scope is on the right). resizeEvent keeps it in the corner. */
    closeBtn = new BarBtn();
    closeBtn->setParent(this);
    closeBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    closeBtn->setToolTip(tr("Close the histogram and vectorscope. Press (G) to cycle scopes."));
    connect(closeBtn, &BarBtn::clicked, this, &ScopesView::closeRequested);

    /* Fixed strip at the top of the dock; the property tree below takes the stretch. */
    setFixedHeight(160);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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
    closeBtn->raise();      // the [X] belongs to the strip, so it stays on top
}

void ScopesView::resizeEvent(QResizeEvent *event)
{
/*
    Pin the strip's [X] to the top right corner, above whichever scope is there.
*/
    QWidget::resizeEvent(event);
    const QSize s = closeBtn->sizeHint();
    closeBtn->setGeometry(width() - s.width() - 2, 2, s.width(), s.height());
    closeBtn->raise();
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
