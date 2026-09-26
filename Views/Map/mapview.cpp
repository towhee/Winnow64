#include "Views/Map/mapview.h"

#include <QtWidgets>
#include <cmath>
#include "Datamodel/datamodel.h"
#include "Main/global.h"

namespace {
/* Lightroom's orange for a pin, and the Module dock's selected-option yellow
   (kSelectedOptionColor in initialize.cpp) for a selected one. */
const QColor kPinColor("#e8742c");
const QColor kPinSelectedColor("#f0c419");

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
    connect(previewImage, &QToolButton::clicked, this, [this]() {
        if (previewCluster < 0) return;
        const bool add = QGuiApplication::keyboardModifiers() & Qt::ControlModifier;
        emit selectRows({clusters.at(previewCluster).ids.at(previewIndex)}, add);
    });

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
}

void MapView::connectModel()
{
    auto *sf = dm->sf;
    auto rebuild = [this]() { scheduleRebuild(); };
    modelConnections << connect(sf, &QAbstractItemModel::modelReset, this, rebuild);
    modelConnections << connect(sf, &QAbstractItemModel::layoutChanged, this, rebuild);
    modelConnections << connect(sf, &QAbstractItemModel::rowsInserted, this, rebuild);
    modelConnections << connect(sf, &QAbstractItemModel::rowsRemoved, this, rebuild);
    modelConnections << connect(sf, &QAbstractItemModel::dataChanged, this,
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
    Every proxy row with a parsable location. The proxy, not the datamodel: the map
    shows what the filters show, as the grid does. The GUI thread only.
*/
    if (G::isLogger) G::log("MapView::rebuildPoints");
    points.clear();
    rowToPoint.clear();
    totalRows = dm->sf->rowCount();
    for (int r = 0; r < totalRows; ++r) {
        const QString s = dm->sf->index(r, G::GPSCoordColumn).data().toString();
        double la, lo;
        if (!Geo::parseCoord(s, la, lo)) continue;
        rowToPoint.insert(r, points.size());
        points.append({r, la, lo});
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
        pts.append({p.sfRow, Geo::lonLatToWorld(p.lat, p.lon, zoom)});
    clusters = Geo::cluster(pts, kClusterPx);
    clusterZoom = zoom;
    if (previewCluster >= clusters.size()) hidePreview();
}

void MapView::rebuildSelected()
{
    selected.clear();
    const QModelIndexList rows = dm->selectionModel->selectedRows();
    for (const QModelIndex &i : rows) selected.insert(i.row());
}

void MapView::currentChanged()
{
    update();
    if (!followSelection) return;
    auto it = rowToPoint.constFind(dm->currentSfRow);
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
    for (const GeoPoint &p : std::as_const(points)) rows.append(p.sfRow);
    fitRows(rows);
}

void MapView::fitSelection()
{
    QList<int> rows;
    for (int r : std::as_const(selected))
        if (rowToPoint.contains(r)) rows.append(r);
    fitRows(rows);
}

void MapView::fitRows(const QList<int> &sfRows)
{
    if (sfRows.isEmpty()) return;
    QRectF b;
    for (int r : sfRows) {
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
                if (id == dm->currentSfRow) isCurrent = true;
            }
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
            }
        }
    }

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
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
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
    if (wasDrag) return;
    const int ci = clusterAt(e->position());
    if (ci < 0) return;
    const bool add = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    emit selectRows(clusters.at(ci).ids, add);
}

void MapView::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    const int ci = clusterAt(e->position());
    if (ci >= 0) {
        emit selectRows(clusters.at(ci).ids, false);
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

void MapView::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
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
    const QModelIndex idx = dm->sf->index(row, 0);
    const QIcon icon = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
    previewImage->setIcon(icon);
    previewImage->setText(icon.isNull() ? tr("No thumbnail yet") : QString());
    previewImage->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly
                                                   : Qt::ToolButtonIconOnly);
    const QString name = dm->sf->index(row, G::NameColumn).data().toString();
    const QVariant created = dm->sf->index(row, G::CreatedColumn).data();
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
