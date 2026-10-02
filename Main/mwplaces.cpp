#include "Main/mainwindow.h"
#include "Datamodel/collectionstore.h"
#include "Dialogs/placedlg.h"
#include "Views/Map/placelist.h"

#include <QInputDialog>
#include <QMessageBox>

/*
    PLACES -- the Map module's saved areas, Lightroom's Saved Locations.

    NAMED "PLACES", NOT "LOCATIONS": the keyword tree already has a Location branch
    (Location|Canada|BC|...), and a panel of the same name that has nothing to do with
    it would be read as a view of it.

    A PLACE is an ellipse or a polygon the user draws on the map, with a name. It is
    stored in collections.db as a node of CollectionStore::Kind::Place, its shape in the
    node's definition (Geo::toJson -- a published format). Places may overlap; an image
    is in every place its GPS location falls inside.

    A PLACE IS A FILTER, as a collection is: an item in the Filters "Places" category,
    which filters on G::PlacesColumn. DataModel answers that column from a side table,
    source path -> place ids, which refreshPlaceMembership works out from each loaded
    row's GPS coordinates (Geo::contains, in the Web Mercator space the map draws in, so
    the outline on the map is exactly the filter's boundary). It is rebuilt after every
    filter build (so after every load), when the places change, and -- debounced -- when
    rows or their GPS coordinates change.

    NOT THE LIBRARY'S. Unlike Collections and Queries, places are pure geometry over
    whatever is loaded, so the panel and the category work in Folders as well.

    THE PANEL (Views/Map/placelist.h) is a second view of the category: its selection
    is the filter. A click replaces every filter with that place, as a Collection or
    Bookmark click does; Cmd+click and Shift+click change the set.
    syncPlaceListFromFilters pushes the category back after every filter change, and
    has the map draw the checked places.

    DRAWING ONE: the + button opens the non-modal PlaceDlg (name, type, directions,
    Cancel / Done) and puts the map into edit mode (MapView::beginPlaceEdit).

    See notes/Documentation.txt "The Map Module > Places".
*/

using Kind = CollectionStore::Kind;

