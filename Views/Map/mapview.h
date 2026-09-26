#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QWidget>
#include <QSet>
#include <QTimer>
#include "Utilities/geo.h"
#include "Views/Map/maptilesource.h"

class DataModel;
class QLabel;
class QPushButton;
class QSlider;
class QToolButton;
class QFrame;
class QActionGroup;
class QAction;

/*
    MAP VIEW: the central page of the Map module (MW::invokeMapWorkflow). A world map of
    raster tiles (MapTileSource) with a pin for every image in the current -- filtered --
    proxy that carries a GPS location.

    A PLAIN QWidget, NOT Qt Location. Qt 6's Qt Location is QML only: it would bring a
    QML runtime, a QQuickWidget and QML plugins into the bundle for one page. The whole
    map here is a tile grid, a projection (Utilities/geo.h) and a paintEvent.

    STATE is the centre (lat/lon) and a fractional zoom. Tiles are drawn from the nearest
    integer zoom, scaled by the difference. Pins are grouped into clusters of about
    kClusterPx on screen (Geo::cluster), rebuilt only when the zoom or the points change.

    POINTS are read from dm->sf on the GUI thread (the proxy is not safe off it) by
    rebuildPoints, which a debounced timer runs when the proxy's rows, filter or GPS
    column change. Those connections exist ONLY while the map is showing (activate /
    hideEvent): the model's signals already fan out to three views, and a hidden map has
    no business adding to that.

    SELECTION goes out, never in: a click emits selectRows and MW hands it to
    Selection::selectRows, so the map changes the selection the same way every view
    does. The map reads the selection model to highlight pins and to follow the current
    image.
*/

class MapView : public QWidget
{
    Q_OBJECT
public:
    MapView(QWidget *parent, DataModel *dm);

    void setProvider(const MapTileSource::Provider &p);
    /* The style menu: which style is in use ("standard", "cyclosm", "opentopo" or
       "custom") and whether a custom URL is set in Preferences > Map. */
    void setStyleState(const QString &key, bool customAvailable);
    void activate();            // connect to the model and read the points
    void fitAll();
    void fitSelection();

    bool followSelection = true;

signals:
    void selectRows(const QList<int> &sfRows, bool add);
    void openInLoupe();
    void openPreferences();
    void styleChosen(const QString &key);

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void hideEvent(QHideEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent *) override;
    bool event(QEvent *) override;

private:
    struct GeoPoint { int sfRow; double lat, lon; };

    void connectModel();
    void disconnectModel();
    void scheduleRebuild();
    void rebuildPoints();
    void rebuildClusters();
    void rebuildSelected();
    void currentChanged();

    QPointF centreWorld() const;
    QPointF screenToWorld(const QPointF &s) const;
    QPointF worldToScreen(const QPointF &w) const;
    void setZoom(double z, const QPointF &anchor);
    void setZoomCentred(double z);
    void panBy(const QPointF &screenDelta);
    void fitRows(const QList<int> &sfRows);
    void syncControls();

    int clusterAt(const QPointF &pos) const;
    void showPreview(int ci);
    void updatePreview();
    void hidePreview();
    void layoutOverlays();
    void updateBanner();
    void updateCount();

    DataModel *dm;
    MapTileSource *tiles;
    QList<QMetaObject::Connection> modelConnections;
    QTimer rebuildTimer;
    QTimer previewHideTimer;

    double lat = 20, lon = 0, zoom = 2;
    static constexpr double kMinZoom = 1;
    static constexpr double kClusterPx = 44;

    QList<GeoPoint> points;
    QHash<int, int> rowToPoint;             // sfRow -> index in points
    QList<Geo::Cluster> clusters;           // world at clusterZoom
    double clusterZoom = -1;
    QSet<int> selected;                     // selected sfRows
    int totalRows = 0;
    int fittedInstance = -1;                // G::dmInstance the view was fitted to

    // interaction
    bool pressed = false;
    bool dragged = false;
    QPointF pressPos, lastPos;
    int hoverCluster = -1;

    // overlays
    QWidget *controls = nullptr;
    QToolButton *zoomOutBtn = nullptr, *zoomInBtn = nullptr;
    QSlider *zoomSlider = nullptr;
    QToolButton *fitAllBtn = nullptr, *fitSelBtn = nullptr, *followBtn = nullptr;
    QToolButton *styleBtn = nullptr;
    QActionGroup *styleGroup = nullptr;
    QAction *customStyleAction = nullptr;
    QFrame *banner = nullptr;
    QLabel *bannerText = nullptr;
    QPushButton *bannerBtn = nullptr;
    QLabel *countLabel = nullptr;
    QFrame *preview = nullptr;
    QToolButton *previewImage = nullptr;    // click: select that image
    QLabel *previewText = nullptr;
    QToolButton *previewPrev = nullptr, *previewNext = nullptr;
    int previewCluster = -1;
    int previewIndex = 0;
};

#endif // MAPVIEW_H
