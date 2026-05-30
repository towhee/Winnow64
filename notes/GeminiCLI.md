1. Event Propagation Bug
In Views/iconview.cpp, IconView::event follows the same flawed pattern as the earlier VideoView and DockWidget bugs:


 1 bool IconView::event(QEvent *event) {
 2     // ... logic ...
 3     QListView::event(event);
 4     return true; // Should return the result of the base class call
 5 }
This unconditionally returns true, which can block internal Qt event processing and cause the UI to become unresponsive or glitchy.


2. Model Modification During Paint (Infinite Loop Risk)
IconViewDelegate::paint (in Views/iconviewdelegate.cpp) and its interaction with IconView contains a highly dangerous pattern:
 * Symbol Rects: paint calls dm->sf->setData(..., G::IconSymbolColumn) to save UI coordinates for tooltips.
 * Icon Rect Role: paint emits update(index, iconRect), which triggers IconView::updateThumbRectRole, which in turn calls emit setValSf(...,
   G::IconRectRole).


Calling setData on the model from within a delegate's paint function is a "forbidden" pattern in Qt. If the model change triggers a view update
(even for a different role), it can lead to an infinite repaint loop or significant performance degradation during scrolling.


3. Synchronization and qApp->processEvents()
I noticed 38 instances of qApp->processEvents() across the codebase, often used to "keep the UI responsive" during long-running tasks. This is a
common source of re-entrancy bugs and unexpected state changes (like a folder being closed while a loop is still iterating over its data).


Recommendation: We should fix the IconView::event propagation and refactor the delegate to avoid writing back to the model during the paint cycle.
UI-specific data like iconRect should be calculated on demand (e.g., during mouse events) or stored in a transient UI-layer cache rather than the
primary DataModel.


How would you like to proceed with these UI-layer stability fixes?
