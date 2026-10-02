#include "Views/Map/mapview.h"

#include <QtWidgets>
#include <cmath>
#include <numbers>
#include "Datamodel/datamodel.h"
#include "Main/global.h"

namespace {
constexpr double kPi = std::numbers::pi;    // not M_PI: MSVC needs _USE_MATH_DEFINES
/* Lightroom's orange for a pin, and the Module dock's selected-option yellow
   (kSelectedOptionColor in initialize.cpp) for a selected one. */
const QColor kPinColor("#e8742c");
const QColor kPinSelectedColor("#f0c419");
// a place's outline: the Develop mask overlay's blue, so "an area" reads the same
const QColor kPlaceColor("#4aa3ff");
// the radial mask editor's handle colours (ImageView::drawRadialMask)
const QColor kAxisHandleColor("#4aa3ff");
const QColor kRotateHandleColor("#5ad06a");
constexpr double kHandlePick = 11;          // px, as ImageView::maskHitTest
constexpr double kRotateKnobPx = 24;        // past the +x handle
constexpr double kCloseSnapPx = 10;         // a click this near corner 0 closes

double pinRadius(int n)
{
    if (n <= 1) return 6;
    return 9 + std::min(8.0, std::log2(double(n)) * 1.5);
}
}

MapView::MapView(QWidget *parent, DataModel *dm) : QWidget(parent), dm(dm)
{
    setObjectName("MapView");
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::NoFocus);
    setAcceptDrops(true);       // drag thumbnails here to geotag them

    tiles = new MapTileSource(this);
    connect(tiles, &MapTileSource::tileReady, this, qOverload<>(&QWidget::update));
    connect(tiles, &MapTileSource::errorChanged, this, &MapView::updateBanner);

    rebuildTimer.setSingleShot(true);
    rebuildTimer.setInterval(150);
    connect(&rebuildTimer, &QTimer::timeout, this, &MapView::rebuildPoints);

    previewHideTimer.setInterval(250);
    connect(&previewHideTimer, &QTimer::timeout, this, [this]() {
        const QPoint p = mapFromGlobal(QCursor::pos());
        if (preview->isVisible() && preview->geometry().contains(p)) return;
        if (previewCluster >= 0 && clusterAt(p) == previewCluster) return;
        hidePreview();
    });

    // Top right: fit, follow and zoom
    controls = new QWidget(this);
    controls->setObjectName("MapControls");
    controls->setStyleSheet(
        "#MapControls { background: rgba(20,20,20,190); border-radius: 5px; }"
        "#MapControls QToolButton { color: #e0e0e0; padding: 2px 6px; }"
        "#MapControls QToolButton:checked { color: #f0c419; }");
    auto *cl = new QHBoxLayout(controls);
    cl->setContentsMargins(6, 3, 6, 3);
    cl->setSpacing(4);
    auto makeBtn = [this](const QString &text, const QString &tip) {
        auto *b = new QToolButton(controls);
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };
    fitAllBtn = makeBtn(tr("Fit All"), tr("Zoom to show every image that has a location"));
    fitSelBtn = makeBtn(tr("Fit Selection"), tr("Zoom to the selected images"));
    followBtn = makeBtn(tr("Follow"),
                        tr("Pan the map to the current image when it is off screen"));
    followBtn->setCheckable(true);
    followBtn->setChecked(followSelection);
    zoomOutBtn = makeBtn("−", tr("Zoom out"));
    zoomInBtn = makeBtn("+", tr("Zoom in"));
    zoomSlider = new QSlider(Qt::Horizontal, controls);
    zoomSlider->setFixedWidth(110);
    zoomSlider->setFocusPolicy(Qt::NoFocus);
    zoomSlider->setToolTip(tr("Zoom"));
    /* Map style: the keyless built-ins, and Custom when Preferences > Map has a URL.
       MW owns the choice (MW::mapStyle, persisted) -- this only asks for it. */
    styleBtn = makeBtn(tr("Standard"), tr("Map style"));
    styleBtn->setPopupMode(QToolButton::InstantPopup);
    auto *styleMenu = new QMenu(styleBtn);
    styleGroup = new QActionGroup(styleMenu);
    const QList<QPair<QString, QString>> styles{
        {"standard", tr("Standard")},
        {"cyclosm",  tr("Cycle (CyclOSM)")},
        {"opentopo", tr("Topographic (OpenTopoMap)")},
        {"custom",   tr("Custom (Preferences > Map)")},
    };
    for (const auto &st : styles) {
        QAction *a = styleMenu->addAction(st.second);
        a->setCheckable(true);
        a->setData(st.first);
        styleGroup->addAction(a);
        if (st.first == "custom") customStyleAction = a;
    }
    styleMenu->setToolTipsVisible(true);        // Custom says why it is greyed
    styleMenu->addSeparator();
    QAction *prefs = styleMenu->addAction(tr("Map Preferences ..."));
    connect(prefs, &QAction::triggered, this, &MapView::openPreferences);
    connect(styleGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        emit styleChosen(a->data().toString());
    });
    styleBtn->setMenu(styleMenu);
    cl->addWidget(styleBtn);
    cl->addSpacing(8);
    cl->addWidget(fitAllBtn);
    cl->addWidget(fitSelBtn);
    cl->addWidget(followBtn);
    cl->addSpacing(8);
    cl->addWidget(zoomOutBtn);
    cl->addWidget(zoomSlider);
    cl->addWidget(zoomInBtn);
    connect(fitAllBtn, &QToolButton::clicked, this, &MapView::fitAll);
    connect(fitSelBtn, &QToolButton::clicked, this, &MapView::fitSelection);
    connect(followBtn, &QToolButton::toggled, this, [this](bool on) {
        followSelection = on;
        if (on) currentChanged();
    });
    connect(zoomOutBtn, &QToolButton::clicked, this, [this]() { setZoomCentred(zoom - 1); });
    connect(zoomInBtn, &QToolButton::clicked, this, [this]() { setZoomCentred(zoom + 1); });
    connect(zoomSlider, &QSlider::valueChanged, this, [this](int v) {
        if (std::abs(v / 10.0 - zoom) > 0.05) setZoomCentred(v / 10.0);
    });

    // Top centre: why the map is blank, with the way to fix it
    banner = new QFrame(this);
    banner->setObjectName("MapBanner");
    banner->setStyleSheet(
        "#MapBanner { background: rgba(20,20,20,210); border-radius: 5px; }"
        "#MapBanner QLabel { color: #e0e0e0; }");
    auto *bl = new QHBoxLayout(banner);
    bl->setContentsMargins(10, 6, 10, 6);
    bannerText = new QLabel(banner);
    bannerText->setWordWrap(true);
    bannerBtn = new QPushButton(tr("Map Preferences ..."), banner);
    bannerBtn->setStyleSheet("min-width: 0px;");       // see widgetcss.cpp min-width
    bannerBtn->setFocusPolicy(Qt::NoFocus);
    bl->addWidget(bannerText, 1);
    bl->addWidget(bannerBtn);
    connect(bannerBtn, &QPushButton::clicked, this, &MapView::openPreferences);

    // Bottom left: how many of the images are on the map
    countLabel = new QLabel(this);
    countLabel->setObjectName("MapCount");
    countLabel->setStyleSheet("#MapCount { background: rgba(20,20,20,190); color: #e0e0e0;"
                              " border-radius: 4px; padding: 2px 6px; }");

    // Hover preview
    preview = new QFrame(this);
    preview->setObjectName("MapPreview");
    preview->setStyleSheet(
        "#MapPreview { background: rgba(20,20,20,230); border: 1px solid #555;"
        " border-radius: 5px; }"
        "#MapPreview QLabel { color: #e0e0e0; }"
        "#MapPreview QToolButton { color: #e0e0e0; }");
    auto *pl = new QVBoxLayout(preview);
    pl->setContentsMargins(6, 6, 6, 6);
    pl->setSpacing(4);
    previewImage = new QToolButton(preview);
    previewImage->setAutoRaise(true);
    previewImage->setIconSize(QSize(160, 160));
    previewImage->setFocusPolicy(Qt::NoFocus);
    previewImage->setToolTip(tr("Select this image"));
    previewText = new QLabel(preview);
    previewText->setAlignment(Qt::AlignCenter);
    auto *nav = new QHBoxLayout;
    previewPrev = new QToolButton(preview);
    previewPrev->setText("◀");
    previewPrev->setAutoRaise(true);
    previewPrev->setFocusPolicy(Qt::NoFocus);
    previewNext = new QToolButton(preview);
    previewNext->setText("▶");
    previewNext->setAutoRaise(true);
    previewNext->setFocusPolicy(Qt::NoFocus);
    nav->addWidget(previewPrev);
    nav->addWidget(previewText, 1);
    nav->addWidget(previewNext);
    pl->addWidget(previewImage, 0, Qt::AlignCenter);
    pl->addLayout(nav);
    preview->hide();
    connect(previewPrev, &QToolButton::clicked, this, [this]() {
        if (previewCluster < 0) return;
        const int n = clusters.at(previewCluster).ids.size();
        previewIndex = (previewIndex + n - 1) % n;
        updatePreview();
    });
    connect(previewNext, &QToolButton::clicked, this, [this]() {
        if (previewCluster < 0) return;
        const int n = clusters.at(previewCluster).ids.size();
        previewIndex = (previewIndex + 1) % n;
        updatePreview();
    });
    connect(previewImage, &QToolButton::clicked, this, &MapView::selectPreviewImage);

    syncControls();
    updateBanner();
    updateCount();
}

