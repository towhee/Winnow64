#include "Develop/Properties/developproperties.h"
#include "Develop/Properties/scopeheaderbase.h"
#include "Develop/Properties/rawpanel.h"
#include "Develop/Properties/maskpanel.h"
#include "Develop/Properties/maskeditor.h"
#include "Develop/History/historyview.h"
#include "Develop/Presets/presetsview.h"
#include "Develop/fillspot.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include "Develop/Scopes/toneregionslider.h"
#include "Develop/whitebalance.h"
#include "Develop/rangemask.h"
#include "Develop/workingimagecache.h"
#include "Dialogs/savedeveloppresetdlg.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QVariantAnimation>
#include <algorithm>
#include <functional>
/*
    See developproperties.h for an overview. Construction mirrors EmbelProperties:
    initialize tree behaviour, read the saved scope list, add the persistent Scopes
    header (tree row 0), then build the Basic / Effects sections for the current scope.
*/

DevelopProperties::DevelopProperties(QWidget *parent, QSettings *setting) : PropertyEditor(parent)
{
    if (G::isLogger) G::log("DevelopProperties::DevelopProperties");
    mw = qobject_cast<MW*>(parent);
    this->setting = setting;

    /* The mask overlay's single colour is a view preference owned by Develop, but read
       by ImageView (which has no settings handle), so it lives in G::. Seed it here,
       before any overlay can paint. */
    {
        const QColor c(setting->value("Develop/maskOverlayColor",
                                      G::maskOverlayColor.name()).toString());
        if (c.isValid()) G::maskOverlayColor = c;
        G::maskOverlayGrayscale =
            setting->value("Develop/maskOverlayGrayscale", false).toBool();
    }

    initialize();
    /* Per-image edit history (the History dock's model). Session scoped: it records a
       labelled EditStack snapshot per action via noteEdit and is never written to the
       sidecar (which holds only the current state). */
    history = new DevelopHistory(this);
    /* The named develop presets (the Presets dock's model). Unlike history these ARE
       persistent -- they live in QSettings under "Develop Presets". */
    presets = new DevelopPresets(setting, this);
    /* The mask-tool chooser, popped when a new mask is created (newScope). */
    maskMenu = new QMenu(this);
    /* The Scope list (in the ScopeHeader widget) shows the CURRENT IMAGE's scopes --
       Global plus one row per mask (per-image EditStack), not app-global QSettings
       presets. Seed one name so it is valid before any image. */
    scopeList = QStringList() << "Global";
    buildTree();        // active scope's top items + Basic / Color / Effects

    updateHiddenRows(QModelIndex());
    setMouseTracking(true);

    /* No row selection in this tree. The rows are ADJUSTMENTS, not a list to pick from:
       nothing here reads the selection (the mask tools track selectedMaskIndex, the
       focused slider is its own cue), and a selected row is purely an artifact -- the
       G::css "QTreeView::item:selected" background paints the row's INDENTATION strip
       (QTreeView fills it before the delegate runs, and drawBranches leaves it alone),
       giving a solid blue tab to the left of a caption. Stripping State_Selected in
       drawRow does NOT fix that: QTreeView::drawRow re-derives the flag PER CELL from
       the selection model, so the only reliable cure is an empty selection model. The
       CURRENT index is unaffected -- QAbstractItemView still sets it on click (with
       NoUpdate), so currentChanged / onMaskSelectionChanged behave as before. */
    setSelectionMode(QAbstractItemView::NoSelection);

    dividerHeight = 5;
    const int c = G::backgroundShade + 20;
    divColor = QColor(c,c,c);

    /* Optional debounce write: flush the in-memory stack to the sidecar a short time after edits
       settle, so a crash loses less. Gated by G::isDevelopDebounceWrite; navigate-away / quit /
       pre-op flushes always run regardless. */
    debounceWriteTimer = new QTimer(this);
    debounceWriteTimer->setSingleShot(true);
    /* flushAll, not just the current image: with a multi-image selection the propagated
       edits made on the OTHER selected images are dirty too, and a crash should lose no
       more of them than it loses of the image being edited. */
    connect(debounceWriteTimer, &QTimer::timeout, this, [this]{ flushAll(); });

    /* Multi-image editing: a slider drag commits continuously, so the copy to the other
       selected images is batched and applied once the edits settle. */
    propagateTimer = new QTimer(this);
    propagateTimer->setSingleShot(true);
    connect(propagateTimer, &QTimer::timeout, this, [this]{ flushPropagation(); });

    /* Persist the Basic/Color/Effects section expand-state across sessions (QSettings). Fires for
       every expand/collapse; persistSectionExpanded filters to the three section headers. */
    connect(this, &QTreeView::expanded,  this, [this](const QModelIndex &idx){
        persistSectionExpanded(idx, true);
        onSectionExpanded(idx);                 // Solo: fold the Scope row in
        scheduleContentFit();                   // fit mode: more rows -> taller block
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex &idx){
        persistSectionExpanded(idx, false);
        scheduleContentFit();
    });

    /* Programmatic row selection reveals that mask tool's settings (clicks go via mousePressEvent,
       since the base PropertyEditor does not select on click). */
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &, const QModelIndex &){ onMaskSelectionChanged(); });

    /* Every adjustment (slider, wheel, dropdown, preset, reset, history) ends by
       emitting paramsChanged, so that is the one place to refresh the " *" edited
       markers on the section headers. */
    connect(this, &DevelopProperties::paramsChanged,
            this, &DevelopProperties::updateSectionHeaderCaptions);
}

void DevelopProperties::initialize()
{
    if (G::isLogger) G::log("DevelopProperties::initialize");

    setSolo(setting->value("Develop/isSolo", false).toBool());
    setIndentation(10);
    setAlternatingRowColors(false);
    setMouseTracking(false);
    setHeaderHidden(true);
    ignoreFontSizeChangeSignals = false;

    /* No frame. The app stylesheet gives every QTreeView a 1px border (widgetcss
       treeView), which drew a box round the tree: a seam between the scope list and the
       first section header, and a 1px break in the containment rail where it crossed
       from the Scope band into the tree. setFrameShape alone is not enough once the QSS
       styles QTreeView -- a widget-level rule is needed too (same as PanelEditor). The
       block's bottom edge is closed by paintEvent's rule instead. */
    setFrameShape(QFrame::NoFrame);
    setStyleSheet("QTreeView { border: none; }");

    /* Column widths. The dock minimum width is set in MW::createDevelopDock so the
       header - and + buttons are always visible. */
    stringToFitCaptions = "=captions column=";
    stringToFitValues   = "======values column======";
    resizeColumns();

    root = model->invisibleRootItem()->index();
}

/* ---------------------------------------------------------------------------------
   Content-height fitting (the tree nested under the active scope row)
   --------------------------------------------------------------------------------- */

void DevelopProperties::setFitToContentHeight(bool on)
{
    if (G::isLogger) G::log("DevelopProperties::setFitToContentHeight");
    if (fitToContents == on) return;
    fitToContents = on;
    if (!on) return;

    /* No scrollbars of our own: the tree is a fixed-height block inside the dock's
       QScrollArea, which does all the scrolling. autoScroll off as well -- a transient
       scroll offset would skew visualRect and the containment rail it drives. */
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumHeight(0);
    setAutoScroll(false);

    /* Any structural model change re-fits. Row HIDING (setRowHidden) emits nothing, so
       those callers schedule the fit themselves (applyScopeItemsCollapsed etc.). */
    connect(model, &QStandardItemModel::rowsInserted, this,
            [this]{ scheduleContentFit(); });
    connect(model, &QStandardItemModel::rowsRemoved, this,
            [this]{ scheduleContentFit(); });
    connect(model, &QStandardItemModel::modelReset, this,
            [this]{ scheduleContentFit(); });
    connect(model, &QStandardItemModel::layoutChanged, this,
            [this]{ scheduleContentFit(); });
    scheduleContentFit();
}

QSize DevelopProperties::sizeHint() const
{
    if (!fitToContents) return PropertyEditor::sizeHint();
    /* viewportSizeHint() executes any posted layout and returns the bottom of the LAST
       VISIBLE item -- hidden rows (Core / mask / raw-only) and collapsed sections are
       already excluded, so it is exactly where the containment rail ends. Reserve the
       closing rule's band below it, plus the clear gap under that (see paintEvent). */
    const QSize vs = viewportSizeHint();
    return QSize(vs.width(), vs.height() + kBlockCloseGap + G::panelBorderHeight
                             + kBlockBottomGap);
}

QSize DevelopProperties::minimumSizeHint() const
{
    /* QAbstractScrollArea's minimum is derived from its scrollbars and would floor the
       block above its content (visible when every section is collapsed). In fit mode the
       content HEIGHT is the minimum.

       WIDTH stays 0. sizeHint().width() is the width the tree currently occupies, and
       returning it as a minimum floors the whole scope block (and so the dock's
       QScrollArea body, which cannot scroll horizontally) at the width it had BEFORE the
       vertical scrollbar appeared: expanding enough sections to need the scrollbar then
       clipped the value column and the eye buttons by the scrollbar's width instead of
       reflowing. The tree has no intrinsic minimum -- its caption column is fixed and the
       value column stretches -- so it must be free to give the scrollbar its space. */
    if (!fitToContents) return PropertyEditor::minimumSizeHint();
    return QSize(0, sizeHint().height());
}

void DevelopProperties::scheduleContentFit()
{
    if (!fitToContents || fitPending) return;
    fitPending = true;
    /* Deferred and coalesced: this is called from model signals, expand/collapse and
       paintEvent, so it must never resize the widget re-entrantly, and a scope switch
       (a full teardown + rebuild) must cost one relayout, not dozens. */
    QTimer::singleShot(0, this, [this]{
        fitPending = false;
        const int h = sizeHint().height();
        if (h == fittedHeight && height() == h) return;     // no change, no oscillation
        fittedHeight = h;
        verticalScrollBar()->setValue(0);        // never leave the viewport offset
        updateGeometry();
    });
}

void DevelopProperties::wheelEvent(QWheelEvent *event)
{
    /* Fit mode: the tree has no scroll range of its own, so ignore the wheel and let it
       reach the dock's QScrollArea. QAbstractScrollArea would otherwise accept and
       swallow it, freezing the dock under the cursor. (Slider editors are separate child
       widgets and still get their own wheel events.) */
    if (fitToContents) { event->ignore(); return; }
    PropertyEditor::wheelEvent(event);
}

void DevelopProperties::resizeEvent(QResizeEvent *event)
{
    PropertyEditor::resizeEvent(event);
    /* A width change can re-measure delegate rows (elided captions, wrapped editors). */
    if (fitToContents && event->oldSize().width() != event->size().width())
        scheduleContentFit();
}

/* ---------------------------------------------------------------------------------
   Scopes (the Scope list: Global + one entry per mask)
   --------------------------------------------------------------------------------- */

QString DevelopProperties::uniqueScopeName(const QString &name) const
{
    const QStringList names = currentScopeNames();
    QString unique = name;
    int n = 1;
    while (names.contains(unique)) {
        unique = name + " " + QString::number(++n);
    }
    return unique;
}

void DevelopProperties::buildTree()
{
    if (G::isLogger) G::log("DevelopProperties::buildTree");

    /* Preserve which sections are expanded across the rebuild. On the first build no header row
       exists yet, so the default comes from the session-persistent QSettings state (first-ever
       run => Basic open only). */
    auto expandedOr = [this](const QString &name, bool def)->bool {
        const QModelIndex idx = findCaptionIndex(name);
        return idx.isValid() ? isExpanded(idx) : def;
    };
    const bool exBasic   = expandedOr("BasicHeader",   sectionExpanded("BasicHeader",   true));
    const bool exColor   = expandedOr("ColorHeader",   sectionExpanded("ColorHeader",   false));
    const bool exEffects = expandedOr("EffectsHeader", sectionExpanded("EffectsHeader", false));

    isPopulating = true;
    isRebuildingMasks = true;

    /* Delete every row's editor widgets (close), then drop the rows, and rebuild for the ACTIVE
       scope: its top items first, then the adjustment sections. close() before removeRows so the
       setIndexWidget editors are freed rather than leaked on each rebuild. */
    btns.clear();
    basicEyeBtn = colorEyeBtn = effectsEyeBtn = nullptr;
    if (model->rowCount() > 0) {
        close(QModelIndex());
        model->removeRows(0, model->rowCount());
    }
    /* removeRows deletes the items but leaves sourceIdx (key -> value index) holding
       stale entries; addItem repopulates it for every row it re-adds, but a key NOT
       re-added this pass (e.g. denoiseLuma/denoiseChroma when the engine is Apple) keeps
       a dangling index that isValid() still returns true for -- setSliderReal would then
       crash on data(). Clear it so only rows present after the rebuild have entries. */
    sourceIdx.clear();

    /* Lab UI: the Global "Core" rows live in the RawPanel above the Scopes list, so the
       tree omits them (addMaskItems stays until the Mask panel lands in Phase 4). */
    if (activeScopeIndex == 0) { if (!G::useScopeHeaderLab) addCoreItems(); }
    else                       addMaskItems();      // this scope's mask tool rows
    addBasic();
    addColor();
    addCalibrate();
    addColorGrade();
    addEffects();
    /* The headers were just re-added with their plain captions, so the cached " *"
       state is stale -- force a full re-evaluation. */
    for (int &st : hdrStar) st = -1;
    updateSectionHeaderCaptions();

    isPopulating = false;
    isRebuildingMasks = false;

    /* Restore section expand-state (leaves the selected mask tool's own expansion, set in
       addToolRow, untouched). */
    auto setExpandOn = [this](const QString &name, bool on){
        const QModelIndex idx = findCaptionIndex(name);
        if (idx.isValid()) setExpanded(idx, on);
    };
    setExpandOn("BasicHeader",   exBasic);
    setExpandOn("ColorHeader",   exColor);
    setExpandOn("EffectsHeader", exEffects);

    populateSlidersFromStack();
    refreshPreviewButtons();
    updateMaskEdit();
    updateHiddenRows(QModelIndex());
    applyScopeItemsCollapsed();      // re-assert the '>' collapse (a rebuild resets row visibility)
    applyCoreVisibility();           // hide Demosaic/Denoise raw on preview
    if (!panelEnabled) applyItemsEnabled(false);   // keep captions greyed if disabled
    syncRawPanel();                  // lab UI: reflect raw state + visibility in RawPanel
    syncMaskPanel();                 // lab UI: the active mask's submask list + settings
    scheduleContentFit();            // nested under a scope row: re-fit the block height
}

bool DevelopProperties::sectionExpanded(const QString &name, bool def) const
{
    return setting->value("Develop/SectionExpanded/" + name, def).toBool();
}

void DevelopProperties::persistSectionExpanded(const QModelIndex &idx, bool expanded)
{
    /* Only the three adjustment section headers persist (mask tool rows also expand/
       collapse, but their state is transient). */
    const QString name = idx.data(UR_Name).toString();
    if (name == "BasicHeader" || name == "ColorHeader" || name == "EffectsHeader")
        setting->setValue("Develop/SectionExpanded/" + name, expanded);
}

void DevelopProperties::addCoreItems()
{
    if (G::isLogger) G::log("DevelopProperties::addCoreItems");
    /* Global scope only. These rows apply to raw sensor data, so they are shown for RAW files only:
       a JPG/TIFF/PNG is already the developable image (no demosaic, no raw denoise, no source
       choice). Non-raw Global scopes get no Core rows. */
    if (!currentIsRaw()) return;

    /* Edit source selector (directly under the scope header): choose whether Develop edits the raw
       sensor data or the embedded preview jpg. An A/B radio pair kept in sync with G::useRaw and
       the status-bar useRaw button. "Edit:" is indented to line up with the Demosaic row
       below it (isIndent = true); the radios are hosted in the value cell (DT_None = no
       built-in editor, so the cell is ours). */
    clearItemInfo(i);
    i.name = "editSource";
    i.parentName = "";              // root row (no section header)
    i.captionText = "Edit:";
    i.tooltip = "Edit the raw sensor data (demosaic) or the embedded preview jpg.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "editSource";
    i.delegateType = DT_None;
    addItem(i);
    /* A little extra vertical breathing room (4px top + 4px bottom) to set the source
       selector apart from the scope header above and the Demosaic row below. */
    model->setData(capIdx, 8, UR_ExtraRowHeight);

    const QModelIndex editValIdx = findValueIndex("editSource");
    if (editValIdx.isValid()) {
        /* Recreated on every rebuild; the previous widget is freed when removeRows() drops the row
           (setIndexWidget gives the view ownership of the index widget). */
        QWidget *w = new QWidget;
        w->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(w);
        hb->setContentsMargins(0, 0, 0, 0);
        hb->setSpacing(12);
        QRadioButton *rawBtn  = new QRadioButton("Raw", w);
        QRadioButton *prevBtn = new QRadioButton("Embedded Preview", w);
        QButtonGroup *grp = new QButtonGroup(w);
        grp->setExclusive(true);
        grp->addButton(rawBtn);
        grp->addButton(prevBtn);
        rawBtn->setChecked(G::useRaw);
        prevBtn->setChecked(!G::useRaw);
        hb->addWidget(rawBtn);
        hb->addWidget(prevBtn);
        hb->addStretch(1);
        editRawRadio = rawBtn;      // QPointer: cleared automatically when the row/widget is freed
        connect(rawBtn, &QRadioButton::toggled, this, &DevelopProperties::onEditSourceChanged);
        setIndexWidget(editValIdx, w);
    }

    /* Demosaic engine + raw noise reduction. Indented (isIndent = true) so they align with the
       Basic-section sliders (e.g. "Temp"). Visible only when editing raw (applyCoreVisibility).
       DT_None: we own the value cell and host a real QComboBox formatted like the scope
       dropdown (ScopeHeader::combo), rather than the delegate's DT_Combo. */
    clearItemInfo(i);
    i.name = "demosaic";
    i.parentName = "";
    i.captionText = "Render using";
    i.tooltip = "Select demosaic engine.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "demosaic";
    i.delegateType = DT_None;
    addItem(i);

    const QModelIndex demosaicValIdx = findValueIndex("demosaic");
    if (demosaicValIdx.isValid()) {
        /* Real QComboBox in the value cell, formatted like the scope dropdown: a 12px
           checkmark on the ACTIVE engine and a blank spacer on the other, expanding width.
           Recreated on every rebuild (setIndexWidget gives the view ownership), so reading
           G::decodeRawEngine here keeps the checkmark on the engine actually in effect. */
        const int iconPx = 12;
        QPixmap cm(":/images/checkmark.png");
        const QIcon checkIcon = cm.isNull() ? QIcon()
            : QIcon(cm.scaled(iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QPixmap blank(iconPx, iconPx);
        blank.fill(Qt::transparent);
        const QIcon blankIcon(blank);

        const bool apple = G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine;
        QComboBox *engine = new QComboBox;
        engine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        engine->setIconSize(QSize(iconPx, iconPx));
        engine->setToolTip("Select demosaic engine (Apple Core Image or in-house Winnow).");
        engine->addItem(apple ? checkIcon : blankIcon, "Apple");
        engine->addItem(apple ? blankIcon : checkIcon, "Winnow");
        engine->setCurrentIndex(apple ? 0 : 1);
        /* Switching the engine forces a re-decode (MW sets G::decodeRawEngine), then a
           deferred rebuild adds/removes the raw-denoise rows -- doing it inline would
           delete this live editor mid-signal. Mirrors the old DT_Combo itemChange. */
        connect(engine, QOverload<int>::of(&QComboBox::activated), this, [this](int idx){
            emit demosaicEngineChanged(idx == 0);       // 0 = Apple
            QTimer::singleShot(0, this, [this]{ buildTree(); });
        });
        /* Host the combo in a container with a 10px right margin so it clears the panel's
           right edge (setIndexWidget otherwise stretches it flush to the cell edge). */
        QWidget *cell = new QWidget;
        cell->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *cellHb = new QHBoxLayout(cell);
        cellHb->setContentsMargins(0, 0, 10, 0);
        cellHb->setSpacing(0);
        cellHb->addWidget(engine);
        setIndexWidget(demosaicValIdx, cell);
    }
    /* The Demosaic row is a plain leaf (no children, no decoration arrow), so it draws
       in the ordinary caption colour like "Edit:" -- not the teal header colour. */

    /* "Denoise raw": Global-scope, decode-time raw noise reduction (denoiseLuma/
       denoiseChroma), baked into the pre-develop WorkingImage (global, not maskable) --
       distinct from the Effects "Denoise" (localDenoiseLuma). Two Lightroom-style 0..100
       sliders mapped to 0..1: Luminance = the master AI-denoise amount; Color = extra
       chroma-noise suppression (default 100).

       Shown only on the WINNOW engine (PMRID needs the CFA mosaic); on Apple the raw
       denoise is inert so they are omitted. They are root-level SIBLINGS of the Demosaic
       row (not children) so they always show with no collapse arrow; UR_ExtraIndent lines
       their captions up one tree level, matching the Basic sliders. Their per-raw
       visibility is handled with the Demosaic row in applyCoreVisibility. */
    if (G::decodeRawEngine != G::DecodeRawEngine::appleDecodeRawEngine) {
        /* 4px gap-only divider (no line) to separate the Demosaic row from the denoise
           sliders. Hidden with them in applyCoreVisibility. */
        addDivider(4, 0, divColor, QModelIndex(), "", "denoiseGap");

        /* Manual "Denoise" trigger + "Auto run" toggle. A full-width (spanned) row:
           "Denoise" checkbox on the left (aligned with the Demosaic caption above), "Auto
           run" checkbox on the right. Checking "Denoise" runs the PMRID denoise now
           (MW::runRawDenoiseNow) and it relabels to "Denoised" when the result lands
           (updateDenoiseRunState, driven by MW). "Auto run" (persisted to
           Develop/autoRunDenoise) runs the denoise automatically instead. Hidden with the
           denoise group in applyCoreVisibility. */
        clearItemInfo(i);
        i.name = "autoRunDenoise";
        i.parentName = "";
        i.captionText = "";
        i.tooltip = "Run raw denoise, automatically (Auto run) or on demand (Denoise).";
        i.isIndent = true;
        i.hasValue = true;
        i.captionIsEditable = false;
        i.key = "autoRunDenoise";
        i.delegateType = DT_None;
        addItem(i);
        const QModelIndex autoCapIdx = capIdx;
        const QModelIndex autoValIdx = findValueIndex("autoRunDenoise");
        model->setData(autoCapIdx, 6, UR_ExtraRowHeight);   // room for the checkboxes
        if (autoCapIdx.isValid()) {
            /* "Denoise" in the CAPTION cell -> left-aligns with the Demosaic caption.
               Manual run + completion state: checked / "Denoised" reflects a ready
               denoised base for the current image (queried from MW). */
            const bool denoised = mw && mw->rawDenoiseReadyForCurrent();
            QCheckBox *denCb = new QCheckBox(denoised ? "Denoised" : "Denoise");
            denCb->setChecked(denoised);
            denCb->setToolTip("Run the raw denoise; shows \"Denoised\" when complete.");
            connect(denCb, &QCheckBox::toggled, this, [this](bool on){
                if (on) emit runRawDenoiseRequested();
                else    emit clearRawDenoiseRequested();
            });
            denoiseRunCheck = denCb;      // QPointer: MW relabels it on completion
            QWidget *capW = new QWidget;
            capW->setAttribute(Qt::WA_TranslucentBackground);
            QHBoxLayout *chb = new QHBoxLayout(capW);
            chb->setContentsMargins(0, 0, 0, 0);
            chb->setSpacing(0);
            chb->addWidget(denCb);
            chb->addStretch(1);
            setIndexWidget(autoCapIdx, capW);
        }
        if (autoValIdx.isValid()) {
            /* "Auto run" in the VALUE cell -> left-aligns with the Demosaic dropdown.
               Persisted flag; when on the denoise runs automatically. */
            QCheckBox *autoCb = new QCheckBox("Auto run");
            autoCb->setChecked(setting->value("Develop/autoRunDenoise", true).toBool());
            autoCb->setToolTip("On: denoise runs automatically. Off: use the Denoise box.");
            connect(autoCb, &QCheckBox::toggled, this, [this](bool on){
                setting->setValue("Develop/autoRunDenoise", on);
                emit autoRunDenoiseToggled(on);
            });
            QWidget *valW = new QWidget;
            valW->setAttribute(Qt::WA_TranslucentBackground);
            QHBoxLayout *vhb = new QHBoxLayout(valW);
            vhb->setContentsMargins(0, 0, 10, 0);
            vhb->setSpacing(0);
            vhb->addWidget(autoCb);
            vhb->addStretch(1);
            setIndexWidget(autoValIdx, valW);
        }

        addSlider("denoiseLuma", "Denoise Lum",
                  "Raw luminance noise reduction (AI, whole image).",
                  QModelIndex(), "", 0, 100, 0, G::darkgray, G::lightgray, 75);
        model->setData(capIdx, true, UR_ExtraIndent);
        addSlider("denoiseChroma", "Denoise Color", "Raw colour (chroma) noise reduction.",
                  QModelIndex(), "", 0, 100, 0, G::darkgray, G::lightgray, 100);
        model->setData(capIdx, true, UR_ExtraIndent);

        /* The amount sliders only scale the blend of a computed PMRID base, so start them
           disabled and enable them once the denoise has produced that base (mirrors
           updateDenoiseRunState, driven by MW on completion / clear). */
        const bool denoiseReady = mw && mw->rawDenoiseReadyForCurrent();
        setItemEnabled("denoiseLuma", denoiseReady);
        setItemEnabled("denoiseChroma", denoiseReady);
    }
    /* Visibility (per G::useRaw + collapse) applied by buildTree() after
       applyScopeItemsCollapsed. */
}

bool DevelopProperties::currentIsRaw() const
{
    return mw && mw->isFileRaw(currentImagePath);
}

void DevelopProperties::updateDenoiseRunState(bool denoised)
{
    /* Reflect the raw-denoise completion state (a ready PMRID base) on the "Denoise"
       checkbox without re-emitting toggled() (that would re-trigger a run/clear), and
       enable the amount sliders only once that base exists -- they merely scale the blend
       of it, so they are inert (and disabled) until the heavy denoise has completed. */
    /* Lab UI: the RawPanel owns the checkbox/sliders (the tree row is absent). */
    if (rawPanel) rawPanel->setDenoiseRunState(denoised);
    if (!denoiseRunCheck) return;      // denoise section absent (Apple engine / non-raw)
    {
        QSignalBlocker block(denoiseRunCheck);
        denoiseRunCheck->setChecked(denoised);
        denoiseRunCheck->setText(denoised ? "Denoised" : "Denoise");
    }
    setItemEnabled("denoiseLuma", denoised);
    setItemEnabled("denoiseChroma", denoised);
}

void DevelopProperties::onEditSourceChanged(bool raw)
{
    if (G::isLogger) G::log("DevelopProperties::onEditSourceChanged");
    /* rawBtn->toggled fires for both halves of the exclusive pair; either way `raw` is the new
       state of the "Raw" button. Only ask MW to switch when it actually differs from G::useRaw
       (syncEditRaw sets the button with signals blocked, so this is also the loop guard). */
    if (raw == G::useRaw) return;
    emit useRawRequested(raw);
}

void DevelopProperties::syncEditRaw(bool useRaw)
{
    if (G::isLogger) G::log("DevelopProperties::syncEditRaw");
    if (editRawRadio) {
        /* An exclusive QButtonGroup auto-unchecks the siblings when a button is CHECKED, but does
           NOT auto-check a sibling when the checked button is unchecked. So editRawRadio->setChecked
           (false) would leave both "Raw" and "Embedded Preview" blank. Always check the button that
           should be selected: "Raw" when useRaw, otherwise its group sibling ("Embedded Preview").
           editRawRadio is the only button connected to a slot, so blocking it also guards re-entry. */
        QSignalBlocker block(editRawRadio);
        QAbstractButton *target = editRawRadio;
        if (!useRaw) {
            if (QButtonGroup *grp = editRawRadio->group()) {
                for (QAbstractButton *b : grp->buttons()) {
                    if (b != editRawRadio) { target = b; break; }
                }
            }
        }
        target->setChecked(true);
    }
    if (rawPanel) rawPanel->setEditSource(useRaw);   // lab UI mirror
    applyCoreVisibility();
}

void DevelopProperties::applyCoreVisibility()
{
    /* The Demosaic row and its raw-denoise sibling sliders (Winnow engine) are visible
       only when editing raw AND the scope items are not collapsed ('>'). Scan root rows
       for those names -- run AFTER applyScopeItemsCollapsed(), which would otherwise
       re-show them on a non-collapsed rebuild. On the Apple engine the denoise rows are
       absent (addCoreItems), so only the Demosaic row matches. */
    const bool hide = !G::useRaw || scopeItemsCollapsed;
    for (int r = 0; r < model->rowCount(); ++r) {
        const QModelIndex cap = model->index(r, CapColumn);
        const QString name = cap.data(UR_Name).toString();
        if (name != "demosaic" && name != "denoiseGap" && name != "autoRunDenoise" &&
            name != "denoiseLuma" && name != "denoiseChroma")
            continue;
        model->setData(cap, hide, UR_isHidden);   // remembered for updateHiddenRows()
        setRowHidden(r, QModelIndex(), hide);
    }
    scheduleContentFit();       // setRowHidden emits nothing: re-fit the block ourselves
}

void DevelopProperties::addMaskItems()
{
    if (G::isLogger) G::log("DevelopProperties::addMaskItems");
    /* Non-Global scopes: the scope's ordered Add/Subtract mask tools as rows at the top of the tree.
       The selected tool is expanded with its settings (addToolRow). */
    EditScope *scope = activeScope();
    if (!scope || activeScopeIndex == 0) { selectedMaskIndex = -1; return; }
    const int n = scope->components.size();

    /* Lab UI: the whole mask lives in the MaskPanel nested under this scope's row -- its
       SubmaskList lists the submasks (including its own [+] for an empty mask) and the
       open one shows its settings there. The tree carries no mask rows at all. */
    if (G::useScopeHeaderLab) {
        if (n == 0) selectedMaskIndex = -1;
        return;
    }

    /* Legacy: the whole ordered tool list, the selected one expanded. A no-mask scope
       shows a single "Add mask" placeholder row with a [+] button. */
    if (n == 0) {
        selectedMaskIndex = -1;
        addAddMaskRow();
        return;
    }
    if (selectedMaskIndex >= n) selectedMaskIndex = n - 1;
    for (int m = 0; m < n; ++m)
        addToolRow(QModelIndex(), m, scope->components[m], m == selectedMaskIndex);
}

void DevelopProperties::addAddMaskRow()
{
    /* Placeholder row shown only while the active (non-Global) scope has no mask: a header-style,
       full-width single-line "Add mask" caption carrying just a [+] glyph (UR_AddBtn, no [-] and no
       expand arrow). Clicking anywhere on the row pops the Add/Subtract chooser (showMaskMenu). It is
       identified in mousePressEvent by its name ("addMask"), as it has no UR_MaskIndex. */
    clearItemInfo(i);
    i.name = "addMask";
    i.parIdx = QModelIndex();
    i.captionText = "Add mask";
    i.tooltip = "This mask is empty. Click [+] to add a submask.";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = false;
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex rowIdx = capIdx;
    model->setData(rowIdx, true, UR_LeafSingleLine);
    model->setData(rowIdx, true, UR_AddBtn);            // [+] add the first mask tool
    setFirstColumnSpanned(rowIdx.row(), QModelIndex(), true);
}

void DevelopProperties::bindScopeHeader(ScopeHeaderBase *header)
{
    if (G::isLogger) G::log("DevelopProperties::bindScopeHeader");
    scopeHeader = header;
    if (!scopeHeader) return;

    connect(scopeHeader, &ScopeHeaderBase::scopeSelected,       this, &DevelopProperties::onScopeSelected);
    connect(scopeHeader, &ScopeHeaderBase::renameRequested,     this, &DevelopProperties::renameActiveScope);
    connect(scopeHeader, &ScopeHeaderBase::resetScopeRequested, this, &DevelopProperties::resetActiveScope);
    connect(scopeHeader, &ScopeHeaderBase::removeScopeRequested,this, &DevelopProperties::deleteScope);
    connect(scopeHeader, &ScopeHeaderBase::addScopeRequested,   this, &DevelopProperties::newScope);
    connect(scopeHeader, &ScopeHeaderBase::addMaskRequested,    this, &DevelopProperties::showMaskMenu);
    connect(scopeHeader, &ScopeHeaderBase::maskOverlayToggled,  this,
            &DevelopProperties::maskOverlayToggleRequested);
    connect(scopeHeader, &ScopeHeaderBase::previewToggled,      this, &DevelopProperties::onScopePreviewToggled);
    connect(scopeHeader, &ScopeHeaderBase::collapseToggled,     this, &DevelopProperties::setTreeCollapsed);
    connect(scopeHeader, &ScopeHeaderBase::scopeEnabledToggled, this, &DevelopProperties::onScopeEnabledToggled);

    /* Seed the dropdown + eye from the current stack. */
    refreshScopeList();
    refreshPreviewButtons();
}

void DevelopProperties::bindRawPanel(RawPanel *panel)
{
    if (G::isLogger) G::log("DevelopProperties::bindRawPanel");
    rawPanel = panel;
    if (!rawPanel) return;

    /* Edit source reuses the existing loop-guarded slot (drives G::useRaw via MW). */
    connect(rawPanel, &RawPanel::editSourceChanged, this, &DevelopProperties::onEditSourceChanged);
    /* Demosaic: ask MW to switch engine + re-decode, then reflect the engine (which shows
       or hides the denoise block). */
    connect(rawPanel, &RawPanel::demosaicChanged, this, [this](bool apple){
        emit demosaicEngineChanged(apple);
        if (rawPanel) rawPanel->setEngine(apple);
    });
    connect(rawPanel, &RawPanel::denoiseRunToggled, this, [this](bool on){
        if (on) emit runRawDenoiseRequested(); else emit clearRawDenoiseRequested();
    });
    connect(rawPanel, &RawPanel::autoRunToggled, this, [this](bool on){
        setting->setValue("Develop/autoRunDenoise", on);
        emit autoRunDenoiseToggled(on);
    });
    connect(rawPanel, &RawPanel::denoiseLumaChanged,   this, [this](int v){ setGlobalDenoise(true,  v); });
    connect(rawPanel, &RawPanel::denoiseChromaChanged, this, [this](int v){ setGlobalDenoise(false, v); });
    connect(rawPanel, &RawPanel::tipsRequested, this, &DevelopProperties::howThisWorks);
    /* Solo: the Raw panel is a peer of the Scope row and the adjustment sections, so
       expanding it folds them away. */
    connect(rawPanel, &RawPanel::collapseToggled, this, [this](bool collapsed){
        if (collapsed) return;
        soloCollapseOthers(SoloOwner::RawPanel);
    });

    syncRawPanel();
}

void DevelopProperties::syncRawPanel()
{
    if (!rawPanel) return;
    const bool raw = currentIsRaw();
    /* Raw-decode strip: only for raw files, and only in the lab UI (legacy keeps the Core
       rows in the tree). */
    rawPanel->setVisible(raw && G::useScopeHeaderLab);
    if (!raw) return;

    const bool apple = (G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine);
    rawPanel->setCaptionWidth(captionColumnWidth);   // align denoise sliders to the tree
    rawPanel->setEditSource(G::useRaw);
    rawPanel->setEngine(apple);
    rawPanel->setAutoRun(setting->value("Develop/autoRunDenoise", true).toBool());
    rawPanel->setDenoiseRunState(mw && mw->rawDenoiseReadyForCurrent());

    /* Push the Global scope's stored denoise amounts (0..1 -> 0..100). */
    float dl = EditParams::kDefaultDenoiseLuma, dc = EditParams::kDefaultDenoiseChroma;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty()) {
        dl = s.scopes.at(0).params.denoiseLuma;
        dc = s.scopes.at(0).params.denoiseChroma;
    }
    rawPanel->setDenoiseValues(qRound(dl * 100.0f), qRound(dc * 100.0f));
}

void DevelopProperties::setGlobalDenoise(bool luma, int value0to100)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    const float f = value0to100 / 100.0f;      // dock 0..100 -> stored 0..1
    if (luma) s.scopes[0].params.denoiseLuma   = f;
    else      s.scopes[0].params.denoiseChroma = f;
    /* Always scope 0, whichever scope is being edited. */
    noteScopeEdit("Global", luma ? "Denoise raw" : "Denoise raw color",
                  QString::number(value0to100),
                  luma ? "global/denoiseLuma" : "global/denoiseChroma");
    emit paramsChanged();               // live preview (Global denoise re-decodes)
}

