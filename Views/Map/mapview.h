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
class QMimeData;

/*
    MAP VIEW: the central page of the Map module (MW::invokeMapWorkflow). A world map of
    raster tiles (MapTileSource) with a pin for every image the filters show that
    carries a GPS location.

    A PLAIN QWidget, NOT Qt Location. Qt 6's Qt Location is QML only: it would bring a
    QML runtime, a QQuickWidget and QML plugins into the bundle for one page. The whole
    map here is a tile grid, a projection (Utilities/geo.h) and a paintEvent.

    STATE is the centre (lat/lon) and a fractional zoom. Tiles are drawn from the nearest
    integer zoom, scaled by the difference. Pins are grouped into clusters of about
    kClusterPx on screen (Geo::cluster), rebuilt only when the zoom or the points change.

    POINTS are read on the GUI thread (the proxy is not safe off it) by rebuildPoints,
    which a debounced timer runs when the rows, the filter or the GPS column change.
    Those connections exist ONLY while the map is showing (activate /
    hideEvent): the model's signals already fan out to three views, and a hidden map has
    no business adding to that.

    A PIN CLICK FILTERS: it emits filterToPin and MW::applyMapPinFilter replaces every
    filter with the Filters "Map pin" category, as a Collection or Bookmark click does
    (Cmd/Shift+click adds the pin to it, or takes it out). So the map cannot draw only
    the proxy's rows, or the click would leave nothing else to click: it draws every
    row the OTHER filters admit (SortFilter::acceptsRowIgnoring on G::MapPinColumn),
    and dims the pins the Map pin filter leaves out. Points are therefore DATAMODEL
    rows; the proxy row is looked up only to select.

    PLACES (Main/mwplaces.cpp) are areas the user draws here -- an ellipse or a polygon.
    The ones the Places filter has checked are drawn (setPlaces / setShownPlaces), in the
    same Web Mercator space Geo::contains tests, so the outline is exactly the boundary
    the filter uses. EDIT MODE (beginPlaceEdit .. endPlaceEdit), driven by the Places
    dialog, gives the clicks to the shape: an ellipse has the radial mask's handles
    (centre, four axis ends, a rotate knob); a polygon gains a corner per click and
    closes on a double-click or a click on its first corner. A drag still pans and the
    wheel still zooms, but pins neither filter nor preview while a place is edited.

    DRAG TO GEOTAG. Thumbnails dropped on the map ask MW::geotagRows to give those
    images the location under the drop (geotagRequested). The drop is a COPY, set on
    dragEnter, dragMove AND drop: IconView trashes the originals when a drop target
    takes the default Move (see IconView::startDrag). Only urls that name loaded images
    count; the rest of the drag (sidecars, files from elsewhere) is ignored.

    SELECTION goes out, never in: the preview's thumbnail emits selectRows and MW hands
    it to Selection::selectRows, so the map changes the selection the same way every
    view does. The map reads the selection model to highlight pins and to follow the
    current image.
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

    /*  THE VIEW, persisted across restarts (MW::writeSettings -> QSettings mapLat,
        mapLon, mapZoom). A restored view also stands in for the first load's automatic
        fit, so the map reopens where it was left rather than jumping to the images. */
    void setView(double lat, double lon, double zoom);
    double viewLat() const { return lat; }
    double viewLon() const { return lon; }
    double viewZoom() const { return zoom; }
    bool hasBeenShown() const { return shownOnce; }

    // places: see the class note
    struct PlaceShape { qint64 id; QString name; Geo::Place place; };
    void setPlaces(const QList<PlaceShape> &places);
    void setShownPlaces(const QList<qint64> &ids);
    void fitPlaces(const QList<qint64> &ids);
    /* Edit a new place of this shape, or (existing) an existing one's. Ends any edit
       in progress. */
    void beginPlaceEdit(Geo::Place::Shape shape, const Geo::Place *existing = nullptr,
                        qint64 existingId = 0);
    void endPlaceEdit();
    bool isEditingPlace() const { return editing; }
    qint64 editingPlaceId() const { return editId; }
    /* A closed polygon of 3+ corners, or an ellipse: what Done needs. */
    bool placeEditComplete() const;
    int placeEditCorners() const { return editPlace.verts.size(); }
    Geo::Place editedPlace() const;
    void removeLastCorner();

    bool followSelection = true;