void MapView::setStyleState(const QString &key, bool customAvailable)
{
    customStyleAction->setEnabled(customAvailable);
    customStyleAction->setToolTip(customAvailable ? QString()
        : tr("Enter a tile URL in Preferences > Map to use a custom style"));
    for (QAction *a : styleGroup->actions()) {
        if (a->data().toString() != key) continue;
        a->setChecked(true);
        QString name = a->text();
        name = name.left(name.indexOf(" (")).trimmed();     // "Cycle (CyclOSM)" -> "Cycle"
        styleBtn->setText(name + "  ▾");
    }
    layoutOverlays();
}

void MapView::setProvider(const MapTileSource::Provider &p)
{
    tiles->setProvider(p);
    zoom = std::min(zoom, double(p.isValid() ? p.maxZoom : 19));
    syncControls();
    updateBanner();
    update();
}

// ---------------------------------------------------------------------------------------
// model

void MapView::activate()
{
    if (G::isLogger) G::log("MapView::activate");
    if (modelConnections.isEmpty()) connectModel();
    rebuildPoints();
    updateBanner();
}

void MapView::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    activate();
}

void MapView::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    disconnectModel();
    rebuildTimer.stop();
    hidePreview();
    /* A shape cannot be drawn on a page that has gone (another workflow, a load's
       message page): the Places dialog cancels. Not for a minimised window. */
    if (editing && !e->spontaneous()) emit placeEditCancelRequested();
}

void MapView::connectModel()
{
    /*  The proxy for a filter change, the datamodel for the rows themselves: points are
        datamodel rows, and a row the proxy filters out moves the rows after it without
        the proxy saying so. */
    auto *sf = dm->sf;
    auto rebuild = [this]() { scheduleRebuild(); };
    for (QAbstractItemModel *m : {static_cast<QAbstractItemModel *>(sf),
                                  static_cast<QAbstractItemModel *>(dm)}) {
        modelConnections << connect(m, &QAbstractItemModel::modelReset, this, rebuild);
        modelConnections << connect(m, &QAbstractItemModel::layoutChanged, this, rebuild);
        modelConnections << connect(m, &QAbstractItemModel::rowsInserted, this, rebuild);
        modelConnections << connect(m, &QAbstractItemModel::rowsRemoved, this, rebuild);
    }
    modelConnections << connect(dm, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles) {
            if (tl.column() <= G::GPSCoordColumn && br.column() >= G::GPSCoordColumn)
                scheduleRebuild();
            if (preview->isVisible() && tl.column() == 0
                && (roles.isEmpty() || roles.contains(Qt::DecorationRole)))
                updatePreview();
        });
    modelConnections << connect(dm->selectionModel, &QItemSelectionModel::selectionChanged,
                                this, [this]() { rebuildSelected(); update(); });
    modelConnections << connect(dm->selectionModel, &QItemSelectionModel::currentChanged,
                                this, [this]() { currentChanged(); });
}

void MapView::disconnectModel()
{
    for (const auto &c : std::as_const(modelConnections)) disconnect(c);
    modelConnections.clear();
}

void MapView::scheduleRebuild()
{
    rebuildTimer.start();
}