void DevelopProperties::bindMaskPanel(MaskPanel *panel)
{
    if (G::isLogger) G::log("DevelopProperties::bindMaskPanel");
    maskPanel = panel;
    if (!maskPanel) return;
    connect(maskPanel, &MaskPanel::committed, this, &DevelopProperties::commitPendingMask);
    connect(maskPanel, &MaskPanel::cancelled, this, &DevelopProperties::cancelMaskTool);
    /* The overlay's colour / grayscale controls are NOT in this panel (they are on the
       action-row tint button's context menu, which calls setMaskOverlayColour /
       setMaskOverlayGrayscale directly), so there is nothing to connect for them. */

    /* The panel's embedded tree edits the tool being built; route its changes into the
       active mask component (onMaskEditorSetting mirrors the main tree's itemChange). */
    if (MaskEditor *ed = maskPanel->editor()) {
        connect(ed, &MaskEditor::settingChanged, this, &DevelopProperties::onMaskEditorSetting);
        connect(ed, &MaskEditor::wheelChanged,   this, &DevelopProperties::onMaskEditorWheel);
    }

    /* The submask list: the mask's contents, and the only way back into a submask that
       has already been committed. Every emission arrives DEFERRED (SubmaskList rebuilds
       its rows from inside these handlers), so they are safe to act on directly. */
    if (SubmaskList *sl = maskPanel->list()) {
        connect(sl, &SubmaskList::addRequested,      this, &DevelopProperties::showMaskMenu);
        connect(sl, &SubmaskList::submaskSelected,   this, &DevelopProperties::reopenSubmask);
        connect(sl, &SubmaskList::enabledToggled,    this, &DevelopProperties::setSubmaskEnabled);
        connect(sl, &SubmaskList::opChanged,         this, &DevelopProperties::setSubmaskOp);
        connect(sl, &SubmaskList::invertRequested,   this, &DevelopProperties::toggleSubmaskInverted);
        connect(sl, &SubmaskList::moveRequested,     this, &DevelopProperties::moveSubmask);
        connect(sl, &SubmaskList::duplicateRequested, this, &DevelopProperties::duplicateSubmask);
        connect(sl, &SubmaskList::deleteRequested,   this, &DevelopProperties::deleteMask);
    }
}

void DevelopProperties::syncMaskPanel()
{
    /* Reflect the active scope's mask in the panel: the ordered submasks, which one is
       open, and whether the panel belongs on screen at all. Called at the end of every
       buildTree, so nothing else has to remember to refresh it.

       The panel is up for any MASK scope (index > 0), not just while a submask is being
       built: the whole point of the list is that a mask's contents stay visible. */
    if (!maskPanel) return;
    EditScope *scope = activeScope();
    const bool isMask = (activeScopeIndex > 0 && scope != nullptr);
    if (!isMask) {
        maskPanel->hide();
        return;
    }

    QVector<SubmaskRowInfo> rows;
    rows.reserve(scope->components.size());
    for (int i = 0; i < scope->components.size(); ++i) {
        const MaskComponent &m = scope->components.at(i);
        SubmaskRowInfo r;
        r.toolName = maskToolName(m.tool);
        /* The PENDING submask shows the op the veil and the button are previewing, not
           the (not yet written) one on the component -- the row and the button must not
           disagree about what is about to happen. */
        r.op       = (i == pendingIdx) ? pendingOp : m.op;
        r.enabled  = m.enabled;
        r.inverted = m.inverted;
        r.pending  = (i == pendingIdx);
        rows.append(r);
    }
    maskPanel->list()->setSubmasks(rows, selectedMaskIndex);
    /* Settings + commit only make sense with a submask open; an empty mask shows nothing
       but its [+]. */
    const bool open = (selectedMaskIndex >= 0 &&
                       selectedMaskIndex < scope->components.size());
    maskPanel->setEditingExisting(open && pendingIdx < 0);
    maskPanel->showAttributes(open);
    syncAttributeScopeLabel();      // names what the settings below will affect
    maskPanel->setVisible(true);
}

void DevelopProperties::reopenSubmask(int index)
{
    if (G::isLogger) G::log("DevelopProperties::reopenSubmask");
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    if (index == selectedMaskIndex) return;          // already the one being edited
    /* Switching away from a submask that is still PENDING would discard it silently, the
       same trap as leaving the scope -- ask with the same dialog. */
    if (!confirmPendingMask()) { syncMaskPanel(); return; }

    /* confirmPendingMask may have removed the pending submask (Discard), which shifts
       everything after it down one. */
    scope = activeScope();
    if (!scope || index >= scope->components.size()) return;

    selectedMaskIndex = index;
    pendingIdx = -1;                 // a committed submask is not pending: no cancel
    pendingOp = int(MaskOp::Add);
    latchedMaskOp = int(MaskOp::Add);
    maskPanelOpen = false;           // nothing outstanding, so nothing to confirm
    const MaskComponent &m = scope->components.at(index);
    if (MaskEditor *ed = maskPanel ? maskPanel->editor() : nullptr) {
        ed->setCaptionWidth(captionColumnWidth - kMaskPanelIndent);
        ed->showTool(m);
        if (m.tool == int(MaskTool::ColorRange))
            ed->setWheelSamples(colorRangeSamplesHS(m.paramsJson));
    }
    if (m.tool == int(MaskTool::Brush)) emitBrushSettings(m);
    /* buildTree ends in updateMaskEdit, which emits maskEditBegin for the selected
       component -- that is what puts the tool's handles back on the canvas. */
    buildTree();
    emit paramsChanged();
}

void DevelopProperties::setSubmaskEnabled(int index, bool on)
{
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    if (scope->components[index].enabled == on) return;
    scope->components[index].enabled = on;
    noteEdit((on ? "Show " : "Hide ") + maskToolName(scope->components.at(index).tool));
    buildTree();
    emit paramsChanged();       // the fold skips a disabled component -> re-composite
}

void DevelopProperties::setSubmaskOp(int index, int op)
{
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    if (op < 0 || op > int(MaskOp::Intersect)) return;
    /* The first submask starts the mask: there is nothing under it to combine with, so
       it always adds. */
    if (index == 0) return;
    if (scope->components[index].op == op) return;
    scope->components[index].op = op;
    /* A pending submask's op is previewed from pendingOp, so move that too or the veil
       and the render would keep showing the old op until commit. */
    if (index == pendingIdx) { pendingOp = op; latchedMaskOp = op; }
    noteEdit(SubmaskList::opName(op) + " "
             + maskToolName(scope->components.at(index).tool));
    buildTree();
    emit paramsChanged();
}

void DevelopProperties::toggleSubmaskInverted(int index)
{
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    MaskComponent &m = scope->components[index];
    m.inverted = !m.inverted;
    noteEdit("Invert " + maskToolName(m.tool), m.inverted ? "On" : "Off");
    /* The panel's Invert checkbox shows the same flag when this submask is the open one;
       buildTree -> syncMaskPanel repaints the row, so only the editor needs seeding. */
    if (index == selectedMaskIndex && maskPanel && maskPanel->editor())
        maskPanel->editor()->setCheckboxValue("maskInvert", m.inverted);
    buildTree();
    emit paramsChanged();
}

void DevelopProperties::moveSubmask(int from, int to)
{
    EditScope *scope = activeScope();
    if (!scope) return;
    const int n = scope->components.size();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    /* Order IS the result: the components are folded sequentially, so a Subtract before
       the Add it was meant to cut into does nothing. */
    scope->components.move(from, to);
    if      (selectedMaskIndex == from) selectedMaskIndex = to;
    else if (selectedMaskIndex > from && selectedMaskIndex <= to) selectedMaskIndex--;
    else if (selectedMaskIndex < from && selectedMaskIndex >= to) selectedMaskIndex++;
    if      (pendingIdx == from) pendingIdx = to;
    else if (pendingIdx > from && pendingIdx <= to) pendingIdx--;
    else if (pendingIdx < from && pendingIdx >= to) pendingIdx++;
    noteEdit("Reorder " + maskToolName(scope->components.at(to).tool));
    buildTree();
    emit paramsChanged();
}

void DevelopProperties::duplicateSubmask(int index)
{
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    /* A copy lands directly after the original, where it composites against everything
       the original did -- the usual reason to duplicate is "same shape, other op". */
    const MaskComponent copy = scope->components.at(index);
    scope->components.insert(index + 1, copy);
    if (selectedMaskIndex > index) selectedMaskIndex++;
    if (pendingIdx > index) pendingIdx++;
    noteEdit("Duplicate " + maskToolName(copy.tool));
    buildTree();
    emit paramsChanged();
}

/* History caption for a mask-tool setting key (the tree/panel row's own wording). */
static QString maskSettingLabel(const QString &key)
{
    if (key == "maskFeather")      return "Feather";
    if (key == "maskInvert")       return "Invert";
    if (key == "maskSize")         return "Brush size";
    if (key == "maskFlow")         return "Brush flow";
    if (key == "maskAutoMask")     return "Auto mask";
    if (key == "maskAutoMaskAi")   return "Auto mask AI";
    if (key == "maskRangeLo" || key == "maskRangeHi") return "Range";
    if (key.startsWith("maskHue") || key.startsWith("maskSat")) return "Color range";
    return "Mask setting";
}

/* How many strokes a Brush/Object submask already carries. Everything that has to say
   "next stroke" vs "all strokes" vs "the last stroke" turns on this. */
static int brushStrokeCount(const QString &paramsJson)
{
    return QJsonDocument::fromJson(paramsJson.toUtf8())
               .object().value("strokes").toArray().size();
}

/* Is this a stroke-based tool -- one whose attributes are baked per stroke rather than
   evaluated for the submask as a whole? */
static bool isStrokeTool(int tool)
{
    return tool == int(MaskTool::Brush) || tool == int(MaskTool::Object);
}

QString DevelopProperties::maskAttributeScopeText()
{
/*
    What a change to the settings below the header will actually affect. Three states,
    matching the three scopes onMaskEditorSetting applies (keep the two in step):

      SELECTED   a committed submask re-opened from the list -> its strokes are all down,
                 so an adjustment rewrites every one of them.
      ADDING     the submask is still being built -> the adjustment arms the NEXT stroke,
                 which is how one submask ends up holding strokes of different sizes.
      ADDING     + Shift -> the LAST stroke just painted, the common correction.

    A tool that is not stroke-based has no such split: its attributes describe the whole
    submask, and the header says so rather than going silent.
*/
    MaskComponent *mm = editingMaskComp();
    if (!mm) return QString();
    if (!isStrokeTool(mm->tool))
        return tr("Attribute adjustment applies to this %1 submask")
                   .arg(maskToolName(mm->tool));

    const int n = brushStrokeCount(mm->paramsJson);
    /* ADDING. With nothing painted yet there is no last stroke to name, so Shift or not
       the only thing an adjustment can reach is the next one. */
    if (pendingIdx >= 0 || n == 0) {
        const bool shift = QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier;
        if (shift && n > 0) return tr("Attribute adjustment applies to last stroke");
        return tr("Attribute adjustment applies to next stroke");
    }
    /* SELECTED. */
    return n == 1 ? tr("Attribute adjustment applies to 1 stroke")
                  : tr("Attribute adjustment applies to %1 strokes").arg(n);
}

void DevelopProperties::syncAttributeScopeLabel()
{
    if (maskPanel) maskPanel->setAttributeScope(maskAttributeScopeText());
}

void DevelopProperties::onMaskEditorSetting(const QString &key, const QVariant &v)
{
    MaskComponent *mm = editingMaskComp();
    if (!mm) return;
    /* Brush next-stroke settings (size/flow/autoMask + brush feather): refresh the
       cursor, do NOT re-composite (existing strokes keep their snapshot). */
    const bool brushLike = key == "maskSize" || key == "maskFlow" ||
                           key == "maskAutoMask" || key == "maskAutoMaskAi" ||
                           (key == "maskFeather" && mm->tool == int(MaskTool::Brush));
    if (brushLike) {
        /* THREE SCOPES, one slider. Size/feather/flow/auto-mask are baked into each
           stroke as it is drawn, which is what lets one submask hold strokes of different
           sizes -- and what makes a bare slider ambiguous. The panel's header row
           (maskAttributeScopeText) always names which of these is in force; keep the two
           in step.

             . ADDING (pendingIdx >= 0, the submask is still being built) -> the NEXT
               stroke, unchanged behaviour: it is how a submask gets varied strokes.
             . ADDING + SHIFT, with a stroke already painted -> the LAST stroke only.
               "I just painted that, make it softer" -- the common correction, and the
               reason Shift is here. The next-stroke default is left alone, so the next
               stroke still comes out as armed.
             . SELECTED (pendingIdx < 0, re-opened from the submask list) -> EVERY stroke.
               Nothing is being painted, so there is no next stroke to arm and no "last"
               one to single out; a slider that only armed would move and change nothing
               at all. */
        const bool adding = (pendingIdx >= 0);
        const bool shift = QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier;
        const bool lastOnly = adding && shift && brushStrokeCount(mm->paramsJson) > 0;
        const bool retro = !adding;
        auto put = [&](const QString &k, const QJsonValue &val) {
            mm->paramsJson = lastOnly ? brushLastStrokeWith(mm->paramsJson, k, val)
                           : retro    ? brushStrokesWith(mm->paramsJson, k, val)
                                      : brushWith(mm->paramsJson, k, val);
        };
        if (key == "maskFeather") {
            /* Only the strokes carry a feather; the next-stroke path passes it to
               ImageView through emitBrushSettings instead of storing it in paramsJson, so
               the component's own feather moves in every case. */
            mm->feather = v.toFloat();
            if (lastOnly)   mm->paramsJson = brushLastStrokeWith(mm->paramsJson, "feather", v.toInt());
            else if (retro) mm->paramsJson = brushStrokesWith(mm->paramsJson, "feather", v.toInt());
        }
        else if (key == "maskSize")     put("size", v.toInt());
        else if (key == "maskFlow")     put("flow", v.toInt());
        else if (key == "maskAutoMask") put("autoMask", v.toBool());
        else if (key == "maskAutoMaskAi") {
            put("autoMaskMode", v.toBool() ? "ai" : "lum");
            if (v.toBool()) emit maskBrushAiEnabled();      // pre-warm the SAM encoder
        }
        emitBrushSettings(*mm);
        /* A stroke that is already down changed -> re-composite. Arming the next stroke
           changes nothing on screen but the cursor. */
        if (lastOnly || retro) emit paramsChanged();
        syncAttributeScopeLabel();   // the stroke count may have made "last stroke" real
    }
    /* Content-range settings (Luminance/Depth lo/hi, Color Range hue/sat): change
       coverage, so re-composite AND live-update the loupe tint. */
    else if (key == "maskRangeLo" || key == "maskRangeHi" || key == "maskHueLo" ||
             key == "maskHueHi"   || key == "maskSatLo"   || key == "maskSatHi") {
        if      (key == "maskRangeLo") mm->paramsJson = brushWith(mm->paramsJson, "lo", v.toDouble() / 100.0);
        else if (key == "maskRangeHi") mm->paramsJson = brushWith(mm->paramsJson, "hi", v.toDouble() / 100.0);
        else if (key == "maskHueLo")   mm->paramsJson = brushWith(mm->paramsJson, "hueLo", v.toInt());
        else if (key == "maskHueHi")   mm->paramsJson = brushWith(mm->paramsJson, "hueHi", v.toInt());
        else if (key == "maskSatLo")   mm->paramsJson = brushWith(mm->paramsJson, "satLo", v.toInt());
        else                           mm->paramsJson = brushWith(mm->paramsJson, "satHi", v.toInt());
        emit maskRangeChanged(mm->paramsJson);
        emit paramsChanged();
    }
    /* Feather (non-brush) / Invert: change the mask -> overlay + re-composite. */
    else if (key == "maskFeather" || key == "maskInvert") {
        if (key == "maskFeather") { mm->feather = v.toFloat(); emit maskFeatherChanged(v.toDouble()); }
        else                      { mm->inverted = v.toBool(); emit maskInvertChanged(v.toBool()); }
        emit paramsChanged();
    }
    else return;
    noteEdit(maskSettingLabel(key), historyValueText(QModelIndex(), v),
             "mask/" + key);
}

void DevelopProperties::onMaskEditorWheel(int hueLo, int hueHi, int satLo, int satHi, bool commit)
{
    MaskComponent *mm = editingMaskComp();
    if (!mm) return;
    mm->paramsJson = brushWith(mm->paramsJson, "hueLo", hueLo);
    mm->paramsJson = brushWith(mm->paramsJson, "hueHi", hueHi);
    mm->paramsJson = brushWith(mm->paramsJson, "satLo", satLo);
    mm->paramsJson = brushWith(mm->paramsJson, "satHi", satHi);
    noteEdit("Color range", QString(), "mask/colorRangeWheel");
    emit maskRangeChanged(mm->paramsJson);      // live-update the loupe tint
    emit paramsChanged();                        // re-composite the masked scope
}

MaskComponent *DevelopProperties::editingMaskComp()
{
    EditScope *l = activeScope();
    if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->components.size())
        return &l->components[selectedMaskIndex];
    return nullptr;
}

void DevelopProperties::onMaskToolChosen(int tool)
{
    /* Every tool builds up through the MaskPanel (blue/red flow); its settings render in
       the panel's embedded MaskEditor. */
    beginMaskTool(tool);
}

void DevelopProperties::beginMaskTool(int tool)
{
    if (G::isLogger) G::log("DevelopProperties::beginMaskTool");
    EditScope *scope = activeScope();
    if (!scope || activeScopeIndex == 0) return;

    const bool first = scope->components.isEmpty();
    MaskComponent m;
    m.op   = int(MaskOp::Add);
    m.tool = tool;
    m.paramsJson = defaultMaskParams(tool);
    if (tool == int(MaskTool::Brush)) m.feather = 0.0f;   // brush -> crisp edge

    /* Append the submask; its settings render in the panel's embedded MaskEditor. EVERY
       submask is pending until the commit button / Return folds it in -- the first one
       too, which
       costs nothing visually (the mask IS that submask) and keeps one commit path. */
    scope->components.append(m);
    selectedMaskIndex = scope->components.size() - 1;
    pendingIdx = selectedMaskIndex;
    pendingPristine = m;                 // baseline for pendingMaskIsUntouched
    pendingOp = int(MaskOp::Add);        // every submask opens Add ...
    latchedMaskOp = int(MaskOp::Add);    // ... with nothing latched from the last one
    maskPanelOpen = true;
    if (maskPanel) {
        maskPanel->beginPending(first);
        /* Populate the panel's embedded settings tree, aligned to the tree's split (less
           the indent the nested panel sits at -- see kMaskPanelIndent). */
        if (MaskEditor *ed = maskPanel->editor()) {
            ed->setCaptionWidth(captionColumnWidth - kMaskPanelIndent);
            ed->showTool(m);
            if (tool == int(MaskTool::ColorRange))
                ed->setWheelSamples(colorRangeSamplesHS(m.paramsJson));
        }
    }

    noteEdit("Add " + maskToolName(tool));
    buildTree();                 // emits maskEditBegin for the on-canvas overlay
    /* The whole-mask veil is a full-resolution overlay rebuilt on every drag tick, so it
       is shown only where it is the ONLY feedback there is: the scope's FIRST submask,
       when nothing on screen yet says the mask exists. A second or later submask starts
       hidden -- the tool's own live preview (brush coverage, gradient handles, range
       swatches) already shows what is being added -- and "O" brings the veil back on
       demand. Must follow buildTree(), whose maskEditBegin drives the overlay, and
       precede paramsChanged(), whose tick reads the visibility to decide whether to
       build at all. */
    if (first) emit maskTintShowRequested();
    else       emit maskTintHideRequested();
    emit paramsChanged();        // overlay/tint (red committed, blue pending)
}

int DevelopProperties::maskOpFromModifiers()
{
    const Qt::KeyboardModifiers m = QGuiApplication::queryKeyboardModifiers();
    const bool alt = m & Qt::AltModifier, shift = m & Qt::ShiftModifier;
    return alt ? (shift ? int(MaskOp::Intersect) : int(MaskOp::Subtract))
               : int(MaskOp::Add);
}

void DevelopProperties::setPendingMaskOp(int op)
{
    /* The held modifier changed (MW arbitrates: Opt = Subtract, Shift+Opt = Intersect).
       The op is NOT written into the component -- it stays pending until commit -- but
       the overlay AND the render composite the pending submask with it, so both preview
       the outcome, and the panel button renames itself to match. The FIRST submask has
       nothing to combine with, so it is pinned to Add. */
    /* Only a PENDING submask takes its op from the modifiers. A re-opened submask's op is
       already real and is changed on its own row in the submask list -- letting a stray
       Opt press rewrite it would be an invisible edit. */
    if (pendingIdx < 0) return;
    if (pendingIdx == 0) op = int(MaskOp::Add);
    /* Nothing held now: fall back to the LATCHED op rather than to Add. Releasing Opt
       must not undo the subtract the user just painted (see latchMaskOp) -- the whole
       point of the modifier is to say what the submask does, not to hold a preview open
       with a spare finger while reaching for the commit button. */
    if (op == int(MaskOp::Add)) op = latchedMaskOp;
    if (op == pendingOp) return;
    pendingOp = op;
    if (maskPanel) maskPanel->setPendingOp(op);
    syncMaskPanel();            // the pending submask's row shows the previewed op too
    /* The render composites the pending submask with this op too (see stackJob), so the
       modifier changes the IMAGE, not just the veil: re-render as well as re-composite
       (paramsChanged is wired to updateMaskOverlayTint, so the veil comes along). */
    emit paramsChanged();
}

void DevelopProperties::latchMaskOp()
{
/*
    A shaping action started on the pending submask -- a brush/object stroke, or a mask
    handle drag (ImageView::maskOpActionStarted). Whatever modifier is held AT THAT
    INSTANT is what this submask does, and it now STICKS: the veil, the render and the
    commit button all keep showing Subtract (or Intersect) after the key is released.

    Before this the op was purely momentary. Painting a subtract stroke and letting go of
    Opt silently turned it back into an add, the image stopped showing the subtraction,
    and clicking the commit button committed the ADD -- "the subtract brush does nothing".

    Reversible by the same gesture that set it: paint (or drag) again with nothing held
    and the latch re-reads the modifiers as Add. A modifier PRESS alone still previews
    momentarily, so peeking at the outcome without committing to it works as before.
*/
    if (!maskPanelOpen) return;
    latchedMaskOp = (pendingIdx <= 0) ? int(MaskOp::Add)   // first submask: nothing to
                                      : maskOpFromModifiers();   // combine with
    setPendingMaskOp(latchedMaskOp);
}

void DevelopProperties::commitPendingMask()
{
    if (G::isLogger) G::log("DevelopProperties::commitPendingMask");
    EditScope *scope = activeScope();
    /* Re-editing an already committed submask: the button reads "Done" and there is
       nothing to fold in -- its edits went into the mask as they were made. Just close
       the editing session, leaving the submask (and the mask) exactly as it is. */
    if (pendingIdx < 0) {
        if (selectedMaskIndex < 0) return;
        selectedMaskIndex = -1;
        maskLatched = true;              // the combined mask stays "O"-toggleable
        buildTree();                     // updateMaskEdit -> take the handles off canvas
        emit paramsChanged();
        emit maskTintHideRequested();
        return;
    }
    if (!scope || pendingIdx >= scope->components.size()) return;
    /* Commit what the user is LOOKING AT: pendingOp is what the veil, the render and the
       button label have all been previewing (the held modifier, else the latched op).
       Re-reading the live modifiers here instead -- as this used to -- meant releasing
       Opt to reach for the commit button committed an Add. */
    int op = (pendingIdx == 0) ? int(MaskOp::Add)   // nothing to combine the first with
                               : pendingOp;
    scope->components[pendingIdx].op = op;        // fold the submask in (sequential)
    const QString toolName = maskToolName(scope->components[pendingIdx].tool);
    pendingIdx = -1;
    pendingOp = int(MaskOp::Add);
    latchedMaskOp = int(MaskOp::Add);
    /* selectedMaskIndex is deliberately LEFT ALONE. Committing folds the submask into the
       mask but does not flatten it away: it stays listed and stays editable, so the user
       can come straight back to it (or keep tuning it) instead of having to delete and
       redraw. Only its PENDING status ends here. */
    maskLatched = true;                    // overlay stays AVAILABLE ("O" re-shows it)
    maskPanelOpen = false;                 // nothing outstanding to confirm any more
    const QString verb = op == int(MaskOp::Subtract)  ? "Subtract "
                       : op == int(MaskOp::Intersect) ? "Intersect "
                                                      : "Add ";
    noteEdit(verb + toolName);
    buildTree();
    emit paramsChanged();
    /* The submask is folded in: get the coverage tint off the image so the user sees the
       developed result. The overlay is still latched, so "O" (or the header menu row)
       brings it back. Must follow paramsChanged() -- that rebuilds/re-sets the tint. */
    emit maskTintHideRequested();
}

bool DevelopProperties::pendingMaskIsUntouched(const MaskComponent &m) const
{
/*
    "Nothing to lose": true when committing this pending submask would achieve nothing,
    so the leave-the-scope prompt is pure noise. Two ways that happens:

      1. It COVERS NOTHING. Brush and Object start with an empty stroke list and Color
         Range with no samples -- picking the tool alone paints no pixels, and neither
         does dragging the brush-size slider around. (The other tools' defaults ARE a
         mask: a gradient lands centred, the range tools open on a band.)
      2. It was NEVER TOUCHED -- still byte-identical to what beginMaskTool built. That
         is what catches the centred-gradient case: a real mask, but not one the user
         made, so there is no work to protect.

    Anything that misses both tests just means the prompt appears -- the safe direction.
*/
    const QJsonObject o = QJsonDocument::fromJson(m.paramsJson.toUtf8()).object();
    if (m.tool == int(MaskTool::Brush) || m.tool == int(MaskTool::Object))
        return o.value("strokes").toArray().isEmpty();
    if (m.tool == int(MaskTool::ColorRange))
        return o.value("samples").toArray().isEmpty();

    return m.tool     == pendingPristine.tool
        && m.inverted == pendingPristine.inverted
        && m.feather  == pendingPristine.feather
        && m.paramsJson == pendingPristine.paramsJson;
}

bool DevelopProperties::confirmPendingMask()
{
/*
    A submask is PENDING from the moment its tool is picked until the commit button /
    Return folds it in -- it exists in scope->components but is discarded on cancel or on
    leaving the image. Anything that leaves the scope it belongs to therefore throws work
    away, and the only cue that work is outstanding is the MaskPanel being up. Ask
    instead, naming the tool and the op the user has been previewing so the dialog reads
    like the button they didn't press.

    Returns true if the caller may proceed (the submask was committed or discarded),
    false for "stay on this scope".
*/
    if (G::isLogger) G::log("DevelopProperties::confirmPendingMask");
    if (!maskPanelOpen) return true;

    EditScope *scope = activeScope();
    if (!scope || pendingIdx < 0 || pendingIdx >= scope->components.size()) {
        cancelMaskTool();          // nothing coherent to commit: just close the panel
        return true;
    }

    /* Picked a tool but never made a mask with it: nothing to commit, so don't ask --
       drop it exactly as leaving used to, silently. */
    const MaskComponent &pending = scope->components.at(pendingIdx);
    if (pendingMaskIsUntouched(pending)) { cancelMaskTool(); return true; }

    const QString tool = maskToolName(pending.tool);
    /* Same resolution as commitPendingMask: the first submask has nothing to combine
       with, otherwise it is the op the veil and the button have been previewing. */
    const int op = (pendingIdx == 0) ? int(MaskOp::Add) : pendingOp;
    /* The op reads as what it DOES to the mask, so the Commit line is a sentence. */
    const QString what = op == int(MaskOp::Subtract)  ? tr("subtract it from the mask")
                       : op == int(MaskOp::Intersect) ? tr("intersect it with the mask")
                                                      : tr("add it to the mask");

    /* Built by hand rather than as a QMessageBox: that lays its buttons out by ROLE, in
       a platform-specific order, and pads them unevenly (mac pushes the destructive one
       to the far left). The order here is fixed -- Cancel, Commit, Discard -- with equal
       widths and one spacing between them on both platforms. */
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Commit submask?"));
    QVBoxLayout *v = new QVBoxLayout(&dlg);
    v->setContentsMargins(20, 18, 20, 16);
    v->setSpacing(10);

    QLabel *heading = new QLabel(tr("The %1 submask on \"%2\" has not been committed.")
                                     .arg(tool, scope->name), &dlg);
    QFont hf = heading->font();
    hf.setBold(true);
    heading->setFont(hf);
    heading->setWordWrap(true);
    v->addWidget(heading);

    /* Spell out all three outcomes: the buttons are one word each, and "Discard" vs
       "Cancel" is exactly the pair a user can read backwards and lose the work. */
    QLabel *detail = new QLabel(tr("Commit — %1, then continue.\n"
                                   "Discard — throw the submask away and continue.\n"
                                   "Cancel — stay on \"%2\" and keep building it.")
                                    .arg(what, scope->name), &dlg);
    detail->setWordWrap(true);
    v->addWidget(detail);
    v->addSpacing(6);

    QHBoxLayout *h = new QHBoxLayout;
    h->setSpacing(12);
    QPushButton *cancelBtn  = new QPushButton(tr("Cancel"),  &dlg);
    QPushButton *commitBtn  = new QPushButton(tr("Commit"),  &dlg);
    QPushButton *discardBtn = new QPushButton(tr("Discard"), &dlg);
    int choice = 0;                                  // 0 = Cancel (also Esc / close box)
    for (QPushButton *b : {cancelBtn, commitBtn, discardBtn}) {
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);  // equal widths
        b->setAutoDefault(false);
        h->addWidget(b);
    }
    v->addLayout(h);
    connect(cancelBtn,  &QPushButton::clicked, &dlg, [&]{ choice = 0; dlg.accept(); });
    connect(commitBtn,  &QPushButton::clicked, &dlg, [&]{ choice = 1; dlg.accept(); });
    connect(discardBtn, &QPushButton::clicked, &dlg, [&]{ choice = 2; dlg.accept(); });
    /* Discard is the default (Return); Esc rejects the dialog, which leaves choice at
       Cancel. autoDefault is off on the other two so neither steals Return. */
    discardBtn->setAutoDefault(true);
    discardBtn->setDefault(true);
    discardBtn->setFocus();
    dlg.exec();

    if (choice == 1) { commitPendingMask(); return true; }
    if (choice == 2) { cancelMaskTool();    return true; }
    return false;
}