void MW::createPlacesDock()
{
    if (G::isLogger) G::log("MW::createPlacesDock");
    placesDockTabText = "Places";
    dockTextNames << placesDockTabText;
    placesDock = new DockWidget(placesDockTabText, "PlacesDock", this);
    placesDock->setObjectName("PlacesDock");

    QWidget *body = new QWidget(placesDock);
    QVBoxLayout *layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    placesReason = new QLabel(body);
    placesReason->setWordWrap(true);
    placesReason->setVisible(false);
    layout->addWidget(placesReason);

    placeList = new PlaceList(body);
    layout->addWidget(placeList, 1);

    /*  THE EMPTY STATE SAYS HOW TO BEGIN. */
    placesEmptyHint = new QLabel(
        tr("No places yet.\n\nClick + above to draw one on the map -- an ellipse or a "
           "polygon around somewhere you take pictures. Then click it here to show the "
           "images taken inside it."), body);
    placesEmptyHint->setWordWrap(true);
    placesEmptyHint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    placesEmptyHint->setStyleSheet(QString("color: %1;").arg(G::disabledColor.name()));
    placesEmptyHint->setVisible(false);
    layout->addWidget(placesEmptyHint);

    placesDock->setWidget(body);
    placesDock->setFloating(false);
    placesDock->setVisible(false);
    connect(placesDock, &DockWidget::focus, this, &MW::focusOnDock);

    connect(placeList, &PlaceList::filterRequested, this, &MW::applyPlaceFilter);
    connect(placeList, &PlaceList::newRequested, this, &MW::newPlace);
    connect(placeList, &PlaceList::renameRequested, this, &MW::renamePlace);
    connect(placeList, &PlaceList::editRequested, this, &MW::editPlace);
    connect(placeList, &PlaceList::deleteRequested, this, &MW::deletePlace);
    connect(placeList, &PlaceList::zoomRequested, this, [this](qint64 id) {
        if (!mapView) return;
        if (!inMapModule()) invokeMapWorkflow();
        mapView->fitPlaces({id});
    });

    // title bar: New, Cancel, collapse, close
    QHBoxLayout *titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    DockTitleBar *titleBar = new DockTitleBar(placesDockTabText, titleLayout);
    placesDock->setTitleBarWidget(titleBar);
    titleBar->setToolTip(dockTabToolTip(placesDockTabText));

    placesNewBtn = new BarBtn();
    placesNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    placesNewBtn->setToolTip(tr("New place: draw an ellipse or a polygon on the map"));
    connect(placesNewBtn, &BarBtn::clicked, this, &MW::newPlace);
    titleLayout->addWidget(placesNewBtn);
    titleLayout->addSpacing(10);

    placesCancelBtn = new BarBtn();
    placesCancelBtn->setIcon(":/images/icon16/reset.png", G::iconOpacity);
    connect(placesCancelBtn, &BarBtn::clicked, this, &MW::cancelPlaceFilter);
    titleLayout->addWidget(placesCancelBtn);
    titleLayout->addSpacing(10);

    if (G::useDWCollapse) {
        BarBtn *collapseBtn = new BarBtn();
        collapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        collapseBtn->setToolTip("Collapse panel.");
        connect(collapseBtn, &BarBtn::clicked, placesDock, &DockWidget::toggleCollapsed);
        connect(placesDock, &DockWidget::collapsedChanged, collapseBtn,
                [collapseBtn](bool c) {
            collapseBtn->setIcon(c ? ":/images/icon16/expand.png"
                                   : ":/images/icon16/collapse.png", G::iconOpacity);
            collapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        titleLayout->addWidget(collapseBtn);
        titleLayout->addSpacing(10);
    }

    BarBtn *closeBtn = new BarBtn();
    closeBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    closeBtn->setToolTip(tr("Hide the Places Panel"));
    connect(closeBtn, &BarBtn::clicked, this, &MW::closePlacesDock);
    titleLayout->addWidget(closeBtn);
    titleLayout->addSpacing(5);

    /*  The store drives the category's items, the list and the map's outlines; the
        rows drive the membership. EVERY FILTER BUILD BLANKS THE COUNTS (as for
        collections), and a build follows every load, so membership and counts are
        put back after it. */
    connect(&CollectionStore::instance(), &CollectionStore::nodesChanged, this,
            [this](Kind k) {
        if (k != Kind::Place) return;
        refreshPlaceNodes();
        refreshPlaceMembership();
    });
    connect(buildFilters, &BuildFilters::finishedBuildFilters,
            this, &MW::refreshPlaceMembership);

    placeRefreshTimer = new QTimer(this);
    placeRefreshTimer->setSingleShot(true);
    placeRefreshTimer->setInterval(400);
    connect(placeRefreshTimer, &QTimer::timeout, this, &MW::refreshPlaceMembership);
    auto later = [this] { if (placeRefreshTimer) placeRefreshTimer->start(); };
    connect(dm, &QAbstractItemModel::rowsInserted, this, later);
    connect(dm, &QAbstractItemModel::rowsRemoved, this, later);
    connect(dm, &QAbstractItemModel::modelReset, this, later);
    connect(dm, &QAbstractItemModel::dataChanged, this,
            [later](const QModelIndex &tl, const QModelIndex &br) {
        if (tl.column() <= G::GPSCoordColumn && br.column() >= G::GPSCoordColumn) later();
    });

    refreshPlaceNodes();
    refreshPlaceMembership();
}

void MW::wirePlacesToMap()
{
/*
    The map is built after the docks (setupCentralWidget follows createDocks), so its
    side of the Places wiring waits for it.
*/
    if (!mapView) return;
    connect(mapView, &MapView::placeEditChanged, this, [this] {
        if (placeDlg)
            placeDlg->setShapeState(mapView->placeEditComplete(),
                                    mapView->placeEditCorners());
    });
    connect(mapView, &MapView::placeEditCancelRequested, this, [this] {
        if (placeDlg && placeDlg->isVisible()) placeDlg->reject();
        else mapView->endPlaceEdit();
    });
    refreshPlaceNodes();
    syncPlaceListFromFilters();
}

void MW::showPlacesDock()
{
    if (G::isLogger) G::log("MW::showPlacesDock");
    if (G::isInitializing || !placesDock) return;
    const bool wanted = placesDockVisibleAction->isChecked();
    placesDock->setVisible(wanted);
    if (wanted) placesDock->raise();
}

void MW::closePlacesDock()
{
    if (!placesDock) return;
    placesDock->setVisible(false);
    placesDockVisibleAction->setChecked(false);
}

void MW::setPlacesDockVisibility()
{
    if (G::isLogger) G::log("MW::setPlacesDockVisibility");
    if (!placesDock) return;
    placesDock->setVisible(placesDockVisibleAction->isChecked());
}

void MW::refreshPlaceNodes()
{
/*
    The places from the store into the Filters category, the panel's list and the map.
    Also greys + with the store's reason when collections.db cannot be used.
*/
    if (!filters || !placeList) return;
    CollectionStore &store = CollectionStore::instance();
    const bool usable = store.isAvailable();
    QVector<Filters::CollectionItem> items;
    QList<PlaceList::Row> rows;
    QList<MapView::PlaceShape> shapes;
    if (usable) {
        for (const CollectionStore::Node &n : store.nodes(Kind::Place)) {
            items << Filters::CollectionItem{QString::number(n.id), QString(), n.name,
                                             QString()};
            rows << PlaceList::Row{n.id, n.name};
            Geo::Place pl;
            if (Geo::fromJson(n.definition, pl))
                shapes << MapView::PlaceShape{n.id, n.name, pl};
        }
    }
    filters->setSetNodes(filters->places, items);
    placeList->reload(rows);
    if (mapView) mapView->setPlaces(shapes);

    const QString reason = usable ? QString() : store.unavailableReason();
    placesReason->setText(reason);
    placesReason->setStyleSheet(QString("color: %1;").arg(G::disabledColor.name()));
    placesReason->setVisible(!usable);
    placeList->setEnabled(usable);
    placesNewBtn->setEnabled(usable);
    placesNewBtn->setToolTip(usable
        ? tr("New place: draw an ellipse or a polygon on the map")
        : tr("New place: unavailable -- %1").arg(reason));
    placesEmptyHint->setVisible(usable && rows.isEmpty());
    syncPlaceListFromFilters();
}

void MW::refreshPlaceMembership()
{
/*
    Rebuild G::PlacesColumn's side table: source path -> ids of the places the image was
    taken inside, and the count per place. One pass over the loaded rows, parsing each
    GPS string once and testing it against every place; nothing at all when there are
    no places.

    Rows are counted as every other category counts them (a version is a row), but the
    table is keyed by the source path, so a version is in its master's places.

    A refilter follows only when the membership CHANGED and a place is filtering: this
    runs after every build and on every GPS change, and an unchanged answer must cost
    the grid nothing.
*/
    if (G::isLogger) G::log("MW::refreshPlaceMembership");
    if (!dm || !filters) return;
    QElapsedTimer t;
    t.start();

    QVector<QPair<QString, Geo::Place>> places;
    CollectionStore &store = CollectionStore::instance();
    if (store.isAvailable()) {
        for (const CollectionStore::Node &n : store.nodes(Kind::Place)) {
            Geo::Place pl;
            if (Geo::fromJson(n.definition, pl))
                places << qMakePair(QString::number(n.id), pl);
        }
    }

    QHash<QString, QStringList> byPath;
    QHash<QString, int> counts;
    QHash<qint64, int> listCounts;
    const int rows = places.isEmpty() ? 0 : dm->rowCount();
    for (int r = 0; r < rows; ++r) {
        const QString gps = dm->index(r, G::GPSCoordColumn).data().toString();
        double la, lo;
        if (!Geo::parseCoord(gps, la, lo)) continue;
        QStringList ids;
        for (const auto &p : std::as_const(places))
            if (Geo::contains(p.second, la, lo)) ids << p.first;
        if (ids.isEmpty()) continue;
        const QString src =
            dm->index(r, G::PathColumn).data(G::SourcePathRole).toString();
        if (!src.isEmpty()) byPath.insert(src, ids);
        for (const QString &id : std::as_const(ids)) {
            ++counts[id];
            ++listCounts[id.toLongLong()];
        }
    }
    const bool changed = dm->setPlaceMembership(byPath);
    filters->setSetCounts(filters->places, counts);
    if (placeList) placeList->setCounts(listCounts);

    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] refreshPlaceMembership" << places.size()
                           << "places x" << rows << "rows =" << t.elapsed() << "ms";

    if (!changed || G::isLoadRunning || G::isModifyingDatamodel) return;
    QStringList inc, exc;
    filters->setFilterState(filters->places, inc, exc);
    if (!inc.isEmpty() || !exc.isEmpty()) filterChange("MW::refreshPlaceMembership");
}

void MW::applyPlaceFilter(const QVector<qint64> &ids)
{
/*
    A Places panel click -- the panel's whole selection. As a Collection click does,
    it clears EVERY filter (all categories, the search text, the pick / rating / colour
    actions) and then checks these places, in ONE filterChange: it means "show me the
    images taken here". An empty selection is Cancel, which clears only this category.

    NOT WHILE A LOAD IS RUNNING: the load's build resets the checks when it lands, as
    MW::applySetFilter explains.
*/
    if (G::isLogger) G::log("MW::applyPlaceFilter", QString::number(ids.size()));
    if (!filters || !dm) return;
    if (G::isLoadRunning || G::isModifyingDatamodel || filters->buildingFilters) {
        if (G::popup)
            G::popup->showPopup(tr("Still loading. Click the place again when it has "
                                   "finished."), 2500);
        syncPlaceListFromFilters();
        return;
    }
    if (ids.isEmpty()) {
        cancelPlaceFilter();
        return;
    }

    QStringList inc;
    for (qint64 id : ids) inc << QString::number(id);

    /*  Every row's GPS must be read before membership can be trusted: a folder whose
        metadata is still arriving would otherwise filter on the part already read. */
    if (!G::allMetadataAttempted) {
        loadEntireMetadataCache("FilterChange");
        refreshPlaceMembership();
    }
    uncheckAllFilters();
    filters->searchString = "";
    dm->searchStringChange("");
    filters->setSetFilter(filters->places, inc, {});
    QStringList nowInc, nowExc;
    filters->setFilterState(filters->places, nowInc, nowExc);
    if (nowInc.isEmpty()) filterChange("MW::applyPlaceFilter");
    syncPlaceListFromFilters();
    if (mapView) mapView->fitPlaces(QList<qint64>(ids.begin(), ids.end()));
}

void MW::cancelPlaceFilter()
{
    if (G::isLogger) G::log("MW::cancelPlaceFilter");
    if (!filters) return;
    filters->setSetFilter(filters->places, {}, {});
    syncPlaceListFromFilters();
}

void MW::syncPlaceListFromFilters()
{
/*
    The Filters "Places" checks into the panel's selection, the Cancel button and the
    map's outlines. Called after every filter change and every build.
*/
    if (!filters || !placeList) return;
    QStringList inc, exc;
    filters->setFilterState(filters->places, inc, exc);
    QList<qint64> ids;
    for (const QString &s : std::as_const(inc)) ids << s.toLongLong();
    placeList->syncFromFilter(ids);
    if (mapView) mapView->setShownPlaces(ids);
    const bool filtering = !inc.isEmpty() || !exc.isEmpty();
    if (placesCancelBtn) {
        placesCancelBtn->setEnabled(filtering);
        placesCancelBtn->setToolTip(filtering
            ? tr("Cancel: stop filtering by place, keeping the other filters.")
            : tr("Cancel: nothing to cancel -- no place is filtering."));
    }
}

/* ---------------------------------------------------------------------------------
   New, edit, rename, delete
   --------------------------------------------------------------------------------- */

void MW::newPlace()
{
    if (G::isLogger) G::log("MW::newPlace");
    openPlaceDlg(0);
}

void MW::editPlace(qint64 id)
{
    if (G::isLogger) G::log("MW::editPlace", QString::number(id));
    openPlaceDlg(id);
}

void MW::openPlaceDlg(qint64 id)
{
/*
    The dialog and the map's edit mode open together. The map must be showing -- it is
    where the shape is drawn -- so this enters the Map module first. A new place starts
    as the type last used; an existing one is edited as it is, with the map moved to it.
*/
    if (!mapView || !CollectionStore::instance().isAvailable()) return;
    if (!inMapModule()) {
        invokeMapWorkflow();
        /*  Keep the panel the + was clicked in: a Map layout saved before the Places
            dock existed would otherwise hide it as the workflow switches. */
        placesDockVisibleAction->setChecked(true);
        setPlacesDockVisibility();
        placesDock->raise();
    }

    if (!placeDlg) {
        placeDlg = new PlaceDlg(this);
        connect(placeDlg, &QDialog::finished, this, [this](int result) {
            finishPlaceDlg(result == QDialog::Accepted);
        });
        connect(placeDlg, &PlaceDlg::shapeChosen, this, [this](Geo::Place::Shape s) {
            /*  A new type restarts the shape; an existing place switched back to its
                own type gets its own shape back. */
            const qint64 pid = placeDlg->placeId();
            Geo::Place pl;
            const bool have = pid
                && Geo::fromJson(CollectionStore::instance().node(pid).definition, pl);
            mapView->beginPlaceEdit(s, have ? &pl : nullptr, pid);
        });
        connect(placeDlg, &PlaceDlg::removeCornerRequested,
                mapView, &MapView::removeLastCorner);
    }

    Geo::Place existing;
    QString name;
    Geo::Place::Shape shape = settings->value("placeDlgShape", 0).toInt() == 1
                                  ? Geo::Place::Polygon : Geo::Place::Ellipse;
    bool have = false;
    if (id) {
        const CollectionStore::Node n = CollectionStore::instance().node(id);
        if (n.id == 0 || n.kind != Kind::Place) return;
        name = n.name;
        have = Geo::fromJson(n.definition, existing);
        if (have) shape = existing.shape;
        if (have) mapView->fitPlaces({id});
    }
    placeDlg->start(id, name, shape);
    mapView->beginPlaceEdit(shape, have ? &existing : nullptr, id);

    // top left of the map, clear of a new ellipse, which starts in the middle
    if (!placeDlg->isVisible())
        placeDlg->move(mapView->mapToGlobal(QPoint(16, 56)));
    placeDlg->show();
    placeDlg->raise();
    placeDlg->activateWindow();
}

void MW::finishPlaceDlg(bool accepted)
{
    if (G::isLogger) G::log("MW::finishPlaceDlg", accepted ? "Done" : "Cancel");
    if (!placeDlg || !mapView) return;
    qint64 id = placeDlg->placeId();
    const QString name = placeDlg->name();
    const bool ok = accepted && mapView->placeEditComplete() && !name.isEmpty();
    const Geo::Place shape = mapView->editedPlace();
    mapView->endPlaceEdit();
    if (!ok) return;

    settings->setValue("placeDlgShape", int(shape.shape));
    CollectionStore &store = CollectionStore::instance();
    const QString json = Geo::toJson(shape);
    const bool isNew = id == 0;
    if (isNew) id = store.create(Kind::Place, 0, name, json);
    else {
        store.rename(id, name);
        store.setDefinition(id, json);
    }
    if (!id) {
        QMessageBox::warning(this, tr("Places"),
                             tr("The place could not be saved.\n\n%1")
                                 .arg(store.unavailableReason()));
        return;
    }
    // a new place shows what it found
    if (isNew) applyPlaceFilter({id});
}

void MW::renamePlace(qint64 id)
{
    CollectionStore &store = CollectionStore::instance();
    const CollectionStore::Node n = store.node(id);
    if (n.id == 0) return;
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Rename Place"), tr("Name:"), QLineEdit::Normal,
                              n.name, &ok).simplified();
    if (ok && !name.isEmpty()) store.rename(id, name);
}

void MW::deletePlace(qint64 id)
{
    CollectionStore &store = CollectionStore::instance();
    const CollectionStore::Node n = store.node(id);
    if (n.id == 0) return;
    QMessageBox box(QMessageBox::Question, tr("Delete Place"),
                    tr("Delete the place \"%1\"?\n\nNo images are changed: only the "
                       "area drawn on the map is removed.").arg(n.name),
                    QMessageBox::Cancel, this);
    QPushButton *del = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != del) return;
    if (mapView && mapView->isEditingPlace() && mapView->editingPlaceId() == id
        && placeDlg)
        placeDlg->reject();
    store.remove(id);
}