void MapView::rebuildPoints()
{
/*
    Every row with a parsable location that the filters show -- the filters other than
    the Map pin, which only dims (see the class note). The GUI thread only: the proxy
    is not safe off it.
*/
    if (G::isLogger) G::log("MapView::rebuildPoints");
    points.clear();
    rowToPoint.clear();
    totalRows = 0;
    dimmedPoints = 0;
    const int rows = dm->rowCount();
    for (int r = 0; r < rows; ++r) {
        if (!dm->sf->acceptsRowIgnoring(r, G::MapPinColumn)) continue;
        ++totalRows;
        const QString s = dm->index(r, G::GPSCoordColumn).data().toString();
        double la, lo;
        if (!Geo::parseCoord(s, la, lo)) continue;
        const bool inProxy = dm->sf->mapFromSource(dm->index(r, 0)).isValid();
        if (!inProxy) ++dimmedPoints;
        rowToPoint.insert(r, points.size());
        points.append({r, la, lo, inProxy});
    }
    clusterZoom = -1;
    hidePreview();
    rebuildSelected();
    updateCount();

    // A new folder or catalog is fitted once; a filter change keeps the view.
    if (fittedInstance != G::dmInstance && !points.isEmpty()) {
        fittedInstance = G::dmInstance;
        fitAll();
    }
    update();
}

void MapView::rebuildClusters()
{
    if (clusterZoom == zoom) return;
    QList<Geo::Point> pts;
    pts.reserve(points.size());
    for (const GeoPoint &p : std::as_const(points))
        pts.append({p.dmRow, Geo::lonLatToWorld(p.lat, p.lon, zoom)});
    clusters = Geo::cluster(pts, kClusterPx);
    clusterZoom = zoom;
    if (previewCluster >= clusters.size()) hidePreview();
}

void MapView::rebuildSelected()
{
    selected.clear();
    const QModelIndexList rows = dm->selectionModel->selectedRows();
    for (const QModelIndex &i : rows) {
        const QModelIndex src = dm->sf->mapToSource(i);
        if (src.isValid()) selected.insert(src.row());
    }
}

QList<int> MapView::sfRowsOf(const QList<int> &dmRows) const
{
    QList<int> out;
    out.reserve(dmRows.size());
    for (int r : dmRows) {
        const QModelIndex i = dm->sf->mapFromSource(dm->index(r, 0));
        if (i.isValid()) out.append(i.row());
    }
    return out;
}

bool MapView::clusterInProxy(int ci) const
{
    for (int id : clusters.at(ci).ids) {
        auto it = rowToPoint.constFind(id);
        if (it != rowToPoint.constEnd() && points.at(*it).inProxy) return true;
    }
    return false;
}

void MapView::selectPreviewImage()
{
/*
    The preview's thumbnail selects that image. One the Map pin filter leaves out is not
    in the grid to be selected, so its pin becomes the filter first.
*/
    if (previewCluster < 0) return;
    const QList<int> ids = clusters.at(previewCluster).ids;
    const int row = ids.at(previewIndex);
    const bool add = QGuiApplication::keyboardModifiers() & Qt::ControlModifier;
    if (sfRowsOf({row}).isEmpty()) emit filterToPin(ids, false);
    const QList<int> sfRows = sfRowsOf({row});
    if (!sfRows.isEmpty()) emit selectRows(sfRows, add);
}

void MapView::currentChanged()
{
    update();
    if (!followSelection) return;
    auto it = rowToPoint.constFind(dm->currentDmRow);
    if (it == rowToPoint.constEnd()) return;
    const GeoPoint &p = points.at(*it);
    const QPointF s = worldToScreen(Geo::lonLatToWorld(p.lat, p.lon, zoom));
    if (rect().adjusted(40, 40, -40, -40).contains(s.toPoint())) return;
    lat = p.lat;
    lon = p.lon;
    hidePreview();
}

// ---------------------------------------------------------------------------------------
// geometry

QPointF MapView::centreWorld() const
{
    return Geo::lonLatToWorld(lat, lon, zoom);
}

QPointF MapView::screenToWorld(const QPointF &s) const
{
    return centreWorld() + (s - QPointF(width() / 2.0, height() / 2.0));
}

QPointF MapView::worldToScreen(const QPointF &w) const
{
    return w - centreWorld() + QPointF(width() / 2.0, height() / 2.0);
}

void MapView::setZoom(double z, const QPointF &anchor)
{
/*
    Zoom keeping the place under `anchor` (a screen point) where it is -- the cursor for
    the wheel and the pinch, the centre for the buttons.
*/
    const double maxZ = tiles->provider().isValid() ? tiles->provider().maxZoom : 19;
    z = std::clamp(z, kMinZoom, maxZ);
    if (z == zoom) return;
    double aLat, aLon;
    Geo::worldToLonLat(screenToWorld(anchor), zoom, aLat, aLon);
    zoom = z;
    const QPointF c = Geo::lonLatToWorld(aLat, aLon, zoom)
                      - (anchor - QPointF(width() / 2.0, height() / 2.0));
    Geo::worldToLonLat(c, zoom, lat, lon);
    lat = std::clamp(lat, -Geo::kMaxLat, Geo::kMaxLat);
    lon = std::remainder(lon, 360.0);
    hidePreview();
    syncControls();
    update();
}

void MapView::setZoomCentred(double z)
{
    setZoom(z, QPointF(width() / 2.0, height() / 2.0));
}

void MapView::panBy(const QPointF &screenDelta)
{
    Geo::worldToLonLat(centreWorld() - screenDelta, zoom, lat, lon);
    lat = std::clamp(lat, -Geo::kMaxLat, Geo::kMaxLat);
    lon = std::remainder(lon, 360.0);
    update();
}

void MapView::fitAll()
{
    QList<int> rows;
    rows.reserve(points.size());
    for (const GeoPoint &p : std::as_const(points)) rows.append(p.dmRow);
    fitRows(rows);
}

void MapView::fitSelection()
{
    QList<int> rows;
    for (int r : std::as_const(selected))
        if (rowToPoint.contains(r)) rows.append(r);
    fitRows(rows);
}

void MapView::fitRows(const QList<int> &dmRows)
{
    if (dmRows.isEmpty()) return;
    QRectF b;
    for (int r : dmRows) {
        auto it = rowToPoint.constFind(r);
        if (it == rowToPoint.constEnd()) continue;
        const GeoPoint &p = points.at(*it);
        const QPointF w = Geo::lonLatToWorld(p.lat, p.lon, 0);
        const QRectF pt(w, QSizeF(1e-9, 1e-9));
        b = b.isNull() ? pt : b.united(pt);
    }
    if (b.isNull()) return;
    const double maxZ = tiles->provider().isValid() ? tiles->provider().maxZoom : 19;
    const double w = std::max(1.0, width() - 120.0);
    const double h = std::max(1.0, height() - 120.0);
    double z = std::log2(std::min(w / std::max(b.width(), 1e-6),
                                  h / std::max(b.height(), 1e-6)));
    z = std::clamp(std::floor(z * 2) / 2, kMinZoom, std::min(16.0, maxZ));
    Geo::worldToLonLat(b.center(), 0, lat, lon);
    zoom = z;
    hidePreview();
    syncControls();
    update();
}