void DevelopProperties::cancelMaskTool()
{
    EditScope *scope = activeScope();
    /* Remove the pending submask being defined (on an empty scope that is its only
       submask -> the scope goes back to global). ONLY a pending submask is discarded: a
       re-opened committed one has nothing to cancel, so this is a plain deselect and its
       edits stay in the mask. */
    if (scope && pendingIdx >= 0 && pendingIdx < scope->components.size())
        scope->components.removeAt(pendingIdx);
    pendingIdx = -1;
    pendingOp = int(MaskOp::Add);
    latchedMaskOp = int(MaskOp::Add);
    selectedMaskIndex = -1;
    /* Cancel discards the in-progress tool. Keep the overlay only if committed masks
       remain on the scope. */
    maskLatched = (scope && !scope->components.isEmpty());
    maskPanelOpen = false;
    /* The panel itself stays up (it belongs to the mask, not to the tool) -- buildTree ->
       syncMaskPanel drops the settings block and leaves the submask list. */
    noteEdit("Cancel submask");
    buildTree();
    emit paramsChanged();
}

void DevelopProperties::setPanelEnabled(bool enabled)
{
    if (G::isLogger) G::log("DevelopProperties::setPanelEnabled");
    panelEnabled = enabled;

    /* The tree view: blocks interaction and (because the sliders/checkboxes/combos are
       persistent editors parented under the viewport) inherits a disabled look to every
       control without clobbering their individual enabled flags. */
    setEnabled(enabled);

    /* The ScopeHeader band (scope dropdown + buttons + eye) sits ABOVE the tree and is a
       sibling widget, so it must be greyed explicitly. */
    if (scopeHeader) scopeHeader->setEnabled(enabled);

    applyItemsEnabled(enabled);
}

void DevelopProperties::applyItemsEnabled(bool enabled)
{
    /* Caption text is painted by PropertyDelegate from the per-item UR_isEnabled role, NOT
       the widget's enabled state, so walk every row (recursively) and set the flag or the
       captions stay black over the greyed controls. buildTree() calls this too, because a
       rebuild recreates rows with the default (enabled) flag and would otherwise un-grey a
       panel that is meant to stay disabled. */
    std::function<void(const QModelIndex &)> setRows = [&](const QModelIndex &parent) {
        const int rows = model->rowCount(parent);
        for (int r = 0; r < rows; ++r) {
            QModelIndex capIdx = model->index(r, CapColumn, parent);
            model->setData(capIdx, enabled, UR_isEnabled);
            setRows(capIdx);            // children hang off the caption (column 0) index
        }
        if (rows)
            emit model->dataChanged(model->index(0, CapColumn, parent),
                                    model->index(rows - 1, CapColumn, parent));
    };
    setRows(QModelIndex());
    /* The raw-denoise amount sliders stay disabled (greyed caption + value) until the
       PMRID base exists -- they only scale that base's blend, which is absent until the
       denoise has run. Re-assert here so enabling the panel doesn't un-grey them. */
    if (enabled && denoiseRunCheck && !(mw && mw->rawDenoiseReadyForCurrent())) {
        setItemEnabled("denoiseLuma", false);
        setItemEnabled("denoiseChroma", false);
    }
    viewport()->update();
}

void DevelopProperties::onScopeSelected(const QString &name)
{
    if (G::isLogger) G::log("DevelopProperties::onScopeSelected", name);
    if (currentImagePath.isEmpty()) return;
    const int idx = currentScopeNames().indexOf(name);
    if (idx < 0 || idx == activeScopeIndex) return;
    /* A submask still being built on the scope being LEFT used to be discarded silently.
       Ask instead; "Cancel" abandons the switch, so put the header selection back (the
       row/dropdown has already moved to the clicked scope). */
    if (!confirmPendingMask()) { refreshScopeList(); return; }
    activeScopeIndex = idx;

    EditScope *l = activeScope();
    const bool hasMask = (idx > 0 && l && !l->components.isEmpty());
    if (G::useScopeHeaderLab) {
        /* Lab UI: arriving at a scope opens no submask -- show the scope's COMBINED mask
           (red, no handles) via the latch when it has one. Its submasks are listed in the
           panel below the row, and clicking one re-opens it. */
        selectedMaskIndex = -1;
        maskLatched = hasMask;
    }
    else {
        /* Legacy: select the first mask tool so its coverage overlays the image. */
        maskLatched = false;
        selectedMaskIndex = hasMask ? 0 : -1;
    }

    refreshScopeList();          // move checkmark + re-caption actions to new scope
    buildTree();                  // swap top items (Core/masks) + repopulate sections

    /* Un-collapse the scope's own items (its mask rows). Bulk-guarded so setTreeCollapsed
       does not fold the adjustment sections under Solo -- those stay as the user left
       them. */
    isBulkExpandCollapse = true;
    setTreeCollapsed(false);
    isBulkExpandCollapse = false;
    if (scopeHeader) scopeHeader->setCollapsed(false);

    emit paramsChanged();         // the renderer shows the active scope
    /* Selecting a scope is an explicit "show me this mask", so clear any sticky hidden
       flag left by finishing a tool or moving an adjustment slider. */
    if (hasMask) emit maskTintShowRequested();
}

void DevelopProperties::onSpotToolToggled(bool active)
{
    if (G::isLogger) G::log("DevelopProperties::onSpotToolToggled");
    if (spotMode == active) return;
    spotMode = active;
    emit spotActiveChanged(active);            // update the action-row spot button icon
    if (active) { emit spotEditBegin(); emitSpotPins(); }   // arm + seed the pins
    else        emit spotEditEnd();
}

void DevelopProperties::onSpotStrokeCommitted(const QString &paramsJson)
{
    if (G::isLogger) G::log("DevelopProperties::onSpotStrokeCommitted");
    if (currentImagePath.isEmpty() || paramsJson.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    FillSpot fs;
    fs.paramsJson = paramsJson;
    s.spots.append(fs);
    noteScopeEdit(QString(), "Spot heal");   // whole-image, belongs to no scope
    emitSpotPins();
    emit paramsChanged();                   // re-render -> MiganFill heals the new spot
}

void DevelopProperties::onSpotRemoveRequested(int index)
{
    if (G::isLogger) G::log("DevelopProperties::onSpotRemoveRequested");
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (index < 0 || index >= s.spots.size()) return;
    s.spots.remove(index);
    noteScopeEdit(QString(), "Remove spot heal");
    emitSpotPins();
    emit paramsChanged();                   // re-render without the removed spot
}

/* Centre (centroid of the stroke points) of each spot, in output-normalized coords, for
   the pins. Order matches EditStack::spots so a pin index maps to a spot. */
QVector<QPointF> DevelopProperties::spotPinCenters() const
{
    QVector<QPointF> centers;
    if (currentImagePath.isEmpty()) return centers;
    const EditStack s = stackCache.value(currentImagePath);
    for (const FillSpot &sp : s.spots) {
        const FillSpotGeom::Parsed p = FillSpotGeom::parse(sp.paramsJson);
        double cx = 0.5, cy = 0.5;
        if (p.valid()) FillSpotGeom::centroid(p, cx, cy);   // add-coverage centroid
        centers.append(QPointF(cx, cy));
    }
    return centers;
}

void DevelopProperties::emitSpotPins()
{
    emit spotPinsChanged(spotPinCenters());
}

void DevelopProperties::renameActiveScope()
{
    if (G::isLogger) G::log("DevelopProperties::renameActiveScope");
    if (currentImagePath.isEmpty()) return;
    if (activeScopeIndex == 0) {
        if (G::popup) G::popup->showPopup("Global cannot be renamed.");
        return;
    }
    EditStack &s = stackCache[currentImagePath];
    if (activeScopeIndex >= s.scopes.size()) return;

    const QString oldName = s.scopes[activeScopeIndex].name;
    bool ok = false;
    const QString entered = QInputDialog::getText(this, "Rename mask", "Mask name:",
                                                  QLineEdit::Normal, oldName, &ok).trimmed();
    if (!ok || entered.isEmpty() || entered == oldName) return;

    /* uniqueScopeName still sees this scope's own OLD name, so it only suffixes on a clash with a
       DIFFERENT scope. */
    s.scopes[activeScopeIndex].name = uniqueScopeName(entered);
    noteEdit("Rename mask", s.scopes[activeScopeIndex].name);
    refreshScopeList();
}

void DevelopProperties::resetActiveScope()
{
    if (G::isLogger) G::log("DevelopProperties::resetActiveScope");
    if (currentImagePath.isEmpty()) return;
    EditScope *l = activeScope();
    if (!l) return;
    /* Restore this scope's adjustments to their defaults and un-hide every group (non-destructive to
       the mask geometry, which defines WHERE the scope applies). */
    l->params = EditParams();
    l->showScope = l->showBasic = l->showColor = l->showCalibrate =
        l->showColorGrade = l->showEffects = true;
    noteEdit("Reset all");
    buildTree();                 // repopulate the sliders at their defaults + the eyes
    emit paramsChanged();
}

void DevelopProperties::onScopePreviewToggled(bool shown)
{
    if (G::isLogger) G::log("DevelopProperties::onScopePreviewToggled");
    EditScope *l = activeScope();
    if (!l) { if (scopeHeader) scopeHeader->setPreviewShown(true); return; }
    l->showScope = shown;        // non-destructive: kept, scope folds off at render
    noteEdit(shown ? "Show scope" : "Hide scope");
    refreshPreviewButtons();
    emit paramsChanged();
}

void DevelopProperties::onScopeEnabledToggled(int index, bool on)
{
    if (G::isLogger) G::log("DevelopProperties::onScopeEnabledToggled");
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    /* Global (index 0) now has a checkbox too; the compositor honours scopes[0].enabled
       (see stackJob) so a disabled Global renders with no develop adjustments. */
    if (index < 0 || index >= s.scopes.size()) return;
    if (s.scopes[index].enabled == on) return;
    s.scopes[index].enabled = on;                 // compositor skips a disabled scope
    noteScopeEdit(s.scopes[index].name, on ? "Show" : "Hide");
    emit paramsChanged();                          // re-composite with the scope on/off
}

void DevelopProperties::setTreeCollapsed(bool collapsed)
{
    /* Hide only the SCOPE items -- the Core rows (Global) or mask-tool rows that sit ABOVE
       the Basic/Color/Effects sections. The sections stay visible. */
    scopeItemsCollapsed = collapsed;
    applyScopeItemsCollapsed();
    applyCoreVisibility();           // core rows also depend on collapse state

    /* Solo: showing the scope's items is one of the mutually-exclusive sections, so
       collapse its peers -- the three adjustment sections and the Raw panel (they would
       otherwise stay open alongside the scope). */
    if (!collapsed) soloCollapseOthers(SoloOwner::ScopeRow);
}

void DevelopProperties::soloCollapseOthers(SoloOwner owner, const QString &keepSection)
{
    /* Skipped during the Expand-all / Collapse-all sweep, which drives every peer. */
    if (isBulkExpandCollapse) return;
    if (!setting->value("Develop/isSolo", false).toBool()) return;

    /* Raw panel (lab UI, raw files only -- null or hidden otherwise). */
    if (owner != SoloOwner::RawPanel && rawPanel && !rawPanel->isCollapsed())
        rawPanel->setCollapsed(true);

    /* The scope's own items; its collapse arrow lives in the ScopeHeader band, not the
       tree, so it has to be driven by hand. */
    if (owner != SoloOwner::ScopeRow && !scopeItemsCollapsed) {
        setTreeCollapsed(true);
        if (scopeHeader) scopeHeader->setCollapsed(true);
    }

    /* The adjustment sections. The base PropertyEditor already collapses tree siblings
       when a root expands; this also covers the Raw panel / Scope row cases. */
    for (const char *h : {"BasicHeader", "ColorHeader", "EffectsHeader"}) {
        if (keepSection == QLatin1String(h)) continue;
        const QModelIndex idx = findCaptionIndex(h);
        if (idx.isValid()) collapse(idx);
    }
}

void DevelopProperties::applyScopeItemsCollapsed()
{
    /* The scope's top items are the root rows BEFORE the Basic section (Core rows for Global, else the
       mask-tool rows). Re-applied after each buildTree, since a rebuild resets row visibility. */
    const QModelIndex basic = findCaptionIndex("BasicHeader");
    const int firstSection = basic.isValid() ? basic.row() : model->rowCount();
    for (int r = 0; r < firstSection; ++r)
        setRowHidden(r, QModelIndex(), scopeItemsCollapsed);
    scheduleContentFit();       // setRowHidden emits nothing: re-fit the block ourselves
}

void DevelopProperties::bindToneSlider(ToneRegionSlider *slider)
{
    if (G::isLogger) G::log("DevelopProperties::bindToneSlider");
    toneSlider = slider;
    if (!toneSlider) return;
    connect(toneSlider, &ToneRegionSlider::valueChanged,
            this, &DevelopProperties::onToneSplitsChanged);
    /* Reflect whatever image is already current (if any). */
    populateSlidersFromStack();
}

void DevelopProperties::onToneSplitsChanged(double shadow, double crossover, double highlight)
{
    if (G::isLogger) G::log("DevelopProperties::onToneSplitsChanged");
    if (isPopulating) return;
    if (currentImagePath.isEmpty()) return;

    EditParams &p = activeParams();
    p.toneShadowCenter    = static_cast<float>(shadow);
    p.toneCrossover       = static_cast<float>(crossover);
    p.toneHighlightCenter = static_cast<float>(highlight);

    /* The splits move only the shadows/highlights centres (blacks/whites are pinned), so they
       change pixels only when one of those two is engaged. Skip the re-render otherwise -- the
       new positions still ride in the params for when a tone slider is next moved -- so dragging
       the handles on an untoned image does not spin up needless full-res renders. */
    if (p.shadows == 0.0f && p.highlights == 0.0f) return;
    noteEdit("Tonal ranges", QString(), "tone/splits");
    emit paramsChanged();
}

void DevelopProperties::newScope()
{
    if (G::isLogger) G::log("DevelopProperties::newScope");
    if (currentImagePath.isEmpty()) return;
    /* Adding a scope switches to it, so it leaves any pending submask behind on the
       current scope just as picking one does -- same prompt. */
    if (!confirmPendingMask()) return;

    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());     // ensure a base scope exists
    s.scopes[0].name = "Global";                    // index 0 is always the Global scope

    /* Prompt for the name, defaulting to "Scope n" (the first above Global is "Scope 1", since Global is
       index 0 and the new scope's index equals the pre-append size). Pop the dialog near the cursor
       (the [+] button just clicked) rather than screen-centre, so the flow stays where the user is
       looking, clamped to the screen. */
    const QString def = uniqueScopeName("Mask " + QString::number(s.scopes.size()));
    QInputDialog dlg(this);
    dlg.setWindowTitle("New mask");
    dlg.setLabelText("Mask name:");
    dlg.setTextEchoMode(QLineEdit::Normal);
    dlg.setTextValue(def);
    dlg.adjustSize();
    const QPoint cur = QCursor::pos();
    QPoint tl(cur.x() - dlg.width() / 2, cur.y() - 20);
    if (QScreen *scr = QGuiApplication::screenAt(cur)) {
        const QRect a = scr->availableGeometry();
        tl.setX(qBound(a.left(), tl.x(), a.right()  - dlg.width()));
        tl.setY(qBound(a.top(),  tl.y(), a.bottom() - dlg.height()));
    }
    dlg.move(tl);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString entered = dlg.textValue().trimmed();

    EditScope l;
    l.name = uniqueScopeName(entered.isEmpty() ? def : entered);
    s.scopes.append(l);
    activeScopeIndex = s.scopes.size() - 1;                   // edit the new scope
    noteScopeEdit(l.name, "New mask scope");

    /* New scope is identity, so the rendered result is unchanged; just refresh the header
       combo and rebuild the tree for it. The stack only persists once a scope actually
       changes a pixel. */
    selectedMaskIndex = -1;
    refreshScopeList();
    buildTree();

    /* A new scope's whole point is to apply a masked adjustment, so the next step is choosing a mask
       tool -- pop that menu straight away (near the cursor). Escape just leaves an empty scope. */
    showMaskMenu();
}

void DevelopProperties::deleteScope()
{
    if (G::isLogger) G::log("DevelopProperties::deleteScope");
    if (currentImagePath.isEmpty()) return;

    EditStack &s = stackCache[currentImagePath];
    if (activeScopeIndex <= 0) {
        if (G::popup) G::popup->showPopup("Global cannot be removed.");
        return;
    }
    if (activeScopeIndex >= s.scopes.size()) activeScopeIndex = s.scopes.size() - 1;
    /* No prompt here: the scope holding the pending submask is being removed, so there is
       nothing to commit it INTO. Just close the panel, or it keeps editing indices into a
       scope that no longer exists. */
    if (maskPanelOpen) cancelMaskTool();
    const QString goneName = s.scopes.at(activeScopeIndex).name;
    s.scopes.removeAt(activeScopeIndex);
    if (activeScopeIndex >= s.scopes.size()) activeScopeIndex = s.scopes.size() - 1;
    noteScopeEdit(QString(), "Delete " + goneName);

    selectedMaskIndex = -1;
    refreshScopeList();
    buildTree();
    emit paramsChanged();                    // removing a scope changes the result
}

/* ----------------------------------------------------------------------------------------
   Mask (one mask per mask, built from an ordered list of Add/Subtract tools)

   Tree shape (the mask-tool rows at the top of the tree, above the Basic/Color/Effects sections):

     Add Linear Gradient       [+][-]    <- one row per tool (caption = op + tool; [+] adds another, [-] removes)
     Subtract Brush            [+][-]    <- selected tool is expanded; its settings are ITS children
        Feather  [slider]
        Invert   [checkbox]
        Done     [checkbox]              <- ends the build (collapses the tool)

   Tool rows are header-style (single-line full-width caption + expand arrow) but flagged
   UR_LeafSingleLine so they paint in the leaf colour, not category teal. The [+] glyph (UR_AddBtn,
   hit-tested in mousePressEvent) pops showMaskMenu to append a tool (newMask); a new scope's first
   tool is offered automatically (newScope -> showMaskMenu). Subtract is offered only once a tool
   exists ("the first must be added"). Only the SELECTED tool carries its settings children (no duplicate tool-name
   row); clicking a tool (mousePressEvent) toggles which one is expanded. Spatial editing (drag/
   paint/AI-select on the image) is deferred to the canvas; rendering does not yet composite the
   mask.
   ---------------------------------------------------------------------------------------- */

QString DevelopProperties::maskToolName(int tool)
{
    switch (static_cast<MaskTool>(tool)) {
    case MaskTool::LinearGradient:  return "Linear Gradient Mask";
    case MaskTool::RadialGradient:  return "Radial Gradient Mask";
    case MaskTool::Brush:           return "Brush Mask";
    case MaskTool::ColorRange:      return "Color Range Mask";
    case MaskTool::LuminanceRange:  return "Luminance Range Mask";
    case MaskTool::Subject:         return "Subject Mask";
    case MaskTool::Sky:             return "Sky Mask";
    case MaskTool::Background:      return "Background Mask";
    case MaskTool::Depth:           return "Depth Range Mask";
    case MaskTool::Object:          return "Object Mask";
    }
    return "Mask";
}

int DevelopProperties::maskToolFromName(const QString &name)
{
    for (int t = 0; t <= int(MaskTool::Object); ++t)
        if (maskToolName(t) == name) return t;
    return int(MaskTool::LinearGradient);
}

QString DevelopProperties::opName(int op)
{
    if (op == int(MaskOp::Subtract))  return "(-)";
    if (op == int(MaskOp::Intersect)) return "(∩)";   // ∩ intersect
    return "(+)";
}

EditScope *DevelopProperties::activeScope()
{
    if (currentImagePath.isEmpty()) return nullptr;
    EditStack &s = stackCache[currentImagePath];
    if (activeScopeIndex < 0 || activeScopeIndex >= s.scopes.size()) return nullptr;
    return &s.scopes[activeScopeIndex];
}

bool DevelopProperties::selfTestAddMaskScope(int submasks)
{
/*
    TEST HOOK -- see the header. Mirrors newScope() without its modal name dialog, then
    beginMaskTool() without showMaskMenu()'s popup, committing each submask so the scope
    ends up with `submasks` COMMITTED brush submasks and one pending. That shape is the
    point: the render cost this exercises is per-component, so a scope with one submask
    proves nothing about the caches the drag path leans on.
*/
    if (currentImagePath.isEmpty()) return false;

    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());   // index 0 is always Global
    s.scopes[0].name = "Global";

    EditScope l;
    l.name = uniqueScopeName("Mask " + QString::number(s.scopes.size()));
    /* A non-identity adjustment, so the scope actually develops pixels: an identity scope
       aliases the accumulator and renderStack skips the develop pass, which would quietly
       hide exactly the work under test. */
    l.params.exposure = 0.35f;
    l.params.contrast = 12.0f;
    s.scopes.append(l);
    activeScopeIndex = s.scopes.size() - 1;
    selectedMaskIndex = -1;
    refreshScopeList();
    buildTree();

    for (int i = 0; i < qMax(1, submasks); ++i) {
        beginMaskTool(int(MaskTool::Brush));
        /* Give each committed submask a stroke of its own, or it rasterizes to nothing and
           the per-component caches have no work to cache. */
        if (MaskComponent *c = editingMaskComp()) {
            const double y = 0.15 + 0.7 * (double(i) / qMax(1, submasks));
            c->paramsJson = QString(
                "{\"size\":18,\"flow\":100,\"autoMask\":false,\"strokes\":"
                "[{\"size\":18,\"feather\":0,\"flow\":100,\"erase\":false,"
                "\"autoMask\":false,\"pts\":[0.15,%1,0.45,%2,0.80,%3]}]}")
                .arg(y).arg(y + 0.03).arg(y);
        }
        if (i < submasks - 1) commitPendingMask();   // leave the LAST one pending
    }
    return true;
}

bool DevelopProperties::maskOverlayActive() const
{
    /* Show the mask overlay on a mask while a tool is being edited
       (selectedMaskIndex) OR after a commit/finish latched it on (the combined result
       stays visible + 'O'-toggleable until the scope/image changes). */
    return activeScopeIndex > 0 && (selectedMaskIndex >= 0 || maskLatched);
}

QVector<MaskComponent> DevelopProperties::activeScopeComponents() const
{
    if (currentImagePath.isEmpty()) return {};
    auto it = stackCache.constFind(currentImagePath);
    if (it == stackCache.constEnd()) return {};
    const EditStack &s = it.value();
    if (activeScopeIndex < 0 || activeScopeIndex >= s.scopes.size()) return {};
    return s.scopes[activeScopeIndex].components;
}

int DevelopProperties::activeMaskIndex() const
{
    /* Position of the expanded tool within activeScopeComponents() (same ordering), or -1. */
    if (currentImagePath.isEmpty()) return -1;
    auto it = stackCache.constFind(currentImagePath);
    if (it == stackCache.constEnd()) return -1;
    const EditStack &s = it.value();
    if (activeScopeIndex <= 0 || activeScopeIndex >= s.scopes.size()) return -1;
    if (selectedMaskIndex < 0 ||
        selectedMaskIndex >= s.scopes[activeScopeIndex].components.size()) return -1;
    return selectedMaskIndex;
}

/* ----------------------------------------------------------------------------------------
   Preview (show/ignore) + Reset per group
   ---------------------------------------------------------------------------------------- */

EditParams::Group DevelopProperties::paramsGroup(int group)
{
    switch (group) {
    case PV_Color:      return EditParams::Group::Color;
    case PV_Calibrate:  return EditParams::Group::Calibrate;
    case PV_ColorGrade: return EditParams::Group::ColorGrade;
    case PV_Effects:    return EditParams::Group::Effects;
    default:          return EditParams::Group::Basic;   // PV_Basic (+ PV_Scope, unused)
    }
}

/* Section name for history captions ("Reset Basic", "Hide Effects"). */
QString DevelopProperties::groupLabel(int group)
{
    switch (group) {
    case PV_Color:      return "Color";
    case PV_Calibrate:  return "Calibrate";
    case PV_ColorGrade: return "Color Grade";
    case PV_Effects:    return "Effects";
    case PV_Scope:      return "all";
    default:            return "Basic";
    }
}

bool *DevelopProperties::previewFlag(EditScope *l, int group)
{
    if (!l) return nullptr;
    switch (group) {
    case PV_Scope:      return &l->showScope;
    case PV_Color:      return &l->showColor;
    case PV_Calibrate:  return &l->showCalibrate;
    case PV_ColorGrade: return &l->showColorGrade;
    case PV_Effects:    return &l->showEffects;
    default:          return &l->showBasic;             // PV_Basic
    }
}

BarBtn *DevelopProperties::makeEyeBtn(const QString &tooltip, int group)
{
    BarBtn *b = new BarBtn();
    b->setIcon(":/images/icon16/eye.png", G::iconOpacity);
    b->setToolTip(tooltip);
    connect(b, &BarBtn::clicked, this, [this, group]{ togglePreviewSection(group); });
    return b;
}

void DevelopProperties::refreshPreviewButtons()
{
    EditScope *l = activeScope();
    auto set = [](BarBtn *b, bool shown) {
        if (!b) return;
        b->setIcon(shown ? ":/images/icon16/eye.png" : ":/images/icon16/eye_off.png", G::iconOpacity);
    };
    set(basicEyeBtn,    l ? l->showBasic    : true);
    set(colorEyeBtn,    l ? l->showColor    : true);
    set(colorGradeEyeBtn, l ? l->showColorGrade : true);
    set(effectsEyeBtn,  l ? l->showEffects  : true);
    /* The whole-mask eye lives in the ScopeHeader widget, not the tree. */
    if (scopeHeader) scopeHeader->setPreviewShown(l ? l->showScope : true);
}

void DevelopProperties::togglePreviewSection(int group)
{
    if (G::isLogger) G::log("DevelopProperties::togglePreviewSection");
    EditScope *l = activeScope();
    bool *f = previewFlag(l, group);
    if (!f) return;
    *f = !*f;                     // non-destructive: values kept, only the fold changes
    noteEdit((*f ? "Show " : "Hide ") + groupLabel(group));
    refreshPreviewButtons();
    emit paramsChanged();                   // re-render with the folded params
}

void DevelopProperties::resetSection(int group)
{
    if (G::isLogger) G::log("DevelopProperties::resetSection");
    if (currentImagePath.isEmpty()) return;
    EditParams &p = activeParams();
    if (group == PV_Scope) {
        EditParams::resetGroup(p, EditParams::Group::Basic);
        EditParams::resetGroup(p, EditParams::Group::Color);
        EditParams::resetGroup(p, EditParams::Group::ColorGrade);
        EditParams::resetGroup(p, EditParams::Group::Effects);
    }
    else {
        EditParams::resetGroup(p, paramsGroup(group));
    }
    noteEdit("Reset " + groupLabel(group));
    populateSlidersFromStack();      // reflect the restored defaults (sliders + tone)
    emit paramsChanged();
}

void DevelopProperties::contextMenuEvent(QContextMenuEvent *event)
{
    if (G::isLogger) G::log("DevelopProperties::contextMenuEvent");
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid()) idx = model->index(idx.row(), CapColumn, idx.parent());
    const QString name = idx.isValid() ? idx.data(UR_Name).toString() : QString();

    /* Map a header row to its Preview/Reset group; other rows only get tree items. */
    int group = -1;
    QString label;
    if      (name == "BasicHeader")    { group = PV_Basic;    label = "Basic"; }
    else if (name == "ColorHeader")    { group = PV_Color;    label = "Color"; }
    else if (name == "CalibrateHeader") { group = PV_Calibrate; label = "Calibrate"; }
    else if (name == "ColorGradeHeader") { group = PV_ColorGrade; label = "Color Grade"; }
    else if (name == "EffectsHeader")  { group = PV_Effects;  label = "Effects"; }

    QMenu menu(this);

    /* Section-specific Preview/Reset (only when the click landed on a section header). */
    QAction *aPreview = nullptr;
    QAction *aReset = nullptr;
    if (group >= 0) {
        EditScope *l = activeScope();
        const bool shown = l ? *previewFlag(l, group) : true;
        aPreview = menu.addAction("Preview");
        aPreview->setCheckable(true);
        aPreview->setChecked(shown);
        aPreview->setEnabled(l != nullptr);
        aReset = menu.addAction("Reset " + label);
        aReset->setEnabled(!currentImagePath.isEmpty());
        menu.addSeparator();
    }

    /* Tree-wide items, available anywhere in the dock (mirrors the Embellish dock). */
    QAction *aExpandAll = menu.addAction("Expand all");
    QAction *aCollapseAll = menu.addAction("Collapse all");
    QAction *aSolo = menu.addAction("Solo mode");
    aSolo->setCheckable(true);
    aSolo->setChecked(setting->value("Develop/isSolo", false).toBool());

    /* Jump to the raw decode rows. They are Core rows on the Global scope (addCoreItems),
       so this activates Global and un-collapses it. Raw files only, and only while editing
       raw (applyCoreVisibility hides them when editing the embedded preview). The Winnow
       engine adds the raw-denoise rows alongside Demosaic, hence the longer caption. */
    QAction *aRawDemosaic = nullptr;
    if (currentIsRaw() && G::useRaw) {
        const bool winnow = G::decodeRawEngine != G::DecodeRawEngine::appleDecodeRawEngine;
        menu.addSeparator();
        aRawDemosaic = menu.addAction(winnow ? "Raw demosaic and denoise" : "Raw demosaic");
    }

    /* Copy / paste the ticked settings between images (also Cmd+Opt+C / Cmd+Opt+V), and
       save them as a reusable preset (also Cmd+Shift+N). Paste names its source, so the
       menu itself says what is on the clipboard. */
    menu.addSeparator();
    QAction *aCopySettings = menu.addAction("Copy Develop Settings…");
    aCopySettings->setEnabled(!currentImagePath.isEmpty() && !currentIsIdentity());
    const QString from = copiedSettingsSource();
    QAction *aPasteSettings = menu.addAction(
        from.isEmpty() ? "Paste Develop Settings" : "Paste Develop Settings from " + from);
    aPasteSettings->setEnabled(!currentImagePath.isEmpty() && hasCopiedSettings());
    QAction *aSavePreset = menu.addAction("Save Develop Preset…");
    aSavePreset->setEnabled(!currentImagePath.isEmpty() && !currentIsIdentity());

    QAction *chosen = menu.exec(event->globalPos());
    if      (chosen == nullptr)      return;
    else if (chosen == aPreview)     togglePreviewSection(group);
    else if (chosen == aReset)       resetSection(group);
    else if (chosen == aExpandAll)   setAllSectionsExpanded(true);
    else if (chosen == aCollapseAll) setAllSectionsExpanded(false);
    else if (chosen == aCopySettings)  copyDevelopSettings();
    else if (chosen == aPasteSettings) pasteDevelopSettings();
    else if (chosen == aSavePreset)  saveDevelopPreset();
    else if (chosen == aRawDemosaic) showRawDemosaic();
    else if (chosen == aSolo) {
        setSolo(aSolo->isChecked());
        setting->setValue("Develop/isSolo", aSolo->isChecked());
    }
}

void DevelopProperties::showRawDemosaic()
{
    if (G::isLogger) G::log("DevelopProperties::showRawDemosaic");
    /* Lab UI: the Demosaic / raw-denoise controls live in the Raw panel, not the tree, so
       just un-collapse it -- under Solo that folds its peers away. */
    if (rawPanel && G::useScopeHeaderLab && currentIsRaw()) {   // == syncRawPanel's test
        if (rawPanel->isCollapsed()) rawPanel->setCollapsed(false);
        soloCollapseOthers(SoloOwner::RawPanel);
        return;
    }

    /* Reveal the Demosaic (and, on the Winnow engine, raw-denoise) rows: they are Core
       rows on the Global scope, hidden while the scope items are collapsed. Bulk-guarded
       so un-collapsing does not fold the adjustment sections under Solo. */
    if (activeScopeIndex != 0) onScopeSelected(currentScopeNames().value(0));
    isBulkExpandCollapse = true;
    setTreeCollapsed(false);
    isBulkExpandCollapse = false;
    if (scopeHeader) scopeHeader->setCollapsed(false);
}

