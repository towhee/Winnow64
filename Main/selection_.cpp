#include "Main/mainwindow.h"

/*
    The DataModel selection model operations occur here.

    The selection model behavior is different than the default:

        • the selected rows must be greater than zero
        • the current row must be selected

        This behavior is managed in the DataModel views (thumbView and gridView)
        mouseReleaseEvent, which calls dm->chkForDeselection: check for attempt to
        deselect only selected row (must always be one selected). Also check to see if
        the current row in a selection range greater than one has been deselected. If so,
        then set current to the nearest selected row.

    Current Index:

        The current index is managed in the DataModel, NOT in the selection model.

        • currentDmIdx, currentDmSfIdx
        • currentDmRow, currentDmSfRow


*/