void MapView::syncControls()
{
    const double maxZ = tiles->provider().isValid() ? tiles->provider().maxZoom : 19;
    QSignalBlocker block(zoomSlider);
    zoomSlider->setRange(int(kMinZoom * 10), int(maxZ * 10));
    zoomSlider->setValue(int(std::lround(zoom * 10)));
    zoomOutBtn->setEnabled(zoom > kMinZoom);
    zoomInBtn->setEnabled(zoom < maxZ);
    fitAllBtn->setEnabled(!points.isEmpty());
}

// ---------------------------------------------------------------------------------------
// painting

void MapView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), G::backgroundColor.darker(115));
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);

    // Tiles, from the nearest integer zoom scaled to the fractional one
    const MapTileSource::Provider &prov = tiles->provider();
    const QPointF cw = centreWorld();
    if (prov.isValid()) {
        const int z = std::clamp(int(std::lround(zoom)), 0, prov.maxZoom);
        const double s = std::pow(2.0, zoom - z);
        const double tilePx = Geo::kTileSize * s;
        const QRectF viewWorld(cw - QPointF(width() / 2.0, height() / 2.0),
                               QSizeF(width(), height()));
        const QRectF viewZ(viewWorld.topLeft() / s, viewWorld.size() / s);
        const Geo::TileRange tr = Geo::tileRange(viewZ, z);
        tiles->beginFrame();
        for (int y = tr.y0; y <= tr.y1; ++y) {
            for (int x = tr.x0; x <= tr.x1; ++x) {
                const QRectF target(worldToScreen(QPointF(x * tilePx, y * tilePx)),
                                    QSizeF(tilePx, tilePx));
                const int tx = Geo::wrapTileX(x, z);
                QPixmap pm = tiles->tile(z, tx, y);
                if (!pm.isNull()) {
                    p.drawPixmap(target, pm, QRectF(pm.rect()));
                    continue;
                }
                // Missing: the nearest cached ancestor, scaled up, until it arrives
                for (int d = 1; d <= 5 && z - d >= 0; ++d) {
                    const int px = tx >> d, py = y >> d;
                    const QPixmap parent = tiles->cachedTile(z - d, px, py);
                    if (parent.isNull()) continue;
                    const double sub = double(parent.width()) / (1 << d);
                    const QRectF src((tx - (px << d)) * sub, (y - (py << d)) * sub, sub, sub);
                    p.drawPixmap(target, parent, src);
                    break;
                }
            }
        }
        tiles->endFrame(z, cw.x() / s / Geo::kTileSize, cw.y() / s / Geo::kTileSize);
    }

    // Places: the checked ones, then the one being edited, under the pins
    for (const PlaceShape &ps : std::as_const(places)) {
        if (editing && ps.id == editId) continue;
        if (shownPlaces.contains(ps.id)) drawPlace(p, ps.place, ps.name, false);
    }
    if (editing) {
        drawPlace(p, editPlace, QString(), true);
        drawEditHandles(p);
    }

    // Pins: unselected first, so the selected ones sit on top
    rebuildClusters();
    const double worldW = Geo::worldSize(zoom);
    const QRectF visible = QRectF(rect()).adjusted(-30, -30, 30, 30);
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(std::max(7.0, f.pointSizeF() * 0.8));
    p.setFont(f);
    for (int pass = 0; pass < 2; ++pass) {
        for (int ci = 0; ci < clusters.size(); ++ci) {
            const Geo::Cluster &c = clusters.at(ci);
            bool isSel = false;
            bool isCurrent = false;
            for (int id : c.ids) {
                if (!isSel && selected.contains(id)) isSel = true;
                if (id == dm->currentDmRow) isCurrent = true;
            }
            // left out by the Map pin filter: there to click, but not what is showing
            const bool dim = dimmedPoints > 0 && !clusterInProxy(ci);
            if (isSel != (pass == 1)) continue;
            const int n = c.ids.size();
            const double r = pinRadius(n);
            for (int k = -1; k <= 1; ++k) {
                const QPointF sp = worldToScreen(c.world + QPointF(k * worldW, 0));
                if (!visible.contains(sp)) continue;
                if (isCurrent) {
                    p.setPen(QPen(Qt::white, 2));
                    p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sp, r + 4, r + 4);
                }
                p.setOpacity(dim && ci != hoverCluster ? 0.45 : 1.0);
                p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
                p.setBrush(isSel ? kPinSelectedColor : kPinColor);
                if (ci == hoverCluster) p.setBrush(p.brush().color().lighter(125));
                p.drawEllipse(sp, r, r);
                if (n > 1) {
                    p.setPen(QColor(20, 20, 20));
                    const QString t = n > 999 ? QString::number(n / 1000) + "k"
                                              : QString::number(n);
                    p.drawText(QRectF(sp.x() - r, sp.y() - r, 2 * r, 2 * r),
                               Qt::AlignCenter, t);
                }
                p.setOpacity(1.0);
            }
        }
    }

    if (dropHover) drawDropMarker(p);

    // Attribution: the provider's terms require it to be visible
    if (prov.isValid() && !prov.attribution.isEmpty()) {
        QFont af = font();
        af.setPointSizeF(std::max(7.0, af.pointSizeF() * 0.8));
        p.setFont(af);
        const QFontMetrics fm(af);
        const QString t = fm.elidedText(prov.attribution, Qt::ElideLeft, width() - 20);
        const QRect tr = fm.boundingRect(t).adjusted(-5, -2, 5, 2);
        const QRect box(width() - tr.width() - 4, height() - tr.height() - 4,
                        tr.width(), tr.height());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 20, 20, 190));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(QColor(230, 230, 230));
        p.drawText(box, Qt::AlignCenter, t);
    }
}

void MapView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutOverlays();
}