void DevelopProperties::setAllSectionsExpanded(bool expand)
{
    if (G::isLogger) G::log("DevelopProperties::setAllSectionsExpanded");
    /* Expand/Collapse all also drives the Scope row, whose collapse arrow lives in the
       ScopeHeader band (not the tree), and the Raw panel. The bulk guard stops
       onSectionExpanded re-collapsing them as the tree sections expand under Solo. */
    isBulkExpandCollapse = true;
    if (expand) expandAll();
    else        collapseAll();
    setTreeCollapsed(!expand);                          // show/hide the scope's own items
    if (scopeHeader) scopeHeader->setCollapsed(!expand);
    if (rawPanel) rawPanel->setCollapsed(!expand);
    isBulkExpandCollapse = false;
}

void DevelopProperties::onSectionExpanded(const QModelIndex &idx)
{
    /* Solo: one section open at a time. The base PropertyEditor collapses the sibling
       tree sections when a root is expanded; extend that to the Scope row and the Raw
       panel (separate widgets above the tree). Only the three adjustment sections are
       peers -- the Demosaic row is itself a scope item (it hides when the scope
       collapses), so expanding it must NOT collapse the scope. */
    const QString name = idx.data(UR_Name).toString();
    if (name != "BasicHeader" && name != "ColorHeader" && name != "EffectsHeader") return;
    soloCollapseOthers(SoloOwner::Section, name);
}

void DevelopProperties::showMaskMenu()
{
    if (G::isLogger) G::log("DevelopProperties::showMaskMenu");
    EditScope *scope = activeScope();
    if (!scope || activeScopeIndex == 0) {
        if (G::popup)
            G::popup->showPopup("Global applies to the whole image. "
                               "Press N to create a mask.");
        return;
    }

    /* Lab UI: a plain tool list -- the Add/Subtract/Intersect choice is made later on the
       MaskPanel, once the tool is defined (build-up model). */
    if (G::useScopeHeaderLab) {
        maskMenu->clear();
        for (int t = 0; t <= int(MaskTool::Object); ++t) {
            QAction *a = maskMenu->addAction(maskToolName(t));
            a->setData(t);
            connect(a, &QAction::triggered, this, [this, t]{ onMaskToolChosen(t); });
            if (t == int(MaskTool::Brush) || t == int(MaskTool::LuminanceRange))
                maskMenu->addSeparator();       // group geometric / range / AI tools
        }
        maskMenu->exec(QCursor::pos());
        return;
    }

    /* Built fresh each click: Subtract is offered only once at least one tool exists (the
       first tool must Add -- there is nothing to subtract from an empty mask). */
    maskMenu->clear();
    for (int t = 0; t <= int(MaskTool::Object); ++t) {
        QAction *a = maskMenu->addAction("Add " + maskToolName(t));
        connect(a, &QAction::triggered, this, &DevelopProperties::newMask);
        if (t == int(MaskTool::Brush) || t == int(MaskTool::LuminanceRange))
            maskMenu->addSeparator();           // group geometric / range / AI tools
    }
    if (!scope->components.isEmpty()) {
        maskMenu->addSeparator();
        for (int t = 0; t <= int(MaskTool::Object); ++t) {
            QAction *a = maskMenu->addAction("Subtract " + maskToolName(t));
            connect(a, &QAction::triggered, this, &DevelopProperties::newMask);
            if (t == int(MaskTool::Brush) || t == int(MaskTool::LuminanceRange))
                maskMenu->addSeparator();           // group geometric / range / AI tools
        }
    }
    maskMenu->exec(QCursor::pos());
}

void DevelopProperties::newMask()
{
    if (G::isLogger) G::log("DevelopProperties::newMask");
    EditScope *scope = activeScope();
    if (!scope || activeScopeIndex == 0) return;            // Global scope has no mask
    QAction *a = qobject_cast<QAction*>(sender());
    if (!a) return;

    /* Action text is "Add <tool>" / "Subtract <tool>"; the first word is the op, the rest the
       tool name (handles multi-word names like "Subject Mask"). */
    const QString txt = a->text();
    MaskComponent m;
    m.op   = txt.startsWith("Subtract")  ? int(MaskOp::Subtract)
           : txt.startsWith("Intersect") ? int(MaskOp::Intersect)
                                         : int(MaskOp::Add);
    m.tool = maskToolFromName(txt.section(' ', 1));
    m.paramsJson = defaultMaskParams(m.tool);
    if (m.tool == int(MaskTool::Brush)) m.feather = 0.0f;   // brush defaults to a crisp edge
    const bool first = scope->components.isEmpty();
    scope->components.append(m);
    selectedMaskIndex = scope->components.size() - 1;      // start editing the new tool
    noteEdit(txt);              // "Add Subject Mask" / "Subtract Brush Mask" / ...

    buildTree();
    /* Veil on the scope's first submask only -- see addMaskTool for why. */
    if (first) emit maskTintShowRequested();
    else       emit maskTintHideRequested();
    emit paramsChanged();       // a mask confines the scope's adjustment -> re-composite
}

void DevelopProperties::deleteMask(int index)
{
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    const QString goneTool = maskToolName(scope->components.at(index).tool);
    scope->components.removeAt(index);
    if      (selectedMaskIndex == index) selectedMaskIndex = -1;   // its settings close
    else if (selectedMaskIndex >  index) selectedMaskIndex--;      // follow the tool
    noteEdit("Delete " + goneTool);

    buildTree();
    emit paramsChanged();       // less mask coverage on the scope -> re-composite
}

void DevelopProperties::addToolRow(QModelIndex parIdx, int index, const MaskComponent &m, bool selected)
{
    /* Header-style row: the "op + tool" caption draws single-line across the full row width (no
       wrap) with an expand arrow; UR_LeafSingleLine paints it in the ordinary leaf colour (not
       category teal). The row is made a single full-width spanned cell (setFirstColumnSpanned)
       and the [-] remove button is drawn by the delegate at the right (UR_DeleteBtn), NOT a
       value-column widget: a column-1 widget would cover and clip the caption overflow. The
       delete click is hit-tested in mousePressEvent. */
    clearItemInfo(i);
    i.name = QString("maskTool%1").arg(index);
    i.parIdx = parIdx;
    i.captionText = opName(m.op) + " " + maskToolName(m.tool);
    i.tooltip = "Submask. Click to show/hide its settings; [+] adds another, [-] removes it.";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = false;
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex toolIdx = capIdx;
    model->setData(toolIdx, index, UR_MaskIndex);
    /* No role tint: the caption already reads "Subtract Brush Mask", and the overlay
       speaks one colour (see MW::updateMaskOverlayTint), so a green/blue caption would
       be re-introducing the colour key this design removed. */
    model->setData(toolIdx, true, UR_LeafSingleLine);
    model->setData(toolIdx, true, UR_DeleteBtn);
    model->setData(toolIdx, true, UR_AddBtn);           // [+] left of [-]: add another mask tool
    model->setData(toolIdx, true, UR_ShowDecoration);   // always show the expand arrow (settings on click)
    /* Nest the tool header one indent (arrow + caption only; child rows unchanged). */
    model->setData(toolIdx, true, UR_ExtraIndent);
    setFirstColumnSpanned(toolIdx.row(), parIdx, true);

    /* The selected tool reveals its settings as its OWN children (no duplicate tool-name row).
       Clicking the tool caption again collapses it (see mousePressEvent), so no separate Done row is
       needed. Gradients: Feather + Invert. Brush: Size, Feather, Flow, Auto mask, Invert. */
    if (selected) {
        if (m.tool == int(MaskTool::Brush)) {
            addSlider("maskSize", "Size", "Brush diameter (% of the long edge).",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soft edge added OUTSIDE the brush size (0 = crisp).",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFlow", "Flow", "How much each stroke builds up.",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskAutoMask", "Auto mask",
                        "Limit the brush to the edge under the stroke start. Toggle with A.",
                        toolIdx, "", false);
            addCheckbox("maskAutoMaskAi", "AI edge (SAM)",
                        "Auto mask mode: on = SAM object under the stroke start; off = similar-luminance band.",
                        toolIdx, "", false);
            addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.", toolIdx, "", false);
            setSliderReal("maskSize", brushNum(m.paramsJson, "size", 20));
            setSliderReal("maskFeather", m.feather);
            setSliderReal("maskFlow", brushNum(m.paramsJson, "flow", 100));
            setCheckboxValue("maskAutoMask", brushBool(m.paramsJson, "autoMask", false));
            setCheckboxValue("maskAutoMaskAi", brushStr(m.paramsJson, "autoMaskMode", "lum") == "ai");
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::LuminanceRange)) {
            /* Selects a luminance band [lo,hi] (stored 0..1, shown 0..100). Feather softens the band
               edges; Invert flips the selection. */
            addSlider("maskRangeLo", "Range Min", "Lower luminance bound of the selected band.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskRangeHi", "Range Max", "Upper luminance bound of the selected band.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the band edges.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskRangeLo", brushNum(m.paramsJson, "lo", 0.25) * 100.0);
            setSliderReal("maskRangeHi", brushNum(m.paramsJson, "hi", 0.75) * 100.0);
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::Depth)) {
            /* Selects a depth band [Near,Far] over the MiDaS depth field (stored 0=near..1=far,
               shown 0..100). Same mechanics as Luminance Range but over depth. */
            addSlider("maskRangeLo", "Near", "Near bound of the selected depth band (0 = nearest).",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskRangeHi", "Far", "Far bound of the selected depth band (100 = farthest).",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the depth-band edges.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskRangeLo", brushNum(m.paramsJson, "lo", 0.0) * 100.0);
            setSliderReal("maskRangeHi", brushNum(m.paramsJson, "hi", 0.5) * 100.0);
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::ColorRange)) {
            /* Colours are sampled by clicking the loupe (shift-click adds, on-image
               swatches remove). A hue/sat WHEEL above the sliders shows each sample + its
               selection band; drag its handles or the Hue/Sat Lo/Hi sliders to widen the
               band toward lower/higher hue and lower/higher saturation. Feather softens
               all edges. The wheel is a spanned index widget, mirroring the Color Grade
               wheel (addColorGrade). */
            /* Hue preset chips: one click seeds the mask with a named band instead of
               hunting for a pixel to pipette. Visible on the row itself, not behind a
               menu -- this is the fast path and it has to be findable. */
            clearItemInfo(i);
            i.name = QString("maskHueChips%1").arg(index);
            i.parIdx = toolIdx;
            i.parentName = "";
            i.captionText = "";
            i.isIndent = true;
            i.hasValue = false;
            i.captionIsEditable = false;
            addItem(i);
            const QModelIndex chipsIdx = capIdx;
            model->setData(chipsIdx, 8, UR_ExtraRowHeight);
            setFirstColumnSpanned(chipsIdx.row(), toolIdx, true);
            {
                QWidget *cw = new QWidget;
                cw->setAttribute(Qt::WA_TranslucentBackground);
                QHBoxLayout *hb = new QHBoxLayout(cw);
                hb->setContentsMargins(QTreeView::indentation() + 4, 2, 4, 2);
                hb->setSpacing(3);
                for (int b = 0; b < hueBandCount(); ++b) {
                    const QColor swatch =
                        QColor::fromHsvF(qMin(0.9999f, hueBandCentre(b) / 360.0f), 0.6, 1.0);
                    QToolButton *chip = new QToolButton(cw);
                    chip->setAutoRaise(true);
                    chip->setFixedSize(18, 18);
                    chip->setToolTip(QString("Select the %1 range").arg(hueBandName(b)));
                    chip->setStyleSheet(
                        QString("QToolButton{background:%1;border:1px solid #00000060;"
                                "border-radius:3px;}"
                                "QToolButton:hover{border:1px solid #ffffffc0;}")
                            .arg(swatch.name()));
                    connect(chip, &QToolButton::clicked, this,
                            [this, b]{ applyColorRangeHuePreset(b); });
                    hb->addWidget(chip);
                }
                hb->addStretch(1);
                setIndexWidget(chipsIdx, cw);
            }

            clearItemInfo(i);
            i.name = QString("maskColorWheel%1").arg(index);
            i.parIdx = toolIdx;
            i.parentName = "";
            i.captionText = "";
            i.isIndent = true;
            i.hasValue = false;
            i.captionIsEditable = false;
            addItem(i);
            const QModelIndex wheelIdx = capIdx;
            model->setData(wheelIdx, 200, UR_ExtraRowHeight);
            setFirstColumnSpanned(wheelIdx.row(), toolIdx, true);
            {
                ColorRangeWheel *wheel = new ColorRangeWheel;
                colorRangeWheel = wheel;
                wheel->setSamples(colorRangeSamplesHS(m.paramsJson));
                wheel->setBounds(brushNum(m.paramsJson, "hueLo", 20),
                                 brushNum(m.paramsJson, "hueHi", 20),
                                 brushNum(m.paramsJson, "satLo", 25) / 100.0,
                                 brushNum(m.paramsJson, "satHi", 25) / 100.0);
                connect(wheel, &ColorRangeWheel::boundsChanged,   this,
                        [this]{ onColorRangeWheelChanged(false); });
                connect(wheel, &ColorRangeWheel::boundsCommitted, this,
                        [this]{ onColorRangeWheelChanged(true); });
                setIndexWidget(wheelIdx, wheel);
            }
            addSlider("maskHueLo", "Hue Lo",
                      "Widen the selection toward LOWER hues (degrees) around the sampled "
                      "colour. Click the image to sample; shift-click to add another colour.",
                      toolIdx, "", 0, 90, 0, G::darkgray, G::lightgray);
            addSlider("maskHueHi", "Hue Hi",
                      "Widen the selection toward HIGHER hues (degrees) around the sample.",
                      toolIdx, "", 0, 90, 0, G::darkgray, G::lightgray);
            addSlider("maskSatLo", "Sat Lo",
                      "Widen the selection toward LOWER saturation around the sample.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskSatHi", "Sat Hi",
                      "Widen the selection toward HIGHER saturation around the sample.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the selection edge.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskHueLo", brushNum(m.paramsJson, "hueLo", 20));
            setSliderReal("maskHueHi", brushNum(m.paramsJson, "hueHi", 20));
            setSliderReal("maskSatLo", brushNum(m.paramsJson, "satLo", 25));
            setSliderReal("maskSatHi", brushNum(m.paramsJson, "satHi", 25));
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::Object)) {
            /* Perimeter-paint: trace the object boundary with a solid brush (Alt erases;
               [ ] or two-finger drag resize). Size = diameter; Feather softens edge. */
            addSlider("maskSize", "Size", "Perimeter brush diameter (% of the long edge).",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the refined cutout edge.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.", toolIdx, "", false);
            setSliderReal("maskSize", brushNum(m.paramsJson, "size", 8));
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else {
            addSlider("maskFeather", "Feather", "Soften the mask edge.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        expand(toolIdx);
    }
}

void DevelopProperties::setSelectedMask(int index)
{
    selectedMaskIndex = index;
    buildTree();            // rebuild so the newly-selected tool shows its settings
}

bool DevelopProperties::escapeMaskTool()
{
    /* Lab UI: Esc while a submask is still PENDING discards it (the blue preview, or the
       just-created first tool) rather than collapsing a tree tool. */
    if (maskPanelOpen) { cancelMaskTool(); return true; }
    /* A re-opened submask has nothing to discard: Esc just closes it, the same as the
       panel's "Done" button. */
    if (G::useScopeHeaderLab && selectedMaskIndex >= 0) {
        commitPendingMask();                  // pendingIdx < 0 -> the "Done" path
        return true;
    }
    if (activeMaskTool() < 0) return false;   // no mask tool expanded
    setSelectedMask(-1);                      // collapse it (as clicking its caption)
    return true;
}

QString DevelopProperties::defaultMaskParams(int tool)
{
    /* Geometry is stored normalized (0..1 of the image), so it is resolution-independent and
       survives proxy/full re-renders. Other tools: empty until implemented. */
    if (tool == int(MaskTool::LinearGradient)) {
        /* A vertical line down the middle, ramping 0% (top) -> 100% (bottom) across the middle third. */
        QJsonObject o;
        o["x1"] = 0.5; o["y1"] = 0.34;
        o["x2"] = 0.5; o["y2"] = 0.66;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::RadialGradient)) {
        /* A centred ellipse covering the middle of the frame; mask 100% inside, ramping to 0% at
           the edge. cx/cy normalized by W/H, rx by W, ry by H, angle in degrees. */
        QJsonObject o;
        o["cx"] = 0.5;  o["cy"] = 0.5;
        o["rx"] = 0.25; o["ry"] = 0.30;
        o["angle"] = 0.0;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::Brush)) {
        /* Freehand: starts empty. size/flow are 0..100 (current settings for the NEXT stroke; feather
           lives in MaskComponent.feather). strokes is the accumulated path list -- each stroke
           snapshots the settings it was painted with. Points are normalized output coords. */
        QJsonObject o;
        o["size"] = 20;
        o["flow"] = 100;
        o["autoMask"] = false;
        o["autoMaskMode"] = "lum";      // "lum" (luminance band) | "ai" (SAM object)
        o["strokes"] = QJsonArray();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::LuminanceRange)) {
        /* Selects a band of display-referred luminance [lo,hi] (0..1). Starts on the mid-tones so the
           mask is visible; the feather (MaskComponent.feather) softens the band edges. */
        QJsonObject o;
        o["lo"] = 0.25;
        o["hi"] = 0.75;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::ColorRange)) {
        /* Selects pixels whose HUE lies within [-hueLo, +hueHi] degrees of a sample
           (added by clicking the loupe). samples are [r,g,b] 0..1 in display space. */
        QJsonObject o;
        o["hueLo"] = 20;
        o["hueHi"] = 20;
        o["satLo"] = 25;
        o["satHi"] = 25;
        o["samples"] = QJsonArray();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::Depth)) {
        /* Selects a depth band [lo,hi] (0=near..1=far) over the model depth field. Starts on the near
           half so the mask is visible; feather (MaskComponent.feather) softens the band edges. */
        QJsonObject o;
        o["lo"] = 0.0;
        o["hi"] = 0.5;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::Object)) {
        /* SAM 2 object mask, perimeter-paint: the user traces the object BOUNDARY with a
           solid brush (multiple strokes; Alt erases). Brush-style blob (mirrors the Brush
           tool): size + a stroke list of normalized output-coord points. Empty -> no
           coverage until a closed loop is drawn, when MW fills the enclosed region and
           SAM refines it to the object edge. */
        QJsonObject o;
        o["size"] = 8;
        o["strokes"] = QJsonArray();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    return QString();
}

int DevelopProperties::activeMaskTool() const
{
    if (currentImagePath.isEmpty()) return -1;
    auto it = stackCache.find(currentImagePath);
    if (it == stackCache.end()) return -1;
    const EditStack &s = it.value();
    if (activeScopeIndex < 0 || activeScopeIndex >= s.scopes.size()) return -1;
    if (selectedMaskIndex < 0 || selectedMaskIndex >= s.scopes[activeScopeIndex].components.size()) return -1;
    return s.scopes[activeScopeIndex].components[selectedMaskIndex].tool;
}

double DevelopProperties::brushNum(const QString &paramsJson, const QString &key, double def)
{
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    return o.contains(key) ? o.value(key).toDouble(def) : def;
}

bool DevelopProperties::brushBool(const QString &paramsJson, const QString &key, bool def)
{
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    return o.contains(key) ? o.value(key).toBool(def) : def;
}

QString DevelopProperties::brushStr(const QString &paramsJson, const QString &key, const QString &def)
{
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    return o.contains(key) ? o.value(key).toString(def) : def;
}

QString DevelopProperties::brushWith(const QString &paramsJson, const QString &key, const QJsonValue &v)
{
    QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    o[key] = v;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString DevelopProperties::brushStrokesWith(const QString &paramsJson, const QString &key,
                                            const QJsonValue &v)
{
    /* Write a brush setting onto EVERY stroke already painted, as well as onto the
       component's own "next stroke" default.

       Size, feather, flow and auto-mask are BAKED INTO EACH STROKE as it is drawn
       (ImageView writes them into the stroke object; BrushStamp reads them back per
       stroke), which is what lets one submask hold strokes of different sizes. The cost
       is that on a submask whose strokes are all already down, the sliders arm a stroke
       that will never come: they move and the image does not. That was invisible while a
       committed submask could not be re-opened, and is a dead end now that it can. So on
       an EXISTING submask the sliders act retroactively instead -- see
       onMaskEditorSetting, which keeps the next-stroke meaning while the submask is still
       being built. */
    QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    o[key] = v;
    QJsonArray strokes = o.value("strokes").toArray();
    for (int i = 0; i < strokes.size(); ++i) {
        QJsonObject s = strokes.at(i).toObject();
        s[key] = v;
        strokes[i] = s;
    }
    if (!strokes.isEmpty()) o["strokes"] = strokes;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString DevelopProperties::brushLastStrokeWith(const QString &paramsJson,
                                               const QString &key, const QJsonValue &v)
{
    /* Only the stroke most recently painted. The component's own next-stroke default is
       NOT touched: this is a correction to what is already on the image ("that last dab
       was too hard"), not a change to how the brush is loaded. */
    QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    QJsonArray strokes = o.value("strokes").toArray();
    if (strokes.isEmpty()) return paramsJson;
    QJsonObject s = strokes.last().toObject();
    s[key] = v;
    strokes[strokes.size() - 1] = s;
    o["strokes"] = strokes;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void DevelopProperties::setActiveBrushSize(double size)
{
    EditScope *l = activeScope();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->components.size()) return;
    MaskComponent &m = l->components[selectedMaskIndex];
    if (m.tool != int(MaskTool::Brush) && m.tool != int(MaskTool::Object)) return;
    const int s = qBound(1, int(size + 0.5), 100);
    /* Same split as the panel slider (onMaskEditorSetting): while the submask is being
       built this arms the next stroke, on a re-opened one it resizes what is there. */
    const bool retro = (pendingIdx < 0);
    m.paramsJson = retro ? brushStrokesWith(m.paramsJson, "size", s)
                         : brushWith(m.paramsJson, "size", s);
    noteEdit("Brush size", QString::number(s), "mask/brushSize");
    syncMaskSlider("maskSize", s);
    if (retro) emit paramsChanged();
}

void DevelopProperties::setActiveMaskFeather(double feather)
{
    /* Shift + wheel on the canvas. Only the tools whose feather ImageView itself owns
       (the two gradients + the brush) -- the content/AI masks feather at composite time
       and are driven from their panel slider. */
    EditScope *l = activeScope();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->components.size()) return;
    MaskComponent &m = l->components[selectedMaskIndex];
    if (m.tool != int(MaskTool::LinearGradient) &&
        m.tool != int(MaskTool::RadialGradient) &&
        m.tool != int(MaskTool::Brush)) return;
    const int f = qBound(0, int(feather + 0.5), 100);
    if (int(m.feather + 0.5f) == f) return;
    m.feather = f;
    noteEdit("Feather", QString::number(f), "mask/maskFeather");
    syncMaskSlider("maskFeather", f);
    /* Brush feather belongs to the NEXT stroke while the submask is being built (each
       stroke keeps its own snapshot), so it only refreshes the cursor; on a RE-OPENED
       submask there is no next stroke, so it re-feathers every stroke instead. A
       gradient's coverage always changed -> re-composite. Either way ImageView already
       holds the new value, so no maskFeatherChanged echo. */
    if (m.tool == int(MaskTool::Brush)) {
        emitBrushSettings(m);
        if (pendingIdx < 0) {
            m.paramsJson = brushStrokesWith(m.paramsJson, "feather", f);
            emit paramsChanged();
        }
    }
    else emit paramsChanged();
}

void DevelopProperties::syncMaskSlider(const QString &key, double value)
{
    if (maskPanel && maskPanel->editor())
        maskPanel->editor()->setSliderReal(key, value);   // guards itself
    isPopulating = true;            // sync the tree row without re-entering itemChange
    setSliderReal(key, value);
    isPopulating = false;
}

void DevelopProperties::setActiveBrushAutoMask(bool on)
{
    EditScope *l = activeScope();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->components.size()) return;
    MaskComponent &m = l->components[selectedMaskIndex];
    if (m.tool != int(MaskTool::Brush)) return;
    /* Next stroke while the submask is being built; retroactive on a re-opened one (see
       onMaskEditorSetting). */
    const bool retro = (pendingIdx < 0);
    m.paramsJson = retro ? brushStrokesWith(m.paramsJson, "autoMask", on)
                         : brushWith(m.paramsJson, "autoMask", on);
    noteEdit("Auto mask", on ? "On" : "Off");
    isPopulating = true;            // sync the checkbox without re-entering itemChange
    setCheckboxValue("maskAutoMask", on);
    isPopulating = false;
    if (retro) emit paramsChanged();
}

void DevelopProperties::emitBrushSettings(const MaskComponent &m)
{
    emit maskBrushSettingsChanged(brushNum(m.paramsJson, "size", 20),
                                  m.feather,
                                  brushNum(m.paramsJson, "flow", 100),
                                  brushBool(m.paramsJson, "autoMask", false),
                                  brushStr(m.paramsJson, "autoMaskMode", "lum"));
}

void DevelopProperties::setMaskOverlayShown(bool shown)
{
    /* ImageView flipped the red coverage tint ("O", an adjustment slider, or a new mask
       edit); mirror it so the scope menu's "Show mask overlay" row checks correctly. */
    if (scopeHeader) scopeHeader->setMaskOverlayShown(shown);
}

void DevelopProperties::setMaskOverlayColour(const QColor &c)
{
    /* The overlay colour, set from the action-row tint button's context menu: persist it
       and rebuild the veil so it repaints in the new colour. The image is untouched. */
    if (G::isLogger) G::log("DevelopProperties::setMaskOverlayColour");
    if (!c.isValid()) return;
    G::maskOverlayColor = c;
    setting->setValue("Develop/maskOverlayColor", G::maskOverlayColor.name());
    emit maskOverlayRefreshRequested();
}

void DevelopProperties::setMaskOverlayGrayscale(bool on)
{
    /* As above, but the veil is unchanged: grayscale is view-only (ImageView desaturates
       the image it draws UNDER the overlay), so a repaint is all it costs. */
    if (G::isLogger) G::log("DevelopProperties::setMaskOverlayGrayscale");
    G::maskOverlayGrayscale = on;
    setting->setValue("Develop/maskOverlayGrayscale", G::maskOverlayGrayscale);
    emit maskOverlayRepaintRequested();
}

void DevelopProperties::updateMaskEdit()
{
    /* Drive the ImageView mask overlay from the active (selected) mask tool. Begin when a spatial
       tool is selected and has geometry; otherwise End. Called at the end of every rebuild, so it
       tracks image/scope switches, selection toggles, add and delete. */
    EditScope *scope = activeScope();
    /* The scope menu's "Show mask overlay" row only applies while a tool is expanded. */
    if (scopeHeader) scopeHeader->setMaskOverlayAvailable(maskOverlayActive());
    if (scope && selectedMaskIndex >= 0 && selectedMaskIndex < scope->components.size()) {
        const MaskComponent &m = scope->components[selectedMaskIndex];
        if (m.tool == int(MaskTool::LinearGradient) || m.tool == int(MaskTool::RadialGradient) ||
            m.tool == int(MaskTool::Brush) ||
            m.tool == int(MaskTool::ColorRange) || m.tool == int(MaskTool::LuminanceRange) ||
            m.tool == int(MaskTool::Subject) || m.tool == int(MaskTool::Sky) ||
            m.tool == int(MaskTool::Background) || m.tool == int(MaskTool::Depth) ||
            m.tool == int(MaskTool::Object)) {
            emit maskEditBegin(m.tool, m.op, m.inverted, m.paramsJson, m.feather);
            return;
        }
    }
    emit maskEditEnd();
}

void DevelopProperties::setActiveMaskParams(const QString &paramsJson)
{
    /* ImageView dragged the overlay: persist the new geometry into the active mask
       component. No tree rebuild (would re-emit maskEditBegin and fight the live drag);
       just store + debounce. The pending (blue) tool is a real component in scope->components
       at selectedMaskIndex, so the normal path below persists it too. */
    EditScope *scope = activeScope();
    if (!scope || selectedMaskIndex < 0 || selectedMaskIndex >= scope->components.size()) return;
    if (scope->components[selectedMaskIndex].paramsJson == paramsJson) return;
    scope->components[selectedMaskIndex].paramsJson = paramsJson;
    /* One entry per tool being shaped, not per drag tick / per brush stroke. */
    noteEdit("Edit " + maskToolName(scope->components[selectedMaskIndex].tool),
             QString(), QString("mask/geom/%1").arg(selectedMaskIndex));
    /* A Color Range pipette pick/remove arrives here too -> refresh the wheel dots
       (bounds unchanged). No tree rebuild, so the wheel survives and just repaints.
       Legacy: the tree's colorRangeWheel; lab: the MaskPanel's embedded editor wheel. */
    if (activeMaskTool() == int(MaskTool::ColorRange)) {
        const QVector<QPointF> hs = colorRangeSamplesHS(paramsJson);
        if (colorRangeWheel) colorRangeWheel->setSamples(hs);
        if (maskPanel && maskPanel->editor()) maskPanel->editor()->setWheelSamples(hs);
    }
    emit paramsChanged();       // new mask geometry -> re-composite the masked scope
}

void DevelopProperties::onMaskSelectionChanged()
{
    /* Clicks do not select in this tree (see mousePressEvent); this only fires on programmatic
       selection. Keep it in sync so any future selectionModel()->select() drives the settings. */
    if (isRebuildingMasks) return;
    QModelIndex idx = selectionModel()->currentIndex();
    if (!idx.isValid()) return;
    const QVariant v = idx.siblingAtColumn(0).data(UR_MaskIndex);
    if (!v.isValid()) return;
    const int m = v.toInt();
    if (m >= 0 && m != selectedMaskIndex) setSelectedMask(m);
}

void DevelopProperties::mousePressEvent(QMouseEvent *event)
{
    /* Clicking a tool row toggles its settings (the base class does not select on click,
       so we read the row's UR_MaskIndex ourselves and rebuild). We manage the tool's
       expand state via the selection, so we do NOT fall through to the base
       expand/collapse for these rows.

       This press is not a branch expand/collapse unless the base says so below: stale
       state would make the following double click (if any) toggle an unrelated branch. */
    pressBranch = QModelIndex();
    if (event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            /* The "Add mask" placeholder row (shown while the scope has no mask) has no UR_MaskIndex;
               a click anywhere on it -- including its [+] -- pops the Add/Subtract chooser. */
            if (idx.siblingAtColumn(0).data(UR_Name).toString() == "addMask") {
                showMaskMenu();
                return;
            }
            const QVariant v = idx.siblingAtColumn(0).data(UR_MaskIndex);
            if (v.isValid()) {
                const int m = v.toInt();
                /* The [+] add and [-] remove glyphs are delegate-drawn at the row's right edge (must
                   mirror the geometry in PropertyDelegate::paint for UR_AddBtn/UR_DeleteBtn): [-] at
                   the edge, [+] one slot to its left. A click on [-] removes the tool, on [+] adds
                   another; a click anywhere else on the row toggles its settings. */
                const QRect rowRect = visualRect(idx);
                const int sz = 16;
                const int gy = rowRect.top() + (rowRect.height() - sz)/2;
                const QRect delRect(rowRect.right() - sz - 4, gy, sz, sz);
                const QRect addRect(rowRect.right() - 2*sz - 8, gy, sz, sz);
                if (delRect.contains(event->pos())) {
                    deleteMask(m);
                    return;
                }
                if (addRect.contains(event->pos())) {
                    showMaskMenu();
                    return;
                }
                setSelectedMask(m == selectedMaskIndex ? -1 : m);   // open -> collapse
                return;
            }
            /* A click on a SLIDER ROW's caption (column 0): focus that slider so arrow
               keys nudge it, drop the mask overlay so the effect shows, and flash the
               caption. The value column (the slider itself) is left alone. A mask-SETTING
               slider (child of a mask tool, e.g. Feather/Hue) keeps the overlay -- it is
               editing the mask, which you want to see. */
            if (idx.column() == CapColumn) {
                const QModelIndex valIdx = model->index(idx.row(), ValColumn, idx.parent());
                if (valIdx.data(UR_DelegateType).toInt() == DT_Slider) {
                    if (auto *se = static_cast<SliderEditor*>(
                            valIdx.data(UR_Editor).value<void*>()))
                        se->focusSlider();
                    const bool isMaskSlider = idx.parent().data(UR_MaskIndex).isValid();
                    if (!isMaskSlider && maskOverlayActive()) emit maskTintHideRequested();
                    flashCaption(idx);
                    return;
                }
            }
        }
    }
    PropertyEditor::mousePressEvent(event);
}

