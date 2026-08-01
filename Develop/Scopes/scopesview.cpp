#include "Develop/Scopes/scopesview.h"
#include "Main/global.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>

ScopesView::ScopesView(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("ScopesView::ScopesView");

    histogram = new HistogramView(this);
    vectorscope = new VectorscopeView(this);
    tone = new ToneRegionSlider(this);

    /* Left column: histogram with the tone-region slider tucked directly under it (sharing the
       same x-axis); vectorscope on the right. */
    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(1);
    leftCol->addWidget(histogram, 1);
    leftCol->addWidget(tone);           // fixed height (sizeHint)

    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);
    lay->addLayout(leftCol, 3);         // histogram column left, wider
    lay->addWidget(vectorscope, 2);     // vectorscope right

    /* Re-emit the vectorscope's menu choices so MW can persist them. */
    connect(vectorscope, &VectorscopeView::zoomChanged,
            this, &ScopesView::vectorscopeZoomChanged);
    connect(vectorscope, &VectorscopeView::skinLineChanged,
            this, &ScopesView::vectorscopeSkinLineChanged);

    /* Fixed strip at the top of the dock; the property tree below takes the stretch. */
    setFixedHeight(160);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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