void MapView::layoutOverlays()
{
    controls->adjustSize();
    controls->move(width() - controls->width() - 8, 8);
    const int bw = std::min(560, width() - controls->width() - 40);
    banner->setFixedWidth(std::max(200, bw));
    banner->adjustSize();
    banner->move(8, 8);
    countLabel->adjustSize();
    countLabel->move(8, height() - countLabel->height() - 6);
}

void MapView::updateBanner()
{
    QString msg;
    if (!tiles->provider().isValid())
        msg = tr("The map tile URL in Preferences > Map is not a valid template, so the "
                 "map has no background. Clear it to use OpenStreetMap.");
    else
        msg = tiles->lastError();
    bannerText->setText(msg);
    banner->setVisible(!msg.isEmpty());
    layoutOverlays();
}

void MapView::updateCount()
{
    if (totalRows == 0) countLabel->setText(tr("No images"));
    else if (dimmedPoints > 0)
        countLabel->setText(tr("Showing the %1 images in the clicked pin -- %2 of %3 "
                               "images have a location")
                                .arg(points.size() - dimmedPoints)
                                .arg(points.size()).arg(totalRows));
    else countLabel->setText(tr("%1 of %2 images have a location")
                                 .arg(points.size()).arg(totalRows));
    syncControls();
    layoutOverlays();
}

// ---------------------------------------------------------------------------------------
// interaction

int MapView::clusterAt(const QPointF &pos) const
{
    const double worldW = Geo::worldSize(zoom);
    int best = -1;
    double bestD = 1e18;
    for (int ci = 0; ci < clusters.size(); ++ci) {
        const Geo::Cluster &c = clusters.at(ci);
        const double r = pinRadius(c.ids.size()) + 3;
        for (int k = -1; k <= 1; ++k) {
            const QPointF d = worldToScreen(c.world + QPointF(k * worldW, 0)) - pos;
            const double dd = std::hypot(d.x(), d.y());
            if (dd <= r && dd < bestD) {
                bestD = dd;
                best = ci;
            }
        }
    }
    return best;
}

void MapView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return QWidget::mousePressEvent(e);
    pressed = true;
    dragged = false;
    pressPos = lastPos = e->position();
    if (!editing) return;

    /*  EDIT MODE: a press on a handle (or a corner) grabs it; anywhere else it is the
        start of a pan, or of a click that adds a corner. The map takes the keyboard
        here, so Delete and Esc reach keyPressEvent. */
    setFocus(Qt::MouseFocusReason);
    editDrag = editHitTest(e->position());
    editDragged = false;
    if (editDrag >= 0 && editDrag < 100) {
        const QPointF c = editCentreScreen();
        editAnchorScreen = c;
        editAnchorRx = editPlace.rx;
        editAnchorRy = editPlace.ry;
        editAnchorAngle = editPlace.angleDeg;
        const QPointF d = e->position() - c;
        editGrabAngle = std::atan2(d.y(), d.x()) * 180.0 / kPi;
    }
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    if (editing) {
        hoverPos = pos;
        hoverValid = true;
        if (pressed && editDrag >= 0) {
            if (!editDragged && (pos - pressPos).manhattanLength() > 3)
                editDragged = true;
            if (editDragged) editMoveTo(pos, e->modifiers());
            return;
        }
        if (!pressed) {
            const int h = editHitTest(pos);
            if (h >= 0) setCursor(Qt::OpenHandCursor);
            else if (editPlace.shape == Geo::Place::Polygon && !polyClosed)
                setCursor(Qt::CrossCursor);
            else unsetCursor();
            if (editPlace.shape == Geo::Place::Polygon && !polyClosed) update();
            return;
        }
    }
    if (pressed) {
        if (!dragged && (pos - pressPos).manhattanLength() > 3) {
            dragged = true;
            setCursor(Qt::ClosedHandCursor);
            hidePreview();
        }
        if (dragged) {
            panBy(pos - lastPos);
            lastPos = pos;
        }
        return;
    }
    const int ci = clusterAt(pos);
    if (ci != hoverCluster) {
        hoverCluster = ci;
        setCursor(ci >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        setToolTip(ci >= 0 ? tr("Click to show only the images in this pin -- it "
                                "replaces the other filters.\nCmd+click to add the pin "
                                "to the ones showing, or take it out.\nDouble-click to "
                                "open them in the loupe.")
                           : QString());
        update();
    }
    if (ci >= 0 && ci != previewCluster) showPreview(ci);
    else if (ci < 0 && previewCluster >= 0 && !previewHideTimer.isActive())
        previewHideTimer.start();
}

void MapView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return QWidget::mouseReleaseEvent(e);
    const bool wasDrag = dragged;
    pressed = dragged = false;
    unsetCursor();
    if (editing) {
        const int grabbed = editDrag;
        const bool moved = editDragged;
        editDrag = -1;
        editDragged = false;
        if (ignoreNextRelease) {
            ignoreNextRelease = false;
            return;
        }
        if (moved) {
            emit placeEditChanged();
            return;
        }
        if (wasDrag || editPlace.shape != Geo::Place::Polygon || polyClosed) return;
        // a click: close on the first corner, add a corner anywhere else
        if (grabbed == 100 && editPlace.verts.size() >= 3) {
            polyClosed = true;
        }
        else if (grabbed < 0) {
            editPlace.verts << screenToLonLat(e->position());
        }
        else return;
        update();
        emit placeEditChanged();
        return;
    }
    if (wasDrag) return;
    const int ci = clusterAt(e->position());
    if (ci < 0) return;
    const bool add = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    emit filterToPin(clusters.at(ci).ids, add);
}

void MapView::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    if (editing) {
        /*  The first click of the pair has already added its corner (or closed the
            shape on corner 0); the double-click closes it, and the release that follows
            must not add another. */
        ignoreNextRelease = true;
        if (editPlace.shape == Geo::Place::Polygon && !polyClosed
            && editPlace.verts.size() >= 3) {
            polyClosed = true;
            update();
            emit placeEditChanged();
        }
        return;
    }
    const int ci = clusterAt(e->position());
    if (ci >= 0) {
        // the release before this one has already filtered to the pin
        emit openInLoupe();
        return;
    }
    setZoom(std::floor(zoom) + 1, e->position());
}

void MapView::wheelEvent(QWheelEvent *e)
{
/*
    A mouse wheel zooms, a trackpad scroll pans. Only the phase tells them apart: a
    wheel click arrives as a lone NoScrollPhase event, and its delta says nothing about
    how many clicks it was (24-26 on some mice, not 120), so each event is one step.
*/
    if (e->phase() == Qt::NoScrollPhase) {
        const int dy = e->angleDelta().y();
        if (dy != 0) setZoom(zoom + (dy > 0 ? 0.5 : -0.5), e->position());
    }
    else {
        QPointF d = e->pixelDelta();
        if (d.isNull()) d = QPointF(e->angleDelta()) / 8.0;
        hidePreview();
        panBy(d);
    }
    e->accept();
}