void DevelopProperties::mouseDoubleClickEvent(QMouseEvent *event)
{
    /* The WB row resets the white balance to "As shot" -- it is a DT_None row (it owns
       both cells), so the base class has no default value to restore for it, and the
       0 sentinel As Shot writes is the exact way back. Checked BEFORE the base call.
       The dropper button and the preset combo handle their own clicks; this catches the
       rest of the row, including the "WB" label. */
    if (event->button() == Qt::LeftButton) {
        const QModelIndex hit = indexAt(event->pos());
        if (hit.isValid() &&
            hit.siblingAtColumn(CapColumn).data(UR_Name).toString() == "whiteBalance") {
            setWbPreset(int(WhiteBalance::Preset::AsShot));
            return;
        }
        /* Temp / Tint are ABSOLUTE, so the base class reset (row default = 0) is wrong
           for them: 0 is the EditParams "untouched" sentinel, not a temperature, and
           pushing it through the slider would slam Temp to the 2000 K floor. Their
           default IS the as-shot value, so reset to that. Checked BEFORE the base call
           so it never sees these rows. */
        const QString key = hit.siblingAtColumn(CapColumn).data(UR_Name).toString();
        if (key == "temp" || key == "tint") {
            resetWbAxisToAsShot(key == "temp");
            /* Keep the row lit blue: a caption double-click moves focus to the tree
               (the caption is delegate-painted), as in the base path below. */
            const QModelIndex valIdx = hit.siblingAtColumn(ValColumn);
            if (auto *se = static_cast<SliderEditor*>(valIdx.data(UR_Editor).value<void*>()))
                se->focusSlider();
            return;
        }
    }

    /* Global resets the row's slider to its default. On a CAPTION double-click Qt has
       moved keyboard focus to the tree (the caption cell is delegate-painted, not a
       widget), so re-focus the slider to keep the row lit blue -- matching the
       single-click cue. */
    PropertyEditor::mouseDoubleClickEvent(event);
    if (event->button() != Qt::LeftButton) return;
    const QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid() || idx.column() != CapColumn) return;
    const QModelIndex valIdx = model->index(idx.row(), ValColumn, idx.parent());
    if (valIdx.data(UR_DelegateType).toInt() == DT_Slider)
        if (auto *se = static_cast<SliderEditor*>(valIdx.data(UR_Editor).value<void*>()))
            se->focusSlider();
}

/* Flash a caption white and fade to nothing over ~0.45s as click feedback. Drives the
   delegate's UR_FlashLevel role (0..1) via a QVariantAnimation, repainting the row each
   step. A fresh click restarts, clearing any previously lit caption first. */
void DevelopProperties::flashCaption(const QModelIndex &capIdx)
{
    if (!capIdx.isValid()) return;
    if (captionFlashAnim) { captionFlashAnim->stop(); captionFlashAnim->deleteLater(); }
    if (flashCaptionIdx.isValid() && flashCaptionIdx != capIdx)
        model->setData(flashCaptionIdx, QVariant(), UR_FlashLevel);   // setData repaints
    flashCaptionIdx = capIdx;
    auto *anim = new QVariantAnimation(this);
    captionFlashAnim = anim;
    anim->setDuration(450);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v){
        if (flashCaptionIdx.isValid()) model->setData(flashCaptionIdx, v.toReal(), UR_FlashLevel);
    });
    connect(anim, &QVariantAnimation::finished, this, [this, anim]{
        if (flashCaptionIdx.isValid()) model->setData(flashCaptionIdx, QVariant(), UR_FlashLevel);
        flashCaptionIdx = QModelIndex();
        anim->deleteLater();
    });
    anim->start();
}

void DevelopProperties::paintEvent(QPaintEvent *event)
{
    PropertyEditor::paintEvent(event);

    /* Fit-mode backstop: whatever changed the visible rows without scheduling a re-fit,
       the block self-corrects on the next repaint. Compared against the height we last
       ASKED for, not the height we were given -- a layout that cannot honour the request
       would otherwise re-schedule on every paint forever. The schedule is deferred, so
       this cannot recurse into the paint either way. */
    if (fitToContents && sizeHint().height() != fittedHeight) scheduleContentFit();

    /* The scope containment rail, continued from ScopeHeaderLab (which draws the same
       2px G::selectionColor line from the selected scope row down to its bottom edge --
       see G::scopeRailX/W). In FIT mode the tree is nested INSIDE the scope list, so the
       rail runs the FULL viewport height: it must meet the list's rail again below this
       widget (the scope rows after the selected one) with no visible gap. In the legacy
       stacked dock it stops at the BOTTOM OF THE LAST ROW -- there the empty space under
       a short tree is dock, not part of the scope's block.
       The frame is off (initialize), so the viewport shares this widget's origin and the
       two halves line up at the same x with no correction. */
    /* Start from the topmost VISIBLE row (indexAt), not model row 0: the Develop tree
       hides rows (Core / mask / raw-only rows), and indexBelow on a hidden index returns
       an invalid index, which would end the walk immediately. */
    QModelIndex last = indexAt(QPoint(0, 0));
    if (!last.isValid()) return;
    for (QModelIndex below = indexBelow(last); below.isValid(); below = indexBelow(below))
        last = below;
    const int bottom = qMin(visualRect(last).bottom() + 1, viewport()->height());
    if (bottom <= 0) return;

    QPainter p(viewport());
    const int ruleY = bottom + kBlockCloseGap;
    if (G::scopeRailW > 0) {
        /* Fit mode: down to the FOOT of the bracket (the closing rule) and no further --
           the clear gap under the rule separates this scope's block from the next scope
           row. Legacy: to the last row, since the space below is plain dock. */
        const int railBottom = fitToContents ? ruleY + G::panelBorderHeight : bottom;
        p.fillRect(G::scopeRailX, 0, G::scopeRailW, railBottom, G::selectionColor);
    }

    /* Close the block: the same separator every Develop panel carries along its bottom
       edge (G::panelBorderHeight in G::tabWidgetBorderColor), just under the last row --
       Effects is the last section, so this is the Effects panel's bottom border. Only
       when there is empty dock below it -- if the rows fill or overflow the viewport,
       the panel edge already ends the block and a rule pinned to the last visible row
       would be a false bottom. In FIT mode the viewport is exactly the rows plus the
       band sizeHint reserves for this rule, so it always draws -- starting at the RIGHT
       EDGE OF THE RAIL and painted in the RAIL'S OWN COLOUR: there it is the foot of the
       bracket around the selected scope, so it reads as the rail turning the corner
       rather than as a divider cutting across it. */
    if (fitToContents || ruleY < viewport()->height() - G::panelBorderHeight) {
        const int ruleX = fitToContents && G::scopeRailW > 0
                              ? G::scopeRailX + G::scopeRailW : 0;
        p.fillRect(ruleX, ruleY, viewport()->width() - ruleX, G::panelBorderHeight,
                   fitToContents ? G::selectionColor : G::tabWidgetBorderColor);
    }
}

void DevelopProperties::drawBranches(QPainter *, const QRect &, const QModelIndex &) const
{
    /* Intentionally empty: draw no native branch indicator. Every expandable Develop row
       draws its own winnow arrow through PropertyDelegate (section headers, mask tools,
       and the Demosaic row), so the native triangle is redundant; on the Demosaic value
       row (whose background fill is skipped when isAlternatingRows) it also showed
       through beside the delegate arrow, giving a doubled, oversized decoration. */
}

/* ----------------------------------------------------------------------------------------
   Item builders
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addHeader(const QString &name, const QString &parent,
                                  const QString &caption, const QString &tooltip, int previewGroup)
{
    clearItemInfo(i);
    i.name = name;
    i.parentName = parent;  // "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = caption;
    i.tooltip = tooltip;
    i.captionIsEditable = false;
    i.path = "Develop/" + name;

    if (previewGroup >= 0) {
        /* A section header (Basic/Color/Effects): a trailing eye toggle in a BarBtn column, drained
           by BarBtnEditor. hasValue + DT_BarBtns are what create that column. */
        BarBtn *eye = makeEyeBtn("Preview: show or ignore these settings", previewGroup);
        if      (previewGroup == PV_Basic)    basicEyeBtn    = eye;
        else if (previewGroup == PV_Color)    colorEyeBtn    = eye;
        else if (previewGroup == PV_Calibrate)  calibrateEyeBtn  = eye;
        else if (previewGroup == PV_ColorGrade) colorGradeEyeBtn = eye;
        else if (previewGroup == PV_Effects)  effectsEyeBtn  = eye;
        btns.append(eye);
        i.hasValue = true;
        i.delegateType = DT_BarBtns;
    }
    else {
        i.hasValue = false;
        i.delegateType = DT_None;
    }
    addItem(i);
    /* Nest the section header (Basic/Color/Color Grade/Effects) under the scope it
       belongs to. Only the header content shifts -- its child rows keep their own
       indentation (the delegate reads UR_ExtraIndent on the header caption), so the
       sliders keep their full width. In the lab UI the tree sits INSIDE the scope row
       list, so the headers indent by the same inset as the other nested details
       (kMaskPanelIndent) and line up under the scope NAME; the legacy dock keeps the
       original one-indent-level nudge. */
    model->setData(capIdx, G::useScopeHeaderLab ? QVariant(kMaskPanelIndent)
                                                : QVariant(true), UR_ExtraIndent);
    /* A section header belongs to the scope selected in the Scope band above, so it is a
       tier BELOW that band: a flat band, not the panel-header gradient the Scope and Raw
       bands use. Without this the sections read as siblings of Scope rather than as its
       contents. */
    if (previewGroup >= 0) model->setData(capIdx, true, UR_HeaderFlat);
}

void DevelopProperties::addSlider(const QString &key, const QString &caption,
                                  const QString &tooltip, QModelIndex parIdx,
                                  const QString &parentName, int min, int max,
                                  int div, QString color, QString color1,
                                  double defaultValue, bool logScale)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = defaultValue;
    /* Per-image now: the editor starts at identity and is populated from the current image's
       EditStack on selection (see populateSlidersFromStack). No app-global QSettings path. */
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Slider;
    i.type = (div == 0) ? "int" : "double";     // div 0 = integer slider, else double
    i.min = min;
    i.max = max;
    i.div = div;
    i.logScale = logScale;
    i.step = 1;
    i.color = color;
    i.color1 = color1;
    i.fixedWidth = 50;
    addItem(i);
}

void DevelopProperties::addCheckbox(const QString &key, const QString &caption, const QString &tooltip,
                                    QModelIndex parIdx, const QString &parentName, bool defaultValue)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = defaultValue;
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
}

/* -------------------------------------------------------------------------------------
   White balance (Basic, above Temp)
   ----------------------------------------------------------------------------------- */

CameraColor DevelopProperties::currentCam() const
{
    if (currentImagePath.isEmpty()) return CameraColor();
    auto work = WorkingImageCache::instance().get(currentImagePath);
    return work ? work->cam : CameraColor();
}

std::shared_ptr<const WorkingImage> DevelopProperties::ensureWorkingImage()
{
/*
    The pre-develop (scene-linear) WorkingImage for the current image, built ON DEMAND.

    RAW caches it at decode, but a DISPLAY-REFERRED file (JPG/TIFF/HEIC) only gets one the
    first time it is DEVELOPED: ImageDecoder::applyDevelop skips identity params and
    MW::applyDevelopPreviewIfEdited skips an unedited image, so a freshly selected,
    untouched JPEG has no entry. The WB dropper and Auto white balance need the pixels
    AND the colour characterisation (cam) before any edit exists, so they ask MW to build
    it rather than refusing the click.

    MW builds a display-referred file synchronously (an InputTransform over the decoded
    image) and returns with the cache populated. A missing raw scene-linear image needs a
    decode, which is asynchronous -- this returns null and the caller reports "not ready",
    as before.
*/
    if (currentImagePath.isEmpty()) return nullptr;
    auto work = WorkingImageCache::instance().get(currentImagePath);
    if (work && work->isValid()) return work;
    emit workingImageNeeded(currentImagePath);      // same thread: direct call into MW
    work = WorkingImageCache::instance().get(currentImagePath);
    return (work && work->isValid()) ? work : nullptr;
}

QIcon DevelopProperties::dropperIcon(bool armed)
{
/*
    Drawn rather than shipped as a resource: one small glyph is not worth a png, a qrc
    entry and a state-version bump (the dock-tab icon rules cover tabbable docks, not a
    plain button). A pipette on the diagonal -- bulb top-right, tip bottom-left.

    ARMED draws the same pipette as a faint dashed OUTLINE, so picking the tool up reads
    as lifting the dropper out of its slot: it is now the mouse cursor (ImageView's
    dropperCursor) and what remains here is the empty socket. Keeping the ghost rather
    than blanking the icon holds the button's size, so the row does not twitch.

    NOTE the painter works in LOGICAL coordinates: setDevicePixelRatio already accounts
    for retina, so an extra p.scale(dpr) would double-scale and throw all but a corner
    fragment off the pixmap (which is exactly what it did the first time).
*/
    const qreal dpr = 2.0;
    const int S = 16;
    QPixmap pm(int(S * dpr), int(S * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    /* Idle sits at G::iconOpacity like every other Develop tool button, so the row does
       not have one conspicuously brighter glyph in it. The ARMED ghost is drawn at full
       opacity in the accent colour instead: dimming it as well would leave the empty
       socket a faint smudge indistinguishable from the idle glyph, and the accent ties
       it to the blue border BarBtn::setActive draws around the button. */
    if (!armed) p.setOpacity(G::iconOpacity);

    const QColor c = armed ? QColor(G::appleBlue.red(), G::appleBlue.green(),
                                    G::appleBlue.blue(), 210)
                           : QColor(225, 225, 225);
    QPen pen(c, armed ? 1.2 : 1.5, armed ? Qt::DashLine : Qt::SolidLine,
             Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(armed ? QBrush(Qt::NoBrush) : QBrush(c));

    p.drawLine(QPointF(5.6, 10.4), QPointF(10.4, 5.6));             // barrel
    const QPolygonF tip({QPointF(2.0, 14.0), QPointF(5.0, 8.8), QPointF(7.2, 11.0)});
    p.drawPolygon(tip);                                             // tip
    p.save();
    p.translate(11.6, 4.4);
    p.rotate(-45.0);
    p.drawRoundedRect(QRectF(-2.6, -2.9, 5.2, 5.8), 2.0, 2.0);      // bulb
    p.restore();
    p.end();
    return QIcon(pm);
}

void DevelopProperties::addWhiteBalanceRow(QModelIndex parIdx)
{
    if (G::isLogger) G::log("DevelopProperties::addWhiteBalanceRow");

    clearItemInfo(i);
    i.name = "whiteBalance";
    i.parIdx = parIdx;
    i.parentName = "BasicHeader";
    i.captionText = "WB";
    i.tooltip = "White balance: pick a neutral with the dropper, or choose a preset.\n"
                "Double click to reset to \"As shot\".";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "whiteBalance";
    i.delegateType = DT_None;       // we own both cells
    addItem(i);
    const QModelIndex wbCapIdx = capIdx;
    const QModelIndex wbValIdx = findValueIndex("whiteBalance");
    model->setData(wbCapIdx, 4, UR_ExtraRowHeight);

    if (wbCapIdx.isValid()) {
        /* Dropper in the CAPTION cell, after the "WB" label so the row still reads as a
           labelled control. Checkable: it stays armed until it is clicked again, Esc,
           or a sample lands (Lightroom's dropper auto-dismisses on use). */
        QWidget *capW = new QWidget;
        capW->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *chb = new QHBoxLayout(capW);
        chb->setContentsMargins(0, 0, 0, 0);
        chb->setSpacing(6);
        QLabel *lbl = new QLabel("WB");
        /* Let clicks fall through to the tree so mouseDoubleClickEvent sees them (double
           click on the row = back to As shot). Set on the LABEL only: the attribute also
           covers a widget's children, so on the cell it would kill the dropper too. */
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        /* A BarBtn, like the Develop action-row tools (Scopes / Transform / Spot): its
           G::css base is transparent with no border, and setActive() adds the white
           accent border when armed. Using the shared class rather than a hand-rolled
           stylesheet keeps it in step if that chrome ever changes. */
        BarBtn *drop = new BarBtn();
        drop->setIcon(dropperIcon(false));
        drop->setIconSize(QSize(16, 16));
        drop->setToolTip("Click a neutral (grey/white) area of the image to set the "
                         "white balance from it.\nOpt/Alt-click a lit area of skin to "
                         "correct the skin colour instead.");
        /* Enabled whenever there IS an image -- deliberately NOT gated on the cached
           WorkingImage. The row is built by buildTree, which can run before the decode
           has populated WorkingImageCache, and the enabled state was only re-evaluated
           on the next rebuild: gating here left the dropper dead for the session (it
           looked armed-able but did nothing, and clicks fell through to the loupe's
           zoom). A genuine cache miss is reported when the sample is taken instead. */
        drop->setEnabled(!currentImagePath.isEmpty());
        connect(drop, &BarBtn::clicked, this,
                &DevelopProperties::toggleWbDropper);   // BarBtn is not checkable
        wbDropperBtn = drop;
        chb->addWidget(lbl);
        chb->addWidget(drop);
        chb->addStretch(1);
        setIndexWidget(wbCapIdx, capW);
    }

    if (wbValIdx.isValid()) {
        /* Preset dropdown in the VALUE cell, aligned with the sliders' value column. The
           named illuminants are RAW-only (see the header): a display-referred file has no
           camera-neutral reference for them to mean anything against. */
        QComboBox *combo = new QComboBox;
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        combo->setToolTip("Standard white balances.");
        using WB = WhiteBalance::Preset;
        QVector<WB> items{WB::AsShot, WB::Auto};
        if (currentIsRaw())
            items << WB::Daylight << WB::Cloudy << WB::Shade
                  << WB::Tungsten << WB::Fluorescent << WB::Flash;
        items << WB::Custom;
        for (WB w : items)
            combo->addItem(WhiteBalance::presetName(w), int(w));
        connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, combo](int ix){
            setWbPreset(combo->itemData(ix).toInt());
        });
        wbCombo = combo;

        QWidget *cell = new QWidget;
        cell->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *vhb = new QHBoxLayout(cell);
        vhb->setContentsMargins(0, 0, 10, 0);
        vhb->setSpacing(0);
        vhb->addWidget(combo);
        setIndexWidget(wbValIdx, cell);
    }
}

void DevelopProperties::setWbDropperActive(bool on)
{
    if (wbDropperActive == on) return;
    wbDropperActive = on;
    if (wbDropperBtn) {
        /* Blue accent border while armed (BarBtn::setActive, the same cue the action-row
           tools use), and the icon goes to its ghost outline -- the dropper has been
           lifted out of the button and is now the cursor. */
        wbDropperBtn->setActive(on);
        wbDropperBtn->setIcon(dropperIcon(on));
    }
    if (on) emit wbDropperBegin();
    else    emit wbDropperEnd();
}

void DevelopProperties::cancelWbDropper()
{
    setWbDropperActive(false);
}

void DevelopProperties::toggleWbDropper()
{
    /* Arming needs a current image to sample; the row's button is disabled without one,
       and "W" must not arm a dropper that can only fail. */
    if (!wbDropperActive && currentImagePath.isEmpty()) return;
    setWbDropperActive(!wbDropperActive);
}

void DevelopProperties::onWbSampled(double nx, double ny, bool skin)
{
/*
    The dropper landed. Read the clicked pixel out of the PRE-DEVELOP WorkingImage --
    the same scene-linear buffer Develop's white balance acts on, so the solve is exact
    and independent of whatever exposure/tone the user has already dialled in -- then
    solve for the (Kelvin, tint) it implies.

    Two modes. A plain click treats the sample as a NEUTRAL and drives it to grey. An
    Opt/Alt click treats it as SKIN and drives it back onto the skin hue line, keeping
    its saturation and lightness (see WhiteBalance::solveSkin) -- for portraits, where
    there is often no neutral in the frame but there is always a face.

    A small patch is averaged rather than a single pixel, which is what Lightroom does:
    on a noisy raw one pixel is a poor estimate of the local colour.
*/
    if (G::isLogger) G::log("DevelopProperties::onWbSampled");
    setWbDropperActive(false);              // auto-dismiss, like Lightroom
    if (currentImagePath.isEmpty()) return;

    auto work = ensureWorkingImage();
    if (!work || !work->cam.valid) {
        /* Still no pre-develop buffer for this image (a raw decode is in flight, or the
           image is not decoded yet). Say so rather than swallowing the click. */
        if (G::popup)
            G::popup->showPopup("The image is not ready to sample yet -- try again in "
                                "a moment.", 2000);
        return;
    }

    const int cx = qBound(0, int(nx * work->width),  work->width  - 1);
    const int cy = qBound(0, int(ny * work->height), work->height - 1);
    const int r = 2;                        // 5x5 patch
    double sum[3] = {0, 0, 0};
    int count = 0;
    for (int y = qMax(0, cy - r); y <= qMin(work->height - 1, cy + r); ++y) {
        for (int x = qMax(0, cx - r); x <= qMin(work->width - 1, cx + r); ++x) {
            const size_t o = (static_cast<size_t>(y) * work->width + x) * 3;
            sum[0] += work->rgb[o]; sum[1] += work->rgb[o + 1]; sum[2] += work->rgb[o + 2];
            ++count;
        }
    }
    if (count == 0) return;

    const float sr = float(sum[0] / count);
    const float sg = float(sum[1] / count);
    const float sb = float(sum[2] / count);

    float k = 0.0f, t = 0.0f;
    if (skin) {
        float hueErr = 0.0f;
        const WhiteBalance::SkinPick res =
            WhiteBalance::solveSkin(work->cam, sr, sg, sb, k, t, &hueErr);
        if (res != WhiteBalance::SkinPick::Ok) {
            /* Say WHICH way it failed -- "pick somewhere else" is not actionable. */
            QString msg;
            if (res == WhiteBalance::SkinPick::TooNeutral)
                msg = "That spot has almost no colour, so there is no skin hue to "
                      "correct. Use a plain click to balance it as a neutral.";
            else if (res == WhiteBalance::SkinPick::NotSkin)
                msg = QString("That does not look like skin (%1%2 off the skin hue). "
                              "Pick a lit midtone area of the face, out of shadow.")
                          .arg(qRound(hueErr)).arg(QChar(0260));
            else
                msg = "Cannot read that spot -- it is black or clipped. Pick a lit "
                      "midtone area of the face.";
            if (G::popup) G::popup->showPopup(msg, 2500);
            return;
        }
    }
    else if (!WhiteBalance::solve(work->cam, sr, sg, sb, k, t)) {
        /* A black or blown sample carries no colour to balance against. */
        if (G::popup)
            G::popup->showPopup("Cannot set white balance from that spot -- it is black "
                                "or clipped. Pick a lighter neutral area.", 2000);
        return;
    }

    EditParams &p = activeParams();
    p.temp = k;
    p.tint = t;
    p.wbPreset = int(WhiteBalance::Preset::Custom);
    noteEdit("White balance", QString::number(qRound(k)) + "K");
    refreshWbRow();
    emit paramsChanged();
}

void DevelopProperties::setWbPreset(int preset)
{
    if (currentImagePath.isEmpty()) return;
    using WB = WhiteBalance::Preset;
    const WB w = static_cast<WB>(preset);
    EditParams &p = activeParams();

    float k = 0.0f, t = 0.0f;
    if (w == WB::AsShot) {
        /* The canonical way back: the 0 sentinel restores the decoded balance EXACTLY
           and leaves the image reading as unedited. */
        p.temp = 0.0f;
        p.tint = 0.0f;
    }
    else if (w == WB::Auto) {
        auto work = ensureWorkingImage();
        if (!work || !WhiteBalance::autoWhiteBalance(*work, k, t)) {
            if (G::popup)
                G::popup->showPopup("Auto white balance could not find a neutral "
                                    "reference in this image.", 2000);
            refreshWbRow();
            return;
        }
        p.temp = k;
        p.tint = t;
    }
    else if (WhiteBalance::presetValues(w, k, t)) {
        p.temp = k;
        p.tint = t;
    }
    else {
        /* Custom is a readout, not a command: picking it changes nothing. */
        refreshWbRow();
        return;
    }

    p.wbPreset = preset;
    noteEdit("White balance", WhiteBalance::presetName(w));
    refreshWbRow();
    emit paramsChanged();
}

void DevelopProperties::resetWbAxisToAsShot(bool isTemp)
{
/*
    Double-clicking Temp (or Tint) restores THAT axis to the as-shot value the camera
    recorded -- the real default for an absolute Kelvin/tint pair. The other axis keeps
    whatever the user set: the pair is always committed together, so the surviving axis
    must be written as its resolved absolute value.

    When both axes end up back at as shot the row goes all the way home: the 0 sentinel
    (which reproduces the decoded balance exactly) and the As Shot preset, so the image
    reads as unedited -- the same state the WB row's double-click gives.
*/
    if (currentImagePath.isEmpty()) return;
    if (maskOverlayActive()) emit maskTintHideRequested();

    const CameraColor cam = currentCam();
    EditParams &p = activeParams();
    float k, t, asK, asT;
    WhiteBalance::resolve(cam, p.temp, p.tint, k, t);      // current, absolute
    WhiteBalance::resolve(cam, 0.0f, 0.0f, asK, asT);      // as shot
    if (isTemp) k = asK;
    else        t = asT;

    if (qFuzzyCompare(k, asK) && qFuzzyCompare(t, asT)) {
        p.temp = 0.0f;
        p.tint = 0.0f;
        p.wbPreset = int(WhiteBalance::Preset::AsShot);
    }
    else {
        p.temp = k;
        p.tint = t;
        p.wbPreset = int(WhiteBalance::Preset::Custom);
    }

    /* Same merge key as a Temp/Tint drag, so a drag followed by a reset reads as one
       white balance step ending where the user left it. */
    noteEdit("White balance", QString::number(qRound(k)) + "K", "wb/tempTint");
    refreshWbRow();
    emit paramsChanged();
}

void DevelopProperties::refreshWbRow()
{
    /* Push the active scope's white balance back into the row: the resolved absolute
       Kelvin/tint onto the two sliders (an untouched image shows its AS-SHOT values, not
       the 0 sentinel) and the preset onto the combo. */
    EditParams p;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty()) {
        const int ix = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size())
                           ? activeScopeIndex : 0;
        p = s.scopes[ix].params;
    }
    const CameraColor cam = currentCam();
    float k, t;
    WhiteBalance::resolve(cam, p.temp, p.tint, k, t);

    const bool wasPopulating = isPopulating;
    isPopulating = true;                    // these setValues must not write back
    setSliderReal("temp", k);
    setSliderReal("tint", t);
    isPopulating = wasPopulating;

    if (wbCombo) {
        QSignalBlocker b(wbCombo);
        const int ix = wbCombo->findData(p.wbPreset);
        wbCombo->setCurrentIndex(ix >= 0 ? ix : 0);
    }
    if (wbDropperBtn) wbDropperBtn->setEnabled(!currentImagePath.isEmpty());
}