signals:
    void selectRows(const QList<int> &sfRows, bool add);
    /* Filter to the images in a pin (datamodel rows); add toggles them in an existing
       Map pin filter. See MW::applyMapPinFilter. */
    void filterToPin(const QList<int> &dmRows, bool add);
    void openInLoupe();
    void openPreferences();
    void styleChosen(const QString &key);
    /* The place being edited changed (a corner, a handle drag, closed). */
    void placeEditChanged();
    /* Esc on the map while a place is edited: the dialog cancels. */
    void placeEditCancelRequested();
    /* Thumbnails were dropped on the map: these datamodel rows, at this location. */
    void geotagRequested(const QList<int> &dmRows, double lat, double lon);

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
    void keyPressEvent(QKeyEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;
    bool event(QEvent *) override;

private:
    struct GeoPoint { int dmRow; double lat, lon; bool inProxy; };

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
    void fitRows(const QList<int> &dmRows);
    QList<int> sfRowsOf(const QList<int> &dmRows) const;
    bool clusterInProxy(int ci) const;
    void selectPreviewImage();
    void syncControls();

    int clusterAt(const QPointF &pos) const;
    void showPreview(int ci);
    void updatePreview();
    void hidePreview();
    void layoutOverlays();
    void updateBanner();
    void updateCount();

    // places
    double worldScale() const;                 // zoom-0 world px -> world px now
    QPolygonF placeWorld(const Geo::Place &p) const;
    void drawPlace(QPainter &p, const Geo::Place &place, const QString &name,
                   bool editingThis);
    void drawEditHandles(QPainter &p);
    QPointF editCentreScreen() const;
    void ellipseHandles(QPointF h[6]) const;   // centre, +x, -x, +y, -y, rotate
    int editHitTest(const QPointF &pos) const;
    void setEditCentre(const QPointF &screen);
    void editMoveTo(const QPointF &pos, Qt::KeyboardModifiers mods);
    QPointF screenToLonLat(const QPointF &s) const;    // x = lon, y = lat
    QList<int> dropRows(const QMimeData *mime) const;
    void drawDropMarker(QPainter &p);

    DataModel *dm;
    MapTileSource *tiles;
    QList<QMetaObject::Connection> modelConnections;
    QTimer rebuildTimer;
    QTimer previewHideTimer;

    double lat = 20, lon = 0, zoom = 2;
    static constexpr double kMinZoom = 1;
    static constexpr double kClusterPx = 44;

    QList<GeoPoint> points;
    QHash<int, int> rowToPoint;             // dmRow -> index in points
    QList<Geo::Cluster> clusters;           // world at clusterZoom; ids are dmRows
    double clusterZoom = -1;
    QSet<int> selected;                     // selected dmRows
    int totalRows = 0;                      // rows the other filters admit
    int dimmedPoints = 0;                   // points the Map pin filter leaves out
    QString fittedDataset;                  // key of dm row 0 when last fitted
    bool viewRestored = false;              // setView: skip the first automatic fit
    bool shownOnce = false;

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

    // places
    QList<PlaceShape> places;
    QList<qint64> shownPlaces;
    bool editing = false;
    qint64 editId = 0;                      // 0: a new place
    Geo::Place editPlace;
    bool polyClosed = false;
    /* The handle a press grabbed: -1 none (the press pans), 0 centre, 1..4 the axis
       ends, 5 the rotate knob, 100+i polygon corner i. */
    int editDrag = -1;
    bool editDragged = false;
    bool ignoreNextRelease = false;         // the second release of a double-click
    QPointF editAnchorScreen;               // centre at the press (screen)
    double editAnchorRx = 0, editAnchorRy = 0, editAnchorAngle = 0, editGrabAngle = 0;
    QPointF hoverPos;                       // the rubber band's free end
    bool hoverValid = false;

    // drag to geotag
    bool dropHover = false;
    QPointF dropPos;
    int dropCount = 0;
};

#endif // MAPVIEW_H