bool MapView::event(QEvent *e)
{
    /*  While a place is edited the map owns Esc, Delete and Backspace -- the last two
        are bound to global actions (delete images), which would otherwise take them. */
    if (e->type() == QEvent::ShortcutOverride && editing) {
        auto *k = static_cast<QKeyEvent *>(e);
        if (k->key() == Qt::Key_Escape || k->key() == Qt::Key_Delete
            || k->key() == Qt::Key_Backspace) {
            e->accept();
            return true;
        }
    }
    if (e->type() == QEvent::NativeGesture) {
        auto *g = static_cast<QNativeGestureEvent *>(e);
        if (g->gestureType() == Qt::ZoomNativeGesture) {
            setZoom(zoom + std::log2(1.0 + g->value()), g->position());
            return true;
        }
        if (g->gestureType() == Qt::SmartZoomNativeGesture) {
            setZoom(std::floor(zoom) + 1, g->position());
            return true;
        }
    }
    return QWidget::event(e);
}

void MapView::keyPressEvent(QKeyEvent *e)
{
    if (editing) {
        if (e->key() == Qt::Key_Escape) {
            emit placeEditCancelRequested();
            return;
        }
        if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
            removeLastCorner();
            return;
        }
    }
    QWidget::keyPressEvent(e);
}

void MapView::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    hoverValid = false;
    if (editing) update();
    if (hoverCluster >= 0) {
        hoverCluster = -1;
        update();
    }
    if (previewCluster >= 0 && !previewHideTimer.isActive()) previewHideTimer.start();
}

// ---------------------------------------------------------------------------------------
// hover preview

void MapView::showPreview(int ci)
{
    previewCluster = ci;
    previewIndex = 0;
    previewHideTimer.start();
    updatePreview();
}

void MapView::updatePreview()
{
    if (previewCluster < 0 || previewCluster >= clusters.size()) return hidePreview();
    const Geo::Cluster &c = clusters.at(previewCluster);
    const int n = c.ids.size();
    previewIndex = std::clamp(previewIndex, 0, n - 1);
    const int row = c.ids.at(previewIndex);
    const QModelIndex idx = dm->index(row, 0);
    const QIcon icon = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
    previewImage->setIcon(icon);
    previewImage->setText(icon.isNull() ? tr("No thumbnail yet") : QString());
    previewImage->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly
                                                   : Qt::ToolButtonIconOnly);
    const QString name = dm->index(row, G::NameColumn).data().toString();
    const QVariant created = dm->index(row, G::CreatedColumn).data();
    QString date = created.toDateTime().isValid()
                       ? created.toDateTime().toString("yyyy-MM-dd  HH:mm")
                       : created.toString();
    QString text = name;
    if (!date.isEmpty()) text += "\n" + date;
    if (n > 1) text += "\n" + tr("%1 of %2").arg(previewIndex + 1).arg(n);
    previewText->setText(text);
    previewPrev->setVisible(n > 1);
    previewNext->setVisible(n > 1);

    preview->adjustSize();
    const QPointF sp = worldToScreen(c.world);
    const double r = pinRadius(n);
    int x = int(sp.x() - preview->width() / 2.0);
    int y = int(sp.y() - r - 8 - preview->height());
    if (y < 4) y = int(sp.y() + r + 8);
    x = std::clamp(x, 4, std::max(4, width() - preview->width() - 4));
    y = std::clamp(y, 4, std::max(4, height() - preview->height() - 4));
    preview->move(x, y);
    preview->show();
    preview->raise();
}

void MapView::hidePreview()
{
    /* Also forgets the hovered pin: every caller is about to change the clusters (a
       zoom, new points) or has moved the pins away from the cursor (a pan). */
    previewHideTimer.stop();
    previewCluster = -1;
    hoverCluster = -1;
    if (preview) preview->hide();
}

// ---------------------------------------------------------------------------------------
// places

void MapView::setPlaces(const QList<PlaceShape> &list)
{
    places = list;
    update();
}

void MapView::setShownPlaces(const QList<qint64> &ids)
{
    shownPlaces = ids;
    update();
}

double MapView::worldScale() const
{
    return std::pow(2.0, zoom);
}

QPolygonF MapView::placeWorld(const Geo::Place &place) const
{
/*
    The outline in world pixels at the current zoom, in the world copy nearest the view
    centre: Geo::outlineWorld0 has already unwrapped it about its anchor, so the whole
    shape moves by one world width at a time.
*/
    QPolygonF poly = Geo::outlineWorld0(place);
    if (poly.isEmpty()) return poly;
    const double s = worldScale();
    for (QPointF &pt : poly) pt *= s;
    const double worldW = Geo::worldSize(zoom);
    const double k = std::round((centreWorld().x() - poly.boundingRect().center().x())
                                / worldW);
    if (k != 0) poly.translate(k * worldW, 0);
    return poly;
}

void MapView::drawPlace(QPainter &p, const Geo::Place &place, const QString &name,
                        bool editingThis)
{
    const QPolygonF w = placeWorld(place);
    if (w.isEmpty()) return;
    const double worldW = Geo::worldSize(zoom);
    const bool openPoly =
        editingThis && place.shape == Geo::Place::Polygon && !polyClosed;
    for (int k = -1; k <= 1; ++k) {
        QPolygonF s;
        s.reserve(w.size());
        for (const QPointF &pt : w) s << worldToScreen(pt + QPointF(k * worldW, 0));
        if (!QRectF(rect()).adjusted(-50, -50, 50, 50).intersects(s.boundingRect()))
            continue;
        QColor fill = kPlaceColor;
        fill.setAlpha(editingThis ? 45 : 60);
        p.setPen(QPen(QColor(255, 255, 255, 230), editingThis ? 1.6 : 2.0));
        if (openPoly) {
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(s);
            // the rubber band from the last corner to the cursor
            if (hoverValid && k == 0) {
                p.setPen(QPen(QColor(255, 255, 255, 170), 1.2, Qt::DashLine));
                p.drawLine(s.last(), hoverPos);
            }
        }
        else {
            p.setBrush(fill);
            p.drawPolygon(s);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(kPlaceColor, 1.0));
            p.drawPolygon(s);
        }
        if (!name.isEmpty()) {
            const QPointF c = s.boundingRect().center();
            QFont f = font();
            f.setBold(true);
            p.setFont(f);
            const QFontMetrics fm(f);
            const QRectF tr = QRectF(fm.boundingRect(name)).adjusted(-5, -2, 5, 2);
            const QRectF box(c.x() - tr.width() / 2, c.y() - tr.height() / 2,
                             tr.width(), tr.height());
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(20, 20, 20, 170));
            p.drawRoundedRect(box, 3, 3);
            p.setPen(QColor(235, 235, 235));
            p.drawText(box, Qt::AlignCenter, name);
        }
    }
}