/* ----------------------------------------------------------------------------------------
   Basic
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addBasic()
{
    if (G::isLogger) G::log("DevelopProperties::addBasic");
    addHeader("BasicHeader", "???", "Basic", "Core tone, white balance and presence adjustments.", PV_Basic);
    QModelIndex parIdx = capIdx;

    /* Lightroom-like ranges. Most adjustments are integer sliders -100..100 (div 0).
       Exposure is a 2-decimal EV slider (-5.00..5.00, div 100). All default to 0
       (identity), matching EditParams. */
    /* White balance: a dropper + preset dropdown, then the two absolute sliders. Temp is
       a KELVIN slider (2000..50000) on a LOG ramp -- linear would bury every useful
       temperature in the first sixth of the track. Its gradient runs blue -> yellow with
       the value, which is right: a HIGHER Kelvin says the light was bluer, so the image
       renders warmer. Tint is +/-150, green -> magenta. See Develop/whitebalance.h. */
    addWhiteBalanceRow(parIdx);
    addSlider("temp",       "Temp",       "Colour temperature of the light, in Kelvin.",
              parIdx, "BasicHeader",
              int(WhiteBalance::kMinKelvin), int(WhiteBalance::kMaxKelvin), 0,
              G::lightblue, G::lightyellow, 0, /*logScale*/ true);
    addSlider("tint",       "Tint",       "White balance tint (green/magenta).", parIdx, "BasicHeader", -150, 150, 0,   G::lightgreen, G::lightmagenta);
    addDivider(dividerHeight, 1, divColor, parIdx, "BasicHeader", "WBDevider");
    addSlider("exposure",   "Exposure",   "Overall exposure in stops (EV).",     parIdx, "BasicHeader", -500, 500, 100, G::darkgray, G::lightgray);
    addSlider("contrast",   "Contrast",   "Global contrast.",                    parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("highlights", "Highlights", "Recover or lift the highlights.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("shadows",    "Shadows",    "Recover or deepen the shadows.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("whites",     "Whites",     "Set the white point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("blacks",     "Blacks",     "Set the black point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addDivider(dividerHeight, 1, divColor, parIdx, "BasicHeader", "ToneDevider");
    addSlider("texture",    "Texture",    "Enhance or smooth fine detail.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addSlider("dehaze",     "Dehaze",     "Remove or add atmospheric haze.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addDivider(dividerHeight, 1, divColor, parIdx, "BasicHeader", "BasicEndDivider");
    // demo colors
    // addSlider("blue", "Blue", "Blue.", parIdx, "BasicHeader", -100, 100, 0, G::darkblue, G::lightblue);
    // addSlider("yellow", "Yellow", "Yellow.", parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    // addSlider("orange", "orange", "orange.", parIdx, "BasicHeader", -100, 100, 0, G::darkorange, G::lightorange);
    // addSlider("cyan", "cyan", "cyan.", parIdx, "BasicHeader", -100, 100, 0, G::darkcyan, G::lightcyan);
    // addSlider("green", "green", "green.", parIdx, "BasicHeader", -100, 100, 0, G::darkgreen, G::lightgreen);
    // addSlider("teal", "teal", "teal.", parIdx, "BasicHeader", -100, 100, 0, G::darkteal, G::lightteal);
    // addSlider("maroon", "maroon", "maroon.", parIdx, "BasicHeader", -100, 100, 0, G::darkmaroon, G::lightmaroon);
    // addSlider("pink", "pink", "pink.", parIdx, "BasicHeader", -100, 100, 0, G::darkpink, G::lightpink);
    // addSlider("purple", "purple", "purple.", parIdx, "BasicHeader", -100, 100, 0, G::darkpurple, G::lightpurple);
    // addSlider("red", "red", "red.", parIdx, "BasicHeader", -100, 100, 0, G::darkred, G::lightred);
}

void DevelopProperties::addColor()
{
    if (G::isLogger) G::log("DevelopProperties::addColor");
    addHeader("ColorHeader", "???", "Color", "RGB and HSL adjustments.", PV_Color);
    QModelIndex parIdx = capIdx;

    /* All integer sliders -100..100 (div 0), default 0 (identity), matching EditParams.
       RGB = per-channel gain; HSL = global hue rotation / saturation / luminance. */
    addSlider("red",        "Red",        "Per-channel red gain.",                 parIdx, "ColorHeader", -100, 100, 0, G::darkred,   G::lightred);
    addSlider("green",      "Green",      "Per-channel green gain.",               parIdx, "ColorHeader", -100, 100, 0, G::darkgreen, G::lightgreen);
    addSlider("blue",       "Blue",       "Per-channel blue gain.",                parIdx, "ColorHeader", -100, 100, 0, G::darkblue,  G::lightblue);
    addDivider(dividerHeight, 1, divColor, parIdx, "ColorHeader", "RGBDevider");
    addSlider("hue",        "Hue",        "Rotate all hues.",                      parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addSlider("saturation", "Saturation", "Global saturation (grey at -100).",     parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addSlider("luminance",  "Luminance",  "Global luminance (brightness).",        parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addDivider(dividerHeight, 1, divColor, parIdx, "ColorHeader", "HSLDevider");
    addSlider("vibrance",   "Vibrance",   "Saturation weighted toward muted colours.", parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addDivider(dividerHeight, 1, divColor, parIdx, "ColorHeader", "ColorEndDivider");
}

/* --------------------------------------------------------------------------------------
   Calibrate (RGB primaries)

   Lightroom's Calibration panel: a hue shift + saturation scale for each of the three
   PRIMARIES of the working space, built into one 3x3 matrix and applied in linear light
   before the tone curve. Same select-then-adjust grammar as Color Grade -- R / G / B
   checkboxes pick which primaries the wheel writes (calActiveMask), then one shared wheel
   whose dots are DELTAS from each primary's home angle (see primarywheel.h).

   Distinct from the Color panel's Red/Green/Blue, which are per-channel linear GAINS
   folded into the white-balance vector -- a cruder second white balance. Both exist; the
   gains are kept until Calibrate proves out. The maths (Develop/calibrate.h) normalises
   the matrix rows so NEUTRALS STAY NEUTRAL, which is the whole point of the panel.
   ------------------------------------------------------------------------------------ */

void DevelopProperties::addCalibrate()
{
    if (G::isLogger) G::log("DevelopProperties::addCalibrate");
    addHeader("CalibrateHeader", "???", "Calibrate",
              "Camera calibration: hue + saturation of the R/G/B primaries.",
              PV_Calibrate);
    QModelIndex parIdx = capIdx;

    /* Primary selector: three checkboxes on one spanned row, mirroring the Color Grade
       range row. UI state (calActiveMask), not EditParams. */
    clearItemInfo(i);
    i.name = "calPrimaries";
    i.parIdx = parIdx;
    i.parentName = "CalibrateHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex primIdx = capIdx;
    model->setData(primIdx, 6, UR_ExtraRowHeight);
    setFirstColumnSpanned(primIdx.row(), parIdx, true);
    {
        QWidget *rw = new QWidget;
        rw->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(rw);
        hb->setContentsMargins(QTreeView::indentation() + 4, 0, 0, 0);
        hb->setSpacing(10);
        QCheckBox *red   = new QCheckBox("Red",   rw);
        QCheckBox *green = new QCheckBox("Green", rw);
        QCheckBox *blue  = new QCheckBox("Blue",  rw);
        red->setChecked(calActiveMask   & 0x1);
        green->setChecked(calActiveMask & 0x2);
        blue->setChecked(calActiveMask  & 0x4);
        auto upd = [this, red, green, blue]{
            calActiveMask = (red->isChecked()   ? 0x1 : 0) |
                            (green->isChecked() ? 0x2 : 0) |
                            (blue->isChecked()  ? 0x4 : 0);
            if (primaryWheel) primaryWheel->setActiveMask(calActiveMask);
        };
        connect(red,   &QCheckBox::toggled, this, [upd](bool){ upd(); });
        connect(green, &QCheckBox::toggled, this, [upd](bool){ upd(); });
        connect(blue,  &QCheckBox::toggled, this, [upd](bool){ upd(); });
        hb->addWidget(red);
        hb->addWidget(green);
        hb->addWidget(blue);
        hb->addStretch(1);
        setIndexWidget(primIdx, rw);
    }

    /* The shared wheel: a tall, full-width spanned row hosting the PrimaryWheel. */
    clearItemInfo(i);
    i.name = "calWheel";
    i.parIdx = parIdx;
    i.parentName = "CalibrateHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex wheelIdx = capIdx;
    model->setData(wheelIdx, 150, UR_ExtraRowHeight);
    setFirstColumnSpanned(wheelIdx.row(), parIdx, true);
    {
        PrimaryWheel *wheel = new PrimaryWheel;
        primaryWheel = wheel;
        wheel->setActiveMask(calActiveMask);
        connect(wheel, &PrimaryWheel::primaryChanged,   this,
                [this]{ onPrimaryWheelChanged(false); });
        connect(wheel, &PrimaryWheel::primaryCommitted, this,
                [this]{ onPrimaryWheelChanged(true); });
        setIndexWidget(wheelIdx, wheel);
    }
    addDivider(dividerHeight, 1, divColor, parIdx, "CalibrateHeader", "CalibrateEndDivider");
}

/* Wheel drag -> the active scope's cal params for every checked primary, then preview.
   commit marks the drag-release. One history entry per drag, keyed by which primaries it
   is moving (a custom editor gets no history for free -- see the checklist). */
void DevelopProperties::onPrimaryWheelChanged(bool commit)
{
    if (currentImagePath.isEmpty() || !primaryWheel) return;
    if (maskOverlayActive()) emit maskTintHideRequested();
    EditParams &p = activeParams();
    if (calActiveMask & 0x1) {
        p.calRedHue = primaryWheel->hue(0);
        p.calRedSat = primaryWheel->sat(0);
    }
    if (calActiveMask & 0x2) {
        p.calGreenHue = primaryWheel->hue(1);
        p.calGreenSat = primaryWheel->sat(1);
    }
    if (calActiveMask & 0x4) {
        p.calBlueHue = primaryWheel->hue(2);
        p.calBlueSat = primaryWheel->sat(2);
    }
    noteEdit("Calibrate", QString(),
             QString("cal/wheel/%1").arg(calActiveMask));
    emit paramsChanged();
}

/* Push the active scope's stored primaries into the wheel dots. Called on image/scope
   load, from populateSlidersFromStack. */
void DevelopProperties::refreshCalibrateRow()
{
    EditParams p;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty()) {
        const int idx = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size())
                            ? activeScopeIndex : 0;
        p = s.scopes[idx].params;
    }
    if (primaryWheel) {
        primaryWheel->setPrimary(0, p.calRedHue,   p.calRedSat);
        primaryWheel->setPrimary(1, p.calGreenHue, p.calGreenSat);
        primaryWheel->setPrimary(2, p.calBlueHue,  p.calBlueSat);
        primaryWheel->setActiveMask(calActiveMask);
    }
}

/* --------------------------------------------------------------------------------------
   Color Grade (colour grading)

   A minimalist take on Lightroom's Color Grading: ONE shared hue/saturation wheel driven
   by Dark / Mid / Light checkboxes (which tonal range the wheel edits -- check all three
   to move them together = a global tint), plus one Luminance slider for the checked
   range(s). The wheel and range checkboxes are custom widgets hosted via setIndexWidget
   (not delegate editors); the wheel writes hue/sat and the slider luminance into the
   active scope's nine grade params (see onGradeWheelChanged / setGradeLum). Distinct from
   the Color panel (RGB + global HSL): grading selects pixels by TONE and adds a tint,
   while the HSL sliders remap existing hues globally.
   ------------------------------------------------------------------------------------ */

void DevelopProperties::addColorGrade()
{
    if (G::isLogger) G::log("DevelopProperties::addColorGrade");
    addHeader("ColorGradeHeader", "???", "Color Grade",
              "Tonal-range colour grading (shadows / midtones / highlights).", PV_ColorGrade);
    QModelIndex parIdx = capIdx;

    /* Range selector: three checkboxes on one spanned row (Dark/Mid/Light), matching the
       compact panel mockup. They are UI state (gradeActiveMask), not EditParams; they
       pick which range(s) the wheel + Luminance slider write. */
    clearItemInfo(i);
    i.name = "gradeRanges";
    i.parIdx = parIdx;
    i.parentName = "ColorGradeHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex rangesIdx = capIdx;
    model->setData(rangesIdx, 6, UR_ExtraRowHeight);
    setFirstColumnSpanned(rangesIdx.row(), parIdx, true);
    {
        QWidget *rw = new QWidget;
        rw->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(rw);
        hb->setContentsMargins(QTreeView::indentation() + 4, 0, 0, 0);
        hb->setSpacing(10);
        QCheckBox *dark  = new QCheckBox("Shadows",    rw);
        QCheckBox *mid   = new QCheckBox("Midtones",   rw);
        QCheckBox *light = new QCheckBox("Highlights", rw);
        QCheckBox *glob  = new QCheckBox("Global",     rw);
        glob->setToolTip("Tint every pixel, whatever its tone.");
        dark->setChecked(gradeActiveMask  & 0x1);
        mid->setChecked(gradeActiveMask   & 0x2);
        light->setChecked(gradeActiveMask & 0x4);
        glob->setChecked(gradeActiveMask  & 0x8);
        auto upd = [this, dark, mid, light, glob]{
            gradeActiveMask = (dark->isChecked()  ? 0x1 : 0) |
                              (mid->isChecked()   ? 0x2 : 0) |
                              (light->isChecked() ? 0x4 : 0) |
                              (glob->isChecked()  ? 0x8 : 0);
            if (colorGradeWheel) colorGradeWheel->setActiveMask(gradeActiveMask);
            refreshColorGradeRow();      // point the Lum slider at the new active range
        };
        connect(dark,  &QCheckBox::toggled, this, [upd](bool){ upd(); });
        connect(mid,   &QCheckBox::toggled, this, [upd](bool){ upd(); });
        connect(light, &QCheckBox::toggled, this, [upd](bool){ upd(); });
        connect(glob,  &QCheckBox::toggled, this, [upd](bool){ upd(); });
        hb->addWidget(dark);
        hb->addWidget(mid);
        hb->addWidget(light);
        hb->addWidget(glob);
        hb->addStretch(1);
        setIndexWidget(rangesIdx, rw);
    }

    /* The shared wheel: a tall, full-width spanned row hosting the ColorGradeWheel. */
    clearItemInfo(i);
    i.name = "gradeWheel";
    i.parIdx = parIdx;
    i.parentName = "ColorGradeHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex wheelIdx = capIdx;
    model->setData(wheelIdx, 150, UR_ExtraRowHeight);
    setFirstColumnSpanned(wheelIdx.row(), parIdx, true);
    {
        ColorGradeWheel *wheel = new ColorGradeWheel;
        colorGradeWheel = wheel;
        wheel->setActiveMask(gradeActiveMask);
        connect(wheel, &ColorGradeWheel::gradeChanged,   this, [this]{ onGradeWheelChanged(false); });
        connect(wheel, &ColorGradeWheel::gradeCommitted, this, [this]{ onGradeWheelChanged(true); });
        setIndexWidget(wheelIdx, wheel);
    }

    /* Luminance for the checked range(s). Integer -100..100 like the colour sliders. */
    addSlider("gradeLum", "Luminance",
              "Brighten / darken the checked tonal range(s).",
              parIdx, "ColorGradeHeader", -100, 100, 0, G::darkgray, G::lightgray);

    /* Window SHAPE: panel-wide, not per-range, so they sit below the divider. Blending
       defaults to 50 and Balance to 0, which reproduce the fixed split the panel used
       before they existed. */
    addDivider(dividerHeight, 1, divColor, parIdx, "ColorGradeHeader", "GradeDivider");
    addSlider("gradeBlending", "Blending",
              "How much the shadow / midtone / highlight ranges overlap "
              "(50 = the default split).",
              parIdx, "ColorGradeHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);
    addSlider("gradeBalance", "Balance",
              "Slide the split between the ranges: right favours highlights, "
              "left favours shadows.",
              parIdx, "ColorGradeHeader", -100, 100, 0, G::darkgray, G::lightgray);
    addDivider(dividerHeight, 1, divColor, parIdx, "ColorGradeHeader", "ColorGradeEndDivider");
}

int DevelopProperties::firstActiveGradeRange() const
{
    if (gradeActiveMask & 0x1) return 0;   // shadows
    if (gradeActiveMask & 0x2) return 1;   // midtones
    if (gradeActiveMask & 0x4) return 2;   // highlights
    if (gradeActiveMask & 0x8) return 3;   // global
    return 1;                              // nothing checked: default to midtones
}

void DevelopProperties::setGradeLum(float lum)
{
    if (currentImagePath.isEmpty()) return;
    EditParams &p = activeParams();
    if (gradeActiveMask & 0x1) p.gradeShadowLum = lum;
    if (gradeActiveMask & 0x2) p.gradeMidLum    = lum;
    if (gradeActiveMask & 0x4) p.gradeHighLum   = lum;
    if (gradeActiveMask & 0x8) p.gradeGlobalLum = lum;
}

/* Wheel drag -> the active scope's grade hue/sat for every checked range, then preview.
   commit marks the drag-release, when the debounced sidecar write is scheduled (live
   moves only render; dirty is set every move so navigate-away still persists). */
void DevelopProperties::onGradeWheelChanged(bool commit)
{
    if (currentImagePath.isEmpty() || !colorGradeWheel) return;
    if (maskOverlayActive()) emit maskTintHideRequested();
    EditParams &p = activeParams();
    if (gradeActiveMask & 0x1) {
        p.gradeShadowHue = colorGradeWheel->hue(0);
        p.gradeShadowSat = colorGradeWheel->sat(0);
    }
    if (gradeActiveMask & 0x2) {
        p.gradeMidHue = colorGradeWheel->hue(1);
        p.gradeMidSat = colorGradeWheel->sat(1);
    }
    if (gradeActiveMask & 0x4) {
        p.gradeHighHue = colorGradeWheel->hue(2);
        p.gradeHighSat = colorGradeWheel->sat(2);
    }
    if (gradeActiveMask & 0x8) {
        p.gradeGlobalHue = colorGradeWheel->hue(3);
        p.gradeGlobalSat = colorGradeWheel->sat(3);
    }
    /* One entry for the whole wheel drag, keyed by which ranges it is moving. */
    noteEdit("Color grade", QString(),
             QString("grade/wheel/%1").arg(gradeActiveMask));
    emit paramsChanged();
}

/* Push the active scope's stored grade into the wheel dots + the Luminance slider (which
   shows the lowest checked range's value). Guards the slider set so it does not feed back
   as an edit. Called on image/scope load and whenever the range checkboxes change. */
void DevelopProperties::refreshColorGradeRow()
{
    EditParams p;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty()) {
        const int idx = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size())
                            ? activeScopeIndex : 0;
        p = s.scopes[idx].params;
    }
    if (colorGradeWheel) {
        colorGradeWheel->setRange(0, p.gradeShadowHue, p.gradeShadowSat);
        colorGradeWheel->setRange(1, p.gradeMidHue,    p.gradeMidSat);
        colorGradeWheel->setRange(2, p.gradeHighHue,   p.gradeHighSat);
        colorGradeWheel->setRange(3, p.gradeGlobalHue, p.gradeGlobalSat);
        colorGradeWheel->setActiveMask(gradeActiveMask);
    }
    const int r = firstActiveGradeRange();
    const float lum = (r == 0) ? p.gradeShadowLum : (r == 1) ? p.gradeMidLum
                                                  : (r == 2) ? p.gradeHighLum
                                                             : p.gradeGlobalLum;
    const bool wasPop = isPopulating;
    isPopulating = true;                 // don't let setValue echo back as an edit
    setSliderReal("gradeLum", lum);
    isPopulating = wasPop;
}

/* Convert the Color Range mask's [r,g,b] samples to (hueDeg, sat) points for the wheel,
   using the SAME HSV conversion the coverage maths use (RangeMask::rgb2hs), so the dots
   and band the user sees match the selection exactly. */
/* --------------------------------------------------------------------------------------
   Color Range hue presets

   The eight named hue bands Lightroom's Color Mixer uses, offered here as one-click chips
   on the Color Range MASK instead. Clicking one writes a synthetic sample at the band's
   centre hue (at a mid saturation, so the sample sits mid-radius on the wheel where its
   sat window has room both ways) plus a +/- half-band hue window, which is exactly what
   the pipette would have written had the user found a perfectly representative pixel.

   Everything downstream is untouched: RangeMask::colorCoverage already takes a sample
   list and hue/sat windows, so this adds NO new selection maths. The chips are a
   shortcut into the existing mask, not a second mechanism.
   ------------------------------------------------------------------------------------ */

namespace {
struct HueBand { const char *name; float centre; };
/* Centres 45 deg apart, starting at red. Half-band = 22.5 deg, so the eight windows tile
   the circle without gaps when used one at a time. */
const HueBand kHueBands[] = {
    {"Red",     0.0f},   {"Orange", 45.0f},  {"Yellow",  90.0f}, {"Green",  135.0f},
    {"Aqua",  180.0f},   {"Blue",  225.0f},  {"Purple", 270.0f}, {"Magenta", 315.0f},
};
constexpr float kHueBandHalf = 22.5f;      // degrees either side of the centre
constexpr float kHueBandSat  = 0.6f;       // the synthetic sample's saturation
}

int DevelopProperties::hueBandCount()
{
    return int(sizeof(kHueBands) / sizeof(HueBand));
}

QString DevelopProperties::hueBandName(int band)
{
    if (band < 0 || band >= hueBandCount()) return QString();
    return QString::fromLatin1(kHueBands[band].name);
}

float DevelopProperties::hueBandCentre(int band)
{
    if (band < 0 || band >= hueBandCount()) return 0.0f;
    return kHueBands[band].centre;
}

void DevelopProperties::applyColorRangeHuePreset(int band)
{
    if (G::isLogger) G::log("DevelopProperties::applyColorRangeHuePreset");
    if (band < 0 || band >= hueBandCount()) return;
    EditScope *l = activeScope();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->components.size()) return;
    MaskComponent &mm = l->components[selectedMaskIndex];
    if (mm.tool != int(MaskTool::ColorRange)) return;

    /* The band centre as an RGB triple -- samples are stored as RGB (see
       colorRangeSamplesHS), and full VALUE keeps the round-trip through rgb2hs exact. */
    const QColor c = QColor::fromHsvF(qMin(0.9999f, kHueBands[band].centre / 360.0f),
                                      kHueBandSat, 1.0);
    QJsonArray sample;
    sample.append(c.redF());
    sample.append(c.greenF());
    sample.append(c.blueF());

    /* REPLACE rather than append: a chip means "select this colour", so clicking Green
       after Red must not leave the reds selected too. Shift-clicking the image still
       adds further samples on top, as before. */
    QJsonObject o = QJsonDocument::fromJson(mm.paramsJson.toUtf8()).object();
    QJsonArray samples;
    samples.append(sample);
    o["samples"] = samples;
    o["hueLo"] = kHueBandHalf;
    o["hueHi"] = kHueBandHalf;
    if (!o.contains("satLo")) o["satLo"] = 25;
    if (!o.contains("satHi")) o["satHi"] = 25;
    mm.paramsJson = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));

    noteEdit("Color range preset", hueBandName(band), "mask/huePreset");

    /* Push the new sample + window into the wheel and the four bound sliders, guarded so
       the sets do not echo back as edits. */
    const bool wasPop = isPopulating;
    isPopulating = true;
    setSliderReal("maskHueLo", kHueBandHalf);
    setSliderReal("maskHueHi", kHueBandHalf);
    isPopulating = wasPop;
    if (colorRangeWheel) {
        colorRangeWheel->setSamples(colorRangeSamplesHS(mm.paramsJson));
        colorRangeWheel->setBounds(kHueBandHalf, kHueBandHalf,
                                   brushNum(mm.paramsJson, "satLo", 25) / 100.0,
                                   brushNum(mm.paramsJson, "satHi", 25) / 100.0);
    }
    emit maskRangeChanged(mm.paramsJson);   // live-update the loupe tint
    emit paramsChanged();                   // re-composite the masked scope
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

QVector<QPointF> DevelopProperties::colorRangeSamplesHS(const QString &paramsJson)
{
    QVector<QPointF> out;
    const QJsonArray sa = QJsonDocument::fromJson(paramsJson.toUtf8())
                              .object().value("samples").toArray();
    for (const QJsonValue &sv : sa) {
        const QJsonArray c = sv.toArray();
        if (c.size() < 3) continue;
        float h, s;
        RangeMask::rgb2hs(float(c[0].toDouble()), float(c[1].toDouble()),
                          float(c[2].toDouble()), h, s);
        out.append(QPointF(h, s));
    }
    return out;
}

/* Push the active Color Range mask's samples + hue/sat bounds into the wheel. Called when
   the pipette adds/removes a sample (the geometry handshake carries no rebuild). */
void DevelopProperties::refreshColorRangeWheel()
{
    if (!colorRangeWheel) return;
    EditScope *l = activeScope();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->components.size()) return;
    const MaskComponent &m = l->components[selectedMaskIndex];
    if (m.tool != int(MaskTool::ColorRange)) return;
    colorRangeWheel->setSamples(colorRangeSamplesHS(m.paramsJson));
    colorRangeWheel->setBounds(brushNum(m.paramsJson, "hueLo", 20),
                               brushNum(m.paramsJson, "hueHi", 20),
                               brushNum(m.paramsJson, "satLo", 25) / 100.0,
                               brushNum(m.paramsJson, "satHi", 25) / 100.0);
}

/* Wheel handle drag -> the active Color Range mask's hue/sat bounds, then re-composite.
   Sliders are synced (guarded) so the two stay together; commit schedules the write. */
void DevelopProperties::onColorRangeWheelChanged(bool commit)
{
    if (!colorRangeWheel) return;
    EditScope *l = activeScope();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->components.size()) return;
    MaskComponent &mm = l->components[selectedMaskIndex];
    if (mm.tool != int(MaskTool::ColorRange)) return;
    const int hueLo = int(colorRangeWheel->hueLo() + 0.5f);
    const int hueHi = int(colorRangeWheel->hueHi() + 0.5f);
    const int satLo = int(colorRangeWheel->satLo() * 100.0f + 0.5f);
    const int satHi = int(colorRangeWheel->satHi() * 100.0f + 0.5f);
    mm.paramsJson = brushWith(mm.paramsJson, "hueLo", hueLo);
    mm.paramsJson = brushWith(mm.paramsJson, "hueHi", hueHi);
    mm.paramsJson = brushWith(mm.paramsJson, "satLo", satLo);
    mm.paramsJson = brushWith(mm.paramsJson, "satHi", satHi);
    noteEdit("Color range", QString(), "mask/colorRangeWheel");
    const bool wasPop = isPopulating;
    isPopulating = true;                 // sync sliders without re-entering itemChange
    setSliderReal("maskHueLo", hueLo);
    setSliderReal("maskHueHi", hueHi);
    setSliderReal("maskSatLo", satLo);
    setSliderReal("maskSatHi", satHi);
    isPopulating = wasPop;
    emit maskRangeChanged(mm.paramsJson);   // live-update the loupe tint
    emit paramsChanged();                   // re-composite the masked scope
    if (commit && G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

/* ----------------------------------------------------------------------------------------
   Effects
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addEffects()
{
    if (G::isLogger) G::log("DevelopProperties::addEffects");
    addHeader("EffectsHeader", "???", "Effects", "Local (post-demosaic) effects.", PV_Effects);
    QModelIndex parIdx = capIdx;

    /* Local noise reduction: per-scope, maskable Develop ops on the decoded image
       (localDenoiseLuma / localDenoiseChroma). Distinct from the Global scope's "Denoise raw"
       (decode-time global raw NR, denoiseLuma/denoiseChroma) -- different function, different keys.
       Two 0..100 strength sliders (mapped to 0..1): Denoise = luminance NR (ratio-preserving),
       Denoise Color = chroma NR (opponent-chroma blur, luma kept exact) -- see Develop::Denoise. */
    addSlider("localDenoise", "Denoise:", "Local luminance noise reduction on the rendered image.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray);
    addSlider("localDenoiseChroma", "   Color", "Local colour (chroma) noise reduction.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray);

    addDivider(dividerHeight, 1, divColor, parIdx, "EffectsHeader", "VignetteDevider");

    /* Vignette: a global radial exposure falloff about the image centre (see
       Develop::Vignette). Two sliders: Exposure (a 2-decimal EV slider like Basic
       Exposure, the corner adjustment -- negative darkens, positive brightens) and
       Feather (0..100, how far it reaches inward). Off-centre / shaped vignettes use
       radial masks instead. */
    addSlider("vignetteExposure", "Vignette:", "Corner exposure (EV): - darkens, + brightens.",
              parIdx, "EffectsHeader", -500, 500, 100, G::darkgray, G::lightgray);
    addSlider("vignetteFeather", "   Feather", "How far the vignette reaches inward.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);

    addDivider(dividerHeight, 1, divColor, parIdx, "EffectsHeader", "GrainDivider");

    /* Grain: monochromatic film grain added to luminance (see Develop::Grain). Amount is
       the strength, size the particle size (scaled to the image so the proxy matches full
       res), roughness how patchy / irregular the grain is. Size and roughness are
       sub-controls of Amount (indented "  " captions, like the vignette feather) and are
       inert until Amount is non-zero. All three 0..100 sliders mapped to 0..1. */

    addSlider("grainAmount", "Grain:", "Add monochromatic film grain to the image.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray);
    addSlider("grainSize", "   Size", "Grain particle size (fine to coarse).",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 25);
    addSlider("grainRoughness", "   Roughness", "How patchy / irregular the grain is.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);

    addDivider(dividerHeight, 1, divColor, parIdx, "EffectsHeader", "EndDivider");
}

void DevelopProperties::updateSectionHeaderCaptions()
{
    if (G::isLogger) G::log("DevelopProperties::updateSectionHeaderCaptions");
    /* The Basic/Color/Effects section headers carry their plain section name only: the
       active scope is already shown (selected) in the scope list above them, so prefixing
       every header with it ("Global: Basic") was redundant noise.

       A section whose params are no longer at their defaults gets a trailing " *"
       ("Basic *"), so an edited panel is obvious while collapsed. The state is compared
       against hdrStar[] and the model is only touched when a section actually flips --
       this runs on every paramsChanged (i.e. every slider tick), and findCaptionIndex is
       a recursive model search. */
    struct HdrDef { const char *name; const char *label; int group; };
    static const HdrDef hdrs[] = {
        {"BasicHeader",      "Basic",       PV_Basic},
        {"ColorHeader",      "Color",       PV_Color},
        {"CalibrateHeader",  "Calibrate",   PV_Calibrate},
        {"ColorGradeHeader", "Color Grade", PV_ColorGrade},
        {"EffectsHeader",    "Effects",     PV_Effects},
    };
    const EditStack s = stackCache.value(currentImagePath);
    const bool haveParams = !currentImagePath.isEmpty() && !s.scopes.isEmpty();
    const int scopeIdx = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size())
                             ? activeScopeIndex : 0;

    for (int i = 0; i < int(sizeof(hdrs) / sizeof(HdrDef)); ++i) {
        const HdrDef &h = hdrs[i];
        const bool edited = haveParams &&
            !EditParams::groupIsDefault(s.scopes.at(scopeIdx).params, paramsGroup(h.group));
        if (hdrStar[i] == int(edited)) continue;     // caption already correct
        const QModelIndex idx = findCaptionIndex(h.name);
        if (!idx.isValid()) continue;
        model->setData(idx, edited ? QString(QString(h.label) + " *") : QString(h.label));
        hdrStar[i] = int(edited);
    }
}

/* ----------------------------------------------------------------------------------------
   Value change handling
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::itemChange(QModelIndex idx)
{
    if (G::isLogger) G::log("DevelopProperties::itemChange");

    /* Ignore changes we drove ourselves while loading an image's saved values into the editors
       (setValue emits editorValueChanged -> itemChanged). */
    if (isPopulating) return;

    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();

    /* Brush current settings (size/feather/flow/autoMask) set what the NEXT stroke uses; existing
       strokes keep their own snapshot, so these do NOT re-composite -- they just refresh the cursor
       and brush state in ImageView. (Invert still flips the whole mask -- handled below.) */
    if (source == "maskSize" || source == "maskFlow" || source == "maskAutoMask" ||
        source == "maskAutoMaskAi" ||
        (source == "maskFeather" && activeMaskTool() == int(MaskTool::Brush))) {
        EditScope *l = activeScope();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->components.size()) {
            MaskComponent &mm = l->components[selectedMaskIndex];
            if      (source == "maskFeather")  mm.feather = v.toFloat();
            else if (source == "maskSize")     mm.paramsJson = brushWith(mm.paramsJson, "size", v.toInt());
            else if (source == "maskFlow")     mm.paramsJson = brushWith(mm.paramsJson, "flow", v.toInt());
            else if (source == "maskAutoMask") mm.paramsJson = brushWith(mm.paramsJson, "autoMask", v.toBool());
            else if (source == "maskAutoMaskAi") {
                mm.paramsJson = brushWith(mm.paramsJson, "autoMaskMode", v.toBool() ? "ai" : "lum");
                if (v.toBool()) emit maskBrushAiEnabled();   // pre-warm the SAM encoder
            }
            noteEdit(maskSettingLabel(source), historyValueText(idx, v),
                     "mask/" + source);
            emitBrushSettings(mm);
        }
        return;
    }

    /* Content-range tool settings (Luminance Range lo/hi, Color Range refine) live in the active
       component's paramsJson. They change coverage, so re-composite AND live-update the loupe tint. */
    if (source == "maskRangeLo" || source == "maskRangeHi" ||
        source == "maskHueLo"   || source == "maskHueHi" ||
        source == "maskSatLo"   || source == "maskSatHi") {
        EditScope *l = activeScope();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->components.size()) {
            MaskComponent &mm = l->components[selectedMaskIndex];
            if      (source == "maskRangeLo") mm.paramsJson = brushWith(mm.paramsJson, "lo", v.toDouble() / 100.0);
            else if (source == "maskRangeHi") mm.paramsJson = brushWith(mm.paramsJson, "hi", v.toDouble() / 100.0);
            else if (source == "maskHueLo")   mm.paramsJson = brushWith(mm.paramsJson, "hueLo", v.toInt());
            else if (source == "maskHueHi")   mm.paramsJson = brushWith(mm.paramsJson, "hueHi", v.toInt());
            else if (source == "maskSatLo")   mm.paramsJson = brushWith(mm.paramsJson, "satLo", v.toInt());
            else                              mm.paramsJson = brushWith(mm.paramsJson, "satHi", v.toInt());
            noteEdit(maskSettingLabel(source), historyValueText(idx, v),
                     "mask/" + source);
            /* Keep the wheel's band in step with a slider drag. */
            if (colorRangeWheel && activeMaskTool() == int(MaskTool::ColorRange))
                colorRangeWheel->setBounds(brushNum(mm.paramsJson, "hueLo", 20),
                                           brushNum(mm.paramsJson, "hueHi", 20),
                                           brushNum(mm.paramsJson, "satLo", 25) / 100.0,
                                           brushNum(mm.paramsJson, "satHi", 25) / 100.0);
            emit maskRangeChanged(mm.paramsJson);   // live-update the loupe tint
            emit paramsChanged();                   // re-composite the masked scope
        }
        return;
    }

    /* The selected mask tool's settings write into the active scope's mask model. Feather/Invert
       change the mask, so they update the live overlay AND re-composite the masked scope. */
    if (source == "maskFeather" || source == "maskInvert") {
        EditScope *l = activeScope();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->components.size()) {
            if (source == "maskFeather") {
                l->components[selectedMaskIndex].feather = v.toFloat();
                emit maskFeatherChanged(v.toDouble());      // live-update the overlay ramp
            }
            else {
                l->components[selectedMaskIndex].inverted = v.toBool();
                emit maskInvertChanged(v.toBool());         // live-flip the overlay
            }
            noteEdit(maskSettingLabel(source), historyValueText(idx, v),
                     "mask/" + source);
            emit paramsChanged();                  // re-composite the masked scope
        }
        return;
    }

    /* The "Demosaic" combo (raw-only, Global) selects the RAW decode ENGINE (Apple Core Image vs the
       in-house Winnow decoder). It is not an EditParams value -- it forces a re-decode -- so handle
       it here (emit to MW) and skip applyKeyToParams / the normal preview render. */
    if (source == "demosaic") {
        emit demosaicEngineChanged(v.toString() == "Apple");   // MW sets decodeRawEngine
        /* The engine drives whether the raw-denoise children (and so the expand arrow)
           exist -- Winnow yes, Apple no. Rebuild the tree to add/remove them, but DEFER
           it: doing it inline would delete this combo's live editor mid-signal.
           G::decodeRawEngine is set synchronously by the MW handler above, so the
           rebuild sees the new engine. */
        QTimer::singleShot(0, this, [this]{ buildTree(); });
        return;
    }

    /* Temp / Tint are an ABSOLUTE PAIR and are always written together: the sliders show
       resolved values, so moving Tint alone must still commit the resolved Kelvin --
       otherwise temp would stay at its "as shot" sentinel 0 and the two would disagree
       about what they are relative to. A manual move also demotes the preset to
       Custom. */
    if (source == "temp" || source == "tint") {
        if (currentImagePath.isEmpty()) return;
        if (maskOverlayActive()) emit maskTintHideRequested();
        EditParams &p = activeParams();
        float k, t;
        WhiteBalance::resolve(currentCam(), p.temp, p.tint, k, t);
        if (source == "temp") k = v.toFloat();
        else                  t = v.toFloat();
        p.temp = k;
        p.tint = t;
        p.wbPreset = int(WhiteBalance::Preset::Custom);
        /* Temp and Tint commit together, so they share one history entry. */
        noteEdit("White balance",
                 QString::number(qRound(k)) + "K", "wb/tempTint");
        if (wbCombo) {
            QSignalBlocker b(wbCombo);
            const int ix = wbCombo->findData(p.wbPreset);
            if (ix >= 0) wbCombo->setCurrentIndex(ix);
        }
        emit paramsChanged();
        return;
    }

    /* Color Grade Luminance slider: writes every range the Dark/Mid/Light checkboxes
       select (not a plain applyKeyToParams key -- it needs gradeActiveMask context). */
    if (source == "gradeLum") {
        if (currentImagePath.isEmpty()) return;
        if (maskOverlayActive()) emit maskTintHideRequested();
        setGradeLum(v.toFloat());
        noteEdit("Color grade luminance", historyValueText(idx, v), "grade/lum");
        emit paramsChanged();
        return;
    }

    /* Write the changed adjustment into the CURRENT IMAGE's active-scope params, mark it
       dirty, and drive the live preview. Persistence to the sidecar happens on navigate-
       away / quit / pre-op (always) and, if G::isDevelopDebounceWrite, once edits settle. */
    if (!source.isEmpty()) {
        if (currentImagePath.isEmpty()) return;
        /* This slider modifies the masked pixels (not the mask itself). If a mask overlay
           is shown, hide its red coverage so the user sees the effect. Covers all Basic/
           Color/Effects sliders and any future adjustment key via applyKeyToParams. */
        if (maskOverlayActive()) emit maskTintHideRequested();
        applyKeyToParams(source, v, activeParams());
        /* History caption = the row's own caption ("Exposure"); the whole drag folds
           into one entry (mergeKey), so the list reads one step per slider. */
        noteEdit(idx.siblingAtColumn(0).data().toString(), historyValueText(idx, v),
                 historyScopeLabel() + "/" + source);
        emit paramsChanged();
    }
}

void DevelopProperties::applyKeyToParams(const QString &key, const QVariant &v, EditParams &p)
{
    /* Slider EditRole carries the real (div-scaled) value, e.g. exposure in EV, or 0..100 for the
       denoise sliders. Map the dock key onto the matching EditParams field. */
    const float f = v.toFloat();
    if      (key == "temp")       p.temp       = f;
    else if (key == "tint")       p.tint       = f;
    else if (key == "exposure")   p.exposure   = f;
    else if (key == "contrast")   p.contrast   = f;
    else if (key == "highlights") p.highlights = f;
    else if (key == "shadows")    p.shadows    = f;
    else if (key == "whites")     p.whites     = f;
    else if (key == "blacks")     p.blacks     = f;
    else if (key == "texture")    p.texture    = f;
    else if (key == "dehaze")     p.dehaze     = f;
    else if (key == "red")        p.red        = f;
    else if (key == "green")      p.green      = f;
    else if (key == "blue")       p.blue       = f;
    else if (key == "hue")        p.hue        = f;
    else if (key == "saturation") p.saturation = f;
    else if (key == "vibrance")   p.vibrance   = f;
    else if (key == "gradeBlending") p.gradeBlending = f;
    else if (key == "gradeBalance")  p.gradeBalance  = f;
    else if (key == "luminance")  p.luminance  = f;
    else if (key == "denoiseLuma")   p.denoiseLuma   = f / 100.0f;   // Global "Denoise raw" (0..100 -> 0..1)
    else if (key == "denoiseChroma") p.denoiseChroma = f / 100.0f;
    else if (key == "localDenoise")       p.localDenoiseLuma   = f / 100.0f;  // "Denoise"
    else if (key == "localDenoiseChroma") p.localDenoiseChroma = f / 100.0f;
    else if (key == "vignetteExposure")   p.vignetteExposure   = f;           // EV
    else if (key == "vignetteFeather")    p.vignetteFeather    = f / 100.0f;
    else if (key == "grainAmount")        p.grainAmount        = f / 100.0f;   // "Grain"
    else if (key == "grainSize")          p.grainSize          = f / 100.0f;
    else if (key == "grainRoughness")     p.grainRoughness     = f / 100.0f;
}

EditParams DevelopProperties::editParams()
{
    /* The renderer shows the ACTIVE scope (no mask/opacity compositing yet). */
    if (currentImagePath.isEmpty()) return EditParams();
    const EditStack s = previewActive ? previewStack : stackCache.value(currentImagePath);
    if (s.scopes.isEmpty()) return EditParams();
    const int idx = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size()) ? activeScopeIndex : 0;
    return effectiveScopeParams(s.scopes[idx]);   // fold previewed-off groups to identity
}