QPointF MapView::editCentreScreen() const
{
    // the ellipse centre, in the world copy nearest the view centre
    QPointF w = Geo::lonLatToWorld(editPlace.lat, editPlace.lon, zoom);
    const double worldW = Geo::worldSize(zoom);
    w.rx() += std::round((centreWorld().x() - w.x()) / worldW) * worldW;
    return worldToScreen(w);
}

void MapView::ellipseHandles(QPointF h[6]) const
{
/*
    The radial mask's handle set (ImageView::maskRadialAxisHandles and
    maskRadialRotateHandleVp): centre, +x, -x, +y, -y, and the rotate knob a fixed
    kRotateKnobPx past +x, so it is as easy to grab at every zoom.
*/
    const QPointF c = editCentreScreen();
    const double s = worldScale();
    const double ax = editPlace.rx * s, ay = editPlace.ry * s;
    const double t = editPlace.angleDeg * kPi / 180.0;
    const QPointF ux(std::cos(t), std::sin(t)), uy(-std::sin(t), std::cos(t));
    h[0] = c;
    h[1] = c + ux * ax;
    h[2] = c - ux * ax;
    h[3] = c + uy * ay;
    h[4] = c - uy * ay;
    h[5] = h[1] + ux * kRotateKnobPx;
}

int MapView::editHitTest(const QPointF &pos) const
{
    if (!editing) return -1;
    if (editPlace.shape == Geo::Place::Ellipse) {
        if (!editPlace.isValid()) return -1;
        QPointF h[6];
        ellipseHandles(h);
        // centre first, then the knob, then the axis ends: maskHitTest's order
        for (int i : {0, 5, 1, 2, 3, 4}) {
            const QPointF d = h[i] - pos;
            if (std::hypot(d.x(), d.y()) <= kHandlePick) return i;
        }
        return -1;
    }
    const QPolygonF w = placeWorld(editPlace);
    for (int i = 0; i < w.size(); ++i) {
        const QPointF d = worldToScreen(w.at(i)) - pos;
        if (std::hypot(d.x(), d.y()) <= kHandlePick) return 100 + i;
    }
    return -1;
}

QPointF MapView::screenToLonLat(const QPointF &s) const
{
    double la = 0, lo = 0;
    Geo::worldToLonLat(screenToWorld(s), zoom, la, lo);
    return QPointF(std::remainder(lo, 360.0),
                   std::clamp(la, -Geo::kMaxLat, Geo::kMaxLat));
}

void MapView::setEditCentre(const QPointF &screen)
{
    const QPointF ll = screenToLonLat(screen);
    editPlace.lon = ll.x();
    editPlace.lat = ll.y();
}

void MapView::editMoveTo(const QPointF &pos, Qt::KeyboardModifiers mods)
{
/*
    A handle drag, ImageView's radial mask maths in screen pixels: the centre follows
    the cursor, an axis end sets its semi-axis from the cursor's distance along that
    axis (Shift scales both, keeping the shape), the knob turns it. Sizes are kept in
    zoom-0 world pixels, so they hold at every zoom.
*/
    const double s = worldScale();
    if (editDrag >= 100) {
        const int i = editDrag - 100;
        if (i < editPlace.verts.size()) editPlace.verts[i] = screenToLonLat(pos);
    }
    else if (editDrag == 0) {
        setEditCentre(editAnchorScreen + (pos - pressPos));
    }
    else if (editDrag == 5) {
        const QPointF d = pos - editAnchorScreen;
        const double a = std::atan2(d.y(), d.x()) * 180.0 / kPi;
        editPlace.angleDeg = std::remainder(editAnchorAngle + (a - editGrabAngle), 360.0);
    }
    else if (editDrag >= 1 && editDrag <= 4) {
        const QPointF d = pos - editAnchorScreen;
        const double t = editPlace.angleDeg * kPi / 180.0;
        const double lx = d.x() * std::cos(t) + d.y() * std::sin(t);
        const double ly = -d.x() * std::sin(t) + d.y() * std::cos(t);
        const double minPx = 4.0;
        const bool xAxis = editDrag <= 2;
        const double px = std::max(minPx, std::abs(xAxis ? lx : ly));
        if (mods & Qt::ShiftModifier) {
            const double anchor = (xAxis ? editAnchorRx : editAnchorRy) * s;
            const double f = anchor > 0 ? px / anchor : 1.0;
            editPlace.rx = std::max(minPx / s, editAnchorRx * f);
            editPlace.ry = std::max(minPx / s, editAnchorRy * f);
        }
        else if (xAxis) editPlace.rx = px / s;
        else editPlace.ry = px / s;
    }
    update();
}

void MapView::drawEditHandles(QPainter &p)
{
    auto handle = [&p](const QPointF &c, const QColor &fill, double r) {
        p.setPen(QPen(QColor(20, 20, 20, 220), 1.2));
        p.setBrush(fill);
        p.drawEllipse(c, r, r);
    };
    if (editPlace.shape == Geo::Place::Ellipse) {
        if (!editPlace.isValid()) return;
        QPointF h[6];
        ellipseHandles(h);
        p.setPen(QPen(QColor(255, 255, 255, 200), 1.0));
        p.drawLine(h[1], h[5]);
        for (int i = 1; i <= 4; ++i) handle(h[i], kAxisHandleColor, 5);
        handle(h[5], kRotateHandleColor, 5);
        handle(h[0], Qt::white, 4.5);
        return;
    }
    const QPolygonF w = placeWorld(editPlace);
    for (int i = 0; i < w.size(); ++i) {
        // corner 0 is larger while the shape is open: clicking it closes the shape
        const bool first = i == 0 && !polyClosed && w.size() >= 3;
        handle(worldToScreen(w.at(i)), first ? kRotateHandleColor : Qt::white,
               first ? 6 : 4);
    }
}