DevelopProperties::StackRenderJob DevelopProperties::stackJob()
{
    /* Capture the WHOLE stack as plain values (GUI thread; also handed to the off-thread full-res
       render). The render shows every enabled scope regardless of which one is active for editing,
       so a saved mask is visible the moment the image loads. */
    StackRenderJob job;
    if (currentImagePath.isEmpty()) return job;
    /* A hovered History row overrides what renders, without disturbing the stored stack
       or the sliders (see previewHistoryEntry). */
    EditStack s = previewActive ? previewStack : stackCache.value(currentImagePath);
    /* PREVIEW THE PENDING OP. The submask being defined carries op = Add in the stack
       until the commit button / Return commits it -- the real op is MOMENTARY, held in
       pendingOp by
       the modifier the user is pressing. The veil already composites with it
       (MW::updateMaskOverlayTint); the RENDER has to as well, or an Opt (Subtract) or
       Shift+Opt (Intersect) submask leaves the image showing that submask being ADDED --
       which over an already-masked area is no visible change at all. Substituted into a
       local copy, so nothing is written into the stored stack before the commit. */
    if (!previewActive && pendingIdx >= 0 && activeScopeIndex > 0 &&
        activeScopeIndex < s.scopes.size()) {
        EditScope &sc = s.scopes[activeScopeIndex];
        if (pendingIdx < sc.components.size()) sc.components[pendingIdx].op = pendingOp;
    }
    /* Transform Preview: previewed off -> bypass geometry at render (identity), while
       the stored crop/warp stays in the cache so the overlay still draws (see
       currentGeometry). */
    const bool useGeometry = s.geometry.show;
    /* Replace preview eye off -> render without the heals (spots stay in the cache). */
    return buildStackJob(s, useGeometry, spotsShown);
}

DevelopProperties::StackRenderJob DevelopProperties::stackJobFor(const QString &fPath)
{
/*
    The EXPORT twin of stackJob(): the stack for ANY image, read from that image's own
    stored EditStack rather than the dock's current one. See the header for why the
    History hover override and the two preview eyes are deliberately not consulted.
*/
    if (fPath.isEmpty()) return StackRenderJob();
    /* Always render the stored recipe in full: geometry on, spots on. */
    return buildStackJob(stackFor(fPath), true, true);
}

DevelopProperties::StackRenderJob
DevelopProperties::buildStackJob(const EditStack &s, bool useGeometry, bool useSpots) const
{
/*
    The shared body behind stackJob() (preview) and stackJobFor() (export). The two
    differ only in WHICH stack they hand in and whether the view toggles apply, so the
    compositing rules below live here once.
*/
    StackRenderJob job;
    job.geometry = useGeometry ? s.geometry : Geometry();
    if (useSpots) job.spots = s.spots;   // healed before geometry (Global-only too)
    if (s.scopes.isEmpty()) return job;
    /* Global (scope 0): apply its params (previewed groups folded off) when enabled; a
       disabled Global contributes identity params -- the image with no develop adjustments,
       matching how a disabled scope is skipped below. */
    job.global = s.scopes[0].enabled ? effectiveScopeParams(s.scopes[0]) : EditParams();
    for (int i = 1; i < s.scopes.size(); ++i) {
        const EditScope &l = s.scopes[i];
        if (!l.enabled) continue;
        const EditParams ep = effectiveScopeParams(l);
        if (ep.isIdentity()) continue;          // no (visible) adjustment -> nothing to composite
        StackRenderJob::Scope lj;
        lj.params  = ep;
        lj.combine = l.combine;
        for (const MaskComponent &m : l.components) {
            if (!m.enabled) continue;
            /* Content-derived AI masks (Select Subject/Sky/Background) carry NO paramsJson -- their
               coverage is the model's map, not stored geometry -- so the empty-params skip must not
               drop them (it still drops a geometry tool that has not been placed yet). */
            const bool contentDerived = (m.tool == int(MaskTool::Subject) ||
                                         m.tool == int(MaskTool::Sky) ||
                                         m.tool == int(MaskTool::Background));
            if (m.paramsJson.isEmpty() && !contentDerived) continue;
            lj.components.append(m);
        }
        job.scopes.append(lj);
    }
    return job;
}

Geometry DevelopProperties::currentGeometry() const
{
    if (currentImagePath.isEmpty()) return Geometry();
    return stackCache.value(currentImagePath).geometry;
}

void DevelopProperties::setCurrentGeometry(const Geometry &g)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());   // keep the stack shape
    /* Name the history entry for WHICH part of the transform moved, so a crop drag and a
       straighten read as separate steps (and each drag coalesces into one). */
    const Geometry &was = s.geometry;
    QString action, mergeKey, value;
    if (g.straighten != was.straighten) {
        action = "Straighten";
        mergeKey = "geom/straighten";
        value = QString::number(g.straighten, 'f', 1) + QChar(0x00B0);
    }
    else if (g.hasWarp != was.hasWarp ||
             !std::equal(std::begin(g.quad), std::end(g.quad), std::begin(was.quad))) {
        action = g.hasWarp ? "Warp" : "Remove warp";
        mergeKey = "geom/warp";
    }
    else if (g.show != was.show) {
        action = g.show ? "Show transform" : "Hide transform";
    }
    else if (!g.cropIsIdentity() || !was.cropIsIdentity()) {
        action = g.cropIsIdentity() ? "Remove crop" : "Crop";
        mergeKey = "geom/crop";
    }
    s.geometry = g;
    /* Whole-image, so no scope prefix. An identical re-set records nothing. */
    if (action.isEmpty()) {
        dirty.insert(currentImagePath);
        if (debounceWriteTimer) debounceWriteTimer->start(kDebounceWriteMs);
        return;
    }
    noteScopeEdit(QString(), action, value, mergeKey);
}

QStringList DevelopProperties::currentScopeNames() const
{
    QStringList names;
    if (!currentImagePath.isEmpty()) {
        const EditStack s = stackCache.value(currentImagePath);
        for (const EditScope &l : s.scopes) names << l.name;
    }
    if (names.isEmpty()) names << "Global";  // combo always has at least one entry
    return names;
}

EditParams &DevelopProperties::activeParams()
{
    /* Caller guarantees currentImagePath is non-empty. Ensures a scope exists and the index is
       in range, then returns the active scope's params for in-place editing. */
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    if (activeScopeIndex < 0 || activeScopeIndex >= s.scopes.size()) activeScopeIndex = 0;
    return s.scopes[activeScopeIndex].params;
}

void DevelopProperties::refreshScopeList()
{
    scopeList = currentScopeNames();
    if (activeScopeIndex < 0 || activeScopeIndex >= scopeList.size()) activeScopeIndex = 0;
    /* Push names + per-scope enabled + selection into the header (no signal). The
       dropdown header ignores enabled/isGlobal (default setScopeRows -> setScopes); the
       list panel (ScopeHeaderLab) uses them to build the per-row show/hide checkboxes. */
    if (scopeHeader) {
        QVector<ScopeHeaderBase::ScopeRowInfo> rows;
        rows.reserve(scopeList.size());
        const EditStack s = stackCache.value(currentImagePath);
        for (int i = 0; i < scopeList.size(); ++i) {
            ScopeHeaderBase::ScopeRowInfo r;
            r.name    = scopeList.at(i);
            r.isGlobal  = (i == 0);
            r.enabled = (i < s.scopes.size()) ? s.scopes.at(i).enabled : true;
            rows.append(r);
        }
        scopeHeader->setScopeRows(rows, activeScopeIndex);
    }
    updateMaskMenuBtn();        // hide [M] + disable rename/remove on the Global scope
    updateSectionHeaderCaptions();
}

void DevelopProperties::updateMaskMenuBtn()
{
    /* The Global scope (index 0) applies globally: no mask [M], and it cannot be renamed or removed. */
    if (scopeHeader) scopeHeader->setGlobalActive(activeScopeIndex == 0);
}

/* ----------------------------------------------------------------------------------------
   Per-image edit state (load / save / populate)
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::setCurrentImage(const QString &fPath)
{
    if (G::isLogger) G::log("DevelopProperties::setCurrentImage", fPath);
    if (fPath == currentImagePath) return;

    /* Leaving the image: drop an uncommitted (blue) mask preview so it is NOT saved (it's
       a real component in scope->components; a committed first tool stays). Must run BEFORE
       flushImage. Then hide the panel. */
    if (maskPanelOpen) {
        EditScope *l = activeScope();
        if (l && pendingIdx >= 0 && pendingIdx < l->components.size())
            l->components.removeAt(pendingIdx);
        pendingIdx = -1;
        maskPanelOpen = false;
    }
    /* Nothing is open on the new image, and the panel belongs to whatever scope turns out
       to be active there (buildTree -> syncMaskPanel decides). */
    selectedMaskIndex = -1;
    if (maskPanel) maskPanel->hide();
    maskLatched = false;         // a fresh image starts with no latched mask overlay
    previewActive = false;       // a History hover does not follow the image
    previewStack = EditStack();
    /* A queued multi-image batch belongs to the image being left; land it before the
       diff base moves to the new image. */
    flushPropagation();
    flushImage(currentImagePath);              // persist edits to the image being left
    cancelWbDropper();                  // an armed dropper does not follow the image
    currentImagePath = fPath;
    if (fPath.isEmpty()) {
        if (historyView) historyView->setImage(QString());
        return;
    }

    /* Load the stack on first touch (cheap: no sidecar => fast empty read), then cache
       it, with at least one scope so the combo + active editing have something to point
       at (stackFor). A sidecar is a user file and can be corrupt: fromBase64 range-checks
       what it can and reports what it could not, so a bad record degrades to "no edits"
       rather than feeding NaNs into the pipeline -- and the user is told, not silently
       emptied. An all-identity stack is still treated as identity (not persisted), so
       this never creates a sidecar for an unedited image. */
    EditStack &s = stackFor(fPath);
    activeScopeIndex = 0;
    selectedMaskIndex = -1;
    syncPropagateBase();     // multi-image edits are diffed from this image's state

    /* Baseline history entry for this image ("Original", or "Saved settings" when the
       sidecar already carried edits). Idempotent, so returning to an image keeps the
       steps taken earlier in this session. */
    if (history) history->seed(fPath, s);
    if (historyView) historyView->setImage(fPath);

    refreshScopeList();
    buildTree();                   // Global active: Core rows + Basic/Color/Effects
    if (spotMode) emitSpotPins();  // spot tool armed across a nav -> refresh pins
}

bool DevelopProperties::currentIsIdentity() const
{
    if (currentImagePath.isEmpty()) return true;
    return stackCache.value(currentImagePath).isIdentity();
}

/* --------------------------------------------------------------------------------
   Edit history (the History dock).  The sidecar stores only the CURRENT EditStack, so
   the timeline is built here: noteEdit snapshots the stack after every committed
   action, hover previews a snapshot, and a click restores one.
   -------------------------------------------------------------------------------- */

void DevelopProperties::bindHistoryView(HistoryView *view)
{
    if (G::isLogger) G::log("DevelopProperties::bindHistoryView");
    historyView = view;
    if (!historyView) return;
    historyView->bindHistory(history);
    historyView->setImage(currentImagePath);
    connect(historyView, &HistoryView::entryHovered,
            this, &DevelopProperties::previewHistoryEntry);
    connect(historyView, &HistoryView::hoverEnded,
            this, &DevelopProperties::endHistoryPreview);
    connect(historyView, &HistoryView::entryChosen,
            this, &DevelopProperties::applyHistoryEntry);
}

void DevelopProperties::noteEdit(const QString &action, const QString &value,
                                 const QString &mergeKey)
{
    noteScopeEdit(historyScopeLabel(), action, value, mergeKey);
}

void DevelopProperties::noteScopeEdit(const QString &scope, const QString &action,
                                      const QString &value, const QString &mergeKey)
{
    if (currentImagePath.isEmpty()) return;
    dirty.insert(currentImagePath);
    /* Do not record our own writes: isPopulating is pushing saved values into the
       editors, isRestoringHistory is a revert repopulating the panel. */
    if (history && !isPopulating && !isRestoringHistory && !action.isEmpty()) {
        history->record(currentImagePath, scope, action, value, mergeKey,
                        stackCache.value(currentImagePath));
    }
    /* Multi-image editing: whatever this commit changed in the GLOBAL scope is queued
       for the other selected images. Hooked here rather than at each edit site so it
       covers every develop action by construction -- present and future. */
    queuePropagation(action, value);
    if (G::isDevelopDebounceWrite && debounceWriteTimer)
        debounceWriteTimer->start(kDebounceWriteMs);
}

QString DevelopProperties::historyScopeLabel() const
{
    const EditStack s = stackCache.value(currentImagePath);
    if (activeScopeIndex < 0 || activeScopeIndex >= s.scopes.size())
        return QStringLiteral("Global");
    const EditScope &l = s.scopes.at(activeScopeIndex);
    /* An unrenamed mask scope ("Mask 2") reads better as what it actually selects, so
       an edit on a Subject mask logs as "Subject Mask: Hue". A user-chosen name wins. */
    static const QRegularExpression autoName("^Mask \\d+$");
    if (!l.components.isEmpty() && autoName.match(l.name).hasMatch())
        return maskToolName(l.components.first().tool);
    return l.name;
}

QString DevelopProperties::historyValueText(const QModelIndex &valIdx, const QVariant &v)
{
    if (v.typeId() == QMetaType::Bool) return v.toBool() ? "On" : "Off";
    /* Match what the slider itself displays (SliderEditor::change): integer when there
       is no divisor, else 2 dp -- with a leading + so the list reads like Lightroom. */
    bool ok = false;
    const double d = v.toDouble(&ok);
    if (!ok) return v.toString();
    const int div = valIdx.isValid() ? valIdx.data(UR_Div).toInt() : 0;
    QString s = (div == 0) ? QString::number(qRound(d)) : QString::number(d, 'f', 2);
    if (d > 0) s.prepend('+');
    return s;
}

void DevelopProperties::previewHistoryEntry(int index)
{
    if (G::isLogger) G::log("DevelopProperties::previewHistoryEntry");
    if (!history || currentImagePath.isEmpty()) return;
    /* The crop overlay and an open mask tool own the canvas; a hover preview underneath
       them would fight for it. */
    if (maskPanelOpen || spotMode) return;
    const HistoryEntry *e = history->at(currentImagePath, index);
    if (!e) return;
    if (index == history->pos(currentImagePath)) {   // already showing this state
        endHistoryPreview();
        return;
    }
    previewStack = e->stack;
    previewActive = true;
    emit historyPreviewChanged();
}

void DevelopProperties::endHistoryPreview()
{
    if (!previewActive) return;
    previewActive = false;
    previewStack = EditStack();
    emit historyPreviewChanged();
}

void DevelopProperties::applyHistoryEntry(int index)
{
    if (G::isLogger) G::log("DevelopProperties::applyHistoryEntry");
    if (!history || currentImagePath.isEmpty()) return;
    const HistoryEntry *e = history->at(currentImagePath, index);
    if (!e) return;
    /* History is per-image: a revert here must not un-edit the rest of the selection,
       but a batch queued BEFORE the revert was a real edit and still has to land. */
    flushPropagation();

    /* A half-built mask tool indexes into the scope we are about to replace, so retire it
       first (same teardown setCurrentImage does when leaving an image). */
    if (maskPanelOpen) {
        EditScope *l = activeScope();
        if (l && pendingIdx >= 0 && pendingIdx < l->components.size())
            l->components.removeAt(pendingIdx);
        pendingIdx = -1;
        maskPanelOpen = false;
    }
    /* Unconditional: a submask RE-OPENED for editing is not pending, so the guard above
       misses it, but its index still points into the scope about to be replaced. */
    selectedMaskIndex = -1;
    if (maskPanel) maskPanel->hide();
    maskLatched = false;
    previewActive = false;              // the previewed state is about to become real
    previewStack = EditStack();

    isRestoringHistory = true;
    EditStack s = e->stack;
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    s.scopes[0].name = "Global";
    stackCache[currentImagePath] = s;
    dirty.insert(currentImagePath);
    history->setPos(currentImagePath, index);
    if (activeScopeIndex >= s.scopes.size()) activeScopeIndex = 0;
    selectedMaskIndex = -1;

    refreshScopeList();
    buildTree();                        // scope/mask structure may differ from now
    if (spotMode) emitSpotPins();
    isRestoringHistory = false;
    syncPropagateBase();                // the restored state is the new diff baseline

    emit paramsChanged();               // full render + settled full-res
    if (G::isDevelopDebounceWrite && debounceWriteTimer)
        debounceWriteTimer->start(kDebounceWriteMs);
}

/* --------------------------------------------------------------------------------
   Develop presets (the Presets dock)

   A preset is a NAMED, PARTIAL develop recipe: the settings the user ticked in the
   Save Develop Preset checklist, captured from ONE scope of one image and stored in
   QSettings by DevelopPresets. Saving is Cmd+Shift+N / the dock context menu / the
   Presets dock [+]; applying is a click in the Presets dock (hover previews it).

   THE APPLY IS A MERGE. Only the keys the preset actually holds are written, into the
   scope that is ACTIVE at apply time -- so a Temp-only preset moves Temp and nothing
   else, and a preset captured from a mask can be dropped onto Global (or vice versa).
   The per-image items (raw decode engine, raw NR, tone splits, crop / level / warp,
   spots) belong to no scope and always go to scope 0 / the geometry.

   Mask geometry never travels: it is specific to the source image's content.
   -------------------------------------------------------------------------------- */

namespace {

struct PresetLeafDef { const char *key; const char *label; };

/* The adjustment leaves, grouped and ordered exactly like the Develop panel's Scope
   tree (addBasic / addColor / addColorGrade / addEffects). One table drives the dialog's
   groups, the pre-checked test and the value write, so the three cannot drift apart.
   A leaf that folds several params in (white balance, a grade range, denoise, vignette,
   grain) is ONE tick -- the panel treats them as one control. */
const PresetLeafDef kBasicLeaves[] = {
    {"whiteBalance", "White balance (Temp + Tint)"},
    {"exposure",     "Exposure"},
    {"contrast",     "Contrast"},
    {"highlights",   "Highlights"},
    {"shadows",      "Shadows"},
    {"whites",       "Whites"},
    {"blacks",       "Blacks"},
    {"texture",      "Texture"},
    {"dehaze",       "Dehaze"},
};
const PresetLeafDef kColorLeaves[] = {
    {"red",        "Red"},
    {"green",      "Green"},
    {"blue",       "Blue"},
    {"hue",        "Hue"},
    {"saturation", "Saturation"},
    {"luminance",  "Luminance"},
    {"vibrance",   "Vibrance"},
};
const PresetLeafDef kCalibrateLeaves[] = {
    {"calRed",   "Red primary"},
    {"calGreen", "Green primary"},
    {"calBlue",  "Blue primary"},
};
const PresetLeafDef kColorGradeLeaves[] = {
    {"gradeShadow", "Shadows"},
    {"gradeMid",    "Midtones"},
    {"gradeHigh",   "Highlights"},
    {"gradeGlobal", "Global"},
    {"gradeShape",  "Blending + Balance"},
};
const PresetLeafDef kEffectsLeaves[] = {
    {"denoise",  "Denoise"},
    {"vignette", "Vignette"},
    {"grain",    "Grain"},
};

struct PresetSectionDef {
    const char *title;
    const PresetLeafDef *leaves;
    int count;
};
const PresetSectionDef kSections[] = {
    {"Basic",     kBasicLeaves,    int(sizeof(kBasicLeaves)    / sizeof(PresetLeafDef))},
    {"Color",     kColorLeaves,    int(sizeof(kColorLeaves)    / sizeof(PresetLeafDef))},
    {"Calibrate", kCalibrateLeaves, int(sizeof(kCalibrateLeaves) / sizeof(PresetLeafDef))},
    {"Color Grade", kColorGradeLeaves, int(sizeof(kColorGradeLeaves) / sizeof(PresetLeafDef))},
    {"Effects",   kEffectsLeaves,  int(sizeof(kEffectsLeaves)  / sizeof(PresetLeafDef))},
};

/* Does this leaf differ from its default? A folded leaf tests its PRIMARY params, the
   same ones EditParams::isIdentity looks at (a feather or grain size on its own is not
   an edit -- it is inert until its amount is non-zero). */
bool leafChanged(const QString &key, const EditParams &p)
{
    const EditParams def;
    if (key == "whiteBalance") return p.temp != def.temp || p.tint != def.tint;
    if (key == "exposure")     return p.exposure   != def.exposure;
    if (key == "contrast")     return p.contrast   != def.contrast;
    if (key == "highlights")   return p.highlights != def.highlights;
    if (key == "shadows")      return p.shadows    != def.shadows;
    if (key == "whites")       return p.whites     != def.whites;
    if (key == "blacks")       return p.blacks     != def.blacks;
    if (key == "texture")      return p.texture    != def.texture;
    if (key == "dehaze")       return p.dehaze     != def.dehaze;
    if (key == "red")          return p.red        != def.red;
    if (key == "green")        return p.green      != def.green;
    if (key == "blue")         return p.blue       != def.blue;
    if (key == "hue")          return p.hue        != def.hue;
    if (key == "saturation")   return p.saturation != def.saturation;
    if (key == "luminance")    return p.luminance  != def.luminance;
    if (key == "vibrance")     return p.vibrance   != def.vibrance;
    if (key == "calRed")       return p.calRedHue   != def.calRedHue ||
                                      p.calRedSat   != def.calRedSat;
    if (key == "calGreen")     return p.calGreenHue != def.calGreenHue ||
                                      p.calGreenSat != def.calGreenSat;
    if (key == "calBlue")      return p.calBlueHue  != def.calBlueHue ||
                                      p.calBlueSat  != def.calBlueSat;
    if (key == "gradeShadow")  return p.gradeShadowSat != def.gradeShadowSat ||
                                      p.gradeShadowLum != def.gradeShadowLum;
    if (key == "gradeMid")     return p.gradeMidSat != def.gradeMidSat ||
                                      p.gradeMidLum != def.gradeMidLum;
    if (key == "gradeHigh")    return p.gradeHighSat != def.gradeHighSat ||
                                      p.gradeHighLum != def.gradeHighLum;
    if (key == "gradeGlobal")  return p.gradeGlobalSat != def.gradeGlobalSat ||
                                      p.gradeGlobalLum != def.gradeGlobalLum;
    if (key == "gradeShape")   return p.gradeBlending != def.gradeBlending ||
                                      p.gradeBalance  != def.gradeBalance;
    if (key == "denoise")      return p.localDenoiseLuma   != def.localDenoiseLuma ||
                                      p.localDenoiseChroma != def.localDenoiseChroma;
    if (key == "vignette")     return p.vignetteExposure != def.vignetteExposure;
    if (key == "grain")        return p.grainAmount      != def.grainAmount;
    return false;
}

}   // namespace

QSet<QString> DevelopProperties::changedLeavesForScope(const EditParams &p)
{
    QSet<QString> changed;
    for (const PresetSectionDef &sec : kSections)
        for (int i = 0; i < sec.count; ++i)
            if (leafChanged(sec.leaves[i].key, p)) changed.insert(sec.leaves[i].key);
    return changed;
}

void DevelopProperties::collectScopeLeaves(const EditParams &p, const QSet<QString> &lk,
                                           QVariantHash &out)
{
    /* Store RAW EditParams field values under their EditStack JSON key names (the source
       of truth, e.g. denoise 0..1), so applying is a straight assignment back --
       DevelopPresets::assignParam is the other half. */
    const struct { const char *leaf; const char *field; float v; } scal[] = {
        {"exposure",   "exposure",   p.exposure},
        {"contrast",   "contrast",   p.contrast},
        {"highlights", "highlights", p.highlights},
        {"shadows",    "shadows",    p.shadows},
        {"whites",     "whites",     p.whites},
        {"blacks",     "blacks",     p.blacks},
        {"texture",    "texture",    p.texture},
        {"dehaze",     "dehaze",     p.dehaze},
        {"red",        "red",        p.red},
        {"green",      "green",      p.green},
        {"blue",       "blue",       p.blue},
        {"hue",        "hue",        p.hue},
        {"saturation", "saturation", p.saturation},
        {"luminance",  "luminance",  p.luminance},
        {"vibrance",   "vibrance",   p.vibrance},
    };
    for (const auto &e : scal)
        if (lk.contains(e.leaf)) out.insert(e.field, e.v);

    if (lk.contains("whiteBalance")) {
        out.insert("temp", p.temp);
        out.insert("tint", p.tint);
        out.insert("wbPreset", p.wbPreset);
    }
    if (lk.contains("calRed")) {
        out.insert("calRedHue", p.calRedHue);
        out.insert("calRedSat", p.calRedSat);
    }
    if (lk.contains("calGreen")) {
        out.insert("calGreenHue", p.calGreenHue);
        out.insert("calGreenSat", p.calGreenSat);
    }
    if (lk.contains("calBlue")) {
        out.insert("calBlueHue", p.calBlueHue);
        out.insert("calBlueSat", p.calBlueSat);
    }
    if (lk.contains("gradeShadow")) {
        out.insert("gradeShadowHue", p.gradeShadowHue);
        out.insert("gradeShadowSat", p.gradeShadowSat);
        out.insert("gradeShadowLum", p.gradeShadowLum);
    }
    if (lk.contains("gradeMid")) {
        out.insert("gradeMidHue", p.gradeMidHue);
        out.insert("gradeMidSat", p.gradeMidSat);
        out.insert("gradeMidLum", p.gradeMidLum);
    }
    if (lk.contains("gradeHigh")) {
        out.insert("gradeHighHue", p.gradeHighHue);
        out.insert("gradeHighSat", p.gradeHighSat);
        out.insert("gradeHighLum", p.gradeHighLum);
    }
    if (lk.contains("gradeGlobal")) {
        out.insert("gradeGlobalHue", p.gradeGlobalHue);
        out.insert("gradeGlobalSat", p.gradeGlobalSat);
        out.insert("gradeGlobalLum", p.gradeGlobalLum);
    }
    if (lk.contains("gradeShape")) {
        out.insert("gradeBlending", p.gradeBlending);
        out.insert("gradeBalance", p.gradeBalance);
    }
    if (lk.contains("denoise")) {
        out.insert("localDenoiseLuma", p.localDenoiseLuma);
        out.insert("localDenoiseChroma", p.localDenoiseChroma);
    }
    if (lk.contains("vignette")) {
        out.insert("vignetteExposure", p.vignetteExposure);
        out.insert("vignetteFeather", p.vignetteFeather);
    }
    if (lk.contains("grain")) {
        out.insert("grainAmount", p.grainAmount);
        out.insert("grainSize", p.grainSize);
        out.insert("grainRoughness", p.grainRoughness);
    }
}

DevelopPreset DevelopProperties::buildPreset(const QString &name, int srcScope,
                                             const QHash<QString, QSet<QString>> &selected,
                                             const EditStack &s) const
{
    DevelopPreset preset;
    preset.name = name;
    const QStringList names = currentScopeNames();
    preset.sourceScope = names.value(srcScope, QStringLiteral("Global"));
    preset.sourceImage = QFileInfo(currentImagePath).fileName();   // display only

    /* Per-image items. The raw NR pair and the tone splits live in the Global scope by
       definition, so they are read from scope 0 whatever the source scope is. */
    const EditParams base = s.scopes.isEmpty() ? EditParams() : s.scopes.first().params;
    const QSet<QString> gk = selected.value("Global settings");
    if (gk.contains("demosaic")) {
        const bool apple =
            G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine;
        preset.globals.insert("demosaicEngine", QString(apple ? "apple" : "winnow"));
    }
    if (gk.contains("rawNoise")) {
        preset.globals.insert("denoiseLuma", base.denoiseLuma);
        preset.globals.insert("denoiseChroma", base.denoiseChroma);
    }
    if (gk.contains("histogram")) {
        preset.globals.insert("toneShadowCenter", base.toneShadowCenter);
        preset.globals.insert("toneCrossover", base.toneCrossover);
        preset.globals.insert("toneHighlightCenter", base.toneHighlightCenter);
    }
    if (gk.contains("crop")) {
        const Geometry &g = s.geometry;
        preset.globals.insert("crop", QStringLiteral("%1,%2,%3,%4")
            .arg(g.cropX).arg(g.cropY).arg(g.cropW).arg(g.cropH));
    }
    if (gk.contains("level"))
        preset.globals.insert("straighten", s.geometry.straighten);
    if (gk.contains("warp") && s.geometry.hasWarp) {
        QStringList q;
        for (double v : s.geometry.quad) q << QString::number(v);
        preset.globals.insert("warp", q.join(','));
    }
    if (gk.contains("spots")) preset.spots = s.spots;

    /* The adjustments, from the chosen scope. */
    if (srcScope >= 0 && srcScope < s.scopes.size()) {
        QSet<QString> lk;
        for (const PresetSectionDef &sec : kSections)
            lk.unite(selected.value(QString::fromLatin1(sec.title)));
        collectScopeLeaves(s.scopes[srcScope].params, lk, preset.params);
    }
    return preset;
}

QVector<PresetGroup> DevelopProperties::buildChecklistGroups(const EditStack &s) const
{
    const EditParams def;
    const Geometry gdef;
    const EditParams base = s.scopes.isEmpty() ? def : s.scopes.first().params;

    QVector<PresetGroup> groups;

    /* Global settings: a non-checkable container, and PER IMAGE -- it does not follow the
       dialog's Scope combo. Raw NR + tone splits come from the Global scope; crop / level
       / warp / spots from the per-image Geometry + spot history. */
    /* Raw demosaic + raw denoise only apply to a raw file, and denoise (PMRID) is the
       Winnow engine only. Both leaves stay listed, but pre-check them only when relevant:
       demosaic when the file is raw; raw NR when it is raw AND not decoded by Apple. */
    const bool isRaw = currentIsRaw();
    const bool usingApple =
        G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine;
    const bool rawNoiseChanged =
        base.denoiseLuma != def.denoiseLuma || base.denoiseChroma != def.denoiseChroma;

    PresetGroup gg;
    gg.title = "Global settings";
    gg.checkable = false;
    gg.showAllNone = false;
    gg.leaves.append({"demosaic", "Raw demosaic", isRaw});
    gg.leaves.append({"rawNoise", "Raw noise reduction",
        isRaw && !usingApple && rawNoiseChanged});
    gg.leaves.append({"histogram", "Histogram tonal ranges",
        base.toneShadowCenter != def.toneShadowCenter ||
        base.toneCrossover != def.toneCrossover ||
        base.toneHighlightCenter != def.toneHighlightCenter});
    gg.leaves.append({"crop", "Crop", !s.geometry.cropIsIdentity()});
    gg.leaves.append({"level", "Level", s.geometry.straighten != gdef.straighten});
    gg.leaves.append({"warp", "Warp", s.geometry.hasWarp});
    gg.leaves.append({"spots", "Spots", !s.spots.isEmpty()});
    groups.append(gg);

    /* One group per Develop section. The leaves are the same in every scope, so the
       dialog switches scopes by re-checking them from changedPerScope. */
    for (const PresetSectionDef &sec : kSections) {
        PresetGroup g;
        g.title = QString::fromLatin1(sec.title);
        g.checkable = true;
        g.showAllNone = true;
        for (int i = 0; i < sec.count; ++i)
            g.leaves.append({sec.leaves[i].key, sec.leaves[i].label, false});
        groups.append(g);
    }
    return groups;
}

void DevelopProperties::saveDevelopPreset()
{
    if (G::isLogger) G::log("DevelopProperties::saveDevelopPreset");
    if (currentImagePath.isEmpty()) {
        if (G::popup) G::popup->showPopup("No image to save a develop preset from.");
        return;
    }
    const EditStack s = stackCache.value(currentImagePath);
    if (s.isIdentity()) {
        if (G::popup) G::popup->showPopup("No develop edits to save as a preset.");
        return;
    }

    QVector<QSet<QString>> changedPerScope;
    for (const EditScope &l : s.scopes)
        changedPerScope.append(changedLeavesForScope(l.params));

    SaveDevelopPresetDlg dlg(buildChecklistGroups(s), currentScopeNames(), changedPerScope,
                             activeScopeIndex, presets->names(),
                             SaveDevelopPresetDlg::Mode::SavePreset, mw);
    if (dlg.exec() != QDialog::Accepted) return;

    presets->write(buildPreset(dlg.presetName(), dlg.scopeIndex(), dlg.selected(), s));
    if (G::popup) G::popup->showPopup("Saved develop preset \"" + dlg.presetName() + "\".");
}

/* ---- Applying a preset ------------------------------------------------------------ */

int DevelopProperties::targetScopeIndex(const EditStack &s) const
{
    if (s.scopes.isEmpty()) return -1;
    return (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size())
           ? activeScopeIndex : 0;
}

EditStack DevelopProperties::mergePreset(const DevelopPreset &preset, EditStack s,
                                         int target) const
{
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    if (target < 0 || target >= s.scopes.size()) target = 0;

    /* The adjustments land on the target scope. */
    for (auto it = preset.params.constBegin(); it != preset.params.constEnd(); ++it)
        DevelopPresets::assignParam(it.key(), it.value(), s.scopes[target].params);

    /* The per-image items belong to no scope: the raw NR pair and the tone splits are
       Global (scope 0) by definition, the rest is geometry. */
    for (auto it = preset.globals.constBegin(); it != preset.globals.constEnd(); ++it) {
        const QString &k = it.key();
        if (k == "crop") {
            const QStringList v = it.value().toString().split(',');
            if (v.size() == 4) {
                s.geometry.cropX = v[0].toDouble();
                s.geometry.cropY = v[1].toDouble();
                s.geometry.cropW = v[2].toDouble();
                s.geometry.cropH = v[3].toDouble();
            }
        }
        else if (k == "straighten") {
            s.geometry.straighten = it.value().toDouble();
        }
        else if (k == "warp") {
            const QStringList v = it.value().toString().split(',');
            if (v.size() == 8) {
                for (int i = 0; i < 8; ++i) s.geometry.quad[i] = v[i].toDouble();
                s.geometry.hasWarp = true;
            }
        }
        else if (k != "demosaicEngine") {   // the decode engine is not part of the stack
            DevelopPresets::assignParam(k, it.value(), s.scopes[0].params);
        }
    }

    /* Spots heal in order, each over the accumulation of the prior ones, so a preset's
       spots are APPENDED rather than replacing what the image already has. */
    s.spots += preset.spots;
    /* Preset values are QVariants from QSettings and reach EditParams as a bare toFloat()
       (DevelopPresets::assignParam), so a hand-edited or damaged store can inject values
       no slider could produce. Range-check the MERGED stack for the same reason the
       sidecar load does -- this covers the hover preview as well as the click. */
    EditStack::sanitize(s);
    return s;
}

void DevelopProperties::bindPresetsView(PresetsView *view)
{
    if (G::isLogger) G::log("DevelopProperties::bindPresetsView");
    presetsView = view;
    if (!presetsView) return;
    presetsView->bindPresets(presets);
    connect(presetsView, &PresetsView::presetHovered,
            this, &DevelopProperties::previewPreset);
    connect(presetsView, &PresetsView::hoverEnded,
            this, &DevelopProperties::endPresetPreview);
    connect(presetsView, &PresetsView::presetChosen,
            this, &DevelopProperties::applyPreset);
    connect(presetsView, &PresetsView::newPresetRequested,
            this, &DevelopProperties::saveDevelopPreset);
    connect(presetsView, &PresetsView::updateRequested,
            this, &DevelopProperties::updatePresetFromCurrent);
    connect(presetsView, &PresetsView::renameRequested,
            this, &DevelopProperties::renamePreset);
    connect(presetsView, &PresetsView::deleteRequested,
            this, &DevelopProperties::deletePreset);
}

void DevelopProperties::previewPreset(const QString &name)
{
    if (G::isLogger) G::log("DevelopProperties::previewPreset", name);
    if (!presets || currentImagePath.isEmpty()) return;
    /* The crop overlay and an open mask tool own the canvas; a hover preview underneath
       them would fight for it (same guard as previewHistoryEntry). */
    if (maskPanelOpen || spotMode) return;
    const EditStack s = stackCache.value(currentImagePath);
    /* NOT previewed: the raw decode engine. Switching it forces a full raw re-decode,
       which a passing cursor must never trigger -- it is applied on click only. */
    previewStack = mergePreset(presets->read(name), s, targetScopeIndex(s));
    previewActive = true;
    emit historyPreviewChanged();       // proxy render only, no full-res settle
}

void DevelopProperties::endPresetPreview()
{
    endHistoryPreview();                // one preview override, one way to end it
}

void DevelopProperties::applyPreset(const QString &name)
{
    if (G::isLogger) G::log("DevelopProperties::applyPreset", name);
    if (!presets) return;
    if (currentImagePath.isEmpty()) {
        if (G::popup) G::popup->showPopup("No image to apply a develop preset to.");
        return;
    }
    const int n = applyPresetObject(presets->read(name), "Preset: " + name,
                                    "Preset \"" + name + "\"");
    /* Silent on a single image (the sliders move, which is confirmation enough), but a
       multi-image selection must say what it just did to the images off-screen. */
    if (n > 1 && G::popup)
        G::popup->showPopup("Applied preset \"" + name + "\" to " +
                            QString::number(n) + " images.");
}

int DevelopProperties::applyPresetObject(const DevelopPreset &preset, const QString &label,
                                         const QString &source)
{
/*
    Merge a preset-shaped object into the current image for real. Shared by applyPreset
    (a named preset from the Presets dock) and pasteDevelopSettings (the unnamed develop
    clipboard) -- the two differ only in where the object came from and how the history
    step and any warning are worded.

    With more than one image selected it lands on the whole selection: the current image
    here, every other selected image via propagatePreset. Returns how many images were
    written.
*/
    /* Whatever the sliders had queued for the selection belongs BEFORE this preset. */
    flushPropagation();
    /* A preset stamped newer than this build's schema may hold keys whose MEANING has
       since changed; DevelopPresets::migrate passes it through untouched and assignParam
       drops what it does not know, so it still applies -- just not necessarily in full.
       Warned here rather than in read(), which also runs on hover-preview. */
    if (preset.version > DevelopPresets::kVersion)
        warnOnce("newerPreset",
                 source + " was saved by a newer version of Winnow.\n"
                 "Settings this version does not understand were skipped.");

    /* A half-built mask tool indexes into the scope we are about to rewrite, so retire it
       first (the same teardown applyHistoryEntry does). */
    if (maskPanelOpen) {
        EditScope *l = activeScope();
        if (l && pendingIdx >= 0 && pendingIdx < l->components.size())
            l->components.removeAt(pendingIdx);
        pendingIdx = -1;
        maskPanelOpen = false;
    }
    /* Unconditional: a submask RE-OPENED for editing is not pending, so the guard above
       misses it, but its index still points into the scope about to be replaced. */
    selectedMaskIndex = -1;
    if (maskPanel) maskPanel->hide();
    maskLatched = false;
    previewActive = false;              // the previewed state is about to become real
    previewStack = EditStack();

    isRestoringHistory = true;          // suppress recording while the panel repopulates
    EditStack s = stackCache.value(currentImagePath);
    s = mergePreset(preset, s, targetScopeIndex(s));
    stackCache[currentImagePath] = s;
    if (activeScopeIndex >= s.scopes.size()) activeScopeIndex = 0;

    refreshScopeList();
    buildTree();                        // repopulates the sliders from the new stack
    if (spotMode) emitSpotPins();
    isRestoringHistory = false;

    /* ONE history step, labelled with the scope that received it ("Sky: Preset: Warm",
       "Sky: Paste settings"). Also marks the sidecar dirty and arms the write.
       propagateSuspended: the other selected images get the WHOLE preset object below,
       not the scope-0 field diff this commit would otherwise queue. */
    propagateSuspended = true;
    noteScopeEdit(historyScopeLabel(), label);
    propagateSuspended = false;

    const int nImages = 1 + propagatePreset(preset, label);

    /* The decode engine is not part of the EditStack: MW owns it and must re-decode.
       The stored token is matched EXPLICITLY against the two engines rather than testing
       for "apple" and treating everything else as winnow -- otherwise a preset naming an
       engine this build does not have (a typo, a newer Winnow, a hand-edited settings
       file) would silently decode with the other one and the user would wonder why the
       preset looks different. */
    if (preset.globals.contains("demosaicEngine")) {
        const QString eng = preset.globals.value("demosaicEngine").toString().toLower();
        if (eng == "apple" || eng == "winnow")
            emit demosaicEngineChanged(eng == "apple");
        else
            warnOnce("unknownEngine",
                     source + " names an unknown raw decoder (\"" + eng +
                     "\").\nThe current decoder was kept.");
    }

    emit paramsChanged();               // full render + settled full-res
    return nImages;
}

/* ---- Copy / paste develop settings ------------------------------------------------ */

void DevelopProperties::copyDevelopSettings()
{
/*
    Lightroom's "Copy Settings" (Cmd+Opt+C). The same checklist as Save Preset, minus the
    name: what the user ticks is captured from the chosen scope into the develop
    clipboard, replacing whatever was there. Nothing is applied to any image here.
*/
    if (G::isLogger) G::log("DevelopProperties::copyDevelopSettings");
    if (!presets) return;
    if (currentImagePath.isEmpty()) {
        if (G::popup) G::popup->showPopup("No image to copy develop settings from.");
        return;
    }
    const EditStack s = stackCache.value(currentImagePath);
    if (s.isIdentity()) {
        if (G::popup) G::popup->showPopup("This image has no develop settings to copy.");
        return;
    }

    QVector<QSet<QString>> changedPerScope;
    for (const EditScope &l : s.scopes)
        changedPerScope.append(changedLeavesForScope(l.params));

    SaveDevelopPresetDlg dlg(buildChecklistGroups(s), currentScopeNames(), changedPerScope,
                             activeScopeIndex, QStringList(),
                             SaveDevelopPresetDlg::Mode::CopySettings, mw);
    if (dlg.exec() != QDialog::Accepted) return;

    /* buildPreset needs a name (it is the QSettings group); writeClipboard supplies the
       fixed slot name, so what is passed here is only a label. */
    const DevelopPreset buf =
        buildPreset("buffer", dlg.scopeIndex(), dlg.selected(), s);
    presets->writeClipboard(buf);
    if (G::popup) {
        G::popup->showPopup(buf.isEmpty()
            ? "Nothing was ticked, so the develop clipboard is now empty."
            : "Copied develop settings from " + buf.sourceImage + ".");
    }
}

void DevelopProperties::pasteDevelopSettings()
{
/*
    Lightroom's "Paste Settings" (Cmd+Opt+V). Merges the develop clipboard into the
    CURRENT image exactly as applying a preset does: only the copied settings are
    written, they land on the ACTIVE scope, and it is one undoable history step.
*/
    if (G::isLogger) G::log("DevelopProperties::pasteDevelopSettings");
    if (!presets) return;
    if (currentImagePath.isEmpty()) {
        if (G::popup) G::popup->showPopup("No image to paste develop settings onto.");
        return;
    }
    const DevelopPreset buf = presets->readClipboard();
    if (buf.isEmpty()) {
        if (G::popup)
            G::popup->showPopup("No develop settings have been copied yet.  "
                                "Use Copy Develop Settings first.");
        return;
    }
    const int n = applyPresetObject(buf, "Paste settings", "The copied develop settings");
    if (G::popup) {
        const QString from = buf.sourceImage.isEmpty()
                             ? QString() : " from " + buf.sourceImage;
        /* Say how many images were pasted onto: with a multi-image selection the paste
           reaches all of them, and silence about that is how a user overwrites 40
           images without noticing. */
        const QString onto = n > 1 ? " onto " + QString::number(n) + " images" : QString();
        G::popup->showPopup("Pasted develop settings" + from + onto + ".");
    }
}

bool DevelopProperties::hasCopiedSettings() const
{
    return presets && presets->hasClipboard();
}

QString DevelopProperties::copiedSettingsSource() const
{
    return presets ? presets->readClipboard().sourceImage : QString();
}

void DevelopProperties::updatePresetFromCurrent(const QString &name)
{
/*
    "Update with Current Settings": keep the preset's SHAPE (the same ticked settings)
    but refill its values from the current image's active scope. Anything the preset
    does not hold stays absent, so updating never silently widens a preset.
*/
    if (G::isLogger) G::log("DevelopProperties::updatePresetFromCurrent", name);
    if (!presets) return;
    if (currentImagePath.isEmpty()) {
        if (G::popup) G::popup->showPopup("No image to update the preset from.");
        return;
    }
    DevelopPreset preset = presets->read(name);
    const EditStack s = stackCache.value(currentImagePath);
    const int src = targetScopeIndex(s);
    if (src < 0) return;
    /* Serialize once: paramsToJson is the same key -> value mapping the preset stores,
       so refilling is a lookup rather than another field-by-field switch. */
    const QJsonObject sj = EditStack::paramsToJson(s.scopes[src].params);
    const QJsonObject bj = EditStack::paramsToJson(s.scopes.first().params);

    for (auto it = preset.params.begin(); it != preset.params.end(); ++it)
        it.value() = sj.value(it.key()).toVariant();

    for (auto it = preset.globals.begin(); it != preset.globals.end(); ++it) {
        const QString &k = it.key();
        if (k == "demosaicEngine") {
            const bool apple =
                G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine;
            it.value() = QString(apple ? "apple" : "winnow");
        }
        else if (k == "crop") {
            const Geometry &g = s.geometry;
            it.value() = QStringLiteral("%1,%2,%3,%4")
                .arg(g.cropX).arg(g.cropY).arg(g.cropW).arg(g.cropH);
        }
        else if (k == "straighten") {
            it.value() = s.geometry.straighten;
        }
        else if (k == "warp") {
            QStringList q;
            for (double v : s.geometry.quad) q << QString::number(v);
            it.value() = q.join(',');
        }
        else {
            it.value() = bj.value(k).toVariant();
        }
    }
    if (!preset.spots.isEmpty()) preset.spots = s.spots;
    preset.sourceScope = currentScopeNames().value(src, QStringLiteral("Global"));

    presets->write(preset);
    if (G::popup) G::popup->showPopup("Updated develop preset \"" + name + "\".");
}

void DevelopProperties::renamePreset(const QString &from, const QString &to)
{
    if (G::isLogger) G::log("DevelopProperties::renamePreset", from);
    if (!presets) return;
    if (!presets->rename(from, to) && G::popup)
        G::popup->showPopup("A preset named \"" + to + "\" already exists.");
}

void DevelopProperties::deletePreset(const QString &name)
{
    if (G::isLogger) G::log("DevelopProperties::deletePreset", name);
    if (presets) presets->remove(name);
}

/* --------------------------------------------------------------------------------
   Reporting corrupt / unreadable develop settings.

   A sidecar is read on EVERY image the user visits, so a folder of damaged files must
   not produce a popup per image -- arrow-keying through 50 of them would bury the app in
   toasts. The policy is therefore WARN ONCE PER ISSUE KIND PER SESSION: the first
   occurrence pops up naming the file and says it may affect others, and every occurrence
   (first or not) goes to the log. warnedIssues holds the kinds already reported.

   What warrants a popup at all: anything the user would otherwise discover as missing or
   wrong work -- settings that could not be read, a mask that will not be drawn, values
   that had to be clamped. What does NOT: an absent sidecar (the normal state of an
   unedited image) and unknown KEYS from a newer build, which forward tolerance is
   designed to ignore. See notes/Documentation.txt "Corrupted or changed develop
   settings".
   -------------------------------------------------------------------------------- */

void DevelopProperties::warnOnce(const QString &kind, const QString &msg)
{
    if (G::isLogger) G::log("DevelopProperties::warnOnce", kind + ": " + msg);
    if (warnedIssues.contains(kind)) return;      // already told them this session
    warnedIssues.insert(kind);
    if (G::popup) G::popup->showPopup(msg, 4000);
}

void DevelopProperties::reportStackIssues(const QString &fPath, int status,
                                          const QStringList &issues)
{
    const QString file = QFileInfo(fPath).fileName();

    if (status == int(EditStack::Status::Corrupt)) {
        /* The stack is now empty, but the DAMAGED SIDECAR IS STILL ON DISK. We do not
           delete or rewrite it here: the user may be able to recover it, and a silent
           overwrite is exactly the failure this whole path exists to prevent. It is
           replaced only if the user edits this image (flushImage), which is an explicit
           act -- so say so plainly. */
        warnOnce("corruptSidecar",
                 "Develop settings for \"" + file + "\" could not be read and have been "
                 "ignored.\nThe file is unchanged until you edit this image; editing it "
                 "will replace those settings.\n(Other images may be affected too -- see "
                 "the log.)");
        return;
    }

    /* Repairs: the stack loaded, but something in it was not renderable as stored. */
    for (const QString &issue : issues) {
        if (issue.startsWith("Written by a newer"))
            warnOnce("newerFormat",
                     "\"" + file + "\" was developed in a newer version of Winnow.\n"
                     "Settings this version does not understand were ignored.");
        else if (issue.contains("unknown tool"))
            warnOnce("unknownMaskTool",
                     "One or more masks on \"" + file + "\" use a tool this version does "
                     "not have, and are not being applied.");
        else
            warnOnce("outOfRange",
                     "Some develop settings for \"" + file + "\" were outside their "
                     "valid range and have been limited.");
    }
}

void DevelopProperties::flushImage(const QString &fPath)
{
    if (fPath.isEmpty() || !dirty.contains(fPath)) return;
    dirty.remove(fPath);
    const EditStack &s = stackCache[fPath];
    /* Identity => write "" so writeDevelopSidecar clears the attribute (or skips if no sidecar),
       rather than persisting an empty edit. */
    const QString blob = s.isIdentity() ? QString() : s.toBase64();
    Metadata::writeDevelopSidecar(fPath, blob);   // synchronous; sidecar is a few KB, never on a drag
}

void DevelopProperties::flushAll()
{
    flushPropagation();      // a queued batch is an edit; it must reach the sidecars too
    const QList<QString> paths = dirty.values();
    for (const QString &p : paths) flushImage(p);
}

/* --------------------------------------------------------------------------------
   Multi-image editing (Lightroom's Auto Sync)

   With more than one image selected, a Global adjustment made on the current image is
   copied to every other selected image. See the "Multi-image editing" block in
   developproperties.h for the rules; this is the machinery.
   -------------------------------------------------------------------------------- */

namespace {

/* Every propagatable EditParams field, as a member pointer. ONE table drives both the
   diff and the copy, so the two can never disagree about what an adjustment is -- and
   adding an adjustment to EditParams means adding exactly one row here for multi-image
   editing to pick it up (see the checklist in notes/Documentation.txt).

   version is deliberately absent: it describes the struct, not an adjustment. */
struct FloatField { const char *name; float EditParams::*m; };
const FloatField kFloatFields[] = {
    {"temp",                &EditParams::temp},
    {"tint",                &EditParams::tint},
    {"exposure",            &EditParams::exposure},
    {"contrast",            &EditParams::contrast},
    {"highlights",          &EditParams::highlights},
    {"shadows",             &EditParams::shadows},
    {"whites",              &EditParams::whites},
    {"blacks",              &EditParams::blacks},
    {"toneShadowCenter",    &EditParams::toneShadowCenter},
    {"toneCrossover",       &EditParams::toneCrossover},
    {"toneHighlightCenter", &EditParams::toneHighlightCenter},
    {"texture",             &EditParams::texture},
    {"dehaze",              &EditParams::dehaze},
    {"red",                 &EditParams::red},
    {"green",               &EditParams::green},
    {"blue",                &EditParams::blue},
    {"hue",                 &EditParams::hue},
    {"saturation",          &EditParams::saturation},
    {"vibrance",            &EditParams::vibrance},
    {"luminance",           &EditParams::luminance},
    {"calRedHue",           &EditParams::calRedHue},
    {"calRedSat",           &EditParams::calRedSat},
    {"calGreenHue",         &EditParams::calGreenHue},
    {"calGreenSat",         &EditParams::calGreenSat},
    {"calBlueHue",          &EditParams::calBlueHue},
    {"calBlueSat",          &EditParams::calBlueSat},
    {"gradeShadowHue",      &EditParams::gradeShadowHue},
    {"gradeShadowSat",      &EditParams::gradeShadowSat},
    {"gradeShadowLum",      &EditParams::gradeShadowLum},
    {"gradeMidHue",         &EditParams::gradeMidHue},
    {"gradeMidSat",         &EditParams::gradeMidSat},
    {"gradeMidLum",         &EditParams::gradeMidLum},
    {"gradeHighHue",        &EditParams::gradeHighHue},
    {"gradeHighSat",        &EditParams::gradeHighSat},
    {"gradeHighLum",        &EditParams::gradeHighLum},
    {"gradeGlobalHue",      &EditParams::gradeGlobalHue},
    {"gradeGlobalSat",      &EditParams::gradeGlobalSat},
    {"gradeGlobalLum",      &EditParams::gradeGlobalLum},
    {"gradeBlending",       &EditParams::gradeBlending},
    {"gradeBalance",        &EditParams::gradeBalance},
    {"denoiseLuma",         &EditParams::denoiseLuma},
    {"denoiseChroma",       &EditParams::denoiseChroma},
    {"localDenoiseLuma",    &EditParams::localDenoiseLuma},
    {"localDenoiseChroma",  &EditParams::localDenoiseChroma},
    {"vignetteExposure",    &EditParams::vignetteExposure},
    {"vignetteFeather",     &EditParams::vignetteFeather},
    {"grainAmount",         &EditParams::grainAmount},
    {"grainSize",           &EditParams::grainSize},
    {"grainRoughness",      &EditParams::grainRoughness},
};
struct IntField { const char *name; int EditParams::*m; };
const IntField kIntFields[] = {
    {"wbPreset", &EditParams::wbPreset},
};

}   // namespace

QSet<QString> DevelopProperties::diffParamFields(const EditParams &a, const EditParams &b)
{
    QSet<QString> changed;
    for (const FloatField &f : kFloatFields)
        if (a.*(f.m) != b.*(f.m)) changed.insert(QString::fromLatin1(f.name));
    for (const IntField &f : kIntFields)
        if (a.*(f.m) != b.*(f.m)) changed.insert(QString::fromLatin1(f.name));
    return changed;
}

void DevelopProperties::copyParamFields(const EditParams &src, EditParams &dst,
                                        const QSet<QString> &fields)
{
    for (const FloatField &f : kFloatFields)
        if (fields.contains(QString::fromLatin1(f.name))) dst.*(f.m) = src.*(f.m);
    for (const IntField &f : kIntFields)
        if (fields.contains(QString::fromLatin1(f.name))) dst.*(f.m) = src.*(f.m);
}

EditStack &DevelopProperties::stackFor(const QString &fPath)
{
    if (!stackCache.contains(fPath)) {
        EditStack::Status st = EditStack::Status::Absent;
        QStringList issues;
        stackCache.insert(fPath, EditStack::fromBase64(
                              Metadata::readDevelopSidecar(fPath), &st, &issues));
        reportStackIssues(fPath, int(st), issues);
    }
    EditStack &s = stackCache[fPath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    s.scopes[0].name = "Global";      // index 0 is the (un-removable) Global scope
    return s;
}

QStringList DevelopProperties::otherSelectedPaths() const
{
    QStringList paths;
    if (!mw || !mw->dm || !mw->dm->selectionModel) return paths;
    const QModelIndexList rows = mw->dm->selectionModel->selectedRows();
    if (rows.count() < 2) return paths;          // nothing to fan out to
    for (const QModelIndex &idx : rows) {
        /* A video has no develop recipe; writing one would create a sidecar the
           pipeline never reads. */
        if (idx.siblingAtColumn(G::VideoColumn).data().toBool()) continue;
        const QString p = idx.data(G::PathRole).toString();
        if (p.isEmpty() || p == currentImagePath) continue;
        paths << p;
    }
    return paths;
}

int DevelopProperties::selectedEditCount() const
{
    if (currentImagePath.isEmpty()) return 0;
    return otherSelectedPaths().count() + 1;     // the current image is always included
}

void DevelopProperties::syncPropagateBase()
{
    const EditStack s = stackCache.value(currentImagePath);
    propagateBase = s.scopes.isEmpty() ? EditParams() : s.scopes.at(0).params;
}

void DevelopProperties::queuePropagation(const QString &action, const QString &value)
{
    if (currentImagePath.isEmpty()) return;
    const EditStack s = stackCache.value(currentImagePath);
    if (s.scopes.isEmpty()) return;
    const EditParams &now = s.scopes.at(0).params;

    /* Our own writes are not user edits: a repopulate or a history restore adopts the
       new state as the diff base so the NEXT real edit measures from it, and nothing is
       queued. Same for the whole-object paths (paste / preset), which propagate the
       merged object themselves. */
    if (propagateSuspended || isPopulating || isRestoringHistory) {
        propagateBase = now;
        return;
    }

    const QSet<QString> changed = diffParamFields(propagateBase, now);
    propagateBase = now;                 // consumed either way: it is the new baseline
    if (changed.isEmpty()) return;       // a mask/spot/geometry edit -- stays per-image

    /* The batch belongs to the selection that was live when it OPENED, so a selection
       change mid-drag cannot redirect edits the user has already made -- and a drag
       (which commits continuously) reads the selection once, not once per tick. */
    if (pendingFields.isEmpty()) {
        propagateTargets = otherSelectedPaths();
        if (propagateTargets.isEmpty()) return;   // single image: nothing to fan out to
    }
    pendingFields += changed;
    propagateAction = action;
    propagateValue  = value;
    if (propagateTimer) propagateTimer->start(kPropagateMs);
}

void DevelopProperties::flushPropagation()
{
    if (propagateTimer) propagateTimer->stop();
    if (pendingFields.isEmpty() || propagateTargets.isEmpty()) {
        pendingFields.clear();
        propagateTargets.clear();
        return;
    }
    const QSet<QString> fields = pendingFields;
    const QStringList targets = propagateTargets;
    pendingFields.clear();
    propagateTargets.clear();

    const EditStack cur = stackCache.value(currentImagePath);
    if (cur.scopes.isEmpty()) return;
    const EditParams src = cur.scopes.at(0).params;

    if (G::isLogger) G::log("DevelopProperties::flushPropagation",
                            QString::number(targets.count()) + " images");

    /* One history entry per target per settled gesture, coalesced on the field set: a
       long Exposure drag reads as one "Exposure" step on every image, not one per tick.
       Only images the user has already opened carry history (seeding an unvisited image
       here would evict the live one from the LRU store for no benefit -- see
       DevelopHistory::kMaxImages).

       The key is SORTED: a QSet's iteration order is not stable, and an unstable key
       would defeat the coalescing it exists for. */
    QStringList names = fields.values();
    names.sort();
    const QString mergeKey = "sync/" + names.join(',');
    for (const QString &p : targets) {
        EditStack &t = stackFor(p);
        copyParamFields(src, t.scopes[0].params, fields);
        EditStack::sanitize(t);
        dirty.insert(p);
        if (history && history->count(p) > 0)
            history->record(p, "Global", propagateAction, propagateValue, mergeKey, t);
    }
    if (G::isDevelopDebounceWrite && debounceWriteTimer)
        debounceWriteTimer->start(kDebounceWriteMs);
}

int DevelopProperties::propagatePreset(const DevelopPreset &preset, const QString &label)
{
/*
    Paste Settings / Apply Preset onto the OTHER selected images. Unlike an adjustment
    (which propagates only the fields that moved) a preset is applied as a whole object
    through the same mergePreset the current image used, so the copied per-image items
    -- crop, straighten, warp and appended spot heals -- travel exactly as they do on
    the current image. The target is always scope 0: a mask scope on the current image
    has no counterpart on another image (the mask never travels).
*/
    const QStringList targets = otherSelectedPaths();
    if (targets.isEmpty()) return 0;
    if (G::isLogger) G::log("DevelopProperties::propagatePreset",
                            QString::number(targets.count()) + " images");
    for (const QString &p : targets) {
        EditStack &t = stackFor(p);
        t = mergePreset(preset, t, 0);
        dirty.insert(p);
        if (history && history->count(p) > 0)
            history->record(p, "Global", label, QString(), QString(), t);
    }
    if (G::isDevelopDebounceWrite && debounceWriteTimer)
        debounceWriteTimer->start(kDebounceWriteMs);
    return targets.count();
}

void DevelopProperties::populateSlidersFromStack()
{
    EditParams p;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty()) {
        const int idx = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size()) ? activeScopeIndex : 0;
        p = s.scopes[idx].params;
    }
    isPopulating = true;
    /* Temp/Tint are absolute and stored with 0 = "as shot", so they resolve through the
       image's own colour characterisation rather than being pushed raw (refreshWbRow). */
    setSliderReal("exposure",   p.exposure);
    setSliderReal("contrast",   p.contrast);
    setSliderReal("highlights", p.highlights);
    setSliderReal("shadows",    p.shadows);
    setSliderReal("whites",     p.whites);
    setSliderReal("blacks",     p.blacks);
    setSliderReal("texture",    p.texture);
    setSliderReal("dehaze",     p.dehaze);
    setSliderReal("red",        p.red);
    setSliderReal("green",      p.green);
    setSliderReal("blue",       p.blue);
    setSliderReal("hue",        p.hue);
    setSliderReal("saturation", p.saturation);
    setSliderReal("vibrance",   p.vibrance);
    setSliderReal("gradeBlending", p.gradeBlending);
    setSliderReal("gradeBalance",  p.gradeBalance);
    setSliderReal("luminance",  p.luminance);
    setSliderReal("denoiseLuma",   p.denoiseLuma   * 100.0);      // Global "Denoise raw" (0..1 -> 0..100)
    setSliderReal("denoiseChroma", p.denoiseChroma * 100.0);
    setSliderReal("localDenoise",       p.localDenoiseLuma   * 100.0);   // "Denoise"
    setSliderReal("localDenoiseChroma", p.localDenoiseChroma * 100.0);
    setSliderReal("vignetteExposure",   p.vignetteExposure);            // "Vignette" (EV)
    setSliderReal("vignetteFeather",    p.vignetteFeather    * 100.0);
    setSliderReal("grainAmount",        p.grainAmount        * 100.0);   // "Grain"
    setSliderReal("grainSize",          p.grainSize          * 100.0);
    setSliderReal("grainRoughness",     p.grainRoughness     * 100.0);
    if (toneSlider)
        toneSlider->setPositions(p.toneShadowCenter, p.toneCrossover, p.toneHighlightCenter);
    refreshCalibrateRow();            // push primaries into the Calibrate wheel
    refreshColorGradeRow();           // push grade values into the wheel + Lum slider
    isPopulating = false;
    refreshWbRow();                 // Temp/Tint resolved + the preset dropdown
    refreshPreviewButtons();        // sync the eye icons to this scope's Preview flags
    updateSectionHeaderCaptions();  // " *" on sections holding non-default values
}

void DevelopProperties::setSliderReal(const QString &key, double real)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;   // editor not realized
    /* SliderEditor::setValue takes the raw slider int (real * div; div 0 => 1). */
    const int div = vIdx.data(UR_Div).toInt();
    const int factor = (div == 0) ? 1 : div;
    setItemValue(vIdx, static_cast<int>(qRound(real * factor)));
}

void DevelopProperties::setCheckboxValue(const QString &key, bool on)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;
    setItemValue(vIdx, on);
}

QString DevelopProperties::diagnostics()
{
    QString rpt;
    QTextStream rs(&rpt);
    rs << "Develop scopes: " << scopeList.join(", ") << "\n";
    rs << "Current scope: " << currentScopeNames().value(activeScopeIndex) << "\n";
    return rpt;
}

void DevelopProperties::howThisWorks()
{
    if (G::isLogger) G::log("DevelopProperties::howThisWorks");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Develop Module",
                   ":/Docs/develophelp.html",
                   QSize(700, 600), r, window());
}