void MapView::beginPlaceEdit(Geo::Place::Shape shape, const Geo::Place *existing,
                             qint64 existingId)
{
/*
    A new ellipse starts centred in the view, a quarter of its size; a new polygon
    starts empty, waiting for its first corner. An existing place is edited as it is,
    and the view moves to it.
*/
    if (G::isLogger) G::log("MapView::beginPlaceEdit");
    editing = true;
    editId = existingId;
    editDrag = -1;
    editDragged = false;
    ignoreNextRelease = false;
    hidePreview();
    if (existing && existing->shape == shape && existing->isValid()) {
        editPlace = *existing;
        polyClosed = true;
    }
    else {
        editPlace = Geo::Place();
        editPlace.shape = shape;
        polyClosed = false;
        if (shape == Geo::Place::Ellipse) {
            const double s = worldScale();
            setEditCentre(QPointF(width() / 2.0, height() / 2.0));
            editPlace.rx = std::max(20.0, width() / 4.0) / s;
            editPlace.ry = std::max(20.0, height() / 4.0) / s;
        }
    }
    setFocusPolicy(Qt::StrongFocus);
    setFocus(Qt::OtherFocusReason);
    update();
    emit placeEditChanged();
}

void MapView::endPlaceEdit()
{
    if (!editing) return;
    editing = false;
    editId = 0;
    editDrag = -1;
    editPlace = Geo::Place();
    polyClosed = false;
    setFocusPolicy(Qt::NoFocus);
    unsetCursor();
    update();
}

bool MapView::placeEditComplete() const
{
    if (!editing || !editPlace.isValid()) return false;
    return editPlace.shape == Geo::Place::Ellipse || polyClosed;
}

Geo::Place MapView::editedPlace() const
{
    return editPlace;
}

void MapView::removeLastCorner()
{
    if (!editing || editPlace.shape != Geo::Place::Polygon || editPlace.verts.isEmpty())
        return;
    editPlace.verts.removeLast();
    polyClosed = false;
    update();
    emit placeEditChanged();
}

void MapView::fitPlaces(const QList<qint64> &ids)
{
    QRectF b;
    for (const PlaceShape &ps : std::as_const(places)) {
        if (!ids.contains(ps.id)) continue;
        const QRectF r = Geo::outlineWorld0(ps.place).boundingRect();
        b = b.isNull() ? r : b.united(r);
    }
    if (b.isNull() || b.width() <= 0 || b.height() <= 0) return;
    const double maxZ = tiles->provider().isValid() ? tiles->provider().maxZoom : 19;
    const double w = std::max(1.0, width() - 120.0);
    const double h = std::max(1.0, height() - 120.0);
    double z = std::log2(std::min(w / b.width(), h / b.height()));
    z = std::clamp(std::floor(z * 2) / 2, kMinZoom, maxZ);
    Geo::worldToLonLat(b.center(), 0, lat, lon);
    lon = std::remainder(lon, 360.0);
    zoom = z;
    hidePreview();
    syncControls();
    update();
}

// ---------------------------------------------------------------------------------------
// drag to geotag

QList<int> MapView::dropRows(const QMimeData *mime) const
{
/*
    The datamodel rows of the loaded images the drag names. IconView puts each file's
    source path in the urls (and, with G::includeSidecars, its companions, which match
    no row). A version is its file, so its row is not listed: MW::geotagRows sets every
    row of each file.
*/
    QList<int> rows;
    if (!mime || !mime->hasUrls()) return rows;
    QSet<int> seen;
    for (const QUrl &u : mime->urls()) {
        if (!u.isLocalFile()) continue;
        const int r = dm->rowFromKey(u.toLocalFile());
        if (r >= 0 && !seen.contains(r)) {
            seen.insert(r);
            rows << r;
        }
    }
    return rows;
}

void MapView::dragEnterEvent(QDragEnterEvent *e)
{
    dropCount = dropRows(e->mimeData()).size();
    if (dropCount == 0) {
        e->ignore();
        return;
    }
    // COPY, never the proposed Move: see the class note
    e->setDropAction(Qt::CopyAction);
    e->accept();
    hidePreview();
    dropHover = true;
    dropPos = e->position();
    update();
}

void MapView::dragMoveEvent(QDragMoveEvent *e)
{
    if (dropCount == 0) {
        e->ignore();
        return;
    }
    e->setDropAction(Qt::CopyAction);
    e->accept();
    dropHover = true;
    dropPos = e->position();
    update();
}

void MapView::dragLeaveEvent(QDragLeaveEvent *e)
{
    QWidget::dragLeaveEvent(e);
    dropHover = false;
    update();
}

void MapView::dropEvent(QDropEvent *e)
{
    const QList<int> rows = dropRows(e->mimeData());
    dropHover = false;
    update();
    if (rows.isEmpty()) {
        e->ignore();
        return;
    }
    // set before the work, so the source can never see a Move (IconView::startDrag)
    e->setDropAction(Qt::CopyAction);
    e->accept();
    const QPointF ll = screenToLonLat(e->position());
    /*  Queued: the geotag may ask a question (replace existing locations?), and a
        modal dialog must not run inside the drag's own event loop. */
    QMetaObject::invokeMethod(this, [this, rows, ll] {
        emit geotagRequested(rows, ll.y(), ll.x());
    }, Qt::QueuedConnection);
}

void MapView::drawDropMarker(QPainter &p)
{
    // a pin with its point at the drop location, and what the drop will do
    const QPointF tip = dropPos;
    const double r = 9;
    const QPointF head = tip - QPointF(0, 22);
    QPainterPath pin;
    pin.moveTo(tip);
    pin.lineTo(head + QPointF(-r * 0.75, r * 0.6));
    pin.arcTo(QRectF(head.x() - r, head.y() - r, 2 * r, 2 * r), 217, -254);
    pin.closeSubpath();
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(kPinSelectedColor);
    p.drawPath(pin);
    p.setBrush(QColor(20, 20, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(head, 3.0, 3.0);

    const QString text = dropCount == 1
        ? tr("Drop to set the location of this image")
        : tr("Drop to set the location of %1 images").arg(dropCount);
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    const QFontMetrics fm(f);
    QRectF box = QRectF(fm.boundingRect(text)).adjusted(-6, -3, 6, 3);
    box.moveCenter(QPointF(tip.x(), head.y() - r - 6 - box.height() / 2));
    p.setBrush(QColor(20, 20, 20, 210));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(QColor(235, 235, 235));
    p.drawText(box, Qt::AlignCenter, text);
}
