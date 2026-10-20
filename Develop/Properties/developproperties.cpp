#include "Develop/Properties/developproperties.h"
#include "Develop/cameraprofilestore.h"
#include "Develop/cameraprofile.h"
#include "Cache/devpreviewcache.h"
#include "Develop/editrotate.h"
#include "Develop/Properties/scopeheader.h"
#include "Develop/Properties/rawpanel.h"
#include "Develop/Properties/maskpanel.h"
#include "Develop/Properties/maskeditor.h"
#include "Develop/Properties/submaskdialog.h"
#include "Develop/Transform/transformpanel.h"
#include "Develop/Replace/replacepanel.h"
#include "Develop/History/historyview.h"
#include "Develop/History/historystore.h"
#include "Develop/Presets/presetsview.h"
#include "Develop/fillspot.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include "Develop/Scopes/toneregionslider.h"
#include "Develop/whitebalance.h"
#include "Develop/lensdb.h"
#include "Develop/calibrate.h"
#include "Develop/outputtransform.h"
#include "Develop/rangemask.h"
#include "Develop/workingimagecache.h"
#include "ImageFormats/Raw/applerawdecode.h"
#include "Utilities/versionkey.h"
#include "Dialogs/savedeveloppresetdlg.h"
#include <QDir>
#include <QUuid>
#include "Utilities/segmenter.h"
#include <QFileInfo>
#include <QPainterPath>
#include <QStandardItemModel>
#include <QRegularExpression>
#include <QVariantAnimation>
#include <algorithm>
#include <functional>

/* Is this a stroke-based tool -- one whose attributes are baked per stroke rather than
   evaluated for the submask as a whole? */
static bool isStrokeTool(int tool)
{
    return tool == int(MaskTool::Brush) || tool == int(MaskTool::Object)
           || tool == int(MaskTool::RefineBrush);
}

/* Does this tool PAINT on the canvas as a brush (size / feather / flow / auto-mask,
   strokes in paramsJson)? The Refine Brush does -- it is a brush on the canvas and a
   refine step in the fold (Develop/maskrefine.h). */
static bool isBrushCanvasTool(int tool)
{
    return tool == int(MaskTool::Brush) || tool == int(MaskTool::RefineBrush);
}

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
    /* Per-image edit history (the History dock's model): a labelled EditStack snapshot
       per action via noteEdit. Never written to the sidecar (which holds only the current
       state) -- it persists in the local index instead: flushImage saves it alongside the
       sidecar write, and seed() reloads it through the loader on an image's first touch,
       if it still ends at the sidecar's recipe (see develophistory.h). */
    history = new DevelopHistory(this);
    history->loader = [](const QString &p, QVector<HistoryEntry> &e, int &pos,
                         QString &stamp) {
        return HistoryStore::load(p, e, pos, stamp);
    };
    history->remover = [](const QString &p) { HistoryStore::remove(p); };
    /* Multi-image link ids are SAVED with the history now, so they must not repeat across
       sessions: start from the clock (ms since epoch, shifted to leave room for 65k
       batches per millisecond) rather than from 0. */
    nextSyncId = quint64(QDateTime::currentMSecsSinceEpoch()) << 16;
    /* The named develop presets (the Presets dock's model). Unlike history these ARE
       persistent -- they live in QSettings under "Develop Presets". */
    presets = new DevelopPresets(setting, this);
    /* The mask-tool chooser, popped when a new mask is created (newScope). */
    /* The Scope list (in the ScopeHeader widget) shows the CURRENT IMAGE's scopes --
       Global plus one row per mask (per-image EditStack), not app-global QSettings
       presets. Seed one name so it is valid before any image. */
    scopeList = QStringList() << "Global";

    /* Divider metrics (G::groupSeparator*, set by WidgetCSS::styleGlobals in
       MW::createAppStyle, before the docks are built) must be non-zero by the first
       buildTree. addGroupDivider stamps the row's height at build time, and building
       with it unset gave every divider a ZERO height -- a zero-height LAST row makes
       QTreeView::viewportSizeHint fall back to its generic 4-line default
       (QRect::isValid() is false for a 0-height rect), which in fit mode collapsed the
       whole block to ~3 rows: the Calibrate / Color Grade / Detail / Effects headers
       vanished when Effects (the last section, ending in a divider) was expanded before
       any folder was loaded. */

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

    /* Optional debounce write: flush the in-memory stack to the sidecar a short time after edits
       settle, so a crash loses less. Gated by G::isDevelopDebounceWrite; navigate-away / quit /
       pre-op flushes always run regardless. */
    debounceWriteTimer = new QTimer(this);
    debounceWriteTimer->setSingleShot(true);
    /* flushAll, not just the current image: with a multi-image selection the propagated
       edits made on the OTHER selected images are dirty too, and a crash should lose no
       more of them than it loses of the image being edited. */
    connect(debounceWriteTimer, &QTimer::timeout, this, [this]{ flushAll(); });

    /* The pending submask commits itself once the user's hand stops (see
       armMaskAutoCommit). No commit button: nothing is buffered for one to flush. */
    maskCommitTimer = new QTimer(this);
    maskCommitTimer->setSingleShot(true);
    connect(maskCommitTimer, &QTimer::timeout, this, [this]{ autoCommitPendingMask(); });

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
        emit kaleidoToolChanged();              // Effects > Kaleidoscope may now show
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex &idx){
        persistSectionExpanded(idx, false);
        /* Collapsing Curves DISARMS the pointer sampler rather than merely gating it
           (wantsCurveSample already hides the marker). It is armed chrome that owns the
           canvas click, so leaving it silently on behind a closed panel means a click on
           the image keeps adding curve points with nothing on screen to say why -- and
           the button would come back armed the next time the panel is opened. This is the
           COLLAPSED signal on purpose, not curveEditor->isVisible(): the plot is also
           momentarily invisible during a buildTree and while the dock is hidden, neither
           of which is the user putting the panel away. */
        if (idx.data(UR_Name).toString() == "CurvesHeader") setCurveSamplerActive(false);
        scheduleContentFit();
        emit kaleidoToolChanged();              // folding Effects puts the handles away
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
    combinedColorPanel = setting->value("Develop/combinedColorPanel", true).toBool();
    colorPanelMode = qBound(0, setting->value("Develop/colorPanelMode", 0).toInt(), 2);
    effectsChoice = qBound(0, setting->value("Develop/effectsChoice", 0).toInt(), 2);
    setIndentation(10);
    setAlternatingRowColors(false);
    /* Fill every NON-header row with the subpanel content background (the delegate's
       valueRowBackground = G::panelContentBg), so the Edits contents read as a lifted
       surface under the header bands -- the same lift the Raw, Transform and scopes
       panels paint. The base class leaves this true, which skips the fill and leaves the
       rows on the plain dock background. Headers are unaffected: they paint their own
       gradient band. */
    propertyDelegate->isAlternatingRows = false;
    propertyDelegate->headerColor = G::scopeSubheaderColor;
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
    const bool exCurves  = expandedOr("CurvesHeader",
                                      sectionExpanded("CurvesHeader", false));
    const bool exColor   = expandedOr("ColorHeader",   sectionExpanded("ColorHeader",   false));
    const bool exPointColor = expandedOr("PointColorHeader",
                                         sectionExpanded("PointColorHeader", false));
    const bool exColorPanel = expandedOr("ColorPanelHeader",
                                         sectionExpanded("ColorPanelHeader", false));
    const bool exDetail  = expandedOr("DetailHeader",  sectionExpanded("DetailHeader",  false));
    const bool exEffects = expandedOr("EffectsHeader", sectionExpanded("EffectsHeader", false));
    const bool exLens    = expandedOr("LensHeader",    sectionExpanded("LensHeader",    false));

    isPopulating = true;
    isRebuildingMasks = true;

    /* Delete every row's editor widgets (close), then drop the rows, and rebuild for the ACTIVE
       scope: its top items first, then the adjustment sections. close() before removeRows so the
       setIndexWidget editors are freed rather than leaked on each rebuild. */
    btns.clear();
    basicEyeBtn = curvesEyeBtn = colorEyeBtn = calibrateEyeBtn = colorGradeEyeBtn =
        detailEyeBtn = effectsEyeBtn = lensEyeBtn = pointColorEyeBtn = colorPanelEyeBtn =
        effectEyeBtn = nullptr;
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

    /* The Global raw-decode rows live in the RawPanel above the scope bar, and a mask's
       submasks in the MaskPanel below it, so the tree carries neither -- it is the
       adjustment sections only. addMaskItems merely keeps selectedMaskIndex honest. */
    if (activeScopeIndex != 0) addMaskItems();
    addBasic();
    addCurves();        // Lightroom's order: the tone curve sits directly under Basic
    if (combinedColorPanel) {
        addColorPanel();    // Tone / Hue / RGB in one section
    }
    else {
        addColor();
        addPointColor();    // directly under Color, as the user asked
        addCalibrate();
        addColorGrade();
    }
    addDetail();        // sharpening + local NR, in pipeline order before Effects
    addLens();          // Global only: the lens profile, Remove CA, defringe (LR's order)
    addEffects();
    /* The headers were just re-added with their plain captions, so the cached " *"
       state is stale -- force a full re-evaluation. */
    for (int &st : hdrStar) st = -1;
    updateSectionHeaderCaptions();

    isPopulating = false;
    isRebuildingMasks = false;

    /* Restore section expand-state. */
    auto setExpandOn = [this](const QString &name, bool on){
        const QModelIndex idx = findCaptionIndex(name);
        if (idx.isValid()) setExpanded(idx, on);
    };
    setExpandOn("BasicHeader",   exBasic);
    setExpandOn("CurvesHeader",  exCurves);
    setExpandOn("ColorHeader",   exColor);
    setExpandOn("PointColorHeader", exPointColor);
    setExpandOn("ColorPanelHeader", exColorPanel);
    setExpandOn("DetailHeader",  exDetail);
    setExpandOn("EffectsHeader", exEffects);
    setExpandOn("LensHeader",    exLens);

    populateSlidersFromStack();
    refreshPreviewButtons();
    updateMaskEdit();
    updateHiddenRows(QModelIndex());
    applyScopeItemsCollapsed();      // re-assert the '>' collapse (a rebuild resets row visibility)
    if (!panelEnabled) applyItemsEnabled(false);   // keep captions greyed if disabled
    syncRawPanel();                  // reflect raw state + visibility in the RawPanel
    syncMaskPanel();                 // the active mask's submask list + settings
    scheduleContentFit();            // a detail of the scope bar: re-fit the block height
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
    if (name == "BasicHeader" || name == "ColorHeader" || name == "DetailHeader" ||
        name == "EffectsHeader" || name == "PointColorHeader" ||
        name == "ColorPanelHeader" || name == "LensHeader")
        setting->setValue("Develop/SectionExpanded/" + name, expanded);
}

bool DevelopProperties::currentIsRaw() const
{
    return mw && mw->isFileRaw(currentImagePath);
}

void DevelopProperties::updateDenoiseRunState(bool denoised)
{
    /* Reflect the raw-denoise completion state (a ready PMRID base): the RawPanel shows
       its Lum / Color sliders only once that base exists -- they merely scale the blend
       of it. The "Run on this image" box is NOT driven from here: it shows the image's
       saved choice (syncRawPanel -> RawPanel::setRunOnImage). */
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

void DevelopProperties::updateDenoiseAvailability()
{
/*
    Grey the "Denoise raw" group, with the reason in its place, when PMRID cannot run --
    no inference backend / no pmrid.onnx in this build, or a sensor whose CFA it leaves
    untouched. Stating it on the panel beats letting the controls look live and reporting
    the disappointment afterwards: the user never spends a decode to find out.
*/
    if (!rawPanel) return;
    QString reason;
    const bool ok = !mw || mw->rawDenoiseAvailable(currentImagePath, &reason);
    rawPanel->setDenoiseAvailable(ok, reason);
    /* Legacy tree rows: no room for a reason line, so just make them inert. */
    if (denoiseRunCheck && !ok) {
        QSignalBlocker block(denoiseRunCheck);
        denoiseRunCheck->setChecked(false);
        denoiseRunCheck->setText("Denoise");
        denoiseRunCheck->setEnabled(false);
        setItemEnabled("denoiseLuma", false);
        setItemEnabled("denoiseChroma", false);
    }
    else if (denoiseRunCheck) {
        denoiseRunCheck->setEnabled(true);
    }
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
    /* The Edit-source radios live in the RawPanel above the scope bar (MW's status-bar
       useRaw button and this panel drive each other through G::useRaw). */
    if (rawPanel) rawPanel->setEditSource(useRaw);
}

void DevelopProperties::addMaskItems()
{
    if (G::isLogger) G::log("DevelopProperties::addMaskItems");
    /* Non-Global scopes: the mask itself lives in the MaskPanel below the scope bar --
       its SubmaskList carries the submasks (including its own [+] for an empty mask) and
       the open one shows its settings there. The tree holds no mask rows at all, so all
       this does is keep selectedMaskIndex honest: nothing can be open on an empty mask. */
    EditScope *scope = activeScope();
    if (!scope || activeScopeIndex == 0 || scope->components.isEmpty())
        selectedMaskIndex = -1;
}

void DevelopProperties::bindScopeHeader(ScopeHeader *header)
{
    if (G::isLogger) G::log("DevelopProperties::bindScopeHeader");
    scopeHeader = header;
    if (!scopeHeader) return;

    connect(scopeHeader, &ScopeHeader::scopeSelected,       this, &DevelopProperties::onScopeSelected);
    connect(scopeHeader, &ScopeHeader::renameRequested,     this, &DevelopProperties::renameActiveScope);
    connect(scopeHeader, &ScopeHeader::resetScopeRequested, this, &DevelopProperties::resetActiveScope);
    connect(scopeHeader, &ScopeHeader::removeScopeRequested,this, &DevelopProperties::deleteScope);
    connect(scopeHeader, &ScopeHeader::addScopeRequested,   this, &DevelopProperties::newScope);
    connect(scopeHeader, &ScopeHeader::addMaskRequested,    this, &DevelopProperties::showMaskMenu);
    connect(scopeHeader, &ScopeHeader::resetAllEditsRequested, this, &DevelopProperties::resetAllEdits);
    connect(scopeHeader, &ScopeHeader::maskOverlayToggled,  this,
            &DevelopProperties::maskOverlayToggleRequested);
    connect(scopeHeader, &ScopeHeader::previewToggled,      this, &DevelopProperties::onScopePreviewToggled);
    connect(scopeHeader, &ScopeHeader::collapseToggled,     this, &DevelopProperties::setTreeCollapsed);
    connect(scopeHeader, &ScopeHeader::scopeEnabledToggled, this, &DevelopProperties::onScopeEnabledToggled);
    connect(scopeHeader, &ScopeHeader::helpRequested,       this, &DevelopProperties::editsHelp);

    /* Seed the combo + eye from the current stack. */
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
    /*  "Render using" is the CURRENT IMAGE's choice (Develop/rawrender.h), recorded in
        its recipe -- not a global switch. THE PANEL SHOWS THE RENDERER IN EFFECT
        (RawRender::resolve: off-mac an Apple choice is Winnow), so the denoise block --
        Winnow only -- follows what will actually render. */
    connect(rawPanel, &RawPanel::demosaicChanged, this, [this](bool apple){
        RawRender::Choice c;
        c.apple = apple;
        setRawRenderer(c);
    });
    connect(rawPanel, &RawPanel::appleVersionChanged, this, [this](int pin){
        RawRender::Choice c;
        c.apple = true;
        c.applePin = pin;
        setRawRenderer(c);
    });
    connect(rawPanel, &RawPanel::denoiseRunToggled, this, [this](bool on){
        setDenoiseRawFlag(on);
        if (on) emit runRawDenoiseRequested(); else emit clearRawDenoiseRequested();
    });
    connect(rawPanel, &RawPanel::autoRunToggled, this, [this](bool on){
        setting->setValue("Develop/autoRunDenoise", on);
        emit autoRunDenoiseToggled(on);
    });
    connect(rawPanel, &RawPanel::denoiseLumaChanged,   this, [this](int v){ setGlobalDenoise(true,  v); });
    connect(rawPanel, &RawPanel::denoiseChromaChanged, this, [this](int v){ setGlobalDenoise(false, v); });
    connect(rawPanel, &RawPanel::tipsRequested, this, &DevelopProperties::rawHelp);
    /* Solo: the Raw panel is a peer of the Scope row and the adjustment sections, so
       expanding it folds them away. */
    connect(rawPanel, &RawPanel::collapseToggled, this, [this](bool collapsed){
        if (collapsed) return;
        soloCollapseOthers(SoloOwner::RawPanel);
    });

    syncRawPanel();
}

void DevelopProperties::syncLensPanel()
{
    /* The rows exist only while Global is the active scope (addLens); otherwise the
       setters below find no row and do nothing. */
    EditParams g;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty()) g = s.scopes.at(0).params;
    const bool wasPop = isPopulating;
    isPopulating = true;
    const bool raw = currentIsRaw();
    setCheckboxValue("lensProfile", g.wantsLensProfile(G::autoLensProfile, raw));
    setSliderReal("lensDistortion", g.lensDistortion * 100.0);
    setSliderReal("lensVignetting", g.lensVignetting * 100.0);
    setSliderReal("lensManualDistortion", g.lensManualDistortion * 100.0);
    setSliderReal("lensManualVignetting", g.lensManualVignetting * 100.0);
    setSliderReal("lensManualMidpoint",   g.lensManualMidpoint   * 100.0);
    setCheckboxValue("removeCA", g.wantsRemoveCA(G::autoRemoveCA, raw));
    setSliderReal("defringePurple", g.defringePurple * 100.0);
    setSliderReal("defringeGreen",  g.defringeGreen  * 100.0);
    repopulateLensCombo();          // the matched lens is per image; also the row enables
    isPopulating = wasPop;
}

void DevelopProperties::setGlobalLensProfile(bool on)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    const int want = on ? 1 : 0;
    if (s.scopes[0].params.lensProfile == want) return;
    s.scopes[0].params.lensProfile = want;
    noteScopeEdit("Global", "Lens profile", on ? "On" : "Off", "global/lensProfile");
    repopulateLensCombo();          // the amount rows follow the checkbox
    emit paramsChanged();
}

void DevelopProperties::setGlobalLensAmount(bool distortion, int value0to200)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    const float f = qBound(0, value0to200, 200) / 100.0f;
    float &field = distortion ? s.scopes[0].params.lensDistortion
                              : s.scopes[0].params.lensVignetting;
    if (field == f) return;
    field = f;
    noteScopeEdit("Global", distortion ? "Lens distortion" : "Lens vignetting",
                  QString::number(value0to200),
                  distortion ? "global/lensDistortion" : "global/lensVignetting");
    emit paramsChanged();
}

void DevelopProperties::setGlobalLensManual(const QString &key, int sliderValue)
{
    /* The Manual rows: -100..100 (Midpoint 0..100) on the panel, /100 in the recipe.
       Global-only like the profile amounts, so always scope 0. */
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    EditParams &p = s.scopes[0].params;
    float *field = nullptr;
    QString label;
    int lo = -100;
    if (key == "lensManualDistortion")      { field = &p.lensManualDistortion; label = "Manual distortion"; }
    else if (key == "lensManualVignetting") { field = &p.lensManualVignetting; label = "Manual vignetting"; }
    else if (key == "lensManualMidpoint")   { field = &p.lensManualMidpoint; label = "Manual midpoint"; lo = 0; }
    if (!field) return;
    const float f = qBound(lo, sliderValue, 100) / 100.0f;
    if (*field == f) return;
    *field = f;
    noteScopeEdit("Global", label, QString::number(sliderValue), "global/" + key);
    emit paramsChanged();
}

void DevelopProperties::setGlobalLensProfileId(const QString &id)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    if (s.scopes[0].params.lensProfileId == id) return;
    s.scopes[0].params.lensProfileId = id;
    const LensDb::Lens *L = id.isEmpty() ? nullptr : LensDb::Database::instance().lensById(id);
    noteScopeEdit("Global", "Lens", L ? L->displayName() : QStringLiteral("Auto"),
                  "global/lensProfileId");
    /* Picking a lens is asking for its correction: an image whose profile is off (or
       unset with the preference off) would otherwise show no change at all. */
    const EditParams &g = s.scopes[0].params;
    if (!id.isEmpty() && !g.wantsLensProfile(G::autoLensProfile, currentIsRaw())) {
        s.scopes[0].params.lensProfile = 1;
        const bool wasPop = isPopulating;
        isPopulating = true;
        setCheckboxValue("lensProfile", true);
        isPopulating = wasPop;
    }
    repopulateLensCombo();
    emit paramsChanged();
}

void DevelopProperties::setGlobalRemoveCA(bool on)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    const int want = on ? 1 : 0;
    if (s.scopes[0].params.removeCA == want) return;
    s.scopes[0].params.removeCA = want;
    noteScopeEdit("Global", "Remove chromatic aberration", on ? "On" : "Off",
                  "global/removeCA");
    emit paramsChanged();
}

void DevelopProperties::setGlobalDefringe(bool purple, int value0to100)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    const float f = qBound(0, value0to100, 100) / 100.0f;
    float &field = purple ? s.scopes[0].params.defringePurple
                          : s.scopes[0].params.defringeGreen;
    if (field == f) return;
    field = f;
    noteScopeEdit("Global", purple ? "Defringe purple" : "Defringe green",
                  QString::number(value0to100),
                  purple ? "global/defringePurple" : "global/defringeGreen");
    emit paramsChanged();
}

void DevelopProperties::reflectEngine()
{
    if (!rawPanel) return;
    rawPanel->setEngine(currentRawRender().apple);
    syncAppleDecoderVersions();         // the selected version is this image's too
    updateDenoiseAvailability();
}

RawRender::Choice DevelopProperties::rawRenderFor(const QString &fPath) const
{
/*
    The image's renderer: its recipe's choice, else the default. The session's copy of
    the recipe when there is one (it holds edits not yet flushed to the sidecar), else
    the sidecar. GUI thread.
*/
    if (fPath.isEmpty()) return RawRender::defaultChoice();
    const auto it = stackCache.constFind(fPath);
    if (it != stackCache.constEnd()) {
        if (it->scopes.isEmpty()) return RawRender::defaultChoice();
        return RawRender::resolve(it->scopes.first().params);
    }
    return RawRender::fromBlob(Metadata::readDevelopSidecar(fPath));
}

RawRender::Choice DevelopProperties::currentRawRender() const
{
    return rawRenderFor(currentImagePath);
}

void DevelopProperties::setRawRenderer(const RawRender::Choice &c)
{
/*
    "Render using" for the CURRENT IMAGE: recorded EXPLICITLY in its recipe (Global
    scope, like "Denoise raw"), so it outlives the session, travels with a new version,
    and other images -- the master of this version among them -- keep their own.
    noteScopeEdit puts it in History and propagates it across a multi-image selection.

    No cache to clear: the decoded base is filed per renderer (WorkingImageCache tags),
    so the render paramsChanged starts asks for this renderer's base, misses, and
    decodes it (MW::ensureDevelopWork). An undo lands the same way.
*/
    if (currentImagePath.isEmpty() || !currentIsRaw()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    EditParams &p = s.scopes[0].params;
    if (p.rawEngine >= 0 && RawRender::resolve(p) == c) return;
    RawRender::apply(p, c);
    noteScopeEdit("Global", "Render using", c.label(), "global/rawRenderer");
    reflectEngine();
    /* The engine decides which rows exist (camera profiles are Winnow-only; the
       denoise rows); rebuild AFTER this signal returns -- the combo is mid-signal. */
    QTimer::singleShot(0, this, [this]{ buildTree(); });
    emit rawRendererChanged();
    emit paramsChanged();
}

void DevelopProperties::syncAppleDecoderVersions()
{
    if (!rawPanel) return;
    QList<int> versions;
    int used = 0;
#ifdef Q_OS_MAC
    /* Recorded under the FILE decoded; a version (virtual copy) decodes its source. */
    const QString src = VersionKey::isVersion(currentImagePath)
                            ? VersionKey::sourceOf(currentImagePath) : currentImagePath;
    if (!AppleRawDecode::DecodedVersions(src, versions, used)) {
        versions.clear();
        used = 0;
        /* Not decoded by Apple this session (Winnow is rendering, or the decode is
           still running): ask Core Image directly, so the list is complete before the
           user opens it. The answer comes back through the decoded callback. */
        AppleRawDecode::RequestVersions(src);
    }
#endif
    /*  The selection is the version THIS IMAGE's choice picks from the file's list --
        not the last decode of the file, which may have been another version (virtual
        copy) of it on another renderer. Same rule as AppleRawDecode::PickVersion: the
        newest <= the pin, else the oldest; pin 0 = the newest. */
    const RawRender::Choice c = currentRawRender();
    if (c.apple && !versions.isEmpty()) {
        used = versions.first();
        if (c.applePin > 0) {
            used = versions.last();
            for (int v : versions) if (v <= c.applePin) { used = v; break; }
        }
    }
    rawPanel->setAppleVersions(versions, used);
}

void DevelopProperties::syncRawPanel()
{
    if (!rawPanel) return;
    const bool raw = currentIsRaw();
    /* Raw-decode strip: raw files only. */
    rawPanel->setVisible(raw);
    if (!raw) return;

    rawPanel->setCaptionWidth(captionColumnWidth);   // align denoise sliders to the tree
    rawPanel->setEditSource(G::useRaw);
    rawPanel->setEngine(currentRawRender().apple);
    syncAppleDecoderVersions();
    rawPanel->setAutoRun(setting->value("Develop/autoRunDenoise", true).toBool());
    /* Two separate facts: the box is the image's SAVED choice (opted in), the sliders
       show once a denoised base actually exists. */
    const EditStack s = stackCache.value(currentImagePath);
    rawPanel->setRunOnImage(!s.scopes.isEmpty() && s.scopes.at(0).params.denoiseRaw > 0);
    rawPanel->setDenoiseRunState(mw && mw->rawDenoiseReadyForCurrent());
    updateDenoiseAvailability();      // may grey the group + explain (after the run state)

    /* Push the Global scope's EFFECTIVE denoise amounts (0..1 -> 0..100): the image's
       own when it set them, else the remembered ones (EditParams::denoiseLuma). */
    float dl = G::denoiseLumaDefault, dc = G::denoiseChromaDefault;
    if (!s.scopes.isEmpty()) {
        dl = s.scopes.at(0).params.denoiseLumaOr(G::denoiseLumaDefault);
        dc = s.scopes.at(0).params.denoiseChromaOr(G::denoiseChromaDefault);
    }
    rawPanel->setDenoiseValues(qRound(dl * 100.0f), qRound(dc * 100.0f));
}

void DevelopProperties::setDenoiseRawFlag(bool on)
{
/*
    The "Run on this image" checkbox records its state IN THE RECIPE
    (EditParams::denoiseRaw), so the decision outlives the session cache the run itself
    fills. Without this the render, the export and the devPreview builder each had to
    guess from G::autoRunDenoise, and a manual run reached none of them: the loupe showed
    a denoised image while the stored preview stayed clean under a recipe hash that
    claimed they matched.

    Written EXPLICITLY (1 / 0), not folded back to "unset" when it happens to agree with
    the preference: an image that opted in stays denoised when Auto run is later turned
    off. (Auto run ON overrides a 0 -- the box is hidden then; see wantsDenoiseRaw.)

    Global scope (0) whatever scope is active, like the two amount sliders -- the denoise
    is baked into the pre-develop WorkingImage, so it cannot mean different things per
    mask. noteScopeEdit is what puts it in History, marks the image edited and propagates
    it across a multi-image selection.
*/
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    const int want = on ? 1 : 0;
    if (s.scopes[0].params.denoiseRaw == want) return;
    s.scopes[0].params.denoiseRaw = want;
    noteScopeEdit("Global", "Denoise raw", on ? "On" : "Off", "global/denoiseRaw");
    emit paramsChanged();
}

void DevelopProperties::setGlobalDenoise(bool luma, int value0to100)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    const float f = value0to100 / 100.0f;      // dock 0..100 -> stored 0..1
    if (luma) s.scopes[0].params.denoiseLuma   = f;
    else      s.scopes[0].params.denoiseChroma = f;
    /* Also the REMEMBERED amount: images that never set their own follow the last value
       the user chose (EditParams::denoiseLuma). This image stores it explicitly above, so
       a later change elsewhere does not move it. */
    if (luma) {
        G::denoiseLumaDefault = f;
        setting->setValue("Develop/denoiseLumaDefault", f);
    } else {
        G::denoiseChromaDefault = f;
        setting->setValue("Develop/denoiseChromaDefault", f);
    }
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
    /* The cursor being in the mask panel is half of "the mask has the user's attention"
       (the other half is having just edited one) -- see maskVeilEngaged. */
    connect(maskPanel, &MaskPanel::hoverChanged, this, &DevelopProperties::setMaskPanelHovered);
    /* The overlay's colour / grayscale controls are NOT in this panel (they are on the
       Mask band tint swatch's context menu, which calls setMaskOverlayColour /
       setMaskOverlayGrayscale directly), so there is nothing to connect for them. */

    /* The panel's embedded tree edits the tool being built; route its changes into the
       active mask component (onMaskEditorSetting mirrors the main tree's itemChange). */
    if (MaskEditor *ed = maskPanel->levelEditor())
        connect(ed, &MaskEditor::settingChanged,
                this, &DevelopProperties::onMaskLevelSetting);
    if (MaskEditor *ed = maskPanel->editor()) {
        connect(ed, &MaskEditor::settingChanged, this, &DevelopProperties::onMaskEditorSetting);
        connect(ed, &MaskEditor::wheelChanged,   this, &DevelopProperties::onMaskEditorWheel);
    }

    /* The submask list: the mask's contents, and the only way back into a submask that
       has already been committed. Every emission arrives DEFERRED (SubmaskList rebuilds
       its rows from inside these handlers), so they are safe to act on directly. */
    if (SubmaskList *sl = maskPanel->list()) {
        connect(sl, &SubmaskList::submaskSelected,   this, &DevelopProperties::reopenSubmask);
        connect(sl, &SubmaskList::enabledToggled,    this, &DevelopProperties::setSubmaskEnabled);
        connect(sl, &SubmaskList::opChanged,         this, &DevelopProperties::setSubmaskOp);
        connect(sl, &SubmaskList::invertRequested,   this, &DevelopProperties::toggleSubmaskInverted);
        connect(sl, &SubmaskList::moveRequested,     this, &DevelopProperties::moveSubmask);
        connect(sl, &SubmaskList::duplicateRequested, this, &DevelopProperties::duplicateSubmask);
        connect(sl, &SubmaskList::deleteRequested,   this, &DevelopProperties::deleteMask);
    }

    /* The Mask band: its [+] (and [:] "Add submask") appends a submask, and its arrow
       folds the submask rows. Its help opens the submasks page: Edge and Halo are
       documented there, and one mask page beats two half ones. */
    connect(maskPanel, &MaskPanel::addSubmaskRequested, this, &DevelopProperties::showMaskMenu);
    connect(maskPanel, &MaskPanel::maskFoldedChanged, this, &DevelopProperties::onMaskFolded);
    connect(maskPanel, &MaskPanel::resetMaskLevelRequested,
            this, &DevelopProperties::resetMaskLevel);
    connect(maskPanel, &MaskPanel::helpRequested, this, &DevelopProperties::submasksHelp);
}

/*
    Has this mask an EDGE IN THE PICTURE for Halo to snap to?

    The two gradients are pure geometry -- their coverage is a ramp the user placed, with
    no relationship to anything in the image -- so refining them against the image would
    either do nothing or drag the ramp onto an unrelated edge. Every other tool is either
    content-derived (the ranges, the AI cutouts) or painted onto content (the brush), and
    all of them can be misregistered, which is the case Halo exists for.
*/
static bool maskHasRealEdge(const EditScope &l)
{
    for (const MaskComponent &m : l.components) {
        if (!m.enabled) continue;
        if (m.tool != int(MaskTool::LinearGradient) &&
            m.tool != int(MaskTool::RadialGradient)) return true;
    }
    return false;
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
        /* "Brush Submask", not "Brush Mask": each row is a submask of the mask the
           Mask band above names. maskToolName stays "... Mask" for the history, the
           canvas legend and the add dialog. */
        r.toolName = maskToolName(m.tool);
        if (r.toolName.endsWith(" Mask"))
            r.toolName = r.toolName.chopped(5) + " Submask";
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
    /* MASK-level Edge, under the list. Always up while the mask HAS submasks -- unlike
       the settings block below it, it does not depend on one being selected. With no
       submasks there is no mask to grow or shrink, so the row goes away entirely. */
    if (MaskEditor *ed = maskPanel->levelEditor()) {
        ed->setCaptionWidth(maskPanelCaptionWidth());
        ed->showMaskLevel(scope->maskEdge, scope->maskHalo, maskHasRealEdge(*scope));
    }
    maskPanel->showMaskLevel(!scope->components.isEmpty());
    /* The settings only make sense with a submask open; an empty mask shows nothing
       but its [+]. */
    const bool open = (selectedMaskIndex >= 0 &&
                       selectedMaskIndex < scope->components.size());
    maskPanel->setEditingExisting(open && pendingIdx < 0);
    maskPanel->showAttributes(open);
    syncAttributeScopeLabel();      // names what the settings below will affect
    syncSegmenterNote();
    maskPanel->setVisible(true);
}

void DevelopProperties::onMaskFolded(bool folded)
{
/*
    The Mask band folded the submask rows away, so anything open inside them closes with
    them. (This was the Submasks section's collapse until that band was folded into the
    Mask band, 2026-10-14.)

    Collapsing used to hide the ROWS and nothing else: selectedMaskIndex survived, so the
    open submask's handles stayed live on the canvas (a submask being edited with no row
    on screen to say which), and MaskPanel::syncAttrVisible merely hid its settings --
    which re-appeared on the next expand, reading as though expanding the header had
    SELECTED a submask. Closing what is open here fixes both: the handles go with the
    rows, and expanding is then pure disclosure, with nothing selected.

    A submask still inside its settle window lands rather than being stranded -- closing
    the section is the user finishing with it, the same as clicking its row.
*/
    if (G::isLogger) G::log("DevelopProperties::onMaskFolded");
    if (!folded) return;                    // unfolding selects nothing: nothing to do
    if (selectedMaskIndex < 0) return;
    flushMaskAutoCommit();
    closeSubmaskEditing();
}

void DevelopProperties::reopenSubmask(int index)
{
    if (G::isLogger) G::log("DevelopProperties::reopenSubmask");
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    /* Clicking the OPEN submask closes it: the row is the list's only disclosure, so it
       has to work both ways. A still-pending one lands first rather than refusing --
       closing is the user saying they are finished with it. */
    if (index == selectedMaskIndex) {
        flushMaskAutoCommit();
        closeSubmaskEditing();
        return;
    }
    /* Switching away from a submask still inside its settle window: land it now rather
       than let the timer fire into a submask the user has already left. */
    flushMaskAutoCommit();

    /* The flush may have DROPPED an untouched pending submask, which shifts everything
       after it down one. */
    scope = activeScope();
    if (!scope || index >= scope->components.size()) return;

    selectedMaskIndex = index;
    pendingIdx = -1;                 // a committed submask is not pending: no cancel
    pendingOp = int(MaskOp::Add);
    latchedMaskOp = int(MaskOp::Add);
    maskPanelOpen = false;           // nothing outstanding, so nothing to confirm
    const MaskComponent &m = scope->components.at(index);
    if (MaskEditor *ed = maskPanel ? maskPanel->editor() : nullptr) {
        ed->setCaptionWidth(maskPanelCaptionWidth());
        ed->showTool(m, index == 0);
        if (m.tool == int(MaskTool::ColorRange))
            ed->setWheelSamples(colorRangeSamplesHS(m.paramsJson));
    }
    if (isBrushCanvasTool(m.tool)) emitBrushSettings(m);
    /* buildTree ends in updateMaskEdit, which emits maskEditBegin for the selected
       component -- that is what puts the tool's handles back on the canvas. */
    buildTree();
    emit paramsChanged();
    noteMaskInteraction();
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
    noteMaskInteraction();
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
    noteMaskInteraction();
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
    if (index == pendingIdx) armMaskAutoCommit();
    noteMaskInteraction();
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
    noteMaskInteraction();
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
    noteMaskInteraction();
}

/* History caption for a mask-tool setting key (the tree/panel row's own wording). */
static QString maskSettingLabel(const QString &key)
{
    if (key == "maskFeather")      return "Feather";
    if (key == "maskEdge")         return "Edge";
    /* Distinct from the submask's "Edge" so the History dock says which level moved. */
    if (key == "scopeMaskEdge")    return "Mask edge";
    if (key == "scopeHalo")        return "Mask halo";
    if (key == "maskInvert")       return "Invert";
    if (key == "maskSize")         return "Brush size";
    if (key == "maskFlow")         return "Brush flow";
    if (key == "maskAutoMask")     return "Auto mask";
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

void DevelopProperties::syncSegmenterNote()
{
    if (!maskPanel) return;
    const MaskComponent *mm = editingMaskComp();
    const bool seg = mm && mm->tool == int(MaskTool::Segmenter);
    maskPanel->setToolNote(seg ? segmenterNote : QString());
}

void DevelopProperties::setSegmenterNote(const QString &text)
{
    if (segmenterNote == text) return;          // progress ticks repeat themselves
    segmenterNote = text;
    syncSegmenterNote();
}

void DevelopProperties::setSegmenterDetail(int level)
{
    level = std::clamp(level, 0, Segmenter::kLevels - 1);
    setting->setValue("Develop/segmenterDetail", level);
    const MaskComponent *mm = editingMaskComp();
    if (maskPanel && maskPanel->editor() && mm && mm->tool == int(MaskTool::Segmenter))
        maskPanel->editor()->setComboValue("maskSegDetail", Segmenter::levelName(level));
}

void DevelopProperties::onMaskEditorSetting(const QString &key, const QVariant &v)
{
    MaskComponent *mm = editingMaskComp();
    if (!mm) return;
    /* Brush next-stroke settings (size/flow/autoMask + brush feather): refresh the
       cursor, do NOT re-composite (existing strokes keep their snapshot). */
    const bool brushLike = key == "maskSize" || key == "maskFlow" ||
                           key == "maskAutoMask" ||
                           (key == "maskFeather" && isBrushCanvasTool(mm->tool));
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
        /* Size is a 0.1-resolution double (div 10 slider) -- toInt would floor every
           sub-1% brush back to the old, far too coarse, whole-percent grid. */
        else if (key == "maskSize")     put("size", v.toDouble());
        else if (key == "maskFlow")     put("flow", v.toInt());
        else if (key == "maskAutoMask") put("autoMask", v.toBool());
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
    /* Refine Brush mode. Add/Remove ARE the op (the same one the Opt-at-stroke latch
       sets), so they go through setSubmaskOp, which also moves a pending submask's
       previewed op; Fix Edge is the "mode" key, which overrides the op in the fold
       (refineModeOf). setSubmaskOp writes its own history entry and rebuilds. */
    else if (key == "maskRefineMode") {
        const QString mode = v.toString();
        const bool edge = (mode == "Fix Edge");
        const QString was = QJsonDocument::fromJson(mm->paramsJson.toUtf8())
                                .object().value("mode").toString();
        if ((was == "edge") != edge) {
            mm->paramsJson = brushWith(mm->paramsJson, "mode", edge ? "edge" : "");
            noteEdit("Refine " + mode, QString(), "mask/" + key);
            emit paramsChanged();
            buildTree();
        }
        if (!edge)
            setSubmaskOp(selectedMaskIndex, mode == "Remove" ? int(MaskOp::Subtract)
                                                             : int(MaskOp::Add));
        armMaskAutoCommit();
        noteMaskInteraction();
        return;
    }
    /* Segmenter Detail: a TOOL preference (which level of the object hierarchy the
       canvas offers), not an edit -- the selection made so far is untouched, so there is
       no history entry, no render and no settle. */
    else if (key == "maskSegDetail") {
        int level = 0;
        for (int l = 0; l < Segmenter::kLevels; ++l)
            if (Segmenter::levelName(l) == v.toString()) level = l;
        setting->setValue("Develop/segmenterDetail", level);
        emit segmenterDetailChanged(level);
        noteMaskInteraction();
        return;
    }
    /* Feather (non-brush) / Edge / Invert: change the mask -> overlay + re-composite. */
    else if (key == "maskFeather" || key == "maskEdge" || key == "maskInvert") {
        if      (key == "maskFeather") { mm->feather = v.toFloat();
                                         emit maskFeatherChanged(v.toDouble()); }
        else if (key == "maskEdge")    { mm->edge = v.toFloat(); }
        else                           { mm->inverted = v.toBool();
                                         emit maskInvertChanged(v.toBool()); }
        emit paramsChanged();
    }
    else return;
    noteEdit(maskSettingLabel(key), historyValueText(QModelIndex(), v),
             "mask/" + key);
    armMaskAutoCommit();       // the submask lands once this drag/typing settles
    noteMaskInteraction();     // ... and the veil stays up while the mask is the subject
}

/* MASK level: reshape the folded mask (Edge, Halo). Edits the SCOPE, so unlike every
   other row in the panel it does not need -- or look for -- a selected submask. */
void DevelopProperties::onMaskLevelSetting(const QString &key, const QVariant &v)
{
    if (G::isLogger) G::log("DevelopProperties::onMaskLevelSetting");
    EditScope *l = activeScope();
    if (!l) return;
    if      (key == "scopeMaskEdge") l->maskEdge = v.toFloat();
    else if (key == "scopeHalo")     l->maskHalo = v.toFloat();
    else return;
    noteEdit(maskSettingLabel(key), historyValueText(QModelIndex(), v), "mask/" + key);
    emit paramsChanged();
    noteMaskInteraction();
}

void DevelopProperties::resetMaskLevel()
{
    if (G::isLogger) G::log("DevelopProperties::resetMaskLevel");
    EditScope *l = activeScope();
    if (!l) return;
    if (l->maskEdge == 0.0f && l->maskHalo == 0.0f) return;
    l->maskEdge = 0.0f;
    l->maskHalo = 0.0f;
    /* No mergeKey: this is one discrete act, not a slider settling, so it gets its own
       history entry rather than folding into the last Edge drag. */
    noteEdit(tr("Reset mask edge and halo"));
    syncMaskPanel();                    // push the zeroed rows back into the sliders
    emit paramsChanged();
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

void DevelopProperties::onMaskToolChosen(int tool, int op)
{
    /* Every tool builds up through the MaskPanel (blue/red flow); its settings render in
       the panel's embedded MaskEditor. */
    beginMaskTool(tool, op);
}

void DevelopProperties::beginMaskTool(int tool, int op)
{
    if (G::isLogger) G::log("DevelopProperties::beginMaskTool");
    EditScope *scope = activeScope();
    if (!scope || activeScopeIndex == 0) return;

    /* The mask tool is taking the canvas, so a latched Before ("\") has to come off it --
       the same reason toggleBeforeAfter declines to go up while a tool is open. FIRST,
       before the submask is appended: ending a preview renders, and that render must not
       re-enter on a half-built pending submask. */
    endHistoryPreview();

    const bool first = scope->components.isEmpty();
    /* An empty mask has nothing to combine with, whatever was asked for. */
    if (first) op = int(MaskOp::Add);
    MaskComponent m;
    m.op   = int(MaskOp::Add);   // the real op is written on commit, from pendingOp
    m.tool = tool;
    m.paramsJson = defaultMaskParams(tool);
    if (tool == int(MaskTool::Brush)) m.feather = 0.0f;   // brush -> crisp edge
    /* Sky is a real matte -- its fractional edge IS the result (Utilities/skyrefine.h) --
       and Feather > 0 re-sharpens it around 0.5, undoing the halo work. Start at the matte
       as-is. (Refine Brush keeps the default: a soft stroke edge fades the refinement in
       rather than leaving a seam where the region ends.) The Segmenter's coverage is a
       matte too -- its outlines snapped to the photo by SegRefine -- so the same holds. */
    if (tool == int(MaskTool::Sky) || tool == int(MaskTool::Segmenter)) m.feather = 0.0f;

    /* Append the submask; its settings render in the panel's embedded MaskEditor. EVERY
       submask is pending until its edits settle (or Return lands it early) -- the first
       one too, which costs nothing visually (the mask IS that submask) and keeps one
       commit path. */
    scope->components.append(m);
    selectedMaskIndex = scope->components.size() - 1;
    pendingIdx = selectedMaskIndex;
    pendingPristine = m;                 // baseline for pendingMaskIsUntouched
    /* The op the user picked in the dialog is what the veil, the render and the canvas
       chip preview from the first instant -- and what a modifier, once released, falls
       back to (chosenMaskOp, read by latchMaskOp and setPendingMaskOp). */
    chosenMaskOp = op;
    pendingOp = op;
    latchedMaskOp = op;
    maskPanelOpen = true;
    if (maskPanel) {
        maskPanel->beginPending(first);
        /* Populate the panel's embedded settings tree, aligned to the tree's split (less
           the indent the nested panel sits at -- see kMaskPanelIndent). */
        if (MaskEditor *ed = maskPanel->editor()) {
            ed->setCaptionWidth(maskPanelCaptionWidth());
            ed->showTool(m, first, op);
            if (tool == int(MaskTool::ColorRange))
                ed->setWheelSamples(colorRangeSamplesHS(m.paramsJson));
        }
    }

    noteEdit("Add " + maskToolName(tool));
    buildTree();                 // emits maskEditBegin for the on-canvas overlay
    /* Adding a submask engages the mask panel, so the veil comes up (maskVeilEngaged).
       A second or later submask used to start HIDDEN, on the grounds that the tool's own
       live preview already showed what was being added and the whole-mask veil was just
       cost -- but the veil now distinguishes the submask being built from the rest of
       the mask and their overlap, which is precisely what a second submask needs and
       what its local preview cannot say. Must follow buildTree(), whose maskEditBegin
       drives the overlay, and precede paramsChanged(), whose tick reads the visibility
       to decide whether to build at all. */
    Q_UNUSED(first)
    noteMaskInteraction();
    emit paramsChanged();        // overlay/tint (red committed, blue pending)

    /* Object Mask starts empty and waits for a traced boundary, and the Segmenter waits
       for clicks; neither is obvious. The first 3 uses get a popup; after that the hint
       stays in the status bar only. */
    if (tool == int(MaskTool::Object) || tool == int(MaskTool::Segmenter)) {
        const bool seg = (tool == int(MaskTool::Segmenter));
        const QString hint = seg ? "Click the objects you want in the mask"
                                 : "Use the brush to outline the object(s)";
        const QString countKey = seg ? "Develop/segmenterHintCount"
                                     : "Develop/objectMaskHintCount";
        const int n = setting->value(countKey, 0).toInt();
        if (n < 3) {
            setting->setValue(countKey, n + 1);
            if (G::popup) G::popup->showPopupNow(hint, 3000);
        }
        else if (G::relay) {
            emit G::relay->updateStatus(true, hint, "DevelopProperties::beginMaskTool");
        }
    }
}

bool DevelopProperties::sharpenMaskPreviewActive() const
{
/*
    The Detail panel's Masking slider shows its edge gate while Opt is held, the way
    Lightroom's Alt-drag does: the photo is replaced by the mask in grayscale -- white is
    sharpened, black is protected -- so the gate can be set by looking at the whole frame
    instead of hunting for the transition at 1:1.

    Two conditions, both live: Opt is down, and the Masking slider is BEING WORKED (its
    handle held). Gating on the drag rather than on focus is the Lightroom behaviour and
    the reason the preview can be this loud -- it covers the photo, so it must not be able
    to sit there once you have let go.

    queryKeyboardModifiers (not keyboardModifiers) for the same reason maskOpFromModifiers
    uses it: this is called from the Opt KeyPress itself, whose event modifiers do not yet
    include the key being pressed.
*/
    if (G::operationMode != G::OperationMode::Develop) return false;
    /* A modifier held as the app loses focus is no longer ours -- the release goes to
       whoever took the focus -- so the preview drops with the window. */
    if (!isActiveWindow()) return false;
    const QModelIndex vIdx = sourceIdx.value("sharpenMasking");
    if (!vIdx.isValid()) return false;
    auto *se = static_cast<SliderEditor*>(vIdx.data(UR_Editor).value<void*>());
    if (!se || !se->sliderIsDown()) return false;
    return QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
}

void DevelopProperties::syncSharpenMaskPreview()
{
    const bool on = sharpenMaskPreviewActive();
    if (on == sharpenMaskPreviewOn) return;
    sharpenMaskPreviewOn = on;
    emit sharpenMaskPreviewChanged(on);
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
       the outcome. The FIRST submask has nothing to combine with, so it is pinned to
       Add. */
    /* Only a PENDING submask takes its op from the modifiers. A re-opened submask's op is
       already real and is changed on its own row in the submask list -- letting a stray
       Opt press rewrite it would be an invisible edit. */
    if (pendingIdx < 0) return;
    if (pendingIdx == 0) op = int(MaskOp::Add);
    /* Nothing held now: fall back to the LATCHED op rather than to Add. Releasing Opt
       must not undo the subtract the user just painted (see latchMaskOp) -- the whole
       point of the modifier is to say what the submask does, not to hold a preview open
       with a spare finger while the submask settles. */
    if (op == int(MaskOp::Add)) op = latchedMaskOp;
    if (op == pendingOp) return;
    pendingOp = op;
    if (maskPanel) maskPanel->setPendingOp(op);
    syncMaskPanel();            // the pending submask's row shows the previewed op too
    /* The render composites the pending submask with this op too (see stackJob), so the
       modifier changes the IMAGE, not just the veil: re-render as well as re-composite
       (paramsChanged is wired to updateMaskOverlayTint, so the veil comes along). */
    emit paramsChanged();
    /* Changing what the submask DOES is an edit like any other: the user gets the full
       settle window to look at the new op before it lands. */
    armMaskAutoCommit();
    noteMaskInteraction();
}

void DevelopProperties::latchMaskOp()
{
/*
    A shaping action started on the pending submask -- a brush/object stroke, or a mask
    handle drag (ImageView::maskOpActionStarted). Whatever modifier is held AT THAT
    INSTANT is what this submask does, and it now STICKS: the veil, the render and the
    on-canvas op chip all keep showing Subtract (or Intersect) after the key is
    released.

    Before this the op was purely momentary. Painting a subtract stroke and letting go of
    Opt silently turned it back into an add, the image stopped showing the subtraction,
    and the ADD is what got committed -- "the subtract brush does nothing".

    Reversible by the same gesture that set it: paint (or drag) again with nothing held
    and the latch falls back to chosenMaskOp -- what the user picked in SubmaskDialog,
    NOT to Add. The dialog is where the op is decided now; a modifier is the shortcut
    that overrides it, so letting go of the shortcut has to return to the decision, not
    overwrite it. A modifier PRESS alone still previews momentarily, so peeking at the
    outcome without committing to it works as before.
*/
    if (!maskPanelOpen) return;
    const int held = maskOpFromModifiers();
    latchedMaskOp = (pendingIdx <= 0) ? int(MaskOp::Add)    // first submask: nothing to
                  : (held == int(MaskOp::Add)) ? chosenMaskOp    // combine with
                                               : held;
    setPendingMaskOp(latchedMaskOp);
}

/*
    End the editing session on a COMMITTED submask, leaving it (and the mask) exactly as
    it is: its edits went into the mask as they were made, so there is nothing to fold in
    and nothing to discard. Reached from the panel's "Done" button and from a click on the
    open submask's row.
*/
void DevelopProperties::closeSubmaskEditing()
{
    if (G::isLogger) G::log("DevelopProperties::closeSubmaskEditing");
    if (selectedMaskIndex < 0) return;
    selectedMaskIndex = -1;
    maskLatched = true;              // the combined mask stays "O"-toggleable
    buildTree();                     // updateMaskEdit -> take the handles off canvas
    emit paramsChanged();
    /* Closing a submask is itself a mask interaction, so the veil follows engagement
       rather than being forced down here: still in the mask panel -> the mask stays
       visible (now without a submask contour, since nothing is selected); touch an
       adjustment slider and it gets out of the way on its own. */
    noteMaskInteraction();
}

void DevelopProperties::commitPendingMask(bool keepVeil)
{
    if (G::isLogger) G::log("DevelopProperties::commitPendingMask");
    maskCommitTimer->stop();                 // whoever got here beat the settle timer
    EditScope *scope = activeScope();
    /* Nothing pending -- an already committed submask being re-edited. Its edits went
       into the mask as they were made, so there is nothing to fold in: just close the
       editing session, leaving the submask (and the mask) exactly as it is. */
    if (pendingIdx < 0) { closeSubmaskEditing(); return; }
    if (!scope || pendingIdx >= scope->components.size()) return;
    /* Commit what the user is LOOKING AT: pendingOp is what the veil and the render have
       been previewing (the held modifier, else the latched op). Re-reading the live
       modifiers here instead -- as this used to -- meant a released Opt committed an
       Add. */
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
    maskPanelOpen = false;                 // nothing outstanding any more
    const QString verb = op == int(MaskOp::Subtract)  ? "Subtract "
                       : op == int(MaskOp::Intersect) ? "Intersect "
                                                      : "Add ";
    /* Recorded even though every edit that built the submask was recorded as it was
       made: the op is only written into the component HERE, and History snapshots the
       EditStack without pendingIdx, so an undo past this point would otherwise restore
       the submask with the wrong op. */
    noteEdit(verb + toolName);
    buildTree();
    emit paramsChanged();
    /* The submask is folded in. The veil follows engagement from here (the commit is a
       mask interaction), so it stays up while the user is still in the mask panel and
       leaves of its own accord when they move to an adjustment. Must follow
       paramsChanged() -- that rebuilds/re-sets the tint.

       keepVeil is now only about not DISTURBING it: the automatic commit fires while the
       user is still working on the submask -- the editing session has not ended, only
       its pending status -- so it must not even re-assert visibility mid-edit. */
    Q_UNUSED(keepVeil)
    noteMaskInteraction();
}

void DevelopProperties::armMaskAutoCommit()
{
/*
    An edit touched the pending submask: restart its settle window.

    There is no commit button because there is nothing for one to flush -- every mask
    edit (shape drag, feather, edge, invert, brush size/flow, the range sliders) is
    written straight into the MaskComponent and rendered live as it is made. The only
    thing a commit decides is the combine op, and a committed submask's op can still be
    changed from its row in the list. So the commit is not a decision worth interrupting
    the user for: it just happens once their hand stops.

    Same shape as debounceWriteTimer and MW's developFullResTimer -- restarted by every
    tick of a drag, fires once on the quiet after it.
*/
    if (pendingIdx < 0 || !maskPanelOpen) return;   // nothing pending to land
    /* Mid-stroke the timer must not fire under the user's hand: a brush stroke is one
       edit, however long they hold the button down. The stroke's end re-arms it. */
    if (maskStrokeActive) { maskCommitTimer->stop(); return; }
    maskCommitTimer->start(kMaskAutoCommitMs);
}

void DevelopProperties::setMaskStrokeActive(bool painting)
{
    maskStrokeActive = painting;
    armMaskAutoCommit();          // painting -> stop; released -> start the window
}

void DevelopProperties::autoCommitPendingMask()
{
    if (G::isLogger) G::log("DevelopProperties::autoCommitPendingMask");
    EditScope *scope = activeScope();
    if (!scope || pendingIdx < 0 || pendingIdx >= scope->components.size()) return;
    /* Picked a tool and never used it: committing would leave a submask that covers no
       pixels sitting in the list. Leave it pending -- the next edit re-arms the timer,
       and leaving drops it (flushMaskAutoCommit). */
    if (pendingMaskIsUntouched(scope->components.at(pendingIdx))) return;
    commitPendingMask(/*keepVeil=*/true);
}

void DevelopProperties::flushMaskAutoCommit()
{
/*
    Something is leaving the scope or the image the pending submask belongs to. Land it
    now rather than let the settle timer fire into a context that has moved on.

    This replaces the old "Commit submask?" dialog. That dialog existed because a pending
    submask was invisible state about to be thrown away, and only the user could say
    whether it mattered. With the commit automatic there is no third answer to ask for:
    anything the user built is kept, anything they never touched was never a submask.
*/
    maskCommitTimer->stop();
    EditScope *scope = activeScope();
    if (pendingIdx < 0) return;
    if (!scope || pendingIdx >= scope->components.size()) { cancelMaskTool(); return; }
    if (pendingMaskIsUntouched(scope->components.at(pendingIdx))) { cancelMaskTool(); return; }
    commitPendingMask(/*keepVeil=*/true);
}

void DevelopProperties::finishSubmaskEditing()
{
    if (G::isLogger) G::log("DevelopProperties::finishSubmaskEditing");
    flushMaskAutoCommit();       // do not make the user wait out the settle window
    closeSubmaskEditing();
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
    if (isStrokeTool(m.tool))
        return o.value("strokes").toArray().isEmpty();
    if (m.tool == int(MaskTool::ColorRange))
        return o.value("samples").toArray().isEmpty();
    /* ONE-CLICK masks: for Subject / Sky / Background, choosing the tool IS the whole
       edit -- the model draws the mask -- so "still as created" is not "never made".
       Test 2 used to call a fresh Sky submask untouched: the settle timer never
       committed it, and the next Add Submask flushed it away ("Cancel submask",
       2026-10-07), leaving the scope empty and the Refine Brush greyed for want of a
       mask to refine. */
    if (m.tool == int(MaskTool::Subject) || m.tool == int(MaskTool::Sky) ||
        m.tool == int(MaskTool::Background))
        return false;

    return m.tool     == pendingPristine.tool
        && m.inverted == pendingPristine.inverted
        && m.feather  == pendingPristine.feather
        && m.edge     == pendingPristine.edge
        && m.paramsJson == pendingPristine.paramsJson;
}

void DevelopProperties::cancelMaskTool()
{
    maskCommitTimer->stop();               // nothing left to land
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
    /* Cancelling can clear maskLatched (an empty scope has no mask left), which changes
       maskOverlayActive() and so the engagement answer -- but it is not an edit, so
       re-evaluate rather than engage. */
    syncMaskVeilEngagement();
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
       sibling widget, so it must be greyed explicitly. Same for the RawPanel (lab UI),
       which holds the decode controls in its own strip above the scopes list. */
    if (scopeHeader) scopeHeader->setEnabled(enabled);
    if (rawPanel) rawPanel->setEnabled(enabled);

    applyItemsEnabled(enabled);
}

void DevelopProperties::applyItemsEnabled(bool enabled)
{
    /* Caption text is painted by PropertyDelegate from the per-item UR_isEnabled role, NOT
       the widget's enabled state, so walk every row (recursively) and set the flag or the
       captions stay black over the greyed controls. buildTree() calls this too, because a
       rebuild recreates rows with the default (enabled) flag and would otherwise un-grey a
       panel that is meant to stay disabled.

       Enabling must not un-grey a row that its OWN gate disabled (setItemEnabled: the
       lens amounts without a usable profile, Point Color without a point, the denoise
       amounts): that left a black caption over a dead slider. Such a row's editor
       carries the explicit disable (WA_ForceDisabled -- set only by its own
       setEnabled(false), never inherited from the tree), so the caption follows it. */
    std::function<void(const QModelIndex &)> setRows = [&](const QModelIndex &parent) {
        const int rows = model->rowCount(parent);
        for (int r = 0; r < rows; ++r) {
            QModelIndex capIdx = model->index(r, CapColumn, parent);
            bool rowEnabled = enabled;
            if (enabled) {
                const QModelIndex vIdx = model->index(r, ValColumn, parent);
                const QWidget *editor =
                    static_cast<QWidget*>(vIdx.data(UR_Editor).value<void*>());
                if (editor && editor->testAttribute(Qt::WA_ForceDisabled))
                    rowEnabled = false;
            }
            model->setData(capIdx, rowEnabled, UR_isEnabled);
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
    /* A submask still inside its settle window on the scope being LEFT: land it before
       the switch, so the timer cannot fire into a scope the user has moved on from. */
    flushMaskAutoCommit();
    activeScopeIndex = idx;

    EditScope *l = activeScope();
    const bool hasMask = (idx > 0 && l && !l->components.isEmpty());
    /* Arriving at a scope opens no submask -- show the scope's COMBINED mask (red, no
       handles) via the latch when it has one. Its submasks are listed in the MaskPanel
       below the bar, and clicking one re-opens it. */
    selectedMaskIndex = -1;
    maskLatched = hasMask;

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
    /* Selecting a scope is an explicit "show me this mask", so it engages the panel and
       clears any hidden state left by an adjustment slider. */
    if (hasMask) noteMaskInteraction();
    else         noteNonMaskInteraction();      // Global: there is no mask to show
}

void DevelopProperties::onSpotToolToggled(bool active)
{
    if (G::isLogger) G::log("DevelopProperties::onSpotToolToggled");
    if (spotMode == active) return;
    spotMode = active;
    /* The tool is taking the canvas, so a latched Before has to come off it first --
       the same reason toggleBeforeAfter declines to go up while a tool is armed. */
    if (active) endHistoryPreview();
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
    l->showScope = l->showBasic = l->showCurves = l->showColor = l->showCalibrate =
        l->showColorGrade = l->showPointColor = l->showDetail = l->showEffects =
        l->showLens = l->showVignette = l->showGrain = l->showKaleido = true;
    noteEdit("Reset all");
    buildTree();                 // repopulate the sliders at their defaults + the eyes
    emit paramsChanged();
}

void DevelopProperties::resetAllEdits()
{
/*
    Menu "Reset all edits": return the image to its undeveloped state. Everything the
    EditStack holds goes -- every scope and its masks, the crop/straighten/warp geometry,
    the spot heals -- along with the image's session history and the winnow:Develop
    record in its sidecar.

    Like a preset apply or a settings paste, it lands on the WHOLE selection: the current
    image here, every other selected image in resetImageEdits (videos are skipped -- they
    have no develop recipe).

    Destructive and NOT undoable (the history it would be undone from is wiped too), so
    it is confirmed first, with the image count spelled out.
*/
    if (G::isLogger) G::log("DevelopProperties::resetAllEdits");
    if (currentImagePath.isEmpty()) return;

    const QStringList others = otherSelectedPaths();
    const int nImages = others.count() + 1;
    const QString file = QFileInfo(currentImagePath).fileName();

    QMessageBox box(this);
    box.setWindowTitle(tr("Reset all edits"));
    box.setIcon(QMessageBox::Warning);
    box.setText(nImages > 1
                ? tr("Delete every develop edit on the %1 selected images?").arg(nImages)
                : tr("Delete every develop edit on \"%1\"?").arg(file));
    box.setInformativeText(tr("All masks, adjustments, crop and spot repairs are removed, "
                              "the edit history is cleared and the saved settings are "
                              "deleted from the sidecar.\nThis cannot be undone."));
    QPushButton *resetBtn = box.addButton(tr("Reset"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != resetBtn) return;

    /* Land any queued multi-image batch first. Its targets were captured when the batch
       opened and need not be the current selection, so it cannot simply be dropped --
       the images it is owed keep their edits, and those inside this selection have them
       wiped again a moment later. */
    flushPropagation();

    /* A half-built mask tool indexes into the scopes about to be replaced, so retire it
       first (the same teardown applyHistoryEntry does). No prompt to commit it: the
       scope it would be committed into is being deleted anyway. */
    if (maskPanelOpen) {
        maskCommitTimer->stop();           // it must not land in a scope that is going
        EditScope *l = activeScope();
        if (l && pendingIdx >= 0 && pendingIdx < l->components.size())
            l->components.removeAt(pendingIdx);
        pendingIdx = -1;
        maskPanelOpen = false;
    }
    /* Unconditional: a submask RE-OPENED for editing is not pending, so the guard above
       misses it, but its index still points into a scope about to go. */
    selectedMaskIndex = -1;
    if (maskPanel) maskPanel->hide();
    maskLatched = false;
    previewActive = false;
    previewStack = EditStack();
    clearBeforeAfterLatch();
    comboHoverActive = false;
    if (comboHoverTimer) comboHoverTimer->stop();

    isRestoringHistory = true;          // suppress recording while the panel repopulates
    resetImageEdits(currentImagePath);
    activeScopeIndex = 0;

    refreshScopeList();
    buildTree();                        // sliders back to their defaults
    if (spotMode) emitSpotPins();       // the pins went with the spots
    isRestoringHistory = false;
    syncPropagateBase();                // identity is the new diff baseline
    if (historyView) historyView->setImage(currentImagePath);

    /* The rest of the selection. No panel work for these -- they are not on screen --
       so the stack, history and sidecar are all resetImageEdits does. */
    for (const QString &p : others) resetImageEdits(p);

    emit paramsChanged();               // full render of the untouched image

    if (nImages > 1 && G::popup)
        G::popup->showPopup("Reset all edits on " + QString::number(nImages) + " images.");
}

void DevelopProperties::resetImageEdits(const QString &fPath)
{
/*
    One image's share of "Reset all edits" (see resetAllEdits): replace its stack with a
    fresh identity stack, drop its history and clear its sidecar. Nothing here touches
    the dock, so it serves the current image and the rest of the selection alike.

    The stack is REPLACED rather than reset field by field: that is what takes the
    geometry and the spot heals with it, not just the scopes.
*/
    if (fPath.isEmpty()) return;

    EditStack s;                        // identity: one Global scope, nothing else
    s.scopes.append(EditScope());
    s.scopes[0].name = "Global";
    stackCache[fPath] = s;

    /* History: drop the whole timeline, then lay a fresh "Original" baseline for the
       state the image is now in -- but only for an image that HAS one (was visited this
       session). Seeding an unvisited image would fill the LRU with images the user never
       opened, evicting timelines they can still use. */
    if (history && history->count(fPath) > 0) {
        history->forget(fPath);
        history->seed(fPath, s);
    }
    else {
        HistoryStore::remove(fPath);     // a saved timeline from an earlier session too
    }

    /* Sidecar: written NOW rather than left to the debounce. An identity stack makes
       flushImage write an empty blob, which clears winnow:Develop (see
       Metadata::writeDevelopSidecar); the user asked for the settings to be gone, so
       they should be gone even if the app never gets a clean shutdown. */
    dirty.insert(fPath);
    flushImage(fPath);
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
    /* Global (index 0) carries the same show/hide eye as every other scope; the
       compositor honours scopes[0].enabled (see stackJob), so a disabled Global
       renders with no develop adjustments. */
    if (index < 0 || index >= s.scopes.size()) return;
    if (s.scopes[index].enabled == on) return;
    s.scopes[index].enabled = on;                 // compositor skips a disabled scope
    noteScopeEdit(s.scopes[index].name, on ? "Show" : "Hide");
    /* Push the new flag back into the header. Without this the eyes keep the state they
       was built with: the bar's eye reads its icon and its next toggle target from the
       last setScopeRows, so a stale cache made the SECOND click re-send the value already
       stored, which this slot drops as a no-op -- the eye appeared dead after one
       toggle. */
    refreshScopeList();
    emit paramsChanged();                          // re-composite with the scope on/off
}

void DevelopProperties::setTreeCollapsed(bool collapsed)
{
    /* Hide only the SCOPE items -- any root rows sitting ABOVE the Basic/Color/Effects
       sections. The sections stay visible. */
    scopeItemsCollapsed = collapsed;
    applyScopeItemsCollapsed();

    /* Solo: showing the scope's items is one of the mutually-exclusive sections, so
       collapse its peers -- the three adjustment sections and the Raw panel (they would
       otherwise stay open alongside the scope). */
    if (!collapsed) soloCollapseOthers(SoloOwner::ScopeRow);
}

int DevelopProperties::maskPanelCaptionWidth() const
{
    /* The Mask panel starts at the panel edge, like the tree, so its caption split is the
       tree's split unchanged. */
    return captionColumnWidth;
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
    for (const char *h : {"BasicHeader", "ColorHeader", "ColorPanelHeader",
                          "EffectsHeader", "LensHeader"}) {
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

void DevelopProperties::setScopeData(const ScopeData &d)
{
    if (curveEditor) curveEditor->setScopeData(d);
}

void DevelopProperties::clearScopeData()
{
    if (curveEditor) curveEditor->clearScopeData();
}

/* True only when the plot is actually on screen: the Curves section exists in the tree
   after every rebuild, so isVisible() (not just non-null) is what says the histogram
   sample is worth taking. */
bool DevelopProperties::wantsScopeData() const
{
    return curveEditor && curveEditor->isVisible();
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

    /* The splits have TWO editors -- the slider under the histogram scope and the one
       under the Curves panel's plot. Cross-push to whichever did not originate this
       change; setPositions deliberately emits nothing, so this cannot loop. The Curves
       plot also redraws its band boundaries from the new positions. */
    const QObject *src = sender();
    if (toneSlider && src != toneSlider)
        toneSlider->setPositions(shadow, crossover, highlight);
    if (curveToneSlider && src != curveToneSlider)
        curveToneSlider->setPositions(shadow, crossover, highlight);
    if (curveEditor) curveEditor->setParams(p);

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
       current scope just as picking one does -- land it first. */
    flushMaskAutoCommit();

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
    case MaskTool::RefineBrush:     return "Refine Brush";
    case MaskTool::Segmenter:       return "Segmenter";
    }
    return "Mask";
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

bool DevelopProperties::selfTestNudgeAdjustment(bool globalScope, float exposure)
{
/*
    TEST HOOK -- see the header. The tail of itemChange()'s adjustment branch, without the
    property editor: write the value into the chosen scope's params, note the edit, and
    emit paramsChanged so the coalescing proxy timer fires a render. Deliberately does NOT
    touch the tree/editors -- repopulating them would drag in the modal UI a headless
    driver cannot use, and the render path is what is under test.
*/
    if (currentImagePath.isEmpty()) return false;

    EditStack &s = stackFor(currentImagePath);
    if (s.scopes.isEmpty()) return false;
    /* Scope 0 is always Global; the last scope is the mask scope selfTestAddMaskScope
       built. With no mask scope yet, both requests fall back to Global. */
    const int idx = globalScope ? 0 : s.scopes.size() - 1;
    s.scopes[idx].params.exposure = exposure;

    /* DELIBERATELY NOT noteScopeEdit(). Mirroring the real edit's bookkeeping looks
       more faithful -- it marks the image dirty, records history and arms the debounced
       sidecar write -- but it wrecks the test. Marking dirty PERSISTS the stack, so the
       scope the driver builds survives the folder churn, and the driver's post-churn
       re-arm then adds another mask scope on top of the reloaded one. Scopes compound
       with every churn: measured 21-23 scopes and just 3 renders in 180 s, against 1-2
       scopes and 7 renders in 60 s without it. The render path is what is under test,
       and a driver that renders three times tests nothing. */
    emit paramsChanged();
    return true;
}

bool DevelopProperties::selfTestAddMaskScope(int submasks)
{
/*
    TEST HOOK -- see the header. Mirrors newScope() without its modal name dialog, then
    beginMaskTool() without showMaskMenu()'s popup, committing each submask so the scope
    ends up with `submasks` COMMITTED brush submasks and one pending. That shape is the
    point: the render cost this exercises is per-component, so a scope with one submask
    proves nothing about the caches the drag path leans on.

    IDEMPOTENT: "ensure this image has a mask scope and make it active", not "append
    one". The driver re-arms whenever the ACTIVE scope is short of components, and an
    image switch resets the active scope to Global -- so on a long run it appended a
    fresh scope on every switch. Measured: 179 switches over a 180 s run left 13-14
    scopes stacked on each image, each render composited all of them, and the render rate
    collapsed to 2 renders in 180 s. The driver's default duration (45 s, ~2 switches)
    was short enough to hide it.
*/
    if (currentImagePath.isEmpty()) return false;

    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());   // index 0 is always Global
    s.scopes[0].name = "Global";

    /* Already has one: re-activate it rather than stacking another. */
    if (s.scopes.size() > 1) {
        activeScopeIndex = s.scopes.size() - 1;
        selectedMaskIndex = -1;
        refreshScopeList();
        buildTree();
        return true;
    }

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

void DevelopProperties::placeMaskTintButton(QWidget *btn)
{
    if (maskPanel) maskPanel->setTintButton(btn);
}

bool DevelopProperties::maskOverlayActive() const
{
    /* Show the mask overlay on a mask while a tool is being edited
       (selectedMaskIndex) OR after a commit/finish latched it on (the combined result
       stays visible + 'O'-toggleable until the scope/image changes). */
    return activeScopeIndex > 0 && (selectedMaskIndex >= 0 || maskLatched);
}

/* ----------------------------------------------------------------------------------------
   Veil engagement (see maskVeilEngaged in the header for WHY)
   ---------------------------------------------------------------------------------------- */

bool DevelopProperties::maskVeilEngaged() const
{
    if (!maskOverlayActive()) return false;
    return maskPanelHovered || lastInteractionWasMask;
}

void DevelopProperties::syncMaskVeilEngagement()
{
/*
    Push the rule at the veil, but only when the answer CHANGED. Every mask mutator and
    every adjustment handler calls in here, so an unconditional emit would re-assert the
    veil's visibility dozens of times a drag -- and worse, would stamp on the user's "O"
    override on every tick instead of letting it stand.
*/
    const bool engaged = maskVeilEngaged();
    if (engaged == maskVeilEngagedNow) return;
    maskVeilEngagedNow = engaged;
    if (engaged) emit maskTintShowRequested();
    else         emit maskTintHideRequested();
}

void DevelopProperties::setMaskPanelHovered(bool hovered)
{
    if (hovered == maskPanelHovered) return;
    maskPanelHovered = hovered;
    syncMaskVeilEngagement();
}

void DevelopProperties::noteMaskInteraction()
{
    lastInteractionWasMask = true;
    syncMaskVeilEngagement();
}

void DevelopProperties::noteNonMaskInteraction()
{
    if (!lastInteractionWasMask) return;
    lastInteractionWasMask = false;
    syncMaskVeilEngagement();
}

void DevelopProperties::setMaskVeilOverride(bool shown)
{
/*
    "O" (or the Mask band's tint swatch) just toggled the veil behind our back. Record the
    veil's REAL state as the one engagement is measured against, and the override falls
    out for free: engagement no longer disagrees with the screen, so nothing is emitted
    until the user's attention actually moves -- and that first genuine transition is
    what retires the override. No separate override flag to keep in step.
*/
    maskVeilEngagedNow = shown;
}

int DevelopProperties::focusSubmaskOp() const
{
    if (pendingIdx >= 0) return pendingIdx == 0 ? int(MaskOp::Add) : pendingOp;
    const QVector<MaskComponent> comps = activeScopeComponents();
    if (selectedMaskIndex <= 0 || selectedMaskIndex >= comps.size()) return int(MaskOp::Add);
    return comps.at(selectedMaskIndex).op;
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

float DevelopProperties::activeScopeMaskEdge() const
{
    if (currentImagePath.isEmpty()) return 0.0f;
    auto it = stackCache.constFind(currentImagePath);
    if (it == stackCache.constEnd()) return 0.0f;
    const EditStack &s = it.value();
    if (activeScopeIndex < 0 || activeScopeIndex >= s.scopes.size()) return 0.0f;
    return s.scopes[activeScopeIndex].maskEdge;
}

float DevelopProperties::activeScopeMaskHalo() const
{
    if (currentImagePath.isEmpty()) return 0.0f;
    auto it = stackCache.constFind(currentImagePath);
    if (it == stackCache.constEnd()) return 0.0f;
    const EditStack &s = it.value();
    if (activeScopeIndex < 0 || activeScopeIndex >= s.scopes.size()) return 0.0f;
    return s.scopes[activeScopeIndex].maskHalo;
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
    case PV_Curves:     return EditParams::Group::Curves;
    case PV_Color:      return EditParams::Group::Color;
    case PV_Calibrate:  return EditParams::Group::Calibrate;
    case PV_ColorGrade: return EditParams::Group::ColorGrade;
    case PV_PointColor: return EditParams::Group::PointColor;
    case PV_Detail:     return EditParams::Group::Detail;
    case PV_Effects:    return EditParams::Group::Effects;
    case PV_Lens:       return EditParams::Group::Lens;
    default:          return EditParams::Group::Basic;   // PV_Basic (+ PV_Scope, unused)
    }
}

/* Section name for history captions ("Reset Basic", "Hide Effects"). */
QString DevelopProperties::groupLabel(int group)
{
    switch (group) {
    case PV_Curves:     return "Curves";
    case PV_Color:      return "Color";
    case PV_Calibrate:  return "Calibrate";
    case PV_ColorGrade: return "Color Grade";
    case PV_PointColor: return "Point Color";
    case PV_ColorPanel: return "Color";
    case PV_Detail:     return "Detail";
    case PV_Effects:    return "Effects";
    case PV_Lens:       return "Lens";
    case PV_Scope:      return "all";
    default:            return "Basic";
    }
}

bool *DevelopProperties::previewFlag(EditScope *l, int group)
{
    if (!l) return nullptr;
    switch (group) {
    case PV_Scope:      return &l->showScope;
    case PV_Curves:     return &l->showCurves;
    case PV_Color:      return &l->showColor;
    case PV_Calibrate:  return &l->showCalibrate;
    case PV_ColorGrade: return &l->showColorGrade;
    case PV_PointColor: return &l->showPointColor;
    case PV_ColorPanel: return nullptr;     // three flags: see togglePreviewSection
    case PV_Detail:     return &l->showDetail;
    case PV_Effects:    return &l->showEffects;
    case PV_Lens:       return &l->showLens;
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

BarBtn *DevelopProperties::makeSectionMenuBtn(int group)
{
    BarBtn *b = new BarBtn();
    b->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    b->setToolTip("Section actions (reset, help)");
    connect(b, &BarBtn::clicked, this, [this, group]{ showSectionMenu(group); });
    return b;
}

void DevelopProperties::showSectionMenu(int group)
{
    if (G::isLogger) G::log("DevelopProperties::showSectionMenu");
    const QString label = groupLabel(group);

    /* No show/hide item here: the eye button beside this menu already toggles the
       section's changes. */
    QMenu menu(this);
    /* The combined Color panel resets the mode on show first, then all three. */
    static const char *kModeName[] = {"Tone", "Hue", "RGB"};
    QAction *aResetMode = nullptr;
    if (group == PV_ColorPanel) {
        aResetMode = menu.addAction(tr("Reset %1").arg(kModeName[colorPanelMode]));
        aResetMode->setEnabled(!currentImagePath.isEmpty());
    }
    QAction *aReset = menu.addAction(group == PV_ColorPanel ? tr("Reset Tone, Hue and RGB")
                                                            : tr("Reset %1").arg(label));
    aReset->setEnabled(!currentImagePath.isEmpty());
    /* The layout switch, while the combined panel awaits sign-off: offered on every
       colour section so either layout leads back to the other. */
    QAction *aLayout = nullptr;
    if (group == PV_ColorPanel || group == PV_Color || group == PV_PointColor ||
        group == PV_Calibrate || group == PV_ColorGrade) {
        menu.addSeparator();
        aLayout = menu.addAction(combinedColorPanel ? tr("Use Separate Color Panels")
                                                    : tr("Use Combined Color Panel"));
    }
    /* Every section carries its own help page (Docs/develop<section>help.html), always
       the last item, so "what does this panel do?" is one click from the panel. */
    menu.addSeparator();
    QAction *aHelp = menu.addAction(group == PV_ColorPanel
                                        ? tr("%1 help").arg(kModeName[colorPanelMode])
                                        : tr("%1 help").arg(label));

    QAction *chosen = menu.exec(QCursor::pos());
    if (!chosen) return;
    if (chosen == aResetMode) {
        const int g = colorPanelModeGroup();
        QTimer::singleShot(0, this, [this, g]{ resetSection(g); });
        return;
    }
    if (chosen == aLayout) {
        QTimer::singleShot(0, this, [this]{ setCombinedColorPanel(!combinedColorPanel); });
        return;
    }
    /* Reset repopulates the tree, which destroys the button this click is being
       handled in, so fire on the next tick (same rule as the scope + submask rows). */
    if (chosen == aReset)
        QTimer::singleShot(0, this, [this, group]{ resetSection(group); });
    else if (chosen == aHelp)
        sectionHelp(group);
}

namespace {
/* The Effects dropdown's entries, in combo order: the item text and the slider rows each
   one shows. applyEffectsChoice hides every other entry's rows. */
struct EffectDef { const char *label; QStringList rows; bool globalOnly; };
/* Indexed by effect id (effectsChoice, the combo items' data). */
enum EffectId { EffectVignette = 0, EffectGrain = 1, EffectKaleido = 2 };
const QVector<EffectDef> &effectDefs()
{
    static const QVector<EffectDef> defs = {
        {"Vignette",     {"vignetteExposure", "vignetteFeather"}, false},
        {"Grain",        {"grainAmount", "grainSize", "grainRoughness"}, false},
        /* Geometry, not EditParams (Geometry::kaleido*): whole-image, so Global only. */
        {"Kaleidoscope", {"kaleidoSegments", "kaleidoAngle", "kaleidoSource"}, true},
    };
    return defs;
}

/* True when effect `e` is off its defaults (its " *"). The kaleidoscope counts only on
   Global, where it lives. */
bool effectEdited(const EditStack &s, int scopeIdx, int e)
{
    const EditParams def;
    const EditParams &p = s.scopes.at(scopeIdx).params;
    switch (e) {
    case EffectVignette:
        return p.vignetteExposure != def.vignetteExposure ||
               p.vignetteFeather != def.vignetteFeather;
    case EffectGrain:
        return p.grainAmount != def.grainAmount || p.grainSize != def.grainSize ||
               p.grainRoughness != def.grainRoughness;
    case EffectKaleido:
        return scopeIdx == 0 && s.geometry.hasKaleido();
    }
    return false;
}
}  // namespace

void DevelopProperties::refreshPreviewButtons()
{
    EditScope *l = activeScope();
    auto set = [](BarBtn *b, bool shown) {
        if (!b) return;
        b->setIcon(shown ? ":/images/icon16/eye.png" : ":/images/icon16/eye_off.png", G::iconOpacity);
    };
    set(basicEyeBtn,    l ? l->showBasic    : true);
    set(curvesEyeBtn,   l ? l->showCurves   : true);
    set(colorEyeBtn,    l ? l->showColor    : true);
    set(calibrateEyeBtn,  l ? l->showCalibrate  : true);
    set(colorGradeEyeBtn, l ? l->showColorGrade : true);
    set(pointColorEyeBtn, l ? l->showPointColor : true);
    /* The combined panel's eye is open only while all three of its groups show. */
    set(colorPanelEyeBtn, l ? (l->showColorGrade && l->showPointColor && l->showCalibrate)
                            : true);
    set(detailEyeBtn,   l ? l->showDetail   : true);
    set(effectsEyeBtn,  l ? l->showEffects  : true);
    if (effectEyeBtn) {
        const bool *f = effectShowFlag(l, shownEffect());
        set(effectEyeBtn, f ? *f : true);
        effectEyeBtn->setToolTip(QString("Show or hide %1")
                                     .arg(effectDefs().at(shownEffect()).label));
    }
    set(lensEyeBtn,     l ? l->showLens     : true);
    /* The whole-mask eye lives in the ScopeHeader widget, not the tree. */
    if (scopeHeader) scopeHeader->setPreviewShown(l ? l->showScope : true);
}

void DevelopProperties::togglePreviewSection(int group)
{
    if (G::isLogger) G::log("DevelopProperties::togglePreviewSection");
    EditScope *l = activeScope();
    if (group == PV_ColorPanel) {
        /* One eye for three groups: any showing -> hide all, else show all. */
        if (!l) return;
        const bool show = !(l->showColorGrade && l->showPointColor && l->showCalibrate);
        l->showColorGrade = l->showPointColor = l->showCalibrate = show;
        noteEdit((show ? "Show " : "Hide ") + groupLabel(group));
        refreshPreviewButtons();
        emit paramsChanged();
        return;
    }
    bool *f = previewFlag(l, group);
    if (!f) return;
    *f = !*f;                     // non-destructive: values kept, only the fold changes
    noteEdit((*f ? "Show " : "Hide ") + groupLabel(group));
    refreshPreviewButtons();
    emit paramsChanged();                   // re-render with the folded params
    if (group == PV_Effects) emit kaleidoToolChanged();   // Global's eye hides the fold
}

void DevelopProperties::resetSection(int group)
{
    if (G::isLogger) G::log("DevelopProperties::resetSection");
    if (currentImagePath.isEmpty()) return;
    EditParams &p = activeParams();
    if (group == PV_Scope) {
        EditParams::resetGroup(p, EditParams::Group::Basic);
        EditParams::resetGroup(p, EditParams::Group::Color);
        EditParams::resetGroup(p, EditParams::Group::Calibrate);
        EditParams::resetGroup(p, EditParams::Group::ColorGrade);
        EditParams::resetGroup(p, EditParams::Group::PointColor);
        EditParams::resetGroup(p, EditParams::Group::Detail);
        EditParams::resetGroup(p, EditParams::Group::Effects);
        EditParams::resetGroup(p, EditParams::Group::Lens);
    }
    else if (group == PV_ColorPanel) {
        EditParams::resetGroup(p, EditParams::Group::ColorGrade);
        EditParams::resetGroup(p, EditParams::Group::PointColor);
        EditParams::resetGroup(p, EditParams::Group::Calibrate);
    }
    else {
        EditParams::resetGroup(p, paramsGroup(group));
    }
    /* Global's Effects (or the whole Global scope) includes the kaleidoscope, which
       lives in Geometry: off, with its settings back to the defaults. */
    if ((group == PV_Effects || group == PV_Scope) && activeScopeIndex == 0) {
        Geometry &g = stackCache[currentImagePath].geometry;
        g.copyKaleidoFrom(Geometry());
    }
    noteEdit("Reset " + groupLabel(group));
    populateSlidersFromStack();      // reflect the restored defaults (sliders + tone)
    emit paramsChanged();
}

void DevelopProperties::bindPanelContextMenu(QWidget *container, TransformPanel *transform,
                                             ReplacePanel *replace)
{
    if (G::isLogger) G::log("DevelopProperties::bindPanelContextMenu");
    panelContainer = container;
    transformPanel = transform;
    replacePanel = replace;
    if (panelContainer) panelContainer->installEventFilter(this);
    /* The Transform panel's [?] had no handler; it opens the same page its context menu
       does. (The Raw panel's [?] is wired in bindRawPanel, the Replace panel's in MW.) */
    if (transformPanel)
        connect(transformPanel, &TransformPanel::tipsRequested,
                this, &DevelopProperties::transformHelp, Qt::UniqueConnection);
}

/*
    Which band of the dock was right-clicked. Walks UP the widget tree from the widget
    under the cursor: a click lands on a label, a slider or a button, and what matters is
    which panel that control belongs to. The mask panel and this tree are both children of
    the scope bar (setRowDetail), so they are tested before it -- the innermost band down
    the chain is the one that answers.
*/
DevelopProperties::PanelSection DevelopProperties::sectionAt(QWidget *w)
{
    for (; w; w = w->parentWidget()) {
        if (w == this)                             return SectionEdits;
        if (maskPanel && w == maskPanel)           return SectionMask;
        if (scopeHeader && w == scopeHeader)       return SectionScopeBar;
        if (rawPanel && w == rawPanel)             return SectionRaw;
        if (transformPanel && w == transformPanel) return SectionTransform;
        if (replacePanel && w == replacePanel)     return SectionReplace;
        if (w == panelContainer) break;      // reached the dock: the action row or a gap
    }
    return SectionOther;
}

/*
    The Develop dock's context menu, for a right-click ANYWHERE in the dock (the scopes
    strip is the one exception: it answers with its own layout menu).

    The menu carries only what the clicked band can actually do -- its reset, its help --
    followed by the panel-wide items (the settings clipboard, presets, help), which make
    sense from anywhere. No show/hide item anywhere: every band that can be bypassed
    carries an eye button, and a menu duplicate of it only made the menus longer.
*/
void DevelopProperties::showPanelContextMenu(PanelSection section, const QPoint &globalPos,
                                             const QModelIndex &treeIdx)
{
    if (G::isLogger) G::log("DevelopProperties::showPanelContextMenu");
    const bool haveImage = !currentImagePath.isEmpty();

    QMenu menu(this);

    /* -------- Edits: the clicked section header's own Reset ("Reset Basic") -------- */
    int group = -1;
    QAction *aResetSection = nullptr;
    if (section == SectionEdits && treeIdx.isValid()) {
        const QString name = treeIdx.data(UR_Name).toString();
        if      (name == "BasicHeader")      group = PV_Basic;
        else if (name == "CurvesHeader")     group = PV_Curves;
        else if (name == "ColorHeader")      group = PV_Color;
        else if (name == "CalibrateHeader")  group = PV_Calibrate;
        else if (name == "ColorGradeHeader") group = PV_ColorGrade;
        else if (name == "PointColorHeader") group = PV_PointColor;
        else if (name == "ColorPanelHeader") group = PV_ColorPanel;
        else if (name == "DetailHeader")     group = PV_Detail;
        else if (name == "EffectsHeader")    group = PV_Effects;
        else if (name == "LensHeader")       group = PV_Lens;
        if (group >= 0) {
            aResetSection = menu.addAction(tr("Reset %1").arg(groupLabel(group)));
            aResetSection->setEnabled(haveImage);
            menu.addSeparator();
        }
    }

    /* -------- The scope bands: the mask band's reset, the scope's, the panel's -------- */
    QAction *aResetScope = nullptr, *aResetMask = nullptr, *aResetAll = nullptr;
    if (section == SectionScopeBar || section == SectionMask) {
        EditScope *l = activeScope();
        const QString scopeName = l ? l->name : tr("scope");
        if (section == SectionMask) {
            aResetMask = menu.addAction(tr("Reset mask Edge and Halo"));
            aResetMask->setEnabled(haveImage && l != nullptr);
        }
        aResetScope = menu.addAction(tr("Reset %1").arg(scopeName));
        aResetScope->setEnabled(haveImage && l != nullptr);
        aResetAll = menu.addAction(tr("Reset all edits"));
        aResetAll->setEnabled(haveImage);
        menu.addSeparator();
    }

    /* -------- Transform / Replace: their header buttons, reachable by right-click -------- */
    QAction *aResetTransform = nullptr, *aReplaceTips = nullptr;
    if (section == SectionTransform && transformPanel) {
        aResetTransform = menu.addAction(tr("Reset crop, straighten, perspective and kaleidoscope"));
        aResetTransform->setEnabled(haveImage);
        menu.addSeparator();
    }
    if (section == SectionReplace && replacePanel) {
        aReplaceTips = menu.addAction(tr("Fill Replace tips"));
        menu.addSeparator();
    }

    /* -------- Tree shape: only where there is a tree to expand -------- */
    QAction *aExpandAll = nullptr, *aCollapseAll = nullptr, *aSolo = nullptr;
    if (section == SectionEdits || section == SectionScopeBar || section == SectionMask) {
        aExpandAll = menu.addAction(tr("Expand all"));
        aCollapseAll = menu.addAction(tr("Collapse all"));
        aSolo = menu.addAction(tr("Solo mode"));
        aSolo->setCheckable(true);
        aSolo->setChecked(setting->value("Develop/isSolo", false).toBool());
    }

    /* Jump to the raw decode rows. They are Core rows on the Global scope (addCoreItems),
       so this activates Global and un-collapses it. Raw files only, and only while editing
       raw (applyCoreVisibility hides them when editing the embedded preview). The Winnow
       engine adds the raw-denoise rows alongside Demosaic, hence the longer caption. It is
       offered from the tree, where the rows live -- the Raw panel does not need it, being
       the controls themselves. */
    QAction *aRawDemosaic = nullptr;
    if (section == SectionEdits && currentIsRaw() && G::useRaw) {
        const bool winnow = !currentRawRender().apple;
        if (!menu.isEmpty()) menu.addSeparator();
        aRawDemosaic = menu.addAction(winnow ? tr("Raw demosaic and denoise")
                                             : tr("Raw demosaic"));
    }

    /* -------- Panel-wide: the settings clipboard and presets, from anywhere -------- */
    /* Copy / paste the ticked settings between images (also Cmd+Opt+C / Cmd+Opt+V), and
       save them as a reusable preset (also Cmd+Shift+N). Paste names its source, so the
       menu itself says what is on the clipboard. */
    if (!menu.isEmpty()) menu.addSeparator();
    QAction *aCopySettings = menu.addAction(tr("Copy Develop Settings…"));
    aCopySettings->setEnabled(haveImage && !currentIsIdentity());
    const QString from = copiedSettingsSource();
    QAction *aPasteSettings = menu.addAction(
        from.isEmpty() ? tr("Paste Develop Settings")
                       : tr("Paste Develop Settings from ") + from);
    aPasteSettings->setEnabled(haveImage && hasCopiedSettings());
    QAction *aSavePreset = menu.addAction(tr("Save Develop Preset…"));
    aSavePreset->setEnabled(haveImage && !currentIsIdentity());

    /* Help goes to the page for the band that was clicked: the Raw and Transform panels
       document themselves, everything else lands on the module page. */
    menu.addSeparator();
    QAction *aHelp = menu.addAction(section == SectionRaw       ? tr("Raw help")
                                  : section == SectionTransform ? tr("Transform help")
                                                                : tr("Develop help"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == nullptr) return;
    /* A reset repopulates the tree, which can destroy the widget this click is being
       handled in, so the resets fire on the next tick (the same rule the scope, submask
       and section [:] menus follow). */
    else if (chosen == aResetSection)
        QTimer::singleShot(0, this, [this, group]{ resetSection(group); });
    else if (chosen == aResetMask)
        QTimer::singleShot(0, this, [this]{ resetMaskLevel(); });
    else if (chosen == aResetScope)
        QTimer::singleShot(0, this, [this]{ resetActiveScope(); });
    else if (chosen == aResetAll)
        QTimer::singleShot(0, this, [this]{ resetAllEdits(); });
    /* The Transform reset and the Replace tips belong to those panels: emit their own
       signals so the handler MW already wires up does the work -- identical to clicking
       the panel's [R] / [?] button. */
    else if (chosen == aResetTransform)
        QTimer::singleShot(0, transformPanel, [this]{ emit transformPanel->resetRequested(); });
    else if (chosen == aReplaceTips)
        QTimer::singleShot(0, replacePanel, [this]{ emit replacePanel->tipsRequested(); });
    else if (chosen == aExpandAll)     setAllSectionsExpanded(true);
    else if (chosen == aCollapseAll)   setAllSectionsExpanded(false);
    else if (chosen == aCopySettings)  copyDevelopSettings();
    else if (chosen == aPasteSettings) pasteDevelopSettings();
    else if (chosen == aSavePreset)    saveDevelopPreset();
    else if (chosen == aRawDemosaic)   showRawDemosaic();
    else if (chosen == aHelp) {
        if      (section == SectionRaw)       rawHelp();
        else if (section == SectionTransform) transformHelp();
        else                                  howThisWorks();
    }
    else if (chosen == aSolo) {
        setSolo(aSolo->isChecked());
        setting->setValue("Develop/isSolo", aSolo->isChecked());
    }
}

void DevelopProperties::contextMenuEvent(QContextMenuEvent *event)
{
    if (G::isLogger) G::log("DevelopProperties::contextMenuEvent");
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid()) idx = model->index(idx.row(), CapColumn, idx.parent());
    showPanelContextMenu(SectionEdits, event->globalPos(), idx);
}

void DevelopProperties::showRawDemosaic()
{
    if (G::isLogger) G::log("DevelopProperties::showRawDemosaic");
    /* The Demosaic / raw-denoise controls live in the Raw panel, not the tree, so just
       un-collapse it -- under Solo that folds its peers away. */
    if (rawPanel && currentIsRaw()) {   // == syncRawPanel's test
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
    if (name != "BasicHeader" && name != "ColorHeader" && name != "ColorPanelHeader" &&
        name != "EffectsHeader" && name != "LensHeader") return;
    soloCollapseOthers(SoloOwner::Section, name);
}

/*
    Adding a submask asks TWO questions, and this asks them together: what the submask
    does to the mask (Add / Subtract / Intersect) and what kind of submask it is.

    It used to be a bare pop-up menu of tool names, with the op left to a modifier held
    at some later instant -- undiscoverable (nothing said Opt subtracts until you were
    already painting) and easy to lose (release the key at the wrong moment and the
    subtract you drew became an add). SubmaskDialog puts the op in words beside the type,
    and the modifiers stay as the shortcut: holding one still overrides the chosen op
    while shaping (latchMaskOp), and releasing it falls back to what was chosen here
    rather than to Add.
*/
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

    int op = int(MaskOp::Add), tool = -1;
    if (!SubmaskDialog::choose(this, scope->components.isEmpty(), op, tool)) return;
    onMaskToolChosen(tool, op);
    // a submask just created by request: show the overlay so the user sees it build
    emit maskOverlayShowRequested();
}

void DevelopProperties::deleteMask(int index)
{
    EditScope *scope = activeScope();
    if (!scope || index < 0 || index >= scope->components.size()) return;
    const QString goneTool = maskToolName(scope->components.at(index).tool);
    scope->components.removeAt(index);
    if      (selectedMaskIndex == index) selectedMaskIndex = -1;   // its settings close
    else if (selectedMaskIndex >  index) selectedMaskIndex--;      // follow the tool
    /* Deleting the submask that has not settled yet is the "changed my mind" path now
       that there is no Cancel button: retire the settle timer with it. */
    if      (pendingIdx == index) { maskCommitTimer->stop(); pendingIdx = -1; maskPanelOpen = false; }
    else if (pendingIdx >  index) pendingIdx--;

    /* That was the mask's last submask: a mask with no submasks covers nothing, so the
       mask goes with it. Done here rather than via deleteScope so History records ONE
       step -- two would let a single undo land on an empty mask. */
    if (scope->components.isEmpty() && activeScopeIndex > 0) {
        EditStack &s = stackCache[currentImagePath];
        const QString goneName = scope->name;
        s.scopes.removeAt(activeScopeIndex);
        if (activeScopeIndex >= s.scopes.size()) activeScopeIndex = s.scopes.size() - 1;
        selectedMaskIndex = -1;
        maskLatched = false;
        noteScopeEdit(QString(), "Delete " + goneName);

        refreshScopeList();
        buildTree();
        emit paramsChanged();
        noteMaskInteraction();
        return;
    }

    noteEdit("Delete " + goneTool);

    buildTree();
    emit paramsChanged();       // less mask coverage on the scope -> re-composite
    noteMaskInteraction();
}

void DevelopProperties::setSelectedMask(int index)
{
    selectedMaskIndex = index;
    buildTree();            // rebuild so the newly-selected tool shows its settings
}

bool DevelopProperties::escapeMaskTool()
{
    /* Esc while a submask has not settled yet discards it. The window is short, but it
       is the one moment where "that is not what I wanted" costs nothing -- after it, the
       submask's row [:] > Delete (or History) is the way back. */
    if (maskPanelOpen) { cancelMaskTool(); return true; }
    /* A committed submask has nothing to discard: Esc just closes its editing session. */
    if (selectedMaskIndex >= 0) {
        closeSubmaskEditing();
        return true;
    }
    if (activeMaskTool() < 0) return false;   // no mask tool expanded
    setSelectedMask(-1);                      // collapse it (as clicking its caption)
    return true;
}

void DevelopProperties::cancelCanvasTools()
{
    if (G::isLogger) G::log("DevelopProperties::cancelCanvasTools");
    cancelWbDropper();
    cancelDetailPick();
    cancelPointColorPick();
    if (spotMode) onSpotToolToggled(false);
    setCurveSamplerActive(false);
    /* escapeMaskTool steps down one level per call (discard a pending submask, close a
       committed submask's session, collapse the tool) -- run it until nothing is left. */
    for (int i = 0; i < 3 && escapeMaskTool(); ++i) {}
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
    if (isBrushCanvasTool(tool)) {
        /* Freehand: starts empty. size/flow are 0..100 (current settings for the NEXT stroke; feather
           lives in MaskComponent.feather). strokes is the accumulated path list -- each stroke
           snapshots the settings it was painted with. Points are normalized output coords. */
        QJsonObject o;
        o["size"] = 20;
        o["flow"] = 100;
        o["autoMask"] = false;
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
    if (tool == int(MaskTool::Segmenter)) {
        /* Nothing selected yet: the objects are found when the tool opens, and each
           click adds or removes one outline (Utilities/segmenter.h). */
        return Segmenter::paramsWith(QJsonArray());
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
    if (!isStrokeTool(m.tool)) return;
    /* Quantised to ImageView's 0.1% step (the canvas gesture already is) -- the old
       whole-percent round made the smallest brush ~1% of the long edge. */
    const double s = qBound(0.1, qRound(size * 10.0) / 10.0, 100.0);
    /* The canvas gesture (wheel / two-finger drag / [ ]) ALWAYS sizes the BRUSH -- the
       next stroke -- never the strokes already painted. It used to follow the panel
       slider's split and resize every stroke once the submask had landed, but a submask
       lands on its own (auto-commit) while the user is still painting, so the same
       wheel that sized the cursor a moment ago suddenly grew or shrank the whole mask
       (2026-10-14). Resizing what is there is the panel slider's job (its header says
       "applies to N strokes"). No re-render: nothing painted changed. */
    m.paramsJson = brushWith(m.paramsJson, "size", s);
    noteEdit("Brush size", QString::number(s, 'f', 1), "mask/brushSize");
    syncMaskSlider("maskSize", s);
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
        !isBrushCanvasTool(m.tool)) return;
    const int f = qBound(0, int(feather + 0.5), 100);
    if (int(m.feather + 0.5f) == f) return;
    m.feather = f;
    noteEdit("Feather", QString::number(f), "mask/maskFeather");
    syncMaskSlider("maskFeather", f);
    /* Brush feather from the canvas belongs to the NEXT stroke (each stroke keeps its
       own snapshot), pending or landed -- see setActiveBrushSize for why the canvas
       gesture never re-feathers strokes already painted; the panel slider does that. So
       it only refreshes the cursor. A gradient's coverage always changed -> re-composite.
       Either way ImageView already holds the new value, so no maskFeatherChanged echo. */
    if (isBrushCanvasTool(m.tool)) emitBrushSettings(m);
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
    if (!isBrushCanvasTool(m.tool)) return;
    /* Next stroke while the submask is being built; retroactive on a re-opened one (see
       onMaskEditorSetting). */
    const bool retro = (pendingIdx < 0);
    m.paramsJson = retro ? brushStrokesWith(m.paramsJson, "autoMask", on)
                         : brushWith(m.paramsJson, "autoMask", on);
    noteEdit("Auto mask", on ? "On" : "Off");
    /* The checkbox the user sees is the MASK PANEL's (its embedded MaskEditor), so sync
       that one -- updating only the tree row, as this did, left the panel's box stale
       when A toggled auto mask on the canvas. Same pair syncMaskSlider updates. */
    if (maskPanel && maskPanel->editor())
        maskPanel->editor()->setCheckboxValue("maskAutoMask", on);   // guards itself
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
                                  brushBool(m.paramsJson, "autoMask", false));
}

void DevelopProperties::setMaskOverlayShown(bool shown)
{
    /* ImageView flipped the red coverage tint ("O", an adjustment slider, or a new mask
       edit); mirror it so the scope menu's "Show mask overlay" row checks correctly. */
    if (scopeHeader) scopeHeader->setMaskOverlayShown(shown);
}

void DevelopProperties::setMaskOverlayColour(const QColor &c)
{
    /* The overlay colour, set from the Mask band tint swatch's context menu: persist it
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
            m.tool == int(MaskTool::Object) || m.tool == int(MaskTool::Segmenter)) {
            emit maskEditBegin(m.tool, m.op, m.inverted, m.paramsJson, m.feather);
            return;
        }
        /* The Refine Brush IS a brush on the canvas: hand ImageView the Brush tool so
           every paint path, cursor and shortcut there serves it unchanged. */
        if (m.tool == int(MaskTool::RefineBrush)) {
            emit maskEditBegin(int(MaskTool::Brush), m.op, m.inverted, m.paramsJson,
                               m.feather);
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
    QString incoming = paramsJson;
    /* ImageView paints a Refine Brush as a plain brush, so the JSON it sends back has no
       "mode" -- carry the submask's own across or every stroke would reset Fix Edge. */
    if (scope->components[selectedMaskIndex].tool == int(MaskTool::RefineBrush)) {
        const QString mode = QJsonDocument::fromJson(
            scope->components[selectedMaskIndex].paramsJson.toUtf8())
            .object().value("mode").toString();
        if (!mode.isEmpty()) incoming = brushWith(incoming, "mode", mode);
    }
    if (scope->components[selectedMaskIndex].paramsJson == incoming) {
        /* Nothing new to store -- but the VEIL may still be behind. A gesture that
           pauses before the mouse comes up (paint, stop, let go) has its whole geometry
           carried by the throttled live emit, so the release emit arrives identical and
           would land here. That live tick's veil rebuild was skipped mid-stroke
           (MW::updateMaskOverlayTint's in-flight early-out), so without this nothing
           ever rebuilt it with the finished stroke: the render showed the stroke, the
           overlay did not. */
        emit maskOverlayRefreshRequested();
        return;
    }
    scope->components[selectedMaskIndex].paramsJson = incoming;
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
    /* A stroke changes the count the settings header names ("applies to 3 strokes");
       nothing else refreshes it between panel rebuilds. */
    syncAttributeScopeLabel();
    /* Every shape edit -- handle drag, brush stroke, pipette pick -- arrives here, so
       this is the one place the settle timer needs arming for geometry -- and the one
       place canvas mask editing registers as engagement, which is what lets the user
       move off the panel onto the photo without the veil dropping. */
    armMaskAutoCommit();
    noteMaskInteraction();
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
                    if (!isMaskSlider) noteNonMaskInteraction();
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

    /* NO CONTAINMENT RAIL: the scope bar sits directly above this tree, so there is no
       list of scopes for a bracket to pick the editor's owner out of (see
       ScopeHeader::paintEvent, which draws none either). All this paints is the rule that
       closes the block.
       Start from the topmost VISIBLE row (indexAt), not model row 0: the Develop tree
       hides rows, and indexBelow on a hidden index returns an invalid index, which would
       end the walk immediately. */
    QModelIndex last = indexAt(QPoint(0, 0));
    if (!last.isValid()) return;
    for (QModelIndex below = indexBelow(last); below.isValid(); below = indexBelow(below))
        last = below;
    const int bottom = qMin(visualRect(last).bottom() + 1, viewport()->height());
    if (bottom <= 0) return;

    QPainter p(viewport());
    const int ruleY = bottom + kBlockCloseGap;
    /* Close the block: the same separator every Develop panel carries along its bottom
       edge (G::panelBorderHeight in G::panelSeparatorColor), just under the last row --
       Effects is the last section, so this is the Effects panel's bottom border. Only
       when there is empty dock below it -- if the rows fill or overflow the viewport,
       the panel edge already ends the block and a rule pinned to the last visible row
       would be a false bottom. In FIT mode the viewport is exactly the rows plus the
       band sizeHint reserves for this rule, so it always draws. */
    if (fitToContents || ruleY < viewport()->height() - G::panelBorderHeight) {
        p.fillRect(0, ruleY, viewport()->width(), G::panelBorderHeight,
                   G::panelSeparatorColor);
    }
}

void DevelopProperties::drawBranches(QPainter *, const QRect &, const QModelIndex &) const
{
    /* Intentionally empty: draw no native branch indicator. Every expandable Develop row
       draws its own winnow arrow through PropertyDelegate (the section headers), so the
       native triangle is redundant; where a row's background fill is skipped (
       isAlternatingRows) it also showed through beside the delegate arrow, giving a
       doubled, oversized decoration. */
}

/* ----------------------------------------------------------------------------------------
   Item builders
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addGroupDivider(QModelIndex parIdx, QString parentName,
                                        QString name)
{
    addDivider(G::groupSeparatorHeight, G::groupSeparatorWidth, G::groupSeparatorColor,
               parIdx, parentName, name);
}

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
        BarBtn *eye = makeEyeBtn("Show or hide these changes", previewGroup);
        if      (previewGroup == PV_Basic)    basicEyeBtn    = eye;
        else if (previewGroup == PV_Curves)   curvesEyeBtn   = eye;
        else if (previewGroup == PV_Color)    colorEyeBtn    = eye;
        else if (previewGroup == PV_Calibrate)  calibrateEyeBtn  = eye;
        else if (previewGroup == PV_ColorGrade) colorGradeEyeBtn = eye;
        else if (previewGroup == PV_PointColor) pointColorEyeBtn = eye;
        else if (previewGroup == PV_ColorPanel) colorPanelEyeBtn = eye;
        else if (previewGroup == PV_Detail)   detailEyeBtn   = eye;
        else if (previewGroup == PV_Effects)  effectsEyeBtn  = eye;
        else if (previewGroup == PV_Lens)     lensEyeBtn     = eye;
        btns.append(eye);
        btns.append(makeSectionMenuBtn(previewGroup));   // menu last, as on every band
        /* Same gap the scope + submask ROWS put between their [eye] and [:], so the pair
           lines up down the whole panel (BarBtnEditor resets this with btns). */
        btnsSpacing = G::headerBtnGap;
        i.hasValue = true;
        i.delegateType = DT_BarBtns;
    }
    else {
        i.hasValue = false;
        i.delegateType = DT_None;
    }
    addItem(i);
    /* Offset under the Edits bar: the bar's arrow folds these sections away, so they are
       its children and sit G::subHeaderIndent right of it, arrow and title, on top of the
       G::headerLeftInset every header in the dock clears the panel edge by (the delegate
       reads UR_ExtraIndent on the header caption and shifts both; 0 = flush). Only the
       header content shifts -- its child rows keep their own indentation, so the sliders
       always keep their full width. */
    model->setData(capIdx, QVariant(G::headerLeftInset + G::subHeaderIndent), UR_ExtraIndent);

    /* A plain spacer (no rule) as the section's FIRST child, so the first row does not
       butt up against the header band. HALF a group divider (lineHeight 0, nothing
       drawn): the band plays the part of the divider's rule, so the first caption sits
       the same distance below the band as a caption sits below a divider's rule
       (2026-10-14; a full divider height left a visibly bigger gap). addDivider leaves
       capIdx on the divider, so restore the header index: every caller takes
       parIdx = capIdx on return. */
    const QModelIndex hdrIdx = capIdx;
    addDivider(G::groupSeparatorHeight / 2, 0, Qt::transparent, hdrIdx, name,
               name + "TopSpacer");
    capIdx = hdrIdx;
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
/*
    The current image's colour characterisation, with AS SHOT SOLVED THE WAY THE RENDER
    SOLVES IT. The cached image's asShotK/asShotTint were solved at decode against the
    built-in per-model matrix; when a camera profile is selected, the render re-solves
    them against the profile's matrices (WorkingImageCache's attachProfile) and renders
    at THAT temperature. Returning the cached pair made the Temp box show the built-in
    answer under a profile -- 5020 K for an E-M1 frame Adobe Standard renders at 5106 K --
    and made a Tint-only drag commit the wrong Kelvin. Same profile lookup and the same
    CameraNative gate as attachProfile, so the two cannot disagree. cam.profile itself is
    left unset: only the readout's as-shot pair changes, not what callers think is
    attached.
*/
    if (currentImagePath.isEmpty()) return CameraColor();
    auto work = WorkingImageCache::instance().get(currentImagePath, currentRawRender().tag());
    if (!work) return CameraColor();
    CameraColor cam = work->cam;
    if (cam.valid && work->space == ColorSpaceMath::ColorSpace::CameraNative) {
        const EditStack s = stackCache.value(currentImagePath);
        const QString name = s.scopes.isEmpty() ? QString()
                                                : s.scopes[0].params.cameraProfile;
        if (const auto prof = CameraProfileStore::instance().profile(cam.cameraModel, name))
            CameraProfile::resolveAsShot(*prof, cam);
    }
    return cam;
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
    auto work = WorkingImageCache::instance().get(currentImagePath, currentRawRender().tag());
    if (work && work->isValid()) return work;
    emit workingImageNeeded(currentImagePath);      // same thread: direct call into MW
    work = WorkingImageCache::instance().get(currentImagePath, currentRawRender().tag());
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

QIcon DevelopProperties::detailTargetIcon(bool armed)
{
/*
    The Detail preview's "pick a location" target, drawn rather than shipped for the same
    reasons dropperIcon is (see its note, including the LOGICAL-coordinates warning): a
    ring with four tick marks, the universal aim glyph, and the same shape ImageView draws
    on the image at the sample point so the button and the marker read as one thing.

    ARMED goes to the accent colour, matching the blue border BarBtn::setActive draws.
    Unlike the dropper this stays SOLID when armed: the dropper's ghost says "the tool is
    in your hand", but here the marker on the image is what moves, and the button keeps
    standing for the location -- hollowing it out would suggest the point was lost.
*/
    const qreal dpr = 2.0;
    const int S = 16;
    QPixmap pm(int(S * dpr), int(S * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!armed) p.setOpacity(G::iconOpacity);
    const QColor c = armed ? QColor(G::appleBlue.red(), G::appleBlue.green(),
                                    G::appleBlue.blue(), 230)
                           : QColor(225, 225, 225);
    p.setPen(QPen(c, 1.5, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(8.0, 8.0), 4.4, 4.4);
    p.drawLine(QPointF(8.0, 1.4), QPointF(8.0, 3.0));       // N
    p.drawLine(QPointF(8.0, 13.0), QPointF(8.0, 14.6));     // S
    p.drawLine(QPointF(1.4, 8.0), QPointF(3.0, 8.0));       // W
    p.drawLine(QPointF(13.0, 8.0), QPointF(14.6, 8.0));     // E
    p.setBrush(QBrush(c));
    p.drawEllipse(QPointF(8.0, 8.0), 1.0, 1.0);             // centre dot
    p.end();
    return QIcon(pm);
}

QIcon DevelopProperties::curveSamplerIcon(bool armed)
{
/*
    The Curves panel's pointer toggle, drawn rather than shipped for the same reasons
    dropperIcon is (see its note, including the LOGICAL-coordinates warning): a rising
    curve with a crosshair sitting on it -- "where the pointer is, on the curve", which is
    exactly the marker CurveEditor::drawSampleMarker puts on the plot.

    ARMED goes SOLID in the accent colour rather than to a ghost outline. The dropper
    hollows out because the tool leaves the button and becomes the cursor; this one does
    not -- the pointer is the ordinary pointer and the button keeps standing for a readout
    that is switched on.
*/
    const qreal dpr = 2.0;
    const int S = 16;
    QPixmap pm(int(S * dpr), int(S * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!armed) p.setOpacity(G::iconOpacity);
    const QColor c = armed ? QColor(G::appleBlue.red(), G::appleBlue.green(),
                                    G::appleBlue.blue(), 230)
                           : QColor(225, 225, 225);
    p.setPen(QPen(c, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath curve;                                 // a gentle S, bottom-left to top-right
    curve.moveTo(1.6, 14.4);
    curve.cubicTo(6.0, 13.6, 7.0, 3.2, 14.4, 1.6);
    p.drawPath(curve);
    const QPointF hit(8.2, 8.0);                        // the sample, sitting on the curve
    p.drawLine(QPointF(hit.x(), 3.2), QPointF(hit.x(), 14.4));
    p.setBrush(c);
    p.drawEllipse(hit, 1.9, 1.9);
    p.end();
    return QIcon(pm);
}

void DevelopProperties::addDetailPreviewRow(QModelIndex parIdx)
{
/*
    The 1:1 preview at the head of the Detail section: ONE spanned row holding "1:1" and
    the target button down its left side and the preview square beside them, the
    square's top level with the button. The square is sized to the patch it shows (the
    patch is square), so there is no unused black well either side of it.
*/
    if (G::isLogger) G::log("DevelopProperties::addDetailPreviewRow");

    /* Side of the preview square = the ROI it renders, in logical (= image) pixels. */
    const int side = G::propertyRowHeight() + 170;
    const int vPad = 2;                 // above and below the row's contents

    clearItemInfo(i);
    i.name = "detailPreview";
    i.parIdx = parIdx;
    i.parentName = "DetailHeader";
    i.captionText = "";
    i.tooltip = "A live 100% view of part of the image, for judging sharpening and "
                "noise.";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex prevIdx = capIdx;
    model->setData(prevIdx, side + 2 * vPad - G::propertyRowHeight(), UR_ExtraRowHeight);
    setFirstColumnSpanned(prevIdx.row(), parIdx, true);

    QWidget *row = new QWidget;
    row->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *hb = new QHBoxLayout(row);
    hb->setContentsMargins(0, vPad, 0, vPad);
    hb->setSpacing(0);

    /* Left column: "1:1" + the target button, pinned to the top. */
    QWidget *capW = new QWidget(row);
    capW->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *chb = new QHBoxLayout(capW);
    chb->setContentsMargins(0, 0, 0, 0);
    chb->setSpacing(6);
    QLabel *lbl = new QLabel("1:1");
    lbl->setStyleSheet(G::labelCss(G::textColor, G::strFontSize.toInt()));  // no opaque slab
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents);   // clicks reach the tree
    BarBtn *tgt = new BarBtn();
    tgt->setIcon(detailTargetIcon(false));
    tgt->setIconSize(QSize(16, 16));
    tgt->setToolTip("Choose the area to show: click here, then click the photo.\n"
                    "You can also drag inside the preview.");
    tgt->setEnabled(!currentImagePath.isEmpty());
    connect(tgt, &BarBtn::clicked, this, &DevelopProperties::toggleDetailPick);
    detailPickBtn = tgt;
    chb->addWidget(lbl);
    chb->addWidget(tgt);
    capW->setFixedWidth(capW->sizeHint().width());
    hb->addWidget(capW, 0, Qt::AlignTop);

    /* The square, centred in the row: a spacer the width of the left column on the right
       balances it, so the column does not push the square off centre. */
    DetailPreview *dp = new DetailPreview(row);
    dp->setFixedSize(side, side);
    detailPreview = dp;
    connect(dp, &DetailPreview::pointNudged, this, &DevelopProperties::nudgeDetailPoint);
    hb->addStretch(1);
    hb->addWidget(dp, 0, Qt::AlignTop);
    hb->addStretch(1);
    hb->addSpacing(capW->width());

    setIndexWidget(prevIdx, row);
}

/* True when a patch render is worth doing at all: the widget is genuinely on screen (the
   Detail section exists in the tree after every rebuild, so isVisible() is the test --
   the same one wantsScopeData uses for the histogram) and a point has been picked. */
bool DevelopProperties::detailPreviewWanted() const
{
    return detailPreview && detailPreview->isVisible() && detailPtSet;
}

int DevelopProperties::detailRoiSize() const
{
    if (!detailPreview || !detailPreview->isVisible()) return 0;
    return detailPreview->roiSize();
}

void DevelopProperties::setDetailRoiImage(const QImage &img)
{
    if (detailPreview) detailPreview->setImage(img);
}

void DevelopProperties::setDetailMessage(const QString &text)
{
    if (detailPreview) detailPreview->setMessage(text);
}

void DevelopProperties::setDetailPoint(QPointF n)
{
    /* Clamped, not rejected: a drag that runs off the edge should stop at the edge
       rather than stop responding. */
    const QPointF p(qBound(0.0, n.x(), 1.0), qBound(0.0, n.y(), 1.0));
    if (detailPtSet && p == detailPt) return;
    detailPt = p;
    detailPtSet = true;
    emit detailRoiNeeded();
}

void DevelopProperties::clearDetailPoint()
{
    if (!detailPtSet) return;
    detailPtSet = false;
    detailPt = QPointF();
    emit detailRoiNeeded();          // drops the marker; nothing left to render
}

void DevelopProperties::nudgeDetailPoint(int dx, int dy)
{
/*
    A drag inside the preview, in IMAGE pixels of the ORIENTED frame. Turning that into a
    step in the normalized point needs the oriented frame's dimensions, i.e. the image's
    size AND its EXIF rotation -- both of which MW owns (developOrientationDegrees reads
    the sort/filter model, which is GUI-thread state the dock deliberately does not touch,
    see MW::renderDevelopPreview). So the drag is forwarded rather than resolved here, and
    MW calls setDetailPoint with the result.
*/
    if (!detailPtSet) return;
    emit detailPointNudged(dx, dy);
}

void DevelopProperties::cancelDetailPick()
{
    setDetailPickActive(false);
}

void DevelopProperties::toggleDetailPick()
{
    /* Arming needs an image to pick on -- the button is disabled without one, and this
       keeps any other caller (a shortcut later) from arming a tool that can only fail. */
    if (!detailPickActive && currentImagePath.isEmpty()) return;
    setDetailPickActive(!detailPickActive);
}

void DevelopProperties::setDetailPickActive(bool on)
{
    if (detailPickActive == on) return;
    detailPickActive = on;
    if (detailPickBtn) {
        detailPickBtn->setActive(on);
        detailPickBtn->setIcon(detailTargetIcon(on));
    }
    if (on) {
        setDetailMessage(tr("Click the part of the photo to inspect"));
        emit detailPickBegin();
    }
    else emit detailPickEnd();
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
        /* Transparent, like every caption the delegate draws: under the app stylesheet a
           bare QLabel fills its background opaquely, which paints a dark slab over the
           row's lifted background (G::panelContentBg). */
        lbl->setStyleSheet(G::labelCss(G::textColor, G::strFontSize.toInt()));
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

    float sr = float(sum[0] / count);
    float sg = float(sum[1] / count);
    float sb = float(sum[2] / count);

    /* The sample came out of the PRE-develop WorkingImage, which for a raw is still in
       camera-native primaries (RawColor stops there). WhiteBalance::solve's forward chain
       ENDS in the working space, so an unconverted sample would be solved against the
       wrong space and hand back the wrong temperature. Converting the AVERAGE rather than
       each of the 25 samples is exact -- the transform is linear. No-op for non-raw. */
    toWorkingColor(*work, sr, sg, sb);

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
    noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way

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

void DevelopProperties::onWorkingImageReady(const QString &fPath)
{
    if (fPath.isEmpty() || fPath != currentImagePath) return;
    const CameraColor cam = currentCam();
    if (!cam.valid) return;                      // decode has not landed yet
    if (cam.asShotK == wbRowAsShotK) return;     // already showing this characterisation
    wbRowAsShotK = cam.asShotK;
    refreshWbRow();             // Temp/Tint, now against the camera's own data
    refreshCameraProfileRow();  // the profile LIST is per-camera, and keys off cam too
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
    /*
        THE CAMERA PROFILE heads the panel -- Lightroom's Profile slot, and for the same
        reason: it characterises the CAMERA, and everything below it is an adjustment made
        on top of that characterisation. It is not merely first by convention, it is first
        by DEPENDENCY: white balance is solved against the camera matrix, so changing the
        profile changes what a given Kelvin means (see Develop/cameraprofile.h).

        Paired with Tone mapping, which follows it in the same group: together they say
        what rendering the adjustments below are made on top of.
    */
    addCameraProfileRow(parIdx);

    /*
        TONE MAPPING SITS DIRECTLY UNDER THE PROFILE, inside the same top group.

        Both describe the RENDERING the adjustments below are made on top of: the profile
        characterises the camera, this decides the overall mapping from scene brightness to
        display, and every slider further down adjusts WITHIN that mapping. General to
        specific, which is a real dependency (AgX leaves far more highlight room before
        anything clips, so the same Exposure value does not mean the same thing under both)
        rather than a visual grouping.

        NOT called "View": Winnow already spends that word on the central widget modes
        (loupe / grid / table / compare, the View menu), and a second meaning for it in
        the Develop dock would be a collision. The stored key stays `viewTransform` -- it
        is a published sidecar format, and a label is free to change without it.

        A combo in the value cell, modelled on the WB preset row -- addItem(name, enum),
        activated (not currentIndexChanged, which would also fire on programmatic
        repopulation), and re-synced by findData under a QSignalBlocker.
    */
    addViewTransformRow(parIdx);
    addGroupDivider(parIdx, "BasicHeader", "ProfileDivider");

    addWhiteBalanceRow(parIdx);
    addSlider("temp",       "Temp",       "Colour temperature of the light, in Kelvin.",
              parIdx, "BasicHeader",
              int(WhiteBalance::kMinKelvin), int(WhiteBalance::kMaxKelvin), 0,
              G::lightblue, G::lightyellow, 0, /*logScale*/ true);
    addSlider("tint",       "Tint",       "White balance tint (green/magenta).", parIdx, "BasicHeader", -150, 150, 0,   G::lightgreen, G::lightmagenta);
    addGroupDivider(parIdx, "BasicHeader", "WBDevider");

    addSlider("exposure",   "Exposure",   "Overall exposure in stops (EV).",     parIdx, "BasicHeader", -500, 500, 100, G::darkgray, G::lightgray);
    addSlider("contrast",   "Contrast",   "Global contrast.",                    parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("highlights", "Highlights", "Recover or lift the highlights.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("shadows",    "Shadows",    "Recover or deepen the shadows.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("whites",     "Whites",     "Set the white point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("blacks",     "Blacks",     "Set the black point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addGroupDivider(parIdx, "BasicHeader", "ToneDevider");
    addSlider("texture",    "Texture",    "Enhance or smooth fine detail.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addSlider("clarity",    "Clarity",    "Midtone punch (+) or soft glow (-).", parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addSlider("dehaze",     "Dehaze",     "Remove or add atmospheric haze.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addGroupDivider(parIdx, "BasicHeader", "PresenceDevider");
    addSlider("vibrance",   "Vibrance",   "Saturation weighted toward muted colours.", parIdx, "BasicHeader", -100, 100, 0, G::darkgray, G::lightgray);
    addSlider("saturation", "Saturation", "Global saturation (grey at -100).",   parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addGroupDivider(parIdx, "BasicHeader", "BasicEndDivider");
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

/* -------------------------------------------------------------------------------------
   Curves (tone curve)
   ----------------------------------------------------------------------------------- */

/* The Basic dock slider each parametric band writes, so a band drag moves the slider the
   user already knows rather than a hidden duplicate. Order matches CurveEditor's
   bands. */
QString DevelopProperties::curveBandKey(int band)
{
    switch (band) {
    case 0:  return "blacks";
    case 1:  return "shadows";
    case 2:  return "highlights";
    default: return "whites";
    }
}

void DevelopProperties::addCurves()
{
    if (G::isLogger) G::log("DevelopProperties::addCurves");
    addHeader("CurvesHeader", "???", "Curves",
              "Tone curve: a parametric view of the Basic tone sliders, and free-form "
              "point curves for RGB and each channel.", PV_Curves);
    QModelIndex parIdx = capIdx;

    /* Mode + channel on one spanned row, the same plain-widget idiom as the Color Grade
       range checkboxes. Both are UI state (curveModeIndex / the combo), not EditParams:
       which curve you are LOOKING at is not part of the recipe. */
    clearItemInfo(i);
    i.name = "curveMode";
    i.parIdx = parIdx;
    i.parentName = "CurvesHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex modeIdx = capIdx;
    model->setData(modeIdx, 6, UR_ExtraRowHeight);
    setFirstColumnSpanned(modeIdx.row(), parIdx, true);
    {
        QWidget *rw = new QWidget;
        rw->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(rw);
        hb->setContentsMargins(QTreeView::indentation() + 4, 0, 0, 0);
        hb->setSpacing(8);
        QComboBox *mode = new QComboBox(rw);
        mode->addItems({"Parametric", "Point"});
        /* Programmatic sync of a REBUILT combo to the state it had before: blocked, like
           every other populate in this panel, so it cannot re-enter applyCurveMode while
           addCurves is still adding the rows applyCurveMode shows and hides. The call at
           the end of addCurves applies the mode once everything exists. */
        {
            QSignalBlocker b(mode);
            mode->setCurrentIndex(curveModeIndex);
        }
        mode->setToolTip("Parametric: drag tonal regions (moves the Basic sliders).\n"
                         "Point: draw your own curve.");
        QComboBox *chan = new QComboBox(rw);
        chan->addItems({"RGB", "Red", "Green", "Blue"});
        chan->setToolTip("The curve to edit: RGB for overall tone, or one colour "
                         "channel.");
        curveChannelCombo = chan;
        connect(mode, &QComboBox::currentIndexChanged, this, [this](int idx){
            curveModeIndex = idx;
            applyCurveMode();
        });
        connect(chan, &QComboBox::currentIndexChanged, this, [this](int idx){
            if (curveEditor) curveEditor->setChannel(idx);
        });
        /* The pointer toggle, ahead of the combos: it applies to the plot as a whole,
           whichever mode and channel are showing. A BarBtn like the WB dropper, so the
           armed chrome (the accent border setActive draws) is the same cue in both
           places. Not checkable -- BarBtn is not -- so the click routes through
           toggleCurveSampler, which owns the state. */
        BarBtn *samp = new BarBtn();
        samp->setIcon(curveSamplerIcon(curveSamplerActive));
        samp->setIconSize(QSize(16, 16));
        samp->setToolTip("Show where the tone under the pointer falls on the curve.\n"
                         "In Point mode, click the image to add a point there.");
        samp->setActive(curveSamplerActive);    // the tree rebuilds; the armed state does not
        connect(samp, &BarBtn::clicked, this, &DevelopProperties::toggleCurveSampler);
        curveSamplerBtn = samp;

        hb->addWidget(samp);
        hb->addWidget(mode);
        hb->addWidget(chan);
        hb->addStretch(1);
        curveChannelRow = chan;      // the widget applyCurveMode shows / hides
        setIndexWidget(modeIdx, rw);
    }

    /* Says, before the user touches the plot, WHERE a band drag lands. Parametric mode
       owns no params: it writes Basic's tone sliders, so a drag moves a control in
       another panel. Standing text rather than a tooltip -- the surprise is the point
       being explained, and a tooltip only arrives after the user has already wondered. */
    clearItemInfo(i);
    i.name = "curveNote";
    i.parIdx = parIdx;
    i.parentName = "CurvesHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex noteIdx = capIdx;
    setFirstColumnSpanned(noteIdx.row(), parIdx, true);
    {
        QWidget *rw = new QWidget;
        rw->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(rw);
        hb->setContentsMargins(QTreeView::indentation() + 4, 0, 0, 0);
        hb->setSpacing(0);
        QLabel *note = new QLabel("Bands edit the Basic tone sliders.");
        note->setStyleSheet(G::labelCss(G::disabledColor, G::strFontSize.toInt()));
        note->setEnabled(false);                     // dim: it is a caption, not a control
        note->setAttribute(Qt::WA_TransparentForMouseEvents);
        hb->addWidget(note);
        hb->addStretch(1);
        setIndexWidget(noteIdx, rw);
    }

    /* The plot: a tall full-width spanned row, like the colour wheels. */
    clearItemInfo(i);
    i.name = "curvePlot";
    i.parIdx = parIdx;
    i.parentName = "CurvesHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex plotIdx = capIdx;
    model->setData(plotIdx, 190, UR_ExtraRowHeight);
    setFirstColumnSpanned(plotIdx.row(), parIdx, true);
    {
        CurveEditor *ed = new CurveEditor;
        curveEditor = ed;
        connect(ed, &CurveEditor::curveChanged,   this, [this]{ onCurveChanged(false); });
        connect(ed, &CurveEditor::curveCommitted, this, [this]{ onCurveChanged(true); });
        connect(ed, &CurveEditor::parametricChanged, this, [this](int b, double v){
            onParametricChanged(b, v, false);
        });
        connect(ed, &CurveEditor::parametricCommitted, this, [this]{
            onParametricChanged(-1, 0.0, true);
        });
        setIndexWidget(plotIdx, ed);
    }

    /* The tone-split handles, directly under the plot and sharing its x-axis -- the same
       three params the histogram scope's slider edits (see onToneSplitsChanged). They
       live
       in BOTH places on purpose: the scopes strip can be hidden entirely (G cycles it
       away), and under the parametric curve is where they belong anyway. */
    clearItemInfo(i);
    i.name = "curveSplits";
    i.parIdx = parIdx;
    i.parentName = "CurvesHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex splitIdx = capIdx;
    model->setData(splitIdx, 20, UR_ExtraRowHeight);
    setFirstColumnSpanned(splitIdx.row(), parIdx, true);
    {
        QWidget *rw = new QWidget;
        rw->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(rw);
        /* Inset by the plot's own margin, and by NOTHING else, so the handles sit under
           the curve's x-axis: this row and the plot are spanned rows at the same tree
           level, so they get the same cell and are centred together in the panel
           (updateEditorGeometries). The caption indent the other Curves rows carry was
           here too and pushed the handles 19px right of the axis they belong to. */
        hb->setContentsMargins(CurveEditor::kPlotMargin, 0, CurveEditor::kPlotMargin, 0);
        hb->setSpacing(0);
        ToneRegionSlider *ts = new ToneRegionSlider(rw);
        ts->setToolTip("Split the tonal range into blacks | shadows | highlights | "
                       "whites. "
                       "These are the same handles as under the histogram.");
        curveToneSlider = ts;
        connect(ts, &ToneRegionSlider::valueChanged,
                this, &DevelopProperties::onToneSplitsChanged);
        hb->addWidget(ts);
        curveSplitsRow = rw;         // centred with the plot above it
        setIndexWidget(splitIdx, rw);
    }

    addGroupDivider(parIdx, "CurvesHeader", "CurvesEndDivider");

    applyCurveMode();
}

/* Point mode shows the channel combo; Parametric mode shows the split handles. Called on
   build and whenever the mode combo changes. */
void DevelopProperties::applyCurveMode()
{
    const bool point = (curveModeIndex == 1);
    if (curveEditor)
        curveEditor->setMode(point ? CurveEditor::Point : CurveEditor::Parametric);
    if (curveChannelRow) curveChannelRow->setVisible(point);
    const QModelIndex splitIdx = findCaptionIndex("curveSplits");
    if (splitIdx.isValid())
        setRowHidden(splitIdx.row(), splitIdx.parent(), point);
    /* The note describes the parametric bands, so it goes with them. */
    const QModelIndex noteIdx = findCaptionIndex("curveNote");
    if (noteIdx.isValid())
        setRowHidden(noteIdx.row(), noteIdx.parent(), point);
    scheduleContentFit();        // setRowHidden emits nothing: re-fit the block ourselves
    updateCurvePickState();      // Parametric has no points to add: the picker goes with it
}

/* Point-curve drag -> the active scope's curve params, then preview. commit marks the
   drag-release, when the debounced sidecar write is scheduled (live moves only render;
   dirty is set every move so navigating away still persists). */
void DevelopProperties::updateEditorGeometries()
{
/*
    Centre the Curves plot, and the split handles under it, in the PANEL rather than in
    their tree cell.

    A spanned row's index widget starts AFTER the tree indentation -- two levels' worth
    (2 x 15px) for a row one level under a section header -- so the plot sat with a 30px
    gap down its left side and none at all on its right. On a row of sliders that is
    invisible; on the one full-width GRAPHIC in the panel it reads as off-centre. The
    widget cannot just be given a negative left margin: it would be clipped to its cell.
    So the geometry the base class computes is nudged here instead -- the WIDTH the view
    handed it is kept and only the x moves, which is why the plot does not shrink.

    This is the right hook rather than resizeEvent: it is where the base class re-lays out
    every persistent index widget, so it covers a resize, a scroll and an expand alike,
    and nothing can move the widgets back behind us afterwards.
*/
    PropertyEditor::updateEditorGeometries();
    /* Same cell, so both get the same width and land on the same x -- which is what keeps
       the handles under the curve's x-axis. */
    const auto centre = [this](QWidget *w) {
        if (!w || !w->isVisible()) return;
        const int x = (viewport()->width() - w->width()) / 2;
        if (x > 0 && x != w->x()) w->move(x, w->y());
    };
    centre(curveEditor);
    centre(curveSplitsRow);
    centre(colorPanelModesRow);   // the Tone / Hue / RGB radios
    /* This is also where an expand / collapse / scroll of the Edits tree lands, which is
       exactly what moves the plot on and off screen. */
    updateCurvePickState();
}

void DevelopProperties::showEvent(QShowEvent *event)
{
    PropertyEditor::showEvent(event);
    updateCurvePickState();
}

void DevelopProperties::hideEvent(QHideEvent *event)
{
    PropertyEditor::hideEvent(event);
    /* The panel is going away -- leaving Develop mode (closeDevelopDock hides the dock,
       and Qt sends the Hide down to us), the dock tabbed behind a sibling, a workspace
       switch. Disarm the pointer sampler for the same reason collapsing the Curves
       section does: it is armed chrome that owns the canvas click, and the loupe must
       not still be swallowing clicks into a panel that is no longer on screen. Cheap and
       safe to call unconditionally -- setCurveSamplerActive returns at once when it is
       already off, which is the startup case (MW::showEvent closes the dock). */
    setCurveSamplerActive(false);
    updateCurvePickState();
}

/* ---- Curves pointer sample ---------------------------------------------------------
   See the header for the contract. The marker is driven by MW from the hover sample it
   already takes for the scopes readout, so there is no mode to arm on the canvas and
   nothing here to cancel on an image change -- a new image simply reports new pixels. */

bool DevelopProperties::wantsCurveSample() const
{
    return curveSamplerActive && curveEditor && curveEditor->isVisible();
}

void DevelopProperties::setCurveSamplerActive(bool on)
{
    if (curveSamplerActive == on) return;
    curveSamplerActive = on;
    if (curveSamplerBtn) {
        curveSamplerBtn->setActive(on);
        curveSamplerBtn->setIcon(curveSamplerIcon(on));
    }
    /* Switching off leaves no stale marker sitting on the plot. */
    if (!on && curveEditor) curveEditor->clearSample();
    updateCurvePickState();
}

void DevelopProperties::toggleCurveSampler()
{
    setCurveSamplerActive(!curveSamplerActive);
}

void DevelopProperties::setCurveSample(int r, int g, int b)
{
    if (!wantsCurveSample()) return;
    curveEditor->setSample(r, g, b);
}

void DevelopProperties::clearCurveSample()
{
    if (curveEditor) curveEditor->clearSample();
}

/* ---- Curves canvas point picking ---------------------------------------------------
   See the header for the contract. The picker is armed by the SAME toggle as the marker,
   with two further conditions: the plot has to be on screen (wantsCurveSample already
   requires that) and the mode has to be Point -- in Parametric mode there are no control
   points to add, so the canvas click must stay the loupe's.
*/

bool DevelopProperties::wantsCurvePointPick() const
{
    return wantsCurveSample() && curveEditor->mode() == CurveEditor::Point;
}

void DevelopProperties::updateCurvePickState()
{
    const bool want = wantsCurvePointPick();
    if (want == curvePickArmed) return;
    curvePickArmed = want;
    if (want) emit curvePickBegin();
    else      emit curvePickEnd();
}

/* A click on the image landed on this pixel (MW read it out of the same developShownImage
   the hover marker uses). Push it as the sample so the marker and the new point agree,
   then add the point -- CurveEditor emits curveChanged + curveCommitted, which run the
   normal render + sidecar path through onCurveChanged. */
void DevelopProperties::addCurvePointFromPixel(int r, int g, int b)
{
    if (G::isLogger) G::log("DevelopProperties::addCurvePointFromPixel");
    if (!wantsCurvePointPick()) return;
    curveEditor->setSample(r, g, b);
    curveEditor->addPointAtSample();
}

void DevelopProperties::onCurveChanged(bool commit)
{
    if (G::isLogger) G::log("DevelopProperties::onCurveChanged");
    if (isPopulating) return;
    if (currentImagePath.isEmpty() || !curveEditor) return;
    noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way

    EditParams &p = activeParams();
    const EditParams &e = curveEditor->params();
    for (int c = 0; c < ToneCurve::kChannels; ++c) {
        p.curveN[c] = e.curveN[c];
        for (int k = 0; k < ToneCurve::kMaxPts; ++k) {
            p.curveX[c][k] = e.curveX[c][k];
            p.curveY[c][k] = e.curveY[c][k];
        }
    }
    /* A custom editor does not get History for free. The mergeKey collapses a whole drag
       into ONE history row instead of one per mouse-move; keying it on the channel keeps
       consecutive edits to different curves separate. */
    static const char *chName[] = {"RGB", "Red", "Green", "Blue"};
    const int ch = qBound(0, curveEditor->channel(), 3);
    noteEdit(QString("Tone curve %1").arg(chName[ch]), QString(),
             QString("curve/%1").arg(ch));
    Q_UNUSED(commit)
    emit paramsChanged();
}

/* Parametric-band drag -> the EXISTING Basic tone slider for that band. The Curves panel
   owns no parametric params: it is a second view of Basic's tone controls, so the drag
   writes Basic's field AND moves Basic's slider. band < 0 means "drag released". */
void DevelopProperties::onParametricChanged(int band, double value, bool commit)
{
    if (G::isLogger) G::log("DevelopProperties::onParametricChanged");
    if (isPopulating) return;
    if (currentImagePath.isEmpty()) return;
    if (commit || band < 0) { lastFlashBand = -1; return; }   // release: re-arm the flash
    noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way

    EditParams &p = activeParams();
    const float v = static_cast<float>(value);
    switch (band) {
    case 0:  p.blacks = v;     break;
    case 1:  p.shadows = v;    break;
    case 2:  p.highlights = v; break;
    default: p.whites = v;     break;
    }
    /* Keep the Basic slider in step -- the same value now has two editors. */
    const bool wasPopulating = isPopulating;
    isPopulating = true;                             // setSliderReal must not echo back
    setSliderReal(curveBandKey(band), value);
    isPopulating = wasPopulating;

    /* Point at the control the drag is actually editing: flash the Basic caption the way
       clicking that caption does. ONCE per drag (see lastFlashBand) -- restarting the
       animation on every mouse-move would both stutter it and storm the model. Invisible
       when Basic is collapsed or scrolled away, which is why the plot's own band readout
       carries the message and this only reinforces it. */
    if (band != lastFlashBand) {
        lastFlashBand = band;
        flashCaption(findCaptionIndex(curveBandKey(band)));
    }

    static const char *bandCaption[] = {"Blacks", "Shadows", "Highlights", "Whites"};
    noteEdit(bandCaption[qBound(0, band, 3)], QString::number(qRound(value)),
             QString("curve/param/%1").arg(band));
    emit paramsChanged();
}

/* Push the stored params into the plot + the split handles. The exact inverse of the two
   handlers above, called from populateSlidersFromStack. */
void DevelopProperties::refreshCurveRow()
{
    if (!curveEditor) return;
    const EditParams &p = activeParams();
    curveEditor->setParams(p);                       // no signal: programmatic sync
    if (curveToneSlider)
        curveToneSlider->setPositions(p.toneShadowCenter, p.toneCrossover,
                                      p.toneHighlightCenter);
    if (curveChannelCombo && curveChannelCombo->currentIndex() != curveEditor->channel())
        curveChannelCombo->setCurrentIndex(curveEditor->channel());
}

void DevelopProperties::addColor()
{
    if (G::isLogger) G::log("DevelopProperties::addColor");
    addHeader("ColorHeader", "???", "Color", "RGB gain, hue and luminance adjustments.", PV_Color);
    QModelIndex parIdx = capIdx;

    /* All integer sliders -100..100 (div 0), default 0 (identity), matching EditParams.
       RGB = per-channel gain; then global hue rotation / luminance. Saturation and
       Vibrance live at the bottom of Basic (Lightroom's Presence group). */
    /* RGB grooves ramp black -> the channel's own primary, so each slider names its
       channel at a glance (black->red, black->green, black->blue). */
    addSlider("red",        "Red",        "Add or remove red.",                    parIdx, "ColorHeader", -100, 100, 0, "#000000", "#ff0000");
    addSlider("green",      "Green",      "Add or remove green.",                  parIdx, "ColorHeader", -100, 100, 0, "#000000", "#00ff00");
    addSlider("blue",       "Blue",       "Add or remove blue.",                   parIdx, "ColorHeader", -100, 100, 0, "#000000", "#0000ff");
    addGroupDivider(parIdx, "ColorHeader", "RGBDevider");
    addSlider("hue",        "Hue",        "Rotate all hues.",                      parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addSlider("luminance",  "Luminance",  "Global luminance (brightness).",        parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addGroupDivider(parIdx, "ColorHeader", "ColorEndDivider");
}

/* --------------------------------------------------------------------------------------
   Calibrate (RGB primaries)

   Lightroom's Calibration panel: a hue shift + saturation scale for each of the three
   PRIMARIES of the working space, built into one 3x3 matrix and applied in linear light
   before the tone curve, plus Shadows Tint (a green/magenta cast in the shadows). Laid
   out as Lightroom lays it out: one slider per value, -100..100. (Until 2026-10-12 it was
   R/G/B checkboxes driving one shared wheel; the user asked for Lightroom's sliders.)

   Distinct from the Color panel's Red/Green/Blue, which are per-channel linear GAINS
   folded into the white-balance vector -- a cruder second white balance. Both exist; the
   gains are kept until Calibrate proves out. The maths (Develop/calibrate.h) normalises
   the matrix rows so NEUTRALS STAY NEUTRAL, which is the whole point of the panel.
   ------------------------------------------------------------------------------------ */

void DevelopProperties::addCalibrate()
{
    if (G::isLogger) G::log("DevelopProperties::addCalibrate");
    addHeader("CalibrateHeader", "???", "Calibrate",
              "Fine-tune the camera's base colours, and the shadow tint.",
              PV_Calibrate);
    QModelIndex parIdx = capIdx;
    addCalibrateBody(parIdx, "CalibrateHeader");
}

/* The section's rows under `parent` -- its own header, or the combined Color
   panel's (addColorPanel). */
void DevelopProperties::addCalibrateBody(const QModelIndex &parIdx, const QString &parent)
{
    /* Lightroom's Calibration panel: Shadows Tint, then each primary's Hue and
       Saturation, all -100..100. Plain sliders keyed by their EditParams field names, so
       the generic applyKeyToParams path (and its history) carries them. The grooves run
       the way Lightroom's do: green -> magenta for the tint, and for each primary from
       one neighbouring hue to the other (Hue) or grey -> the primary (Saturation). The
       grooves name the primary, so the captions are plain "Hue" / "Saturation" (the
       user's call); itemChange gives History the primary's name instead. */
    addSlider("calShadowTint", "Tint",
              "Shadows only: left adds green, right adds magenta.",
              parIdx, parent, -100, 100, 0, "#3fa93f", "#c040c0");
    addGroupDivider(parIdx, parent, "CalibrateShadowsDivider");
    addSlider("calRedHue", "Hue",
              "Shift the red primary toward magenta (left) or orange (right).",
              parIdx, parent, -100, 100, 0, "#d0306a", "#e08a30");
    addSlider("calRedSat", "Saturation",
              "Desaturate (left) or saturate (right) the red primary.",
              parIdx, parent, -100, 100, 0, G::darkgray, "#d03a3a");
    addGroupDivider(parIdx, parent, "CalibrateRedDivider");
    addSlider("calGreenHue", "Hue",
              "Shift the green primary toward yellow (left) or cyan (right).",
              parIdx, parent, -100, 100, 0, "#b0b030", "#30b0a0");
    addSlider("calGreenSat", "Saturation",
              "Desaturate (left) or saturate (right) the green primary.",
              parIdx, parent, -100, 100, 0, G::darkgray, "#3fa93f");
    addGroupDivider(parIdx, parent, "CalibrateGreenDivider");
    addSlider("calBlueHue", "Hue",
              "Shift the blue primary toward cyan (left) or purple (right).",
              parIdx, parent, -100, 100, 0, "#30a8b0", "#6040c0");
    addSlider("calBlueSat", "Saturation",
              "Desaturate (left) or saturate (right) the blue primary.",
              parIdx, parent, -100, 100, 0, G::darkgray, "#3a5ad0");
    addGroupDivider(parIdx, parent, "CalibrateEndDivider");
}

/*
    Tone mapping -> the GLOBAL scope (scope 0), never the active one: it maps the whole
    image for display, so it is not a per-mask adjustment and applying it to a scope would
    make it mean different things in different parts of one picture.

    A combo gets no history for free (the generic tree-edit handler only covers sliders),
    hence the explicit noteEdit -- see the checklist in notes/Documentation.txt.
*/
/*
    Build the Tone mapping row. Raw-only in EFFECT -- OutputTransform forces the transform
    to None for display-referred input, because a JPEG already carries its camera's tone
    curve and tone-mapping it twice would wreck it. So for a non-raw file the control is
    DISABLED with the reason shown in place of the value, rather than left live and
    silently doing nothing.
*/
/* ----------------------------------------------------------------------------------------
   Camera profile (the first row of Basic)
   ---------------------------------------------------------------------------------------- */

/* The built-in entry: no DCP at all, the matrix Winnow has always used. Its DATA is the
   empty string, which is EditParams::cameraProfile's identity -- so selecting it is a
   reset, not a choice, and an untouched image writes no sidecar. */
static const char *kBuiltInProfileLabel = "Winnow Standard";

/* Shown when the combo has nothing but the built-in entry in it. A user with no Adobe
   application installed sees exactly that, and without this the row reads as "this camera
   has no profiles" when the truth is "no profiles are installed on this computer" -- a
   different statement, and the only one of the two the user can act on. */
static const char *kNoProfilesLabel = "No profiles installed for this camera";

/*
    The folders swept, for the tooltip. CameraProfileStore::roots() is public for exactly
    this -- the list is built in one place, so a root added there cannot go unmentioned
    here, and the platform branching lives there rather than being duplicated here.

    THE HOME PREFIX IS ABBREVIATED ON UNIX ONLY. "~" is what a macOS or Linux user reads
    a home path as, and it keeps the tooltip from wrapping to an unreadable width. On
    Windows it is not a convention the user knows -- "~\AppData\Roaming\..." would read
    as a typo -- so the path is shown whole.
*/
static QString profileRootsText()
{
    QString out;
    const QStringList roots = CameraProfileStore::roots();
    for (const QString &r : roots) {
        QString shown = r;
#ifndef Q_OS_WIN
        const QString home = QDir::homePath();
        if (!home.isEmpty() && shown.startsWith(home))
            shown = QLatin1Char('~') + shown.mid(home.size());
#endif
        out += "\n\u2022 " + QDir::toNativeSeparators(shown);
    }
    return out;
}

/* The invitation to drop profiles in Winnow's own folder -- and NOTHING when there is no
   such folder, because on a platform that gives no writable app-data location it is not
   swept either (CameraProfileStore::ownRoot). Telling the user to put files somewhere
   nothing reads would be worse than staying quiet. */
static QString ownRootText()
{
    if (CameraProfileStore::ownRoot().isEmpty()) return QString();
    return "\nThe last is Winnow's own: drop .dcp files there and they appear here.";
}

void DevelopProperties::addCameraProfileRow(const QModelIndex &parIdx)
{
    const bool raw = currentIsRaw();
    const bool usingApple = currentRawRender().apple;
    const bool available = raw && !usingApple;

    clearItemInfo(i);
    i.name = "cameraProfile";
    i.parIdx = parIdx;
    i.parentName = "BasicHeader";
    i.captionText = "Profile";
    i.tooltip = available
        ? "The starting colour for this raw file.\n"
          "Winnow Standard: Winnow's own colour. Camera Base: the camera maker's colour "
          "with no added look. Other entries are camera profiles installed on this "
          "computer (Adobe's free DNG Converter installs them).\n\n"
          "Profiles are read from:"
          + profileRootsText() + ownRootText()
        : (raw ? "Apple's decoder uses its own profile. Choose Winnow as the decoder to "
                 "pick a profile here."
               : "Raw files only. This file already has its camera's colour.");
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "cameraProfile";
    i.delegateType = DT_None;       // we own the value cell
    addItem(i);
    setItemEnabled("cameraProfile", available);

    const QModelIndex valIdx = findValueIndex("cameraProfile");
    if (!valIdx.isValid()) return;

    QWidget *cell = new QWidget;
    cell->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *vhb = new QHBoxLayout(cell);
    vhb->setContentsMargins(0, 0, 10, 0);
    vhb->setSpacing(0);

    if (!available) {
        /*
            THE REASON, IN THE VALUE CELL, and the two reasons are NOT interchangeable --
            greyed control plus a brief inline reason, never a popup after the fact. A
            JPEG has no sensor to characterise; the Apple decoder HAS a sensor but has
            already applied a profile of its own before Winnow sees the pixels, and the
            fix for that one is a setting the user can change.
        */
        QLabel *why = new QLabel(raw ? "Apple decoder" : "raw only");
        why->setStyleSheet(G::labelCss(G::disabledColor, G::strFontSize.toInt()));
        why->setEnabled(false);
        why->setAttribute(Qt::WA_TransparentForMouseEvents);
        vhb->addWidget(why);
        vhb->addStretch(1);
        cameraProfileCombo = nullptr;
        setIndexWidget(valIdx, cell);
        return;
    }

    QComboBox *combo = new QComboBox;
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cameraProfileCombo = combo;
    connect(combo, QOverload<int>::of(&QComboBox::activated), this,
            [this, combo](int ix){
                /* The click ends the hover: the selection becomes the real state, and
                   leaving the override up would make the next render fight it. */
                endComboHoverPreview();
                setCameraProfile(combo->itemData(ix).toString());
            });
    vhb->addWidget(combo);

    armComboHover(combo, [this, combo](int ix){
        previewCameraProfile(combo->itemData(ix).toString());
    });

    setIndexWidget(valIdx, cell);

    /*
        The index is built on a background thread the first time anything asks for it
        (~0.5 s over ~4400 installed profiles), so the combo is filled now with whatever
        is known -- the built-in entry at least -- and refilled when the sweep lands. A
        UniqueConnection because this row is rebuilt every time the tree is.
    */
    CameraProfileStore::instance().ensureIndex();
    connect(&CameraProfileStore::instance(), &CameraProfileStore::indexChanged,
            this, &DevelopProperties::refreshCameraProfileRow,
            Qt::UniqueConnection);
    refreshCameraProfileRow();
}

void DevelopProperties::repopulateCameraProfileCombo()
{
    if (!cameraProfileCombo) return;

    QString stored;
    if (!currentImagePath.isEmpty()) {
        const EditStack &s = stackCache[currentImagePath];
        if (!s.scopes.isEmpty()) stored = s.scopes[0].params.cameraProfile;
    }

    const QString model = mw ? mw->cameraModelFor(currentImagePath) : QString();
    const QList<CameraProfileStore::Entry> found =
        CameraProfileStore::instance().forModel(model);

    QSignalBlocker block(cameraProfileCombo);
    cameraProfileCombo->clear();
    /* The built-in FIRST: it is the identity, so it heads the list the way every other
       adjustment's identity sits at the start of its range. The rest arrive sorted, with
       any synthesised "Camera Base" among them rather than tacked on the end. */
    cameraProfileCombo->addItem(kBuiltInProfileLabel, QString());
    /* A RULE UNDER THE BUILT-IN, and only when there is something below it. Winnow
       Standard is the identity -- choosing it is a reset, not a choice -- so it is a
       different kind of thing from the installed profiles and reads better set apart
       from them. An empty group would draw a rule with nothing under it, hence the
       guard. */
    if (!found.isEmpty()) {
        cameraProfileCombo->insertSeparator(cameraProfileCombo->count());
        for (const CameraProfileStore::Entry &e : found)
            cameraProfileCombo->addItem(e.name, e.name);
    }
    else if (CameraProfileStore::instance().indexReady()) {
        /*
            NOTHING FOUND, AND THE SWEEP IS FINISHED -- so this is the answer, not the
            half-second before the answer, which is why it waits on indexReady() exactly
            as the "(not installed)" row below does.

            It is a DISABLED ROW IN THE LIST rather than a label in the value cell: the
            value cell's greyed-reason pattern is for a control that cannot be used, and
            this one can -- Winnow Standard is a real choice and stays selectable. The
            explanation belongs where the user goes looking for the missing profiles,
            which is inside the dropdown. The tooltip names the folders.
        */
        cameraProfileCombo->insertSeparator(cameraProfileCombo->count());
        cameraProfileCombo->addItem(kNoProfilesLabel);
        if (auto *m = qobject_cast<QStandardItemModel *>(cameraProfileCombo->model()))
            if (QStandardItem *it = m->item(cameraProfileCombo->count() - 1))
                it->setFlags(it->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    }

    if (stored.isEmpty()) { cameraProfileCombo->setCurrentIndex(0); return; }

    int ix = cameraProfileCombo->findData(stored);
    if (ix < 0) {
        /*
            A STORED PROFILE THAT NO LONGER RESOLVES -- uninstalled, or a sidecar carried
            from another machine. It is shown, marked, and SELECTED rather than quietly
            replaced by the built-in: the render falls back to the built-in matrix either
            way, and the user is entitled to know which of those two things happened. The
            stored name is kept intact (the item's DATA is still the bare name), so
            re-installing the profile restores the picture rather than needing it
            re-chosen.

            Not added while the index is still building: "not installed" would be a lie
            for the half-second before the sweep lands.
        */
        if (!CameraProfileStore::instance().indexReady()) return;
        cameraProfileCombo->addItem(stored + "  (not installed)", stored);
        ix = cameraProfileCombo->count() - 1;
    }
    cameraProfileCombo->setCurrentIndex(ix);
}

/*
    Show a profile on the loupe without committing to it.

    The stored stack, the sliders and the history are untouched -- this is the same
    previewStack override the Presets list and History put up, so there is ONE preview
    state and one way to end it however it was started.
*/
void DevelopProperties::previewCameraProfile(const QString &name)
{
    if (currentImagePath.isEmpty()) return;
    const EditStack s = stackCache.value(currentImagePath);
    if (s.scopes.isEmpty()) return;
    EditStack preview = s;
    /* Scope 0, whatever scope is active: a profile characterises the CAMERA, exactly as
       setCameraProfile writes it. */
    preview.scopes[0].params.cameraProfile = name;
    showComboHoverPreview(preview, s.scopes[0].params.cameraProfile == name);
}

void DevelopProperties::previewViewTransform(int vt)
{
    if (currentImagePath.isEmpty()) return;
    const EditStack s = stackCache.value(currentImagePath);
    if (s.scopes.isEmpty()) return;
    EditStack preview = s;
    preview.scopes[0].params.viewTransform = vt;    // scope 0, as setViewTransform
    showComboHoverPreview(preview, s.scopes[0].params.viewTransform == vt);
}

void DevelopProperties::showComboHoverPreview(const EditStack &preview, bool sameAsStored)
{
    /* The crop overlay and an open mask tool own the canvas; a hover preview underneath
       them would fight for it (the same guard previewPreset and previewHistoryEntry use). */
    if (maskPanelOpen || spotMode) return;
    /* Back on the value already in effect: take the override down rather than putting up
       an identical one. Hovering it with nothing previewed costs nothing at all -- the
       loupe is already showing it, and a render producing the same picture is still a
       whole develop pass. */
    if (sameAsStored) { endComboHoverPreview(); return; }

    clearBeforeAfterLatch();            // this hover is now the override, not Before
    previewStack = preview;
    previewActive = true;
    comboHoverActive = true;
    emit historyPreviewChanged();       // proxy render only, no full-res settle
}

void DevelopProperties::endComboHoverPreview()
{
    if (comboHoverTimer) comboHoverTimer->stop();
    if (!comboHoverActive) return;
    comboHoverActive = false;
    endHistoryPreview();                // one preview override, one way to end it
}

/*
    HOVER PREVIEW for a Basic dropdown. `highlighted` is the right signal rather than a
    mouse handler on the popup view: it fires for the KEYBOARD too, so arrowing through
    the list previews exactly as hovering does, and it reports the index the combo itself
    considers current.

    The popup view is watched for Hide because QComboBox reports a selection and never a
    dismissal -- without it, pressing Esc or clicking away would leave the loupe showing
    a value the image does not have (see eventFilter).
*/
void DevelopProperties::armComboHover(QComboBox *combo, std::function<void(int ix)> fire)
{
    /* ONE timer for the life of the panel, not one per tree rebuild: these rows are
       rebuilt whenever the tree is, and a new QTimer each time would accumulate as
       children of `this`, each still connected and each able to fire a preview for a
       combo that no longer exists. What it runs is the pending action the last
       `highlighted` set, so the one timer serves every row. */
    if (!comboHoverTimer) {
        comboHoverTimer = new QTimer(this);
        comboHoverTimer->setSingleShot(true);
        connect(comboHoverTimer, &QTimer::timeout, this, [this]{
            if (comboHoverPending) comboHoverPending();
        });
    }
    connect(combo, &QComboBox::highlighted, this, [this, combo, fire](int ix){
        if (ix < 0) return;
        /* A QPointer guard: the combo dies with a tree rebuild, possibly while the
           debounce is still pending. */
        QPointer<QComboBox> guard(combo);
        comboHoverPending = [guard, fire, ix]{ if (guard) fire(ix); };
        comboHoverTimer->start(kComboHoverDelayMs);
    });
    if (combo->view()) combo->view()->installEventFilter(this);
}

bool DevelopProperties::eventFilter(QObject *watched, QEvent *event)
{
    /* A right-click anywhere in the Develop dock that no child answered bubbles up to the
       dock container (this tree and the scopes strip are the only children with a menu of
       their own), where it arrives with the position translated into container
       coordinates. childAt names the control under the cursor -> the band it belongs to. */
    if (event->type() == QEvent::ContextMenu && watched == panelContainer && panelContainer) {
        QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
        showPanelContextMenu(sectionAt(panelContainer->childAt(e->pos())), e->globalPos());
        return true;
    }
    if (event->type() == QEvent::Hide &&
        ((cameraProfileCombo && watched == cameraProfileCombo->view()) ||
         (viewTransformCombo && watched == viewTransformCombo->view()))) {
        /* Stopped SYNCHRONOUSLY so a hover that has not fired yet cannot put a preview up
           after the popup has already gone. */
        if (comboHoverTimer) comboHoverTimer->stop();
        /* Deferred: on a CLICK the view hides before activated() is delivered, so ending
           the preview inline here would restore the old value and then immediately be
           overwritten -- one wasted full render, and a visible flash of the previous
           rendering on the way past. By the time this runs, activated() has been handled
           and has already ended the preview itself, so this is a no-op on that path and
           only does work on a dismissal. */
        QMetaObject::invokeMethod(this, [this]{ endComboHoverPreview(); },
                                  Qt::QueuedConnection);
    }
    return PropertyEditor::eventFilter(watched, event);
}

void DevelopProperties::setCameraProfile(const QString &name)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    if (s.scopes[0].params.cameraProfile == name) return;
    s.scopes[0].params.cameraProfile = name;    // always scope 0, whichever is active
    noteScopeEdit("Global", "Profile",
                  name.isEmpty() ? QString(kBuiltInProfileLabel) : name,
                  "global/cameraProfile");
    /* A different profile may or may not carry a tone curve ("Profile curve"). */
    refreshViewTransformRow();
    /* ...and reads "as shot" as a different temperature (see currentCam). */
    refreshWbRow();
    emit paramsChanged();
}

/* Push the stored value back into the combo. The LIST can change with the image (a
   different camera has different profiles), so this repopulates rather than just
   re-selecting. Blocked throughout, so re-populating on an image change cannot look like
   a user edit and write a history entry. */
void DevelopProperties::refreshCameraProfileRow()
{
    repopulateCameraProfileCombo();
}

void DevelopProperties::addViewTransformRow(const QModelIndex &parIdx)
{
    const bool raw = currentIsRaw();

    clearItemInfo(i);
    i.name = "viewTransform";
    i.parIdx = parIdx;
    i.parentName = "BasicHeader";
    i.captionText = "Tone mapping";
    i.tooltip = raw
        ? "How the brightness range is fitted to the screen.\n"
          "Standard is the usual choice. Hue-preserving keeps bright skies blue. Soft "
          "rolls off highlights more gently. Camera contrast matches the camera's JPEGs. "
          "None is flat and dark, a starting point only."
        : "Raw files only. This file already has its camera's tone.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "viewTransform";
    i.delegateType = DT_None;       // we own the value cell
    addItem(i);
    /* Breathing room between the white-balance divider above and this row. The delegate
       splits UR_ExtraRowHeight top+bottom around the vertically-centred content (see
       PropertyDelegate::sizeHint), so 6 buys the 3px gap above -- and the same 3px below,
       which suits a row that heads a group rather than belonging to the one above it. */
    model->setData(capIdx, 6, UR_ExtraRowHeight);
    setItemEnabled("viewTransform", raw);

    const QModelIndex valIdx = findValueIndex("viewTransform");
    if (!valIdx.isValid()) return;

    QWidget *cell = new QWidget;
    cell->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *vhb = new QHBoxLayout(cell);
    vhb->setContentsMargins(0, 0, 10, 0);
    vhb->setSpacing(0);

    if (!raw) {
        /* The REASON, in the value cell where the control would have been -- the rule is
           greyed control + brief inline reason, never a popup after the fact. */
        QLabel *why = new QLabel("raw only");
        why->setStyleSheet(G::labelCss(G::disabledColor, G::strFontSize.toInt()));
        why->setEnabled(false);
        why->setAttribute(Qt::WA_TransparentForMouseEvents);
        vhb->addWidget(why);
        vhb->addStretch(1);
        viewTransformCombo = nullptr;
        setIndexWidget(valIdx, cell);
        return;
    }

    QComboBox *combo = new QComboBox;
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    /*
        THE TWO RENDERINGS FIRST, THEN A SEPARATOR, THEN None.

        This does NOT follow the usual "identity heads the range" rule, and the exception
        is the point. Every other adjustment's identity is a neutral starting position; this
        one's is unrendered scene-linear data, which is dark and flat (see
        EditParams::kDefaultViewTransform). Heading the list with it put the one entry
        nobody wants where the eye lands first and buried the default below it.

        So the list is ordered by what a user is actually choosing between -- two ways of
        rolling highlights off, differing by one word because that is their real
        relationship -- with None set apart by the separator as what it is: not a third
        rendering but the absence of one, for building a look from scratch.
    */
    /* LABELS, not identifiers. The enum, the sidecar key and every constant stay "Filmic"
       and "AgX" -- those are the published names of the transforms and what the maths
       implements. The USER-FACING labels say what the options DO.

       "Filmic" had to go once it became the default: it reads as a creative choice, a film
       emulation to opt into, when it is the plain rendering that lands where every other
       raw developer lands. Someone looking for the ordinary picture would not have picked
       it. "AgX" is short for silver halide and is guessable by nobody. The help page names
       both so the terms stay searchable for anyone who has read about them elsewhere. */
    combo->addItem("Standard roll-off", int(OutputTransform::ViewTransform::Filmic));
    combo->addItem(viewTransformName(int(OutputTransform::ViewTransform::FilmicHue)),
                   int(OutputTransform::ViewTransform::FilmicHue));
    combo->addItem("Soft roll-off", int(OutputTransform::ViewTransform::AgX));
    /* The camera-style pair: shapes taken from cameras' own renderings, finished with the
       same roll-off. Profile curve's availability follows the PROFILE, so its text and
       enabled state are set by refreshViewTransformRow. */
    combo->addItem(viewTransformName(int(OutputTransform::ViewTransform::CameraContrast)),
                   int(OutputTransform::ViewTransform::CameraContrast));
    combo->addItem(viewTransformName(int(OutputTransform::ViewTransform::ProfileCurve)),
                   int(OutputTransform::ViewTransform::ProfileCurve));
    combo->insertSeparator(combo->count());
    combo->addItem("None", int(OutputTransform::ViewTransform::None));
    connect(combo, QOverload<int>::of(&QComboBox::activated), this,
            [this, combo](int ix){
                endComboHoverPreview();     // the click makes the hover the real state
                setViewTransform(combo->itemData(ix).toInt());
            });
    /* Hover previews, exactly as the Profile row above (armComboHover). A disabled
       "Profile curve" is never highlighted, so it cannot preview a fallback. */
    armComboHover(combo, [this, combo](int ix){
        previewViewTransform(combo->itemData(ix).toInt());
    });
    viewTransformCombo = combo;
    /* NO trailing stretch: the combo is Expanding, and a stretch item would take the
       spare width and leave the dropdown at its sizeHint -- narrower than Profile's,
       directly above it, which reads as a mistake. */
    vhb->addWidget(combo);

    setIndexWidget(valIdx, cell);
    refreshViewTransformRow();
}

/*
    Does the selected camera profile carry a tone curve? The Camera Standard / Neutral /
    Vivid ... family do; Adobe Standard, a synthesised "Camera Base" and Winnow's own
    matrix do not. Decides whether "Profile curve" is available -- the curve is one tone
    mapping among the others now, never an override (see OutputTransform::EffectiveView).
*/
bool DevelopProperties::profileHasToneCurve() const
{
    if (currentImagePath.isEmpty()) return false;
    const EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) return false;
    const QString name = s.scopes[0].params.cameraProfile;
    if (name.isEmpty()) return false;
    const QString model = mw ? mw->cameraModelFor(currentImagePath) : QString();
    const auto prof = CameraProfileStore::instance().profile(model, name);
    return prof && !prof->toneCurve.empty();
}

void DevelopProperties::setViewTransform(int vt)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.scopes.isEmpty()) s.scopes.append(EditScope());
    if (s.scopes[0].params.viewTransform == vt) return;
    s.scopes[0].params.viewTransform = vt;      // always scope 0, whichever is active
    noteScopeEdit("Global", "Tone mapping", viewTransformName(vt),
                  "global/viewTransform");
    emit paramsChanged();
}

/* The label for a stored value, shared by the combo and the history entry so the two can
   never disagree. An unknown value reads as the IDENTITY rather than as a number, which is
   also what OutputTransform::ViewFromInt renders it as -- so the history says what the
   image actually shows. */
QString DevelopProperties::viewTransformName(int vt)
{
    switch (static_cast<OutputTransform::ViewTransform>(vt)) {
    case OutputTransform::ViewTransform::AgX:            return "Soft roll-off";
    case OutputTransform::ViewTransform::Filmic:         return "Standard roll-off";
    case OutputTransform::ViewTransform::CameraContrast: return "Camera contrast";
    case OutputTransform::ViewTransform::ProfileCurve:   return "Profile curve";
    case OutputTransform::ViewTransform::FilmicHue:
        return "Standard roll-off (hue-preserving)";
    default: break;
    }
    return "None";
}

/* Push the stored value back into the combo. Blocked so re-populating on image change
   cannot look like a user edit and write a history entry. */
void DevelopProperties::refreshViewTransformRow()
{
    if (!viewTransformCombo) return;

    /* "Profile curve" is only real while the profile carries a curve. Otherwise the item
       is greyed with its reason in the text -- greyed control + brief inline reason --
       and, if it is the stored choice, the render falls back to Standard roll-off
       (EffectiveView), which the text says. The stored value is left ALONE: it comes back
       into effect when a profile with a curve is chosen. */
    const int pcIx = viewTransformCombo->findData(
        int(OutputTransform::ViewTransform::ProfileCurve));
    if (auto *m = qobject_cast<QStandardItemModel*>(viewTransformCombo->model());
        m && pcIx >= 0) {
        const bool has = profileHasToneCurve();
        QStandardItem *it = m->item(pcIx);
        it->setEnabled(has);
        it->setText(has ? QString("Profile curve")
                        : QString("Profile curve (none in this profile)"));
        it->setToolTip(has ? QString("The tone curve of the selected camera profile.")
                           : QString("This profile has no tone curve. Choose a Camera "
                                     "Standard, Neutral or Vivid profile. Until then it "
                                     "looks like Standard."));
    }

    int stored = 0;
    if (!currentImagePath.isEmpty()) {
        const EditStack &s = stackCache[currentImagePath];
        if (!s.scopes.isEmpty()) stored = s.scopes[0].params.viewTransform;
    }
    const int ix = viewTransformCombo->findData(stored);
    if (ix >= 0) {
        QSignalBlocker block(viewTransformCombo);
        viewTransformCombo->setCurrentIndex(ix);
    }
}

/* Lowest checked primary: which one the Hue / Saturation sliders READ (they write all of
   them). Mirrors firstActiveGradeRange. */
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
    addColorGradeBody(parIdx, "ColorGradeHeader");
}

/* The section's rows under `parent` -- its own header, or the combined Color
   panel's (addColorPanel). */
void DevelopProperties::addColorGradeBody(const QModelIndex &parIdx, const QString &parent)
{
    /* Range selector: four radio buttons on one spanned row (Shadows / Midtones /
       Highlights / Global), exclusive, so the wheel and the Luminance slider always edit
       exactly ONE range and always show that range's own values. They are UI state
       (gradeActiveMask, one bit set), not EditParams. */
    clearItemInfo(i);
    i.name = "gradeRanges";
    i.parIdx = parIdx;
    i.parentName = parent;
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
        QRadioButton *dark  = new QRadioButton("Shadows",    rw);
        QRadioButton *mid   = new QRadioButton("Midtones",   rw);
        QRadioButton *light = new QRadioButton("Highlights", rw);
        QRadioButton *glob  = new QRadioButton("Global",     rw);
        glob->setToolTip("Tint every pixel, whatever its tone.");
        /* The app QSS gives every QWidget the widget background, which paints a band
           behind each caption on the tree's panel colour; the Raw and Fill Replace
           panels' radios clear it the same way. */
        for (QRadioButton *r : {dark, mid, light, glob})
            r->setStyleSheet("QRadioButton { background: transparent; }");
        QButtonGroup *group = new QButtonGroup(rw);   // exclusive by default
        group->addButton(dark,  0x1);
        group->addButton(mid,   0x2);
        group->addButton(light, 0x4);
        group->addButton(glob,  0x8);
        /* Older builds allowed several ranges at once; keep the first of them. */
        const int range = firstActiveGradeRange();
        gradeActiveMask = 1 << range;
        if (QAbstractButton *b = group->button(gradeActiveMask)) b->setChecked(true);
        connect(group, &QButtonGroup::idToggled, this, [this](int id, bool on){
            if (!on) return;             // the button being unchecked; its partner follows
            gradeActiveMask = id;
            if (colorGradeWheel) colorGradeWheel->setActiveMask(gradeActiveMask);
            refreshColorGradeRow();      // point the Lum slider at the new active range
        });
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
    i.parentName = parent;
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

    /* Hue / Saturation: the wheel's two axes as sliders (Lightroom shows the same pair
       under its wheels), so the selected range can be read as numbers and nudged by the
       arrow keys. They write the same params the wheel does and track it both ways (see
       setGradeHueSat / refreshColorGradeRow). Hue 0..360 deg, Saturation 0..100 (the
       wheel radius, stored as 0..1). */
    addSlider("gradeHue", "Hue",
              "Hue of the selected tonal range's tint (degrees around the wheel).",
              parIdx, parent, 0, 360, 0, "spectrum", "");   // hue ramp groove
    addSlider("gradeSat", "Saturation",
              "Strength of the selected tonal range's tint (distance from the wheel "
              "centre).",
              parIdx, parent, 0, 100, 0, G::darkgray, G::lightgray);

    /* Luminance for the checked range(s). Integer -100..100 like the colour sliders. */
    addSlider("gradeLum", "Luminance",
              "Brighten / darken the selected tonal range.",
              parIdx, parent, -100, 100, 0, G::darkgray, G::lightgray);

    /* Window SHAPE: panel-wide, not per-range, so they sit below the divider. Blending
       defaults to 50 and Balance to 0, which reproduce the fixed split the panel used
       before they existed. */
    addGroupDivider(parIdx, parent, "GradeDivider");
    addSlider("gradeBlending", "Blending",
              "How much the shadow, midtone and highlight tints overlap.",
              parIdx, parent, 0, 100, 0, G::darkgray, G::lightgray, 50);
    addSlider("gradeBalance", "Balance",
              "Slide the split between the ranges: right favours highlights, "
              "left favours shadows.",
              parIdx, parent, -100, 100, 0, G::darkgray, G::lightgray);
    addGroupDivider(parIdx, parent, "ColorGradeEndDivider");
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

/* Hue or Saturation slider -> the selected range's grade hue/sat (an absolute write: the
   radio buttons are exclusive, so there is exactly one range and nothing to keep apart).
   sat arrives on the slider's 0..100 scale and is stored as the wheel's 0..1. */
void DevelopProperties::setGradeHueSat(bool isHue, float v)
{
    if (currentImagePath.isEmpty()) return;
    EditParams &p = activeParams();
    float *hue[4] = {&p.gradeShadowHue, &p.gradeMidHue, &p.gradeHighHue, &p.gradeGlobalHue};
    float *sat[4] = {&p.gradeShadowSat, &p.gradeMidSat, &p.gradeHighSat, &p.gradeGlobalSat};
    for (int r = 0; r < 4; ++r) {
        if (!(gradeActiveMask & (1 << r))) continue;
        if (isHue) *hue[r] = qBound(0.0f, v, 360.0f);
        else       *sat[r] = qBound(0.0f, v / 100.0f, 1.0f);
    }
}

/* Wheel drag -> the active scope's grade hue/sat for every checked range, then preview.
   commit marks the drag-release, when the debounced sidecar write is scheduled (live
   moves only render; dirty is set every move so navigate-away still persists). */
void DevelopProperties::onGradeWheelChanged(bool commit)
{
    if (currentImagePath.isEmpty() || !colorGradeWheel) return;
    noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way
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
    refreshColorGradeRow();      // the Hue / Saturation readout tracks the drag
    emit paramsChanged();
}

/* Push the active scope's stored grade into the wheel dots + the Hue / Saturation /
   Luminance sliders (which show the selected range's values). Guards the slider set so it
   does not feed back as an edit. Called on image/scope load, whenever the range radio
   buttons change, and on every wheel drag / Hue-Sat nudge so the two faces track. */
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
    const float hue[4] = {p.gradeShadowHue, p.gradeMidHue, p.gradeHighHue, p.gradeGlobalHue};
    const float sat[4] = {p.gradeShadowSat, p.gradeMidSat, p.gradeHighSat, p.gradeGlobalSat};
    const float lum[4] = {p.gradeShadowLum, p.gradeMidLum, p.gradeHighLum, p.gradeGlobalLum};
    const bool wasPop = isPopulating;
    isPopulating = true;                 // don't let setValue echo back as an edit
    setSliderReal("gradeHue", qRound(hue[r]));
    setSliderReal("gradeSat", qRound(sat[r] * 100.0f));
    setSliderReal("gradeLum", lum[r]);
    isPopulating = wasPop;
}

/* --------------------------------------------------------------------------------------
   The combined Color panel

   One "Color" section in place of the separate colour panels: three radio buttons pick
   which adjustment it shows -- Tone = Color Grade, Hue = Point Color, RGB = Calibrate.
   Only the selected body is BUILT (each body is the same builder its own section uses,
   under this header), so every key, widget pointer and refresh path is exactly as in
   the separate layout; a switch rebuilds the tree, deferred because the click is inside
   a row the rebuild destroys. The mode is UI state, remembered across sessions.
   ------------------------------------------------------------------------------------ */

int DevelopProperties::colorPanelModeGroup() const
{
    switch (colorPanelMode) {
    case 1:  return PV_PointColor;
    case 2:  return PV_Calibrate;
    default: return PV_ColorGrade;
    }
}

void DevelopProperties::setCombinedColorPanel(bool on)
{
    if (G::isLogger) G::log("DevelopProperties::setCombinedColorPanel");
    if (combinedColorPanel == on) return;
    combinedColorPanel = on;
    setting->setValue("Develop/combinedColorPanel", on);
    buildTree();
}

void DevelopProperties::addColorPanel()
{
    if (G::isLogger) G::log("DevelopProperties::addColorPanel");
    addHeader("ColorPanelHeader", "???", "Color",
              "Colour: Tone (grading by tonal range), Hue (adjust sampled colours) and "
              "RGB (re-point the primaries).", PV_ColorPanel);
    QModelIndex parIdx = capIdx;

    /* THE GAPS around the radios are EXPLICIT, kModeGapPt points (12 px at the default
       12 pt font, scaling with it) above them to the header band and below them to the
       divider's rule. So the header's TopSpacer is shrunk to its 1 px minimum here (it
       would otherwise put a full groupSeparatorHeight above), and the radio row is sized
       to exactly gap + radios + gap, less what the neighbours already contribute: that
       1 px above, and half the divider below (it draws its rule at its own middle). */
    constexpr double kModeGapPt = 12.0;
    const int gap = qMax(2, int(std::lround(G::strFontSize.toInt() * G::ptToPx
                                            * kModeGapPt / 12.0)));
    const int halfSep = G::groupSeparatorHeight / 2;
    model->setData(model->index(0, CapColumn, parIdx), 1, UR_DividerHeight);  // TopSpacer

    clearItemInfo(i);
    i.name = "colorPanelModes";
    i.parIdx = parIdx;
    i.parentName = "ColorPanelHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex modesIdx = capIdx;
    setFirstColumnSpanned(modesIdx.row(), parIdx, true);
    {
        QWidget *rw = new QWidget;
        colorPanelModesRow = rw;           // centred in the panel, updateEditorGeometries
        rw->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(rw);
        hb->setSpacing(10);
        hb->addStretch(1);
        QRadioButton *tone = new QRadioButton("Tone", rw);
        QRadioButton *hue  = new QRadioButton("Hue",  rw);
        QRadioButton *rgb  = new QRadioButton("RGB",  rw);
        tone->setToolTip("Grade by tonal range: tint the shadows, midtones, highlights "
                         "or the whole image.");
        hue->setToolTip("Sample colours in the image and shift their hue, saturation "
                        "and luminance.");
        rgb->setToolTip("Adjust the base red, green and blue colours.");
        for (QRadioButton *r : {tone, hue, rgb})      // panel background, see gradeRanges
            r->setStyleSheet("QRadioButton { background: transparent; }");
        QButtonGroup *group = new QButtonGroup(rw);   // exclusive by default
        group->addButton(tone, 0);
        group->addButton(hue,  1);
        group->addButton(rgb,  2);
        colorPanelRadios[0] = tone;        // updateSectionHeaderCaptions stars them
        colorPanelRadios[1] = hue;
        colorPanelRadios[2] = rgb;
        if (QAbstractButton *b = group->button(colorPanelMode)) b->setChecked(true);
        connect(group, &QButtonGroup::idToggled, this, [this](int id, bool on){
            if (!on || id == colorPanelMode) return;
            colorPanelMode = id;
            setting->setValue("Develop/colorPanelMode", id);
            /* The eyedropper belongs to Hue: leaving it puts the dropper away. */
            if (id != 1) cancelPointColorPick();
            QTimer::singleShot(0, this, [this]{ buildTree(); });
        });
        hb->addWidget(tone);
        hb->addWidget(hue);
        hb->addWidget(rgb);
        hb->addStretch(1);               // stretch both sides: centred in the row
        /* Now the radios exist, size the row around them (see the gap note above). */
        int radioH = 0;
        for (QRadioButton *r : {tone, hue, rgb}) radioH = qMax(radioH, r->sizeHint().height());
        const int top = qMax(0, gap - 1);
        const int bottom = qMax(0, gap - halfSep);
        hb->setContentsMargins(0, top, 0, bottom);
        model->setData(modesIdx, top + radioH + bottom - G::propertyRowHeight(),
                       UR_ExtraRowHeight);
        setIndexWidget(modesIdx, rw);
    }
    addGroupDivider(parIdx, "ColorPanelHeader", "ColorPanelModesDivider");

    switch (colorPanelMode) {
    case 1:  addPointColorBody(parIdx, "ColorPanelHeader"); break;
    case 2:  addCalibrateBody(parIdx, "ColorPanelHeader");  break;
    default: addColorGradeBody(parIdx, "ColorPanelHeader"); break;
    }
}

/* --------------------------------------------------------------------------------------
   Point Color

   Lightroom's Color Mixer > Point Color, without the Mixer. The eyedropper samples a
   colour off the image (one-shot, like the WB dropper); each sample is a swatch, and the
   sliders below the PointColorWidget edit the SELECTED one (pointColorSel -- UI state,
   like gradeActiveMask). The sliders are not plain applyKeyToParams keys: they need the
   selection, so itemChange routes the pc* keys through setPointColorSlider. Maths and
   encoding in Develop/pointcolor.h.
   ------------------------------------------------------------------------------------ */

void DevelopProperties::addPointColor()
{
    if (G::isLogger) G::log("DevelopProperties::addPointColor");
    addHeader("PointColorHeader", "???", "Point Color",
              "Sample colours from the image and shift each one's hue, saturation and "
              "luminance.", PV_PointColor);
    QModelIndex parIdx = capIdx;
    addPointColorBody(parIdx, "PointColorHeader");
}

/* The section's rows under `parent` -- its own header, or the combined Color
   panel's (addColorPanel). */
void DevelopProperties::addPointColorBody(const QModelIndex &parIdx, const QString &parent)
{
    /* The dropper / swatches / field / before|after widget, on one tall spanned row. */
    clearItemInfo(i);
    i.name = "pointColorWidget";
    i.parIdx = parIdx;
    i.parentName = parent;
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex wIdx = capIdx;
    model->setData(wIdx, PointColorWidget::preferredHeight() - 16, UR_ExtraRowHeight);
    setFirstColumnSpanned(wIdx.row(), parIdx, true);
    {
        PointColorWidget *w = new PointColorWidget;
        pointColorWidget = w;
        w->setDropperIcons(dropperIcon(false), dropperIcon(true));
        w->setPickArmed(pointColorPickActive, pointColorPickAdd);
        /* A dropper button toggles ITS mode: pressing the other one switches over. */
        connect(w, &PointColorWidget::pickToggled, this, [this]{
            setPointColorPickActive(!pointColorPickActive || pointColorPickAdd, false);
        });
        connect(w, &PointColorWidget::addSampleToggled, this, [this]{
            setPointColorPickActive(!pointColorPickActive || !pointColorPickAdd, true);
        });
        connect(w, &PointColorWidget::lastSampleRemoved, this, [this](int idx){
            removePointColorSamples(idx, false);
        });
        connect(w, &PointColorWidget::addedSamplesCleared, this, [this](int idx){
            removePointColorSamples(idx, true);
        });
        connect(w, &PointColorWidget::pointSelected, this, [this](int idx){
            pointColorSel = idx;
            refreshPointColorRow();
            if (pointColorVisualize) emit paramsChanged();   // show the new point's range
        });
        connect(w, &PointColorWidget::pointDeleted, this, [this](int idx){
            deletePointColor(idx);
        });
        connect(w, &PointColorWidget::allPointsDeleted, this, [this]{
            deletePointColor(-1);
        });
        connect(w, &PointColorWidget::shiftsDragged,
                this, &DevelopProperties::onPointColorShiftsDragged);
        setIndexWidget(wIdx, w);
    }

    /* The selected point's shifts. Integer -100..100, 0 = no change. */
    addSlider("pcHueShift", "Hue Shift",
              "Rotate the hue of the selected colour.",
              parIdx, parent, -100, 100, 0, G::darkgray, G::lightgray);
    addSlider("pcSatShift", "Sat. Shift",
              "Saturate or desaturate the selected colour.",
              parIdx, parent, -100, 100, 0, G::darkgray, G::lightgray);
    addSlider("pcLumShift", "Lum. Shift",
              "Brighten or darken the selected colour.",
              parIdx, parent, -100, 100, 0, G::darkgray, G::lightgray);
    addSlider("pcVariance", "Variance",
              "Left evens out a colour (e.g. blotchy skin); right spreads it apart.",
              parIdx, parent, -100, 100, 0, G::darkgray, G::lightgray);

    /* Which pixels count as "the selected colour". Range widens or narrows all three
       at once; the indented three refine one axis each. 50 = the nominal width. */
    addGroupDivider(parIdx, parent, "PointColorRangeDivider");
    addSlider("pcRange", "Range",
              "How far from the sampled colour the adjustment reaches.",
              parIdx, parent, 0, 100, 0, G::darkgray, G::lightgray, 50);
    addSlider("pcHueRange", "   Hue",
              "How far in hue from the sample the adjustment reaches.",
              parIdx, parent, 0, 100, 0, G::darkgray, G::lightgray, 50);
    addSlider("pcSatRange", "   Saturation",
              "How far in saturation from the sample the adjustment reaches.",
              parIdx, parent, 0, 100, 0, G::darkgray, G::lightgray, 50);
    addSlider("pcLumRange", "   Luminance",
              "How far in luminance from the sample the adjustment reaches.",
              parIdx, parent, 0, 100, 0, G::darkgray, G::lightgray, 50);
    addCheckbox("pcVisualize", "Visualize Range",
                "Show the pixels the selected point affects in colour and everything "
                "else in grey. A view only: nothing is saved.",
                parIdx, parent, pointColorVisualize);
    addGroupDivider(parIdx, parent, "PointColorEndDivider");
}

void DevelopProperties::setPointColorPickActive(bool on, bool addSample)
{
    /* Arming needs an image to sample and a free slot to put the sample in: a free
       point for a new one, a free sample in the selected point for an added one. */
    if (on && currentImagePath.isEmpty()) on = false;
    if (on) {
        const EditParams &p = activeParams();
        if (addSample) {
            if (pointColorSel < 0 || pointColorSel >= p.pointColorN ||
                p.pointColor[pointColorSel].extraN >= PointColor::kMaxExtra)
                on = false;
        }
        else if (p.pointColorN >= PointColor::kMaxPts) on = false;
    }
    addSample = on && addSample;
    if (pointColorPickActive == on) {
        /* Already armed: only the mode can change (no re-arm of ImageView needed). */
        if (pointColorPickAdd != addSample) {
            pointColorPickAdd = addSample;
            if (pointColorWidget) pointColorWidget->setPickArmed(on, addSample);
        }
        return;
    }
    pointColorPickActive = on;
    pointColorPickAdd = addSample;
    /* The dropper reads the DISPLAYED image, so it must not sample the grey of a
       visualised range: showing the real colours again is the only sensible state. */
    if (on && pointColorVisualize) {
        pointColorVisualize = false;
        const bool wasPop = isPopulating;
        isPopulating = true;
        setCheckboxValue("pcVisualize", false);
        isPopulating = wasPop;
        emit paramsChanged();
    }
    if (pointColorWidget) pointColorWidget->setPickArmed(on, addSample);
    if (on) emit pointColorPickBegin();
    else    emit pointColorPickEnd();
}

/* The eyedropper landed (MW::onPointColorPicked, from the DISPLAYED image). Append the
   sample with neutral shifts and select it, so the sliders immediately edit it -- or,
   from the add-sample dropper or on a Shift-click (extend), add it to the SELECTED
   point's samples, widening the colours that point reaches. A Shift-click keeps the
   dropper armed so several samples can be taken in a row. */
void DevelopProperties::addPointColorFromPixel(float r, float g, float b, bool extend)
{
    if (G::isLogger) G::log("DevelopProperties::addPointColorFromPixel");
    const bool toSelected = pointColorPickAdd || extend;
    if (currentImagePath.isEmpty()) { setPointColorPickActive(false); return; }
    noteNonMaskInteraction();
    EditParams &p = activeParams();
    /* Four decimals: finer than an 8-bit display step, and a stable sidecar string. */
    auto q = [](float v) { return std::round(qBound(0.0f, v, 1.0f) * 10000.0f) / 10000.0f; };
    if (toSelected && pointColorSel >= 0 && pointColorSel < p.pointColorN) {
        PointColor::Point &sp = p.pointColor[pointColorSel];
        if (!sp.addSample(q(r), q(g), q(b))) { setPointColorPickActive(false); return; }
        /* Stay armed on a Shift-click while the point has room; else one-shot. */
        if (!extend || sp.extraN >= PointColor::kMaxExtra) setPointColorPickActive(false);
        noteEdit("Point color add sample",
                 QString("%1 (%2 samples)").arg(pointColorSel + 1).arg(sp.sampleCount()),
                 QString("pointColor/sample/%1/%2").arg(pointColorSel).arg(sp.extraN));
        refreshPointColorRow();
        emit paramsChanged();
        return;
    }
    /* A new point. A Shift-click with nothing selected yet starts the first point and
       stays armed, so the next Shift-click adds to it. */
    if (!extend || p.pointColorN + 1 >= PointColor::kMaxPts) setPointColorPickActive(false);
    if (p.pointColorN >= PointColor::kMaxPts) return;
    PointColor::Point pt;
    pt.r = q(r);
    pt.g = q(g);
    pt.b = q(b);
    p.pointColor[p.pointColorN] = pt;
    pointColorSel = p.pointColorN;
    ++p.pointColorN;
    noteEdit("Point color sample", QString::number(pointColorSel + 1),
             QString("pointColor/add/%1").arg(pointColorSel));
    refreshPointColorRow();
    updateSectionHeaderCaptions();
    emit paramsChanged();
}

/* Drop the point's most recent added sample (all = every added one), narrowing it back
   toward its first sample. The first sample itself goes only with the point. */
void DevelopProperties::removePointColorSamples(int index, bool all)
{
    if (G::isLogger) G::log("DevelopProperties::removePointColorSamples");
    if (currentImagePath.isEmpty()) return;
    EditParams &p = activeParams();
    if (index < 0 || index >= p.pointColorN) return;
    PointColor::Point &pt = p.pointColor[index];
    if (pt.extraN <= 0) return;
    if (all) pt.clearExtraSamples();
    else pt.removeLastSample();
    /* The selected point may no longer be full: the add-sample dropper's state is
       re-evaluated by the refresh below. */
    noteEdit(all ? "Point color remove added samples" : "Point color remove sample",
             QString("%1 (%2 samples)").arg(index + 1).arg(pt.sampleCount()));
    pointColorSel = index;
    refreshPointColorRow();
    emit paramsChanged();
}

void DevelopProperties::deletePointColor(int index)
{
    if (G::isLogger) G::log("DevelopProperties::deletePointColor");
    if (currentImagePath.isEmpty()) return;
    EditParams &p = activeParams();
    if (p.pointColorN <= 0) return;
    if (pointColorPickAdd) setPointColorPickActive(false);   // its target is going
    if (index < 0) {
        EditParams::resetGroup(p, EditParams::Group::PointColor);
        noteEdit("Delete all point colors");
    }
    else {
        if (index >= p.pointColorN) return;
        for (int k = index; k + 1 < p.pointColorN; ++k) p.pointColor[k] = p.pointColor[k + 1];
        --p.pointColorN;
        /* The vacated slot back to default: EditParams::operator== compares them all. */
        p.pointColor[p.pointColorN] = PointColor::Point();
        noteEdit("Delete point color", QString::number(index + 1));
        if (pointColorSel >= index) pointColorSel = qMax(0, pointColorSel - 1);
    }
    refreshPointColorRow();
    updateSectionHeaderCaptions();
    emit paramsChanged();
}

/* One pc* slider -> the selected point. Values arrive on the slider's own scale, which
   is also how they are stored. */
void DevelopProperties::setPointColorSlider(const QString &key, float v)
{
    EditParams &p = activeParams();
    if (pointColorSel < 0 || pointColorSel >= p.pointColorN) return;
    PointColor::Point &pt = p.pointColor[pointColorSel];
    if      (key == "pcHueShift") pt.hueShift = qBound(-100.0f, v, 100.0f);
    else if (key == "pcSatShift") pt.satShift = qBound(-100.0f, v, 100.0f);
    else if (key == "pcLumShift") pt.lumShift = qBound(-100.0f, v, 100.0f);
    else if (key == "pcVariance") pt.variance = qBound(-100.0f, v, 100.0f);
    else if (key == "pcRange")    pt.range    = qBound(0.0f, v, 100.0f);
    else if (key == "pcHueRange") pt.hueRange = qBound(0.0f, v, 100.0f);
    else if (key == "pcSatRange") pt.satRange = qBound(0.0f, v, 100.0f);
    else if (key == "pcLumRange") pt.lumRange = qBound(0.0f, v, 100.0f);
}

/* The field / strip drag: write the selected point's three shifts, keep the sliders in
   step, and render. One history row per drag (the mergeKey). */
void DevelopProperties::onPointColorShiftsDragged(float hue, float sat, float lum,
                                                  bool commit)
{
    if (currentImagePath.isEmpty()) return;
    noteNonMaskInteraction();
    EditParams &p = activeParams();
    if (pointColorSel < 0 || pointColorSel >= p.pointColorN) return;
    PointColor::Point &pt = p.pointColor[pointColorSel];
    pt.hueShift = hue;
    pt.satShift = sat;
    pt.lumShift = lum;
    noteEdit("Point color", QString(),
             QString("pointColor/drag/%1").arg(pointColorSel));
    const bool wasPop = isPopulating;
    isPopulating = true;
    setSliderReal("pcHueShift", hue);
    setSliderReal("pcSatShift", sat);
    setSliderReal("pcLumShift", lum);
    isPopulating = wasPop;
    Q_UNUSED(commit)
    emit paramsChanged();
}

/* Stored points -> the widget and the sliders (which show the selected point). Greys
   the sliders when there is no point to edit -- the widget says why, in their place. */
void DevelopProperties::refreshPointColorRow()
{
    EditParams p;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty()) {
        const int idx = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size())
                            ? activeScopeIndex : 0;
        p = s.scopes[idx].params;
    }
    if (pointColorSel >= p.pointColorN) pointColorSel = p.pointColorN - 1;
    if (pointColorSel < 0 && p.pointColorN > 0) pointColorSel = 0;
    const bool have = pointColorSel >= 0;
    if (pointColorWidget) {
        pointColorWidget->setPoints(p.pointColorN, p.pointColor, pointColorSel);
        pointColorWidget->setPickArmed(pointColorPickActive, pointColorPickAdd);
    }
    const PointColor::Point pt = have ? p.pointColor[pointColorSel] : PointColor::Point();
    const bool wasPop = isPopulating;
    isPopulating = true;
    setSliderReal("pcHueShift", pt.hueShift);
    setSliderReal("pcSatShift", pt.satShift);
    setSliderReal("pcLumShift", pt.lumShift);
    setSliderReal("pcVariance", pt.variance);
    setSliderReal("pcRange",    pt.range);
    setSliderReal("pcHueRange", pt.hueRange);
    setSliderReal("pcSatRange", pt.satRange);
    setSliderReal("pcLumRange", pt.lumRange);
    setCheckboxValue("pcVisualize", pointColorVisualize);
    isPopulating = wasPop;
    static const char *keys[] = {"pcHueShift", "pcSatShift", "pcLumShift", "pcVariance",
                                 "pcRange", "pcHueRange", "pcSatRange", "pcLumRange",
                                 "pcVisualize"};
    for (const char *k : keys) {
        const QModelIndex vIdx = sourceIdx.value(k);
        if (vIdx.isValid() && vIdx.data(UR_Editor).value<void*>() != nullptr)
            setItemEnabled(k, have);
    }
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

void DevelopProperties::addDetail()
{
    if (G::isLogger) G::log("DevelopProperties::addDetail");
    addHeader("DetailHeader", "???", "Detail",
              "Sharpening and noise reduction. Judge both at 100% zoom.", PV_Detail);
    QModelIndex parIdx = capIdx;

    /* The 1:1 preview FIRST, as Lightroom has it: everything below is judged in it, and
       both halves of the panel (sharpening and noise reduction) need it. */
    addDetailPreviewRow(parIdx);
    /* A plain spacer (no rule) between the preview and the first slider: the graphic
       otherwise butts straight up against the Sharpening row. Same idiom as the spacer
       under every section header (addHeader). */
    addDivider(G::groupSeparatorHeight, 0, Qt::transparent, parIdx, "DetailHeader",
               "DetailPreviewSpacer");

    /* Sharpening: capture sharpening, an unsharp mask on luminance (see Develop::Sharpen
       and Develop/sharpen.h). Amount is the strength (0..150 -> 0..1.5); Radius, Detail
       and Masking are sub-controls (indented "   " captions, like the vignette feather)
       and are inert until Amount is non-zero.

       Radius is the ONE Develop control measured in absolute pixels -- every other
       spatial radius is a fraction of the image's long edge. It is therefore only true
       at 1:1, which is what the note row below the group says. Radius is a 2-decimal
       slider (50..300 -> 0.50..3.00 px); the other three are 0..100 mapped to 0..1.

       Distinct from Basic's Texture, which is a resolution-PROPORTIONAL mid-frequency
       band: Texture changes detail contrast, this changes edge acutance. */
    addSlider("sharpenAmount", "Sharpening:",
              "Sharpen fine detail. Judge it at 100% zoom.",
              parIdx, "DetailHeader", 0, 150, 0, G::darkgray, G::lightgray);
    addSlider("sharpenRadius", "   Radius",
              "Width of the sharpened edges. Around 1.0 suits most images;\n"
              "larger values can cause halos.",
              parIdx, "DetailHeader", 50, 300, 100, G::darkgray, G::lightgray, 1.0);
    addSlider("sharpenDetail", "   Detail", "How much fine detail is sharpened vs. "
              "suppressed as noise.",
              parIdx, "DetailHeader", 0, 100, 0, G::darkgray, G::lightgray, 25);
    addSlider("sharpenMasking", "   Masking",
              "Sharpen only edges, leaving smooth areas like sky and skin alone.\n"
              "Opt-drag to see which areas are sharpened (white).",
              parIdx, "DetailHeader", 0, 100, 0, G::darkgray, G::lightgray);

    addGroupDivider(parIdx, "DetailHeader", "DenoiseDivider");

    /* Local noise reduction: per-scope, maskable Develop ops on the decoded image
       (localDenoiseLuma / localDenoiseChroma). Lives here rather than under Effects
       because NR and sharpening are the two halves of one decision -- NR removes the
       detail sharpening amplifies. Distinct from the Global scope's "Denoise raw"
       (decode-time global raw NR, denoiseLuma/denoiseChroma) -- different function,
       different keys. Two 0..100 strength sliders (mapped to 0..1): Denoise = luminance
       NR (ratio-preserving), Denoise Color = chroma NR (opponent-chroma blur, luma kept
       exact) -- see Develop::Denoise. */
    addSlider("localDenoise", "Denoise:", "Reduce grainy noise.",
              parIdx, "DetailHeader", 0, 100, 0, G::darkgray, G::lightgray);
    addSlider("localDenoiseChroma", "   Color", "Local colour (chroma) noise reduction.",
              parIdx, "DetailHeader", 0, 100, 0, G::darkgray, G::lightgray);

    addGroupDivider(parIdx, "DetailHeader", "DetailEndDivider");
}


void DevelopProperties::addEffects()
{
    if (G::isLogger) G::log("DevelopProperties::addEffects");
    addHeader("EffectsHeader", "???", "Effects",
              "Vignette, grain and (Global scope) kaleidoscope.\n"
              "Choose an effect from the list.", PV_Effects);
    QModelIndex parIdx = capIdx;

    /* The effect picker: a spanned row with a plain combo, the same idiom as the Curves
       mode row. Only the chosen effect's rows show below it (applyEffectsChoice), so
       the section stays short as effects are added. Which effect you are LOOKING at is
       not part of the recipe, so it is UI state, not EditParams. */
    clearItemInfo(i);
    i.name = "effectsChoice";
    i.parIdx = parIdx;
    i.parentName = "EffectsHeader";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex choiceIdx = capIdx;
    model->setData(choiceIdx, 6, UR_ExtraRowHeight);
    setFirstColumnSpanned(choiceIdx.row(), parIdx, true);
    {
        QWidget *rw = new QWidget;
        rw->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(rw);
        /* No left inset: the spanned cell starts where the slider captions do, so the
           combo lines up with them (the Curves mode row's inset does not apply here). */
        hb->setContentsMargins(0, 0, 0, 0);
        hb->setSpacing(8);
        QComboBox *combo = new QComboBox(rw);
        const QVector<EffectDef> &defs = effectDefs();
        for (int e = 0; e < defs.size(); ++e)
            if (!defs.at(e).globalOnly || activeScopeIndex == 0)
                combo->addItem(defs.at(e).label, e);
        /* 10% taller than the style's natural combo, so the picker reads as the
           section's lead control; the row grows by the same amount so it still has its
           6px of breathing room. */
        const int natH = combo->sizeHint().height();
        const int tallH = qRound(natH * 1.1);
        combo->setFixedHeight(tallH);
        model->setData(choiceIdx, 6 + (tallH - natH), UR_ExtraRowHeight);
        /* Blocked, as the Curves mode combo: the rows it shows and hides are not built
           yet. The call at the end of addEffects applies the choice. */
        {
            QSignalBlocker b(combo);
            combo->setCurrentIndex(qMax(0, combo->findData(shownEffect())));
        }
        combo->setToolTip("The effect to adjust. An effect marked * has been changed.");
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int idx){
            const int id = combo->itemData(idx).toInt();
            if (idx < 0 || id == shownEffect()) return;
            effectsChoice = id;
            setting->setValue("Develop/effectsChoice", id);
            applyEffectsChoice();
        });
        effectsCombo = combo;
        hb->addWidget(combo);
        hb->addStretch(1);

        /* [eye] [reset] for the shown effect, laid out like a section header's trailing
           pair (BarBtnEditor: G::headerBtnGap between, G::headerBtnRightInset after), so
           they sit straight under the Effects header's [eye] [:]. The row is spanned,
           so its right edge is the header value cell's right edge. */
        QHBoxLayout *bb = new QHBoxLayout;
        bb->setContentsMargins(0, 0, G::headerBtnRightInset, 0);
        bb->setSpacing(G::headerBtnGap);
        BarBtn *eye = new BarBtn();
        connect(eye, &BarBtn::clicked, this, &DevelopProperties::toggleEffectShown);
        effectEyeBtn = eye;
        BarBtn *reset = new BarBtn();
        reset->setIcon(":/images/icon16/reset.png", G::iconOpacity);
        reset->setToolTip("Reset this effect");
        connect(reset, &BarBtn::clicked, this, &DevelopProperties::resetEffect);
        bb->addWidget(eye);
        bb->addWidget(reset);
        hb->addLayout(bb);
        setIndexWidget(choiceIdx, rw);
    }
    /* A group divider (rule) under the picker, separating it and its [eye] [reset] from
       the chosen effect's sliders. addGroupDivider leaves capIdx on the divider; parIdx
       is unchanged. */
    addGroupDivider(parIdx, "EffectsHeader", "EffectsChoiceDivider");

    /* Every effect's rows sit flush at the section's caption indent, sub-controls
       included: the dropdown above names the effect, so its first slider is a plain
       "Amount" and there is no "Effect:" head row to hang the others under. History
       names them in full (kEffectsHistory in itemChange), since "Amount" alone would
       not say which effect moved. */

    /* Vignette: a global radial exposure falloff about the image centre (see
       Develop::Vignette). Two sliders: Amount (a 2-decimal EV slider like Basic
       Exposure, the corner adjustment -- negative darkens, positive brightens) and
       Feather (0..100, how far it reaches inward). Off-centre / shaped vignettes use
       radial masks instead. */
    addSlider("vignetteExposure", "Amount", "Corner exposure (EV): - darkens, + brightens.",
              parIdx, "EffectsHeader", -500, 500, 100, G::darkgray, G::lightgray);
    addSlider("vignetteFeather", "Feather", "How far the vignette reaches inward.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);

    /* Grain: monochromatic film grain added to luminance (see Develop::Grain). Amount is
       the strength, size the particle size (scaled to the image so the proxy matches full
       res), roughness how patchy / irregular the grain is. Size and roughness are
       sub-controls of Amount and are inert until Amount is non-zero. All three 0..100
       sliders mapped to 0..1. No divider above it: the dropdown shows one effect at a
       time. */

    addSlider("grainAmount", "Amount", "Add monochromatic film grain to the image.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray);
    addSlider("grainSize", "Size", "Grain particle size (fine to coarse).",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 25);
    addSlider("grainRoughness", "Roughness", "How patchy / irregular the grain is.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);

    /* Kaleidoscope (Global only): mirror the cropped photo around a centre. It is
       GEOMETRY (Geometry::kaleido*, folded last in CropTransform::applyGeometry, after
       the crop), not an EditParams value, so its rows are handled in itemChange and
       seeded by syncKaleidoSliders; MW::setDevelopKaleido is the writer. While it shows,
       MW puts the centre / angle handles on the image (kaleidoToolWanted). Segments 0 =
       off, else even (an odd value rounds up); the angles are whole degrees here, while
       a handle drag may store a fraction (shown rounded). */
    if (activeScopeIndex == 0) {
        addSlider("kaleidoSegments", "Segments",
                  "Number of mirrored wedges around the centre (0 = off).\n"
                  "Even counts only, so every seam is a mirror; 2 is a plain mirror.\n"
                  "Drag the white centre or the yellow spoke on the image.",
                  parIdx, "EffectsHeader", 0, 32, 0, G::darkgray, G::lightgray, 0);
        addSlider("kaleidoAngle", "Angle",
                  "Turns the mirror lines (degrees). Or drag the yellow spoke.",
                  parIdx, "EffectsHeader", -180, 180, 0, G::darkgray, G::lightgray, 0);
        addSlider("kaleidoSource", "Source",
                  "Turns the slice of the photo that is mirrored,\n"
                  "without turning the pattern (degrees).",
                  parIdx, "EffectsHeader", -180, 180, 0, G::darkgray, G::lightgray, 0);
    }

    addGroupDivider(parIdx, "EffectsHeader", "EndDivider");

    applyEffectsChoice();
}

void DevelopProperties::applyEffectsChoice()
{
    /* Show the chosen effect's rows and hide the others'. Hiding keeps every slider in
       the model, so populateSlidersFromStack, presets and resets still reach the hidden
       ones. */
    const QVector<EffectDef> &defs = effectDefs();
    const int shown = shownEffect();
    for (int e = 0; e < defs.size(); ++e) {
        for (const QString &row : defs.at(e).rows) {
            const QModelIndex idx = findCaptionIndex(row);
            if (!idx.isValid()) continue;
            /* UR_isHidden as well as the view flag: buildTree ends with
               updateHiddenRows, which re-derives every row's visibility from the model
               and would otherwise show all the effects again. */
            model->setData(idx, e != shown, UR_isHidden);
            setRowHidden(idx.row(), idx.parent(), e != shown);
        }
    }
    scheduleContentFit();        // setRowHidden emits nothing: re-fit the block ourselves
    refreshPreviewButtons();     // the row's eye now speaks for the newly shown effect
    emit kaleidoToolChanged();   // Kaleidoscope in or out of view: the handles follow
}

bool *DevelopProperties::effectShowFlag(EditScope *l, int effect)
{
    if (!l) return nullptr;
    switch (effect) {
    case EffectVignette: return &l->showVignette;
    case EffectGrain:    return &l->showGrain;
    case EffectKaleido:  return &l->showKaleido;
    }
    return nullptr;
}

void DevelopProperties::toggleEffectShown()
{
/*
    The dropdown row's eye: hide / show the SHOWN effect only, non-destructively, as a
    section eye does for its section (the values are kept; the render folds them out --
    effectiveScopeParams for vignette and grain, buildStackJob for the kaleidoscope).
    Inside the Effects eye: an effect renders only while both are open.
*/
    if (G::isLogger) G::log("DevelopProperties::toggleEffectShown");
    if (currentImagePath.isEmpty()) return;
    const int e = shownEffect();
    bool *f = effectShowFlag(activeScope(), e);
    if (!f) return;
    *f = !*f;
    noteEdit(QString(*f ? "Show " : "Hide ") + effectDefs().at(e).label);
    refreshPreviewButtons();
    emit paramsChanged();
    if (e == EffectKaleido) emit kaleidoToolChanged();   // hidden: the handles go too
}

void DevelopProperties::resetEffect()
{
/*
    The dropdown row's reset: the SHOWN effect's values back to their defaults (its eye
    is left alone, as a section reset leaves the section eye). The kaleidoscope is
    Geometry, so its reset writes the stored geometry: off, defaults restored.
*/
    if (G::isLogger) G::log("DevelopProperties::resetEffect");
    if (currentImagePath.isEmpty()) return;
    const int e = shownEffect();
    const EditParams def;
    if (e == EffectKaleido) {
        if (activeScopeIndex != 0) return;
        stackCache[currentImagePath].geometry.copyKaleidoFrom(Geometry());
    }
    else {
        EditParams &p = activeParams();
        if (e == EffectVignette) {
            p.vignetteExposure = def.vignetteExposure;
            p.vignetteFeather  = def.vignetteFeather;
        }
        else {
            p.grainAmount    = def.grainAmount;
            p.grainSize      = def.grainSize;
            p.grainRoughness = def.grainRoughness;
        }
    }
    noteEdit(QString("Reset ") + effectDefs().at(e).label);
    populateSlidersFromStack();      // the sliders, the " *" and the kaleidoscope handles
    emit paramsChanged();
}

int DevelopProperties::shownEffect() const
{
    /* Kaleidoscope is listed on Global only: a mask scope shows Vignette instead, and
       the stored choice is kept for the next visit to Global. */
    if (effectsChoice < 0 || effectsChoice >= effectDefs().size()) return EffectVignette;
    if (effectDefs().at(effectsChoice).globalOnly && activeScopeIndex != 0)
        return EffectVignette;
    return effectsChoice;
}

bool DevelopProperties::kaleidoToolWanted() const
{
    if (currentImagePath.isEmpty() || activeScopeIndex != 0) return false;
    if (shownEffect() != EffectKaleido || !isVisible()) return false;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.scopes.isEmpty() &&
        (!s.scopes.at(0).showEffects || !s.scopes.at(0).showKaleido)) return false;   // eye shut
    /* The row is really on screen: every section above it expanded, none hidden. */
    const QModelIndex idx = sourceIdx.value("kaleidoSegments");   // its value cell
    if (!idx.isValid() || isRowHidden(idx.row(), idx.parent())) return false;
    for (QModelIndex p = idx.parent(); p.isValid(); p = p.parent())
        if (!isExpanded(p) || isRowHidden(p.row(), p.parent())) return false;
    return true;
}

void DevelopProperties::syncKaleidoSliders()
{
    const Geometry g = currentGeometry();
    const bool wasPop = isPopulating;
    isPopulating = true;
    setSliderReal("kaleidoSegments", g.kaleidoSegments);
    setSliderReal("kaleidoAngle", g.kaleidoAngle);
    setSliderReal("kaleidoSource", g.kaleidoSource);
    isPopulating = wasPop;
    updateSectionHeaderCaptions();   // the Effects " *" and the list's Kaleidoscope " *"
}

void DevelopProperties::addLens()
{
/*
    The Lens section, Global scope only: every row here is applied to the BASE image
    before any scope develops (MW's lensCorrectedSource), so every mask, guide and scope
    sees the corrected frame -- which is also why a mask scope cannot carry them and does
    not get the section.

    Lens profile (Develop/lensdb.h, Develop/lensprofile.h): lensfun's calibration for the
    lens in the EXIF -- distortion, transverse chromatic aberration and vignetting --
    with the lens overridable and the distortion / vignetting amounts 0..200%. Then the
    image-measured corrections that were in Effects before this section existed: Remove
    CA (whose estimate a profile's own TCA replaces) and the two fringe sliders.
*/
    if (G::isLogger) G::log("DevelopProperties::addLens");
    if (activeScopeIndex != 0) return;
    addHeader("LensHeader", "???", "Lens",
              "Correct lens distortion, vignetting and colour fringes.\n"
              "Global scope only: it applies to the whole image.", PV_Lens);
    QModelIndex parIdx = capIdx;

    addCheckbox("lensProfile", "Lens profile",
                "Correct distortion and vignetting using a profile for this lens.\n"
                "Usually not needed for JPEGs: the camera already corrected them.",
                parIdx, "LensHeader");
    addLensProfileRow(parIdx);
    addSlider("lensDistortion", "   Distortion",
              "Strength of the profile's distortion fix (100 = normal).",
              parIdx, "LensHeader", 0, 200, 0, G::darkgray, G::lightgray, 100);
    addSlider("lensVignetting", "   Vignetting",
              "Strength of the profile's vignetting fix (100 = normal).\n"
              "Needs the photo's aperture.",
              parIdx, "LensHeader", 0, 200, 0, G::darkgray, G::lightgray, 100);

    /* Manual (Lightroom's Manual tab): corrections that need NO profile, so a lens
       lensfun never calibrated -- or a JPEG with the profile off -- can still be fixed
       by eye. Applied on top of the profile when it is on, in the same resample
       (LensProfile::Amounts::manual*). A caption row heads them so they do not read as
       a second copy of the profile's two amounts above. */
    addGroupDivider(parIdx, "LensHeader", "LensManualDivider");
    clearItemInfo(i);
    i.name = "lensManualCaption";
    i.parIdx = parIdx;
    i.parentName = "LensHeader";
    i.captionText = "Manual";
    i.tooltip = "Lens corrections by eye, with or without a profile. Applied on top of\n"
                "the profile when it is on.";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    addSlider("lensManualDistortion", "   Distortion",
              "Straighten bulging (+) or pinched (-) lines by eye.",
              parIdx, "LensHeader", -100, 100, 0, G::darkgray, G::lightgray, 0);
    addSlider("lensManualVignetting", "   Vignetting",
              "Brighten (+) or darken (-) the corners to fix lens shading.\n"
              "Unlike Effects > Vignette, it ignores the crop.",
              parIdx, "LensHeader", -100, 100, 0, G::darkgray, G::lightgray, 0);
    addSlider("lensManualMidpoint", "   Midpoint",
              "How far in from the corners the vignetting fix reaches.",
              parIdx, "LensHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);

    addGroupDivider(parIdx, "LensHeader", "LensCADivider");
    addCheckbox("removeCA", "Remove CA",
                "Remove the colour fringes that appear toward the corners of the\n"
                "frame.",
                parIdx, "LensHeader");
    addSlider("defringePurple", "Purple fringe:",
              "Defringe the cyan, blue, purple and magenta rim along\n"
              "high-contrast edges.",
              parIdx, "LensHeader", 0, 100, 0, G::darkgray, G::lightgray);
    addSlider("defringeGreen", "Green fringe:",
              "Defringe the green rim along high-contrast edges.",
              parIdx, "LensHeader", 0, 100, 0, G::darkgray, G::lightgray);

    addGroupDivider(parIdx, "LensHeader", "LensEndDivider");
}

/* lensfun's database is CC BY-SA: the attribution rides on the row that uses it. */
static const char *kLensfunCredit =
    "Lens profiles from the lensfun project (lensfun.github.io), CC BY-SA 3.0.";

void DevelopProperties::addLensProfileRow(const QModelIndex &parIdx)
{
    clearItemInfo(i);
    i.name = "lensProfileId";
    i.parIdx = parIdx;
    i.parentName = "LensHeader";
    i.captionText = "   Lens";
    i.tooltip = QString("The lens to correct for. Auto finds it from the photo; choose "
                        "one if it is wrong or missing (e.g. a manual lens).\n\n")
                + kLensfunCredit;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "lensProfileId";
    i.delegateType = DT_None;       // we own the value cell
    addItem(i);

    const QModelIndex valIdx = findValueIndex("lensProfileId");
    if (!valIdx.isValid()) return;

    QWidget *cell = new QWidget;
    cell->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *vhb = new QHBoxLayout(cell);
    vhb->setContentsMargins(0, 0, 10, 0);
    vhb->setSpacing(0);

    QComboBox *combo = new QComboBox;
    /* Lens names run long ("Nikon AF-S Zoom-Nikkor 24-70mm f/2.8G ED"): the combo takes
       the cell's width and elides; only its popup is allowed to be wide. Letting the
       combo size to its contents would widen the whole dock. */
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(8);
    lensProfileCombo = combo;
    connect(combo, QOverload<int>::of(&QComboBox::activated), this,
            [this, combo](int ix){ setGlobalLensProfileId(combo->itemData(ix).toString()); });
    vhb->addWidget(combo);
    setIndexWidget(valIdx, cell);
    repopulateLensCombo();
}

void DevelopProperties::repopulateLensCombo()
{
    if (!lensProfileCombo) return;

    EditParams g;
    if (!currentImagePath.isEmpty()) {
        const EditStack &s = stackCache[currentImagePath];
        if (!s.scopes.isEmpty()) g = s.scopes[0].params;
    }
    const LensDb::Shot shot = mw ? mw->lensShotFor(currentImagePath) : LensDb::Shot();
    const LensDb::Database &db = LensDb::Database::instance();
    const LensDb::Resolved autoMatch = LensDb::resolve(shot);
    const LensDb::Resolved used = g.lensProfileId.isEmpty()
                                      ? autoMatch : LensDb::resolve(shot, g.lensProfileId);

    QSignalBlocker block(lensProfileCombo);
    lensProfileCombo->clear();
    /* The automatic entry says what it found -- or why it found nothing -- so the reason
       is where the user is already looking, not in a popup. A lens found but unusable
       for this shot (no calibration at this focal length, a smaller sensor) says so too:
       otherwise the amount rows below grey with a valid-looking lens named above them. */
    QString autoText = autoMatch.lens
        ? "Auto: " + autoMatch.lens->displayName()
        : "Auto (" + autoMatch.reason + ")";
    if (autoMatch.lens && !autoMatch.ok() && !autoMatch.reason.isEmpty())
        autoText += "  (" + autoMatch.reason + ")";
    lensProfileCombo->addItem(autoText, QString());

    const QVector<const LensDb::Lens *> fits = db.lensesFor(autoMatch.camera);
    if (!fits.isEmpty()) lensProfileCombo->insertSeparator(lensProfileCombo->count());
    /* A name the database uses twice (one lens, calibrated on two sensor sizes) gets its
       crop factor, or the two rows would be indistinguishable. */
    QHash<QString, int> nameCount;
    for (const LensDb::Lens *L : fits) ++nameCount[L->displayName()];
    for (const LensDb::Lens *L : fits) {
        QString name = L->displayName();
        if (nameCount.value(name) > 1)
            name += QString("  (crop %1)").arg(double(L->cropFactor));
        lensProfileCombo->addItem(name, L->id());
    }

    int ix = 0;
    if (!g.lensProfileId.isEmpty()) {
        ix = lensProfileCombo->findData(g.lensProfileId);
        if (ix < 0) {
            /* A picked lens this camera cannot take (or one gone from the database):
               shown, marked and selected, never silently swapped for Auto. */
            const LensDb::Lens *L = db.lensById(g.lensProfileId);
            const QString name = L ? L->displayName()
                                   : g.lensProfileId.section(QLatin1Char('|'), 1, 1);
            lensProfileCombo->addItem(name + "  (" + (used.reason.isEmpty()
                                          ? QStringLiteral("not for this camera")
                                          : used.reason) + ")",
                                      g.lensProfileId);
            ix = lensProfileCombo->count() - 1;
        }
    }
    lensProfileCombo->setCurrentIndex(ix);

    /* What the selected profile corrects, for the tooltip -- and the two amount rows are
       live only when the profile is on AND has that correction for this shot. */
    QStringList corrects;
    if (used.ok()) {
        if (used.coeffs.hasDistortion()) corrects << "distortion";
        if (used.coeffs.hasTca())        corrects << "chromatic aberration";
        if (used.coeffs.vig)             corrects << "vignetting";
    }
    QString tip;
    if (used.lens) tip = used.lens->displayName() + "\n";
    tip += used.ok() ? "Corrects: " + corrects.join(", ")
                     : "No correction: " + (used.reason.isEmpty() ? QString("no profile")
                                                                  : used.reason);
    if (!shot.lens.isEmpty()) tip += "\nEXIF lens: " + shot.lens;
    tip += QString("\n\n") + kLensfunCredit;
    lensProfileCombo->setToolTip(tip);

    const bool on = g.wantsLensProfile(G::autoLensProfile, currentIsRaw());
    setItemEnabled("lensDistortion", on && used.ok() && used.coeffs.hasDistortion());
    setItemEnabled("lensVignetting", on && used.ok() && used.coeffs.vig);
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
        {"CurvesHeader",     "Curves",      PV_Curves},
        {"ColorHeader",      "Color",       PV_Color},
        {"PointColorHeader", "Point Color", PV_PointColor},
        {"ColorPanelHeader", "Color",       PV_ColorPanel},
        {"CalibrateHeader",  "Calibrate",   PV_Calibrate},
        {"ColorGradeHeader", "Color Grade", PV_ColorGrade},
        {"DetailHeader",     "Detail",      PV_Detail},
        {"EffectsHeader",    "Effects",     PV_Effects},
        {"LensHeader",       "Lens",        PV_Lens},
    };
    const EditStack s = stackCache.value(currentImagePath);
    const bool haveParams = !currentImagePath.isEmpty() && !s.scopes.isEmpty();
    const int scopeIdx = (activeScopeIndex >= 0 && activeScopeIndex < s.scopes.size())
                             ? activeScopeIndex : 0;

    for (int i = 0; i < int(sizeof(hdrs) / sizeof(HdrDef)); ++i) {
        const HdrDef &h = hdrs[i];
        bool edited = false;
        if (haveParams) {
            const EditParams &sp = s.scopes.at(scopeIdx).params;
            if (h.group == PV_ColorPanel)      // the combined panel: any of its three
                edited = !EditParams::groupIsDefault(sp, EditParams::Group::ColorGrade) ||
                         !EditParams::groupIsDefault(sp, EditParams::Group::PointColor) ||
                         !EditParams::groupIsDefault(sp, EditParams::Group::Calibrate);
            else
                edited = !EditParams::groupIsDefault(sp, paramsGroup(h.group));
            /* The kaleidoscope is an Effect that lives in Geometry (Global only). */
            if (h.group == PV_Effects && scopeIdx == 0 && s.geometry.hasKaleido())
                edited = true;
        }
        if (hdrStar[i] == int(edited)) continue;     // caption already correct
        const QModelIndex idx = findCaptionIndex(h.name);
        if (!idx.isValid()) continue;
        model->setData(idx, edited ? QString(QString(h.label) + " *") : QString(h.label));
        hdrStar[i] = int(edited);
    }

    /* The combined Color panel's mode radios get the same " *" per group (Tone =
       ColorGrade, Hue = PointColor, RGB = Calibrate), so an edit in a mode that isn't
       showing is still visible. setText only on a flip: the row re-centres on resize. */
    static const char *kModeLabel[] = {"Tone", "Hue", "RGB"};
    static const EditParams::Group kModeGroup[] = {EditParams::Group::ColorGrade,
                                                   EditParams::Group::PointColor,
                                                   EditParams::Group::Calibrate};
    for (int m = 0; m < 3; ++m) {
        QRadioButton *r = colorPanelRadios[m];
        if (!r) continue;
        const bool edited = haveParams &&
            !EditParams::groupIsDefault(s.scopes.at(scopeIdx).params, kModeGroup[m]);
        const QString text = edited ? QString(kModeLabel[m]) + " *" : QString(kModeLabel[m]);
        if (r->text() != text) r->setText(text);
    }

    /* The Effects dropdown's items get the same " *", so an edited effect that is not
       the one showing is still visible from the list. */
    if (effectsCombo) {
        const QVector<EffectDef> &defs = effectDefs();
        for (int ix = 0; ix < effectsCombo->count(); ++ix) {
            const int e = effectsCombo->itemData(ix).toInt();
            if (e < 0 || e >= defs.size()) continue;
            const bool edited = haveParams && effectEdited(s, scopeIdx, e);
            const QString text = edited ? QString(defs.at(e).label) + " *"
                                        : QString(defs.at(e).label);
            if (effectsCombo->itemText(ix) != text) effectsCombo->setItemText(ix, text);
        }
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
        (source == "maskFeather" && isBrushCanvasTool(activeMaskTool()))) {
        EditScope *l = activeScope();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->components.size()) {
            MaskComponent &mm = l->components[selectedMaskIndex];
            if      (source == "maskFeather")  mm.feather = v.toFloat();
            else if (source == "maskSize")     mm.paramsJson = brushWith(mm.paramsJson, "size", v.toDouble());
            else if (source == "maskFlow")     mm.paramsJson = brushWith(mm.paramsJson, "flow", v.toInt());
            else if (source == "maskAutoMask") mm.paramsJson = brushWith(mm.paramsJson, "autoMask", v.toBool());
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
    if (source == "maskFeather" || source == "maskEdge" || source == "maskInvert") {
        EditScope *l = activeScope();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->components.size()) {
            if (source == "maskFeather") {
                l->components[selectedMaskIndex].feather = v.toFloat();
                emit maskFeatherChanged(v.toDouble());      // live-update the overlay ramp
            }
            else if (source == "maskEdge") {
                l->components[selectedMaskIndex].edge = v.toFloat();
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
        RawRender::Choice c;
        c.apple = v.toString() == "Apple";
        setRawRenderer(c);          // per image; rebuilds the tree deferred
        return;
    }

    /* Temp / Tint are an ABSOLUTE PAIR and are always written together: the sliders show
       resolved values, so moving Tint alone must still commit the resolved Kelvin --
       otherwise temp would stay at its "as shot" sentinel 0 and the two would disagree
       about what they are relative to. A manual move also demotes the preset to
       Custom. */
    if (source == "temp" || source == "tint") {
        if (currentImagePath.isEmpty()) return;
        noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way
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

    /* Color Grade Luminance slider: writes the range the Shadows/Midtones/Highlights/
       Global radio buttons select (not a plain applyKeyToParams key -- it needs
       gradeActiveMask context). */
    if (source == "gradeLum") {
        if (currentImagePath.isEmpty()) return;
        noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way
        setGradeLum(v.toFloat());
        noteEdit("Color grade luminance", historyValueText(idx, v), "grade/lum");
        emit paramsChanged();
        return;
    }

    /* Color Grade Hue / Saturation sliders: the wheel's axes for the selected range.
       The wheel dot follows (refreshColorGradeRow). */
    if (source == "gradeHue" || source == "gradeSat") {
        if (currentImagePath.isEmpty()) return;
        noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way
        setGradeHueSat(source == "gradeHue", v.toFloat());
        noteEdit(source == "gradeHue" ? "Color grade hue" : "Color grade saturation",
                 historyValueText(idx, v),
                 QString("grade/%1/%2").arg(source).arg(gradeActiveMask));
        refreshColorGradeRow();         // move the wheel dot with the slider
        emit paramsChanged();
        return;
    }


    /* Point Color sliders: write the point the swatches select (setPointColorSlider).
       The mergeKey carries the point, so nudging two points reads as two steps.
       Visualize Range is VIEW state, not an edit -- no history, no sidecar; it only
       changes what stackJob() asks the render for. */
    if (source == "pcVisualize") {
        pointColorVisualize = v.toBool();
        emit paramsChanged();
        return;
    }
    if (source.startsWith("pc") && source.size() > 2 && source.at(2).isUpper()) {
        if (currentImagePath.isEmpty()) return;
        noteNonMaskInteraction();
        setPointColorSlider(source, v.toFloat());
        noteEdit(QString("Point color ") + idx.siblingAtColumn(0).data().toString()
                     .trimmed().toLower(),
                 historyValueText(idx, v),
                 QString("pointColor/%1/%2").arg(source).arg(pointColorSel));
        if (pointColorWidget) {
            const EditParams &p = activeParams();
            pointColorWidget->setPoints(p.pointColorN, p.pointColor, pointColorSel);
        }
        emit paramsChanged();
        return;
    }

    /* Effects > Kaleidoscope (Global only): geometry, not EditParams -- MW writes it
       (setDevelopKaleido), which re-seeds these sliders. Odd segment counts round up
       to even, so moving off 0 turns it on at 2. */
    if (source == "kaleidoSegments" || source == "kaleidoAngle" ||
        source == "kaleidoSource") {
        if (currentImagePath.isEmpty()) return;
        noteNonMaskInteraction();
        const Geometry g = currentGeometry();
        int n = g.kaleidoSegments;
        double angle = g.kaleidoAngle, src = g.kaleidoSource;
        if (source == "kaleidoSegments") {
            n = qRound(v.toDouble());
            if (n & 1) n += 1;
            if (n != qRound(v.toDouble())) {
                isPopulating = true;
                setSliderReal("kaleidoSegments", n);
                isPopulating = false;
            }
        }
        else if (source == "kaleidoAngle") angle = v.toDouble();
        else                               src = v.toDouble();
        emit kaleidoEdited(n, angle, src);
        return;
    }

    /* Lens corrections: Global-only recipe values with their own setters (removeCA is
       tri-state, and both always write scope 0). The rows exist only on Global. */
    if (source == "removeCA") {
        noteNonMaskInteraction();
        setGlobalRemoveCA(v.toBool());
        return;
    }
    if (source == "defringePurple" || source == "defringeGreen") {
        noteNonMaskInteraction();
        setGlobalDefringe(source == "defringePurple", qRound(v.toDouble()));
        return;
    }
    if (source == "lensProfile") {
        noteNonMaskInteraction();
        setGlobalLensProfile(v.toBool());
        return;
    }
    if (source == "lensDistortion" || source == "lensVignetting") {
        noteNonMaskInteraction();
        setGlobalLensAmount(source == "lensDistortion", qRound(v.toDouble()));
        return;
    }
    if (source == "lensManualDistortion" || source == "lensManualVignetting" ||
        source == "lensManualMidpoint") {
        noteNonMaskInteraction();
        setGlobalLensManual(source, qRound(v.toDouble()));
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
        noteNonMaskInteraction();   // not a mask edit: the veil gets out of the way
        applyKeyToParams(source, v, activeParams());
        /* The Curves panel's Parametric view DRAWS Basic's tone sliders, so any of them
           moving has to redraw the plot. Cheap (a copy + an update), and it keeps the two
           views of the same params from disagreeing. */
        if (curveEditor) curveEditor->setParams(activeParams());
        /* History caption = the row's own caption ("Exposure"); the whole drag folds
           into one entry (mergeKey), so the list reads one step per slider. */
        /* The Calibrate captions are a bare "Hue" / "Saturation" (the groove colour
           names the primary), which would read ambiguously in History. */
        static const QHash<QString, QString> kCalHistory = {
            {"calShadowTint", "Shadows tint"},
            {"calRedHue", "Red hue"},     {"calRedSat", "Red saturation"},
            {"calGreenHue", "Green hue"}, {"calGreenSat", "Green saturation"},
            {"calBlueHue", "Blue hue"},   {"calBlueSat", "Blue saturation"}};
        /* The Effects rows are captioned "Amount" / "Feather" / "Size" under the effect
           dropdown, which History would read without the effect's name. */
        static const QHash<QString, QString> kEffectsHistory = {
            {"vignetteExposure", "Vignette"}, {"vignetteFeather", "Vignette feather"},
            {"grainAmount", "Grain"},         {"grainSize", "Grain size"},
            {"grainRoughness", "Grain roughness"}};
        const QString action = kCalHistory.value(source, kEffectsHistory.value(source,
                                                 idx.siblingAtColumn(0).data().toString()));
        noteEdit(action, historyValueText(idx, v), historyScopeLabel() + "/" + source);
        emit paramsChanged();
    }
}

void DevelopProperties::applyKeyToParams(const QString &key, const QVariant &v, EditParams &p)
{
    /* Slider EditRole carries the real (div-scaled) value, e.g. exposure in EV, or 0..100 for the
       denoise sliders. Map the dock key onto the matching EditParams field. */
    const float f = v.toFloat();
    if      (key == "temp")       p.temp       = f;
    /* Calibrate (the Color panel's RGB mode): keys ARE the field names, stored raw. */
    else if (key == "calShadowTint") p.calShadowTint = f;
    else if (key == "calRedHue")     p.calRedHue     = f;
    else if (key == "calRedSat")     p.calRedSat     = f;
    else if (key == "calGreenHue")   p.calGreenHue   = f;
    else if (key == "calGreenSat")   p.calGreenSat   = f;
    else if (key == "calBlueHue")    p.calBlueHue    = f;
    else if (key == "calBlueSat")    p.calBlueSat    = f;
    else if (key == "tint")       p.tint       = f;
    else if (key == "exposure")   p.exposure   = f;
    else if (key == "contrast")   p.contrast   = f;
    else if (key == "highlights") p.highlights = f;
    else if (key == "shadows")    p.shadows    = f;
    else if (key == "whites")     p.whites     = f;
    else if (key == "blacks")     p.blacks     = f;
    else if (key == "texture")    p.texture    = f;
    else if (key == "clarity")    p.clarity    = f;
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
    else if (key == "sharpenAmount")      p.sharpenAmount      = f / 100.0f;  // 0..1.5
    else if (key == "sharpenRadius")      p.sharpenRadius      = f;           // px, div 100
    else if (key == "sharpenDetail")      p.sharpenDetail      = f / 100.0f;
    else if (key == "sharpenMasking")     p.sharpenMasking     = f / 100.0f;
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
       until it settles (or Return lands it) -- the real op is MOMENTARY, held in
       pendingOp by the modifier the user is pressing. The veil already composites with it
       (MW::updateMaskOverlayTint); the RENDER has to as well, or an Opt (Subtract) or
       Shift+Opt (Intersect) submask leaves the image showing that submask being ADDED --
       which over an already-masked area is no visible change at all. Substituted into a
       local copy, so nothing is written into the stored stack before the commit. */
    if (!previewActive && pendingIdx >= 0 && activeScopeIndex > 0 &&
        activeScopeIndex < s.scopes.size()) {
        EditScope &sc = s.scopes[activeScopeIndex];
        if (pendingIdx < sc.components.size()) sc.components[pendingIdx].op = pendingOp;
    }
    /* Point Color's Visualize Range: a VIEW of the active scope's selected point,
       written into this local copy only -- never into the stored stack, so the sidecar,
       an export and the devPreview builder (all stackJobFor) never see it. */
    if (!previewActive && pointColorVisualize && activeScopeIndex >= 0 &&
        activeScopeIndex < s.scopes.size()) {
        EditParams &vp = s.scopes[activeScopeIndex].params;
        if (pointColorSel >= 0 && pointColorSel < vp.pointColorN)
            vp.pointColorVisualize = pointColorSel;
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
    /* The kaleidoscope is an Effect (Effects > Kaleidoscope), so the Transform eye does
       not hide it -- Global's Effects eye does, here and in every export (stackJobFor). */
    if (!useGeometry) job.geometry.copyKaleidoFrom(s.geometry);
    if (!s.scopes.isEmpty() &&
        (!s.scopes.at(0).showEffects || !s.scopes.at(0).showKaleido))
        job.geometry.suppressKaleido();
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
        lj.maskEdge = l.maskEdge;
        lj.maskHalo = l.maskHalo;
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
    /* Before the crop test, which matches whenever ANY crop exists, changed or not. */
    else if (!g.kaleidoEquals(was)) {
        if (!g.hasKaleido()) action = "Kaleidoscope off";
        else {
            action = "Kaleidoscope";
            value = QString::number(g.kaleidoSegments);
        }
        mergeKey = "geom/kaleido";
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
    /* Push names + per-scope enabled + selection into the header (no signal): the bar's
       eye reads the ACTIVE scope's enabled flag from here. */
    if (scopeHeader) {
        QVector<ScopeHeader::ScopeRowInfo> rows;
        rows.reserve(scopeList.size());
        const EditStack s = stackCache.value(currentImagePath);
        for (int i = 0; i < scopeList.size(); ++i) {
            ScopeHeader::ScopeRowInfo r;
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

void DevelopProperties::forgetImage(const QString &key)
{
    if (G::isLogger) G::log("DevelopProperties::forgetImage", key);
    if (key.isEmpty()) return;
    dirty.remove(key);
    stackCache.remove(key);
    if (history) history->forget(key);
    if (key == toppedUpPath) toppedUpPath.clear();
    if (key != currentImagePath) return;
    /* Reload in place: clear the current path so setCurrentImage does not return early
       (and has nothing of the old recipe to flush), then load the new one. */
    currentImagePath.clear();
    setCurrentImage(key);
    emit paramsChanged();
}

void DevelopProperties::setCurrentImage(const QString &fPath)
{
    if (G::isLogger) G::log("DevelopProperties::setCurrentImage", fPath);
    if (fPath == currentImagePath) return;

    /* Leaving the image: land a submask still inside its settle window rather than drop
       it. This used to discard it silently -- defensible when committing was an explicit
       act the user had visibly not performed, but not now that committing is what
       happens on its own two seconds later: navigating away a moment early is not a
       decision to throw the work out. An untouched tool is still dropped (that branch is
       inside flushMaskAutoCommit). Must run BEFORE flushImage so the sidecar gets it. */
    flushMaskAutoCommit();
    /* Nothing is open on the new image, and the panel belongs to whatever scope turns out
       to be active there (buildTree -> syncMaskPanel decides). */
    selectedMaskIndex = -1;
    /* A fresh image is not a mask edit in progress: engagement starts over, or the veil
       would come up on an image the user only navigated to. */
    lastInteractionWasMask = false;
    maskVeilEngagedNow = false;
    if (maskPanel) maskPanel->hide();
    maskLatched = false;         // a fresh image starts with no latched mask overlay
    previewActive = false;       // a History hover does not follow the image
    previewStack = EditStack();
    /* ...and neither does a latched Before: the new image has its own baseline, and the
       key must start from After there. */
    clearBeforeAfterLatch();
    /* ...and neither does a Profile / Tone mapping dropdown hover. Cleared alongside, so the flag cannot
       outlive the override it describes and leave endComboHoverPreview() firing a
       render for a preview that is already gone. */
    comboHoverActive = false;
    if (comboHoverTimer) comboHoverTimer->stop();
    /* A queued multi-image batch belongs to the image being left; land it before the
       diff base moves to the new image. */
    flushPropagation();
    flushImage(currentImagePath);              // persist edits to the image being left
    /* The image being left may have been rendered without being edited (its recipe came
       from an earlier session), in which case flushImage did nothing. Capture its
       previews now, while the frame still exists. */
    topUpDevPreviews(currentImagePath);
    cancelWbDropper();                  // an armed dropper does not follow the image
    cancelPointColorPick();             // nor does the Point Color eyedropper
    pointColorSel = -1;                 // the new image's first point (if any)
    /* The pointer marker DOES follow the image -- the toggle is a view preference, like
       the curve mode -- but the pixel it is marking does not: drop the stale sample and
       wait for the next hover. */
    clearCurveSample();
    /* The new image's characterisation has not arrived yet (the raw decode is async and
       starts after this), so the WB row is about to resolve against an invalid cam.
       onWorkingImageReady re-resolves it when the decode lands. */
    wbRowAsShotK = -1.0f;
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

    /* History's "Original" entry for this image, plus "Saved settings" when the sidecar
       already carried edits. Idempotent, so returning to an image keeps the
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
    /* Editing while Before is latched is the user saying they are done comparing: take
       the override down so the edit they just made is the thing on screen. Guarded on the
       latch so a History HOVER (which also runs through no edit path) is untouched. */
    if (beforeAfterActive) endHistoryPreview();
    dirty.insert(currentImagePath);
    /* The AI mask scopes this commit changed -- BEFORE the step is recorded, because
       this is where an AI scope first gets its link, and the step must hold it: a
       revert to a step without it would make the next edit mint a second link and a
       second copy of the scope on every other image. Our own writes are not edits. */
    QHash<QString, ScopeSync> scopeChanges;
    if (!propagateSuspended && !isPopulating && !isRestoringHistory)
        scopeChanges = diffLinkedScopes(stackCache[currentImagePath]);
    /* Do not record our own writes: isPopulating is pushing saved values into the
       editors, isRestoringHistory is a revert repopulating the panel. */
    if (history && !isPopulating && !isRestoringHistory && !action.isEmpty()) {
        history->record(currentImagePath, scope, action, value, mergeKey,
                        stackCache.value(currentImagePath));
    }
    /* Multi-image editing: whatever this commit changed in the GLOBAL scope is queued
       for the other selected images. Hooked here rather than at each edit site so it
       covers every develop action by construction -- present and future. */
    queuePropagation(action, value, mergeKey, scopeChanges);
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
    /* A hover is the more recent statement of intent, so it supersedes a latched Before
       rather than the two fighting over previewStack. */
    clearBeforeAfterLatch();
    previewStack = e->stack;
    previewActive = true;
    emit historyPreviewChanged();
}

void DevelopProperties::endHistoryPreview()
{
    clearBeforeAfterLatch();
    if (!previewActive) return;
    previewActive = false;
    previewStack = EditStack();
    emit historyPreviewChanged();
}

void DevelopProperties::clearBeforeAfterLatch()
{
    if (!beforeAfterActive) return;
    beforeAfterActive = false;
    emit beforeAfterChanged(false);
}

EditStack DevelopProperties::originalStack() const
{
/*
    The Before image: this image with NO develop edits. A default EditStack with one
    Global scope renders exactly as a never-edited image does -- default params, empty
    (= default) camera profile, identity geometry, no spots -- so the crop and the spot
    heals are dropped along with the sliders. The same stack as History's "Original"
    (entry 0), from the one definition in DevelopHistory.
*/
    return DevelopHistory::originalStack();
}

void DevelopProperties::toggleBeforeAfter()
{
/*
    "\" -- flip the loupe between Before (the image with no develop edits at all) and
    After (the state the image is actually in).  See the header for why this latches and
    renders the full path where a History hover does neither.
*/
    if (G::isLogger) G::log("DevelopProperties::toggleBeforeAfter");

    int delay = 500;

    if (beforeAfterActive) {            // back to After
        previewActive = false;
        previewStack = EditStack();
        clearBeforeAfterLatch();
        /* NOT endHistoryPreview(): that renders the proxy only, and coming back to After
           has to end as crisp as the Before it replaces. */
        emit paramsChanged();
        /* Bottom right of the central widget, not the middle of it: this message names
           which of two pictures is up, and the picture is the thing being looked at. */
        if (G::popup)
            G::popup->showPopup("After", delay, true, 0.75, Qt::AlignHCenter,
                                Qt::AlignBottom | Qt::AlignRight);
        return;
    }

    if (currentImagePath.isEmpty()) return;
    /* The crop overlay and an open mask tool own the canvas; swapping the picture out
       from under them would fight for it (the same guard previewHistoryEntry uses). */
    if (maskPanelOpen || spotMode) {
        if (G::popup)
            G::popup->showPopup("Close the tool to compare Before and After.", 2500);
        return;
    }
    /* Nothing to compare only when the image itself is unedited: Before IS After, and a
       silent no-op would read as a dead key. NOT keyed on the history position -- an
       image opened in a later session carrying saved edits sits at position 0 and still
       has a Before worth seeing. */
    if (currentIsIdentity()) {
        if (G::popup)
            G::popup->showPopup("No develop edits yet -- Before and After are the same.",
                                2500);
        return;
    }

    /* Some other preview (a Presets, Profile or Tone mapping hover) may be up; this replaces it, and
       its own "end" is a no-op once previewActive goes down with the latch. */
    comboHoverActive = false;
    if (comboHoverTimer) comboHoverTimer->stop();

    previewStack = originalStack();
    previewActive = true;
    beforeAfterActive = true;
    emit beforeAfterChanged(true);
    emit paramsChanged();               // proxy now, crisp full-res on settle
    if (G::popup)
        G::popup->showPopup("Before  (Original)", delay, true, 0.75, Qt::AlignHCenter,
                            Qt::AlignBottom | Qt::AlignRight);
}

void DevelopProperties::applyHistoryEntry(int index)
{
    if (G::isLogger) G::log("DevelopProperties::applyHistoryEntry");
    if (!history || currentImagePath.isEmpty()) return;
    const HistoryEntry *e = history->at(currentImagePath, index);
    if (!e) return;
    /* A batch queued BEFORE the revert was a real edit and must land first -- both so it
       is not lost and so its linked steps exist for revertSelectionToMatch below. */
    flushPropagation();

    /* A half-built mask tool indexes into the scope we are about to replace, so retire it
       first (same teardown setCurrentImage does when leaving an image). */
    if (maskPanelOpen) {
        maskCommitTimer->stop();           // it must not land in a scope that is going
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
    clearBeforeAfterLatch();            // committing a state is not comparing two

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
    revertSelectionToMatch(index);      // linked multi-image steps revert with it

    emit historyApplied();              // re-seed the crop overlay before the render
    emit paramsChanged();               // full render + settled full-res
    if (G::isDevelopDebounceWrite && debounceWriteTimer)
        debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::revertSelectionToMatch(int index)
{
/*
    The multi-image half of a History revert. The current image has just moved to entry
    `index`; every OTHER selected image whose history shares linked steps with it
    (HistoryEntry::syncId -- one multi-image edit) moves to the matching point in its own
    history: before the first linked edit that is no longer in force, or to its newest
    step when all of them are (a redo). An image that shares no linked step is left
    alone, as is every image outside the selection -- a revert follows the same rule as
    an edit: it applies to what is selected.
*/
    if (!history) return;
    const QStringList others = otherSelectedPaths();
    if (others.isEmpty()) return;
    QSet<quint64> known, applied;
    history->syncIds(currentImagePath, index, known, applied);
    if (known.isEmpty()) return;

    int n = 0;
    for (const QString &p : others) {
        const int q = history->syncedPos(p, known, applied);
        if (q < 0 || q == history->pos(p)) continue;
        const HistoryEntry *e = history->at(p, q);
        if (!e) continue;
        EditStack s = e->stack;
        if (s.scopes.isEmpty()) s.scopes.append(EditScope());
        s.scopes[0].name = "Global";
        stackCache[p] = s;
        dirty.insert(p);                 // flushed (and its thumbnail rebuilt) as an edit
        history->setPos(p, q);
        ++n;
    }
    if (G::isLogger) G::log("DevelopProperties::revertSelectionToMatch",
                            QString::number(n) + " images");
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
    /* First, matching its position in the panel. A profile TRAVELS BY NAME, so a preset
       carrying one only reproduces the look on a machine where that profile is installed
       -- and on a DIFFERENT CAMERA it will not resolve at all. That is the honest
       behaviour for a per-sensor characterisation: the alternative, silently dropping it,
       would make the preset claim a colour rendering it did not deliver. */
    {"cameraProfile", "Camera profile"},
    /* Then Tone mapping: it re-scopes everything below it. */
    {"viewTransform", "Tone mapping"},
    {"whiteBalance", "White balance (Temp + Tint)"},
    {"exposure",     "Exposure"},
    {"contrast",     "Contrast"},
    {"highlights",   "Highlights"},
    {"shadows",      "Shadows"},
    {"whites",       "Whites"},
    {"blacks",       "Blacks"},
    {"texture",      "Texture"},
    {"clarity",      "Clarity"},
    {"dehaze",       "Dehaze"},
    {"vibrance",     "Vibrance"},
    {"saturation",   "Saturation"},
};
/* Two leaves, not one: a look built on the composite curve is often wanted WITHOUT the
   per-channel colour crossover that came with it. */
const PresetLeafDef kCurvesLeaves[] = {
    {"curveRgb",      "Tone curve (RGB)"},
    {"curveChannels", "Tone curve (Red / Green / Blue)"},
};
const PresetLeafDef kColorLeaves[] = {
    {"red",        "Red"},
    {"green",      "Green"},
    {"blue",       "Blue"},
    {"hue",        "Hue"},
    {"luminance",  "Luminance"},
};
/* ONE leaf: the samples are meaningless apart (a preset carrying them is a "move these
   colours" look, e.g. a skin-tone fix), and applying it REPLACES the target's points. */
const PresetLeafDef kPointColorLeaves[] = {
    {"pointColors", "Point colors"},
};
const PresetLeafDef kCalibrateLeaves[] = {
    {"calShadowTint", "Shadows tint"},
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
/* Sharpening's four params fold into ONE leaf, as Grain's three do: the panel treats
   them as a single control, so a preset ticks "Sharpening" and gets all four. */
const PresetLeafDef kDetailLeaves[] = {
    {"sharpen",  "Sharpening"},
    {"denoise",  "Denoise"},
};
const PresetLeafDef kEffectsLeaves[] = {
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
    {"Curves",    kCurvesLeaves,   int(sizeof(kCurvesLeaves)   / sizeof(PresetLeafDef))},
    {"Color",     kColorLeaves,    int(sizeof(kColorLeaves)    / sizeof(PresetLeafDef))},
    {"Point Color", kPointColorLeaves,
                  int(sizeof(kPointColorLeaves) / sizeof(PresetLeafDef))},
    {"Calibrate", kCalibrateLeaves, int(sizeof(kCalibrateLeaves) / sizeof(PresetLeafDef))},
    {"Color Grade", kColorGradeLeaves, int(sizeof(kColorGradeLeaves) / sizeof(PresetLeafDef))},
    {"Detail",    kDetailLeaves,   int(sizeof(kDetailLeaves)   / sizeof(PresetLeafDef))},
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
    if (key == "clarity")      return p.clarity    != def.clarity;
    if (key == "dehaze")       return p.dehaze     != def.dehaze;
    if (key == "curveRgb")
        return !ToneCurve::isIdentity(p.curveX[0], p.curveY[0], p.curveN[0]);
    if (key == "curveChannels") {
        for (int c = 1; c < ToneCurve::kChannels; ++c)
            if (!ToneCurve::isIdentity(p.curveX[c], p.curveY[c], p.curveN[c])) return true;
        return false;
    }
    if (key == "red")          return p.red        != def.red;
    if (key == "green")        return p.green      != def.green;
    if (key == "blue")         return p.blue       != def.blue;
    if (key == "hue")          return p.hue        != def.hue;
    if (key == "saturation")   return p.saturation != def.saturation;
    if (key == "luminance")    return p.luminance  != def.luminance;
    if (key == "vibrance")     return p.vibrance   != def.vibrance;
    if (key == "pointColors")  return p.pointColorN > 0;
    if (key == "calShadowTint") return p.calShadowTint != def.calShadowTint;
    if (key == "calRed")       return p.calRedHue   != def.calRedHue ||
                                      p.calRedSat   != def.calRedSat;
    if (key == "calGreen")     return p.calGreenHue != def.calGreenHue ||
                                      p.calGreenSat != def.calGreenSat;
    if (key == "calBlue")      return p.calBlueHue  != def.calBlueHue ||
                                      p.calBlueSat  != def.calBlueSat;
    if (key == "viewTransform") return p.viewTransform != def.viewTransform;
    if (key == "cameraProfile") return p.cameraProfile != def.cameraProfile;
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
    if (key == "sharpen")      return p.sharpenAmount != def.sharpenAmount;
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
        {"clarity",    "clarity",    p.clarity},
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
    /* Tone curve. Each leaf is encoded over a COPY with the channels it does not own
       reset to the diagonal, and DevelopPresets::assignParam merges each key back into
       only its own channels -- so ticking one leaf never clears the other. */
    if (lk.contains("curveRgb")) {
        EditParams t = p;
        for (int c = 1; c < ToneCurve::kChannels; ++c)
            ToneCurve::setIdentity(t.curveN[c], t.curveX[c], t.curveY[c]);
        out.insert("curveComposite", ToneCurve::encode(t.curveN, t.curveX, t.curveY));
    }
    if (lk.contains("curveChannels")) {
        EditParams t = p;
        ToneCurve::setIdentity(t.curveN[0], t.curveX[0], t.curveY[0]);
        out.insert("curveChannels", ToneCurve::encode(t.curveN, t.curveX, t.curveY));
    }
    if (lk.contains("calShadowTint")) out.insert("calShadowTint", p.calShadowTint);
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
    if (lk.contains("viewTransform")) out.insert("viewTransform", p.viewTransform);
    if (lk.contains("cameraProfile")) out.insert("cameraProfile", p.cameraProfile);
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
    if (lk.contains("sharpen")) {
        out.insert("sharpenAmount", p.sharpenAmount);
        out.insert("sharpenRadius", p.sharpenRadius);
        out.insert("sharpenDetail", p.sharpenDetail);
        out.insert("sharpenMasking", p.sharpenMasking);
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
    if (lk.contains("pointColors"))
        out.insert("pointColors", PointColor::encode(p.pointColorN, p.pointColor));
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
        /* The image's EFFECTIVE renderer -- what it looks like -- with the Apple version
           beside the published "demosaicEngine" token (added 2026-10-14). */
        const RawRender::Choice rc = RawRender::resolve(base);
        preset.globals.insert("demosaicEngine", QString(rc.apple ? "apple" : "winnow"));
        if (rc.apple) preset.globals.insert("appleRawVersion", rc.applePin);
    }
    if (gk.contains("rawNoise")) {
        /* The EFFECTIVE amounts: a preset captures what the image looks like, not the
           "follow the remembered value" marker. */
        preset.globals.insert("denoiseLuma", base.denoiseLumaOr(G::denoiseLumaDefault));
        preset.globals.insert("denoiseChroma",
                              base.denoiseChromaOr(G::denoiseChromaDefault));
        /* WHETHER it runs travels with the amounts -- the panel treats the checkbox and
           its two sliders as one control, and amounts alone cannot express "off". */
        preset.globals.insert("denoiseRaw", base.denoiseRaw);
    }
    /* Lens corrections: Global-only, so read from scope 0 like the raw NR. removeCA
       travels as stored -- an unset (-1) value stays "follow the preference" on the
       target rather than being frozen to this image's effective state. */
    if (gk.contains("lensCA"))
        preset.globals.insert("removeCA", base.removeCA);
    if (gk.contains("defringe")) {
        preset.globals.insert("defringePurple", base.defringePurple);
        preset.globals.insert("defringeGreen", base.defringeGreen);
    }
    /* The lens profile travels whole: on/off (as stored, like removeCA), both amounts,
       and the hand-picked lens (empty = match each target's own EXIF). */
    if (gk.contains("lensProfile")) {
        preset.globals.insert("lensProfile", base.lensProfile);
        preset.globals.insert("lensDistortion", base.lensDistortion);
        preset.globals.insert("lensVignetting", base.lensVignetting);
        preset.globals.insert("lensProfileId", base.lensProfileId);
    }
    if (gk.contains("lensManual")) {
        preset.globals.insert("lensManualDistortion", base.lensManualDistortion);
        preset.globals.insert("lensManualVignetting", base.lensManualVignetting);
        preset.globals.insert("lensManualMidpoint", base.lensManualMidpoint);
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
    if (gk.contains("kaleido")) {
        const Geometry &g = s.geometry;
        preset.globals.insert("kaleido", QStringLiteral("%1,%2,%3,%4,%5")
            .arg(g.kaleidoSegments).arg(g.kaleidoX).arg(g.kaleidoY)
            .arg(g.kaleidoAngle).arg(g.kaleidoSource));
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
    const bool usingApple = RawRender::resolve(base).apple;
    const bool rawNoiseChanged =
        base.denoiseLuma != def.denoiseLuma || base.denoiseChroma != def.denoiseChroma ||
        base.denoiseRaw != def.denoiseRaw;

    PresetGroup gg;
    gg.title = "Global settings";
    gg.checkable = false;
    gg.showAllNone = false;
    gg.leaves.append({"demosaic", "Raw demosaic", isRaw});
    gg.leaves.append({"rawNoise", "Raw noise reduction",
        isRaw && !usingApple && rawNoiseChanged});
    /* The Lens section's rows belong to no scope's adjustments (always scope 0), so they
       are per-image items here. */
    gg.leaves.append({"lensProfile", "Lens profile",
        base.lensProfile >= 0 || base.lensDistortion != def.lensDistortion ||
        base.lensVignetting != def.lensVignetting || !base.lensProfileId.isEmpty()});
    gg.leaves.append({"lensManual", "Lens manual corrections",
        base.lensManualDistortion != def.lensManualDistortion ||
        base.lensManualVignetting != def.lensManualVignetting});
    gg.leaves.append({"lensCA", "Remove chromatic aberration", base.removeCA >= 0});
    gg.leaves.append({"defringe", "Defringe (Purple + Green)",
        base.defringePurple != def.defringePurple || base.defringeGreen != def.defringeGreen});
    gg.leaves.append({"histogram", "Histogram tonal ranges",
        base.toneShadowCenter != def.toneShadowCenter ||
        base.toneCrossover != def.toneCrossover ||
        base.toneHighlightCenter != def.toneHighlightCenter});
    gg.leaves.append({"crop", "Crop", !s.geometry.cropIsIdentity()});
    gg.leaves.append({"level", "Level", s.geometry.straighten != gdef.straighten});
    gg.leaves.append({"warp", "Warp", s.geometry.hasWarp});
    gg.leaves.append({"kaleido", "Kaleidoscope", s.geometry.hasKaleido()});
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
    saveDevelopPresetIn(QString());
}

void DevelopProperties::saveDevelopPresetIn(const QString &category)
{
    if (G::isLogger) G::log("DevelopProperties::saveDevelopPresetIn", category);
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
    /* Default category: the one asked for, else the last used (if it still exists),
       else the permanent Winnow. */
    QString defCat = category.isEmpty() ? lastPresetCategory : category;
    if (defCat.isEmpty() || !presets->hasCategory(defCat))
        defCat = DevelopPresets::kWinnowCategory;
    dlg.setCategories(presets->categoryNames(), defCat);
    if (dlg.exec() != QDialog::Accepted) return;

    lastPresetCategory = dlg.category();
    presets->write(buildPreset(dlg.presetName(), dlg.scopeIndex(), dlg.selected(), s),
                   dlg.category());
    if (G::popup) G::popup->showPopup("Saved develop preset \"" + dlg.presetName() +
                                      "\" in " + dlg.category() + ".");
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

    /*
        The adjustments land on the target scope -- EXCEPT the whole-image ones.

        WAS A REAL BUG (found 2026-09-14): cameraProfile and viewTransform are read from
        scopes[0] and ONLY scopes[0] (they characterise the camera and map the whole
        image, so a per-scope value could not mean anything). Written to scopes[target]
        they landed somewhere nothing reads, so applying a preset while a mask scope was
        active silently dropped them -- no error, just a preset that did not do what it
        said.

        Keep this list in step with the whole-image fields in EditParams: a field that
        belongs here and is missing fails exactly this silently.
    */
    static const QSet<QString> kWholeImageParamKeys = {
        "cameraProfile", "viewTransform"
    };
    for (auto it = preset.params.constBegin(); it != preset.params.constEnd(); ++it) {
        const int scope = kWholeImageParamKeys.contains(it.key()) ? 0 : target;
        DevelopPresets::assignParam(it.key(), it.value(), s.scopes[scope].params);
    }

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
        else if (k == "kaleido") {
            /* segments,x,y,angle,source. A checked leaf with the kaleidoscope off travels
               as segments 0, which turns the target's off -- what the checkbox said. */
            const QStringList v = it.value().toString().split(',');
            if (v.size() == 5) {
                Geometry &g = s.geometry;
                g.kaleidoSegments = v[0].toInt();
                g.kaleidoX = v[1].toDouble();
                g.kaleidoY = v[2].toDouble();
                g.kaleidoAngle = v[3].toDouble();
                g.kaleidoSource = v[4].toDouble();   // range-checked with the merge, below
            }
        }
        else if (k == "demosaicEngine") {
            /* The renderer is per image (EditParams::rawEngine), so it lands in the
               recipe like any Global setting. An unknown token is skipped here and
               reported once by applyPreset. */
            const QString eng = it.value().toString().toLower();
            if (eng == "apple" || eng == "winnow") {
                RawRender::Choice c;
                c.apple = eng == "apple";
                if (c.apple) c.applePin = qMax(0, preset.globals.value("appleRawVersion").toInt());
                RawRender::apply(s.scopes[0].params, c);
            }
        }
        else if (k != "appleRawVersion") {  // read with demosaicEngine above
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
            this, &DevelopProperties::saveDevelopPresetIn);
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
    clearBeforeAfterLatch();            // this hover is now the override, not Before
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
        maskCommitTimer->stop();           // it must not land in a scope that is going
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
    clearBeforeAfterLatch();            // committing a state is not comparing two

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

    /* The renderer landed in each image's recipe (mergePreset), so the render below
       decodes with it -- the base is filed per renderer. The stored token is matched
       EXPLICITLY against the two engines rather than testing for "apple" and treating
       everything else as winnow -- otherwise a preset naming an engine this build does
       not have (a typo, a newer Winnow, a hand-edited settings file) would silently
       decode with the other one and the user would wonder why the preset looks
       different. */
    if (preset.globals.contains("demosaicEngine")) {
        const QString eng = preset.globals.value("demosaicEngine").toString().toLower();
        if (eng == "apple" || eng == "winnow") {
            /*  A preset (or Paste Settings) switching engine used to leave the Raw panel
                on the old one -- its "Render using" and its denoise block, which is
                Winnow-only, both out of step with what rendered. */
            reflectEngine();
            QTimer::singleShot(0, this, [this]{ buildTree(); });
            emit rawRendererChanged();
        }
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
            it.value() = QString(RawRender::resolve(s.scopes.first().params).apple
                                     ? "apple" : "winnow");
        }
        else if (k == "appleRawVersion") {
            it.value() = RawRender::resolve(s.scopes.first().params).applePin;
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
        else if (k == "kaleido") {
            const Geometry &g = s.geometry;
            it.value() = QStringLiteral("%1,%2,%3,%4,%5")
                .arg(g.kaleidoSegments).arg(g.kaleidoX).arg(g.kaleidoY)
                .arg(g.kaleidoAngle).arg(g.kaleidoSource);
        }
        /* paramsToJson OMITS a field at its default (the tri-states when unset, the
           defringe amounts at 0), and an absent key would be stored as an invalid
           QVariant that reads back as 0 -- turning "unset" into "off". */
        else if (bj.contains(k)) {
            it.value() = bj.value(k).toVariant();
        }
        else if (k == "denoiseRaw" || k == "removeCA" || k == "lensProfile") {
            it.value() = -1;
        }
        else if (k == "lensDistortion" || k == "lensVignetting") {
            it.value() = 1.0;
        }
        else if (k == "lensManualMidpoint") {
            it.value() = 0.5;
        }
        else if (k == "lensProfileId") {
            it.value() = QString();
        }
        else {
            it.value() = 0.0;
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

bool DevelopProperties::renderMatchesStoredRecipe() const
{
/*
    stackJob() applies three view overrides that make the rendered frame diverge from the
    stored recipe: a hovered History row (previewActive), the Transform preview eye
    (geometry.show -- off means the frame is UNCROPPED and unstraightened), and the
    Replace eye (spotsShown). Any of them makes the frame unsafe to cache as "what this
    image looks like". So does Point Color's Visualize Range, which greys most of it.
*/
    if (previewActive) return false;
    if (pointColorVisualize) return false;
    if (!spotsShown) return false;
    if (currentImagePath.isEmpty()) return false;
    return stackCache.value(currentImagePath).geometry.show;
}

QString DevelopProperties::developBlobFor(const QString &fPath)
{
    if (fPath.isEmpty()) return QString();
    const EditStack &s = stackFor(fPath);
    return s.isIdentity() ? QString() : s.toBase64();
}

void DevelopProperties::setDevPreviewProvider(DevPreviewProvider provider)
{
    devPreviewProvider = std::move(provider);
}

void DevelopProperties::topUpDevPreviews(const QString &fPath)
{
/*
    See the header. Fills in the previews for an image whose edits predate this session,
    from the frame the develop render already produced.

    Both tiers are gated together: if EITHER is missing or stale for the recipe in force,
    both are rewritten. Checking only one would let the grid thumbnail and the loupe
    placeholder come from different recipes.
*/
    if (fPath.isEmpty() || !devPreviewProvider) return;

    const QString blob = developBlobFor(fPath);
    if (blob.isEmpty()) return;                       // unedited: camera render is correct

    const QByteArray key = Metadata::devPreviewKey(blob, fPath).toLatin1();
    if (fPath == toppedUpPath && key == toppedUpKey) return;   // already handled

    const bool loupeOk = DevPreviewCache::instance().contains(fPath, key);
    /* readDevThumb returns nothing unless the stored key matches the recipe, so
       this is a staleness check as well as a presence check. */
    const bool thumbOk = !Metadata::readDevThumb(fPath).isEmpty();
    if (loupeOk && thumbOk) {
        toppedUpPath = fPath;
        toppedUpKey = key;
        return;
    }

    QByteArray thumbJpg;
    QByteArray loupeJpg;
    if (!devPreviewProvider(fPath, thumbJpg, loupeJpg)) return;   // no faithful frame
    if (thumbJpg.isEmpty() && loupeJpg.isEmpty()) return;

    if (!loupeJpg.isEmpty()) DevPreviewCache::instance().put(fPath, key, loupeJpg);

    /* A repaired stack keeps its on-disk recipe: this blob is the repaired one, and
       writing it would make the repair permanent on a view. The loupe tier above is
       keyed to the repaired recipe and lives only in the cache, so it is safe. */
    if (!thumbJpg.isEmpty() && sidecarRecipeRewritable(fPath)) {
        /* Rewrites the sidecar for an image the user only VIEWED. Deliberate: the two
           tiers must stay in step, and the recipe itself is passed through unchanged --
           only the preview attributes and modifydate move. */
        Metadata::writeDevelopSidecar(fPath, blob,
                                      QString::fromLatin1(thumbJpg.toBase64()));
        QImage thumb;
        if (thumb.loadFromData(thumbJpg, "JPG"))
            emit devPreviewUpdated(fPath, thumb);   // refresh the grid icon
    }

    toppedUpPath = fPath;
    toppedUpKey = key;
}

void DevelopProperties::rotateImageEdits(const QString &fPath, int degrees)
{
/*
    Called by MW::setRotation for every rotated image, AFTER MW has dropped its own
    frames and proxy for it (MW::invalidateDevelopAfterRotation) -- the order matters:
    flushImage asks MW for a preview, and MW must by then have nothing from the old
    orientation to give, so the stale previews are CLEARED rather than re-saved under
    the new recipe's hash.

    Every stored coordinate is re-expressed in the rotated frame (EditRotate::stack), and
    so is every History snapshot, so undo does not bring back masks in the old frame.

    PREVIEWS GO EVEN WHEN NOTHING MOVED. A recipe of tone and colour alone is unchanged
    by a rotation, so its hash -- the only thing the stored previews are checked against
    -- is unchanged too, and the thumbnail and loupe previews rendered in the old
    orientation would be served as current. An unedited image has no recipe to flush,
    but a raw's default-render preview is just as stale, so it is dropped directly.
*/
    if (G::isLogger) G::log("DevelopProperties::rotateImageEdits", fPath);
    if (fPath.isEmpty() || !EditRotate::isRotation(degrees)) return;

    EditStack &s = stackFor(fPath);
    EditRotate::stack(s, degrees);
    if (history)
        history->transformAll(fPath, [degrees](EditStack &e) { EditRotate::stack(e, degrees); });

    if (!s.isIdentity()) {
        dirty.insert(fPath);
        flushImage(fPath);              // new recipe, previews cleared (see above)
    }
    else {
        DevPreviewCache::instance().onDeleted(fPath);
        emit devPreviewUpdated(fPath, QImage());
    }

    if (fPath != currentImagePath) return;
    if (spotMode) emitSpotPins();
    syncPropagateBase();                // the rotated recipe is the new diff baseline
    emit paramsChanged();               // re-render in the new frame
}

void DevelopProperties::flushImage(const QString &fPath)
{
/*
    Write one image's edit state to its sidecar, together with the cached 256px develop
    preview that feeds the thumbnail grid and the screen-resolution preview that feeds
    the loupe while a raw decodes.

    The previews are a byproduct: the provider hands back the developProxy MW already has
    in memory, so this costs a downscale and a JPEG encode, not a render. No raw is ever
    decoded to make a preview.

    When the provider declines -- which it does for every image that is not the one
    currently proxied, i.e. every multi-image propagation target -- the previews are
    CLEARED rather than left behind. A preview that no longer matches its recipe would
    show the user a stale image; falling back to the camera thumbnail is honest. The
    develop badge is driven by the recipe, not the preview, so those images still read as
    edited in the grid.
*/
    if (fPath.isEmpty() || !dirty.contains(fPath)) return;
    dirty.remove(fPath);
    repairedStacks.remove(fPath);   // an edit: the user's stack now replaces the sidecar's
    const EditStack &s = stackCache[fPath];
    /* Identity => write "" so writeDevelopSidecar clears the attribute (or skips if
       there is no sidecar), rather than persisting an empty edit. */
    const QString blob = s.isIdentity() ? QString() : s.toBase64();

    QByteArray thumbJpg;
    QByteArray loupeJpg;
    if (!blob.isEmpty() && devPreviewProvider) devPreviewProvider(fPath, thumbJpg, loupeJpg);

    // synchronous; sidecar is a few KB plus ~20 KB of preview, and never on a drag
    Metadata::writeDevelopSidecar(fPath, blob,
                                  QString::fromLatin1(thumbJpg.toBase64()));
    /* The History steps that led here, saved WITH the recipe so the two are written
       together: a later session trusts the saved history only if its current step is
       this recipe (DevelopHistory::seed). */
    if (history && history->count(fPath) > 0)
        HistoryStore::save(fPath, history->entries(fPath), history->pos(fPath),
                           HistoryEntry::recipeStamp(s));

    if (!blob.isEmpty() && !loupeJpg.isEmpty()) {
        DevPreviewCache::instance().put(
            fPath, Metadata::devPreviewKey(blob, fPath).toLatin1(), loupeJpg);
    }
    else {
        /* No usable preview for this recipe: drop any older one so the loupe never paints
           pixels from a recipe the user has moved on from. */
        DevPreviewCache::instance().onDeleted(fPath);
    }

    /* Tell the grid. A null image means "this row's thumbnail is now wrong" rather than
       "here is the new one", and MW makes the loader re-read it. */
    QImage thumb;
    if (!thumbJpg.isEmpty()) thumb.loadFromData(thumbJpg, "JPG");
    emit devPreviewUpdated(fPath, thumb);
}

void DevelopProperties::flushAll()
{
    flushPropagation();      // a queued batch is an edit; it must reach the sidecars too
    const QList<QString> paths = dirty.values();
    for (const QString &p : paths) flushImage(p);
    /* Leaving Develop, quitting, or any file operation: the current image may have been
       rendered without ever being edited, so capture its previews before the frame goes
       away. No-op when it is unedited or both tiers are already current. */
    topUpDevPreviews(currentImagePath);
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
    {"clarity",             &EditParams::clarity},
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
    {"calShadowTint",       &EditParams::calShadowTint},
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
    {"sharpenAmount",       &EditParams::sharpenAmount},
    {"sharpenRadius",       &EditParams::sharpenRadius},
    {"sharpenDetail",       &EditParams::sharpenDetail},
    {"sharpenMasking",      &EditParams::sharpenMasking},
    {"vignetteExposure",    &EditParams::vignetteExposure},
    {"vignetteFeather",     &EditParams::vignetteFeather},
    {"grainAmount",         &EditParams::grainAmount},
    {"grainSize",           &EditParams::grainSize},
    {"grainRoughness",      &EditParams::grainRoughness},
    {"defringePurple",      &EditParams::defringePurple},
    {"defringeGreen",       &EditParams::defringeGreen},
    {"lensDistortion",      &EditParams::lensDistortion},
    {"lensVignetting",      &EditParams::lensVignetting},
    {"lensManualDistortion", &EditParams::lensManualDistortion},
    {"lensManualVignetting", &EditParams::lensManualVignetting},
    {"lensManualMidpoint",   &EditParams::lensManualMidpoint},
};
struct IntField { const char *name; int EditParams::*m; };
const IntField kIntFields[] = {
    {"wbPreset", &EditParams::wbPreset},
    {"viewTransform", &EditParams::viewTransform},
    {"denoiseRaw", &EditParams::denoiseRaw},
    {"rawEngine", &EditParams::rawEngine},
    {"appleRawVersion", &EditParams::appleRawVersion},
    {"removeCA", &EditParams::removeCA},
    {"lensProfile", &EditParams::lensProfile},
};

/* The tone curve cannot go in kFloatFields: it is three ARRAYS, and the table above is
   built on scalar member pointers. It gets one pseudo-field, "curves", covering all four
   channels together -- a curve is edited as a whole, so there is nothing to gain by
   splitting it. Both halves are here so the diff and the copy stay in step, exactly as
   the table does for the scalars. */
constexpr const char *kCurvesField = "curves";

/* The camera profile cannot go in kIntFields either -- it is a QString, and both tables
   are built on scalar member pointers. Same treatment as the curve: one named
   pseudo-field plus the differs/copy pair, wired into BOTH diffParamFields and
   copyParamFields so the diff and the copy cannot drift apart.

   It propagates across a multi-image selection like any other Basic control. Applying one
   camera's profile to another camera's raw is not a hazard here: the name is resolved
   per image against that image's OWN model, so it simply does not resolve, and the panel
   marks it "(not installed)" rather than rendering something wrong. */
constexpr const char *kProfileField = "cameraProfile";

/* The lens the user picked (a QString, like the camera profile). It propagates across a
   selection like any other control -- the common case is a batch shot on one manual lens
   whose EXIF says nothing -- and on an image whose camera cannot take that lens it simply
   does not resolve, which the Lens row reports. */
constexpr const char *kLensIdField = "lensProfileId";

/* Point Color's samples: a non-scalar field like the curves, so the same named
   pseudo-field + differs/copy pair. The whole point list travels as one: propagating
   "point 2's hue shift" to an image whose point 2 is a different colour would be
   nonsense. */
constexpr const char *kPointColorField = "pointColors";
bool pointColorsDiffer(const EditParams &a, const EditParams &b)
{
    if (a.pointColorN != b.pointColorN) return true;
    for (int k = 0; k < a.pointColorN; ++k)
        if (!(a.pointColor[k] == b.pointColor[k])) return true;
    return false;
}
void copyPointColors(const EditParams &src, EditParams &dst)
{
    dst.pointColorN = src.pointColorN;
    for (int k = 0; k < PointColor::kMaxPts; ++k) dst.pointColor[k] = src.pointColor[k];
}

bool curvesDiffer(const EditParams &a, const EditParams &b)
{
    for (int c = 0; c < ToneCurve::kChannels; ++c) {
        if (a.curveN[c] != b.curveN[c]) return true;
        for (int k = 0; k < a.curveN[c]; ++k)
            if (a.curveX[c][k] != b.curveX[c][k] || a.curveY[c][k] != b.curveY[c][k])
                return true;
    }
    return false;
}

void copyCurves(const EditParams &src, EditParams &dst)
{
    for (int c = 0; c < ToneCurve::kChannels; ++c) {
        dst.curveN[c] = src.curveN[c];
        for (int k = 0; k < ToneCurve::kMaxPts; ++k) {
            dst.curveX[c][k] = src.curveX[c][k];
            dst.curveY[c][k] = src.curveY[c][k];
        }
    }
}

/* Geometry is not in EditParams at all -- it is per-STACK, not per-scope -- but a crop,
   straighten or warp made with several images selected travels like any adjustment
   (Lightroom's Auto Sync does the same). Three pseudo-fields, one per Transform control,
   so a crop on the current image does not drag its straighten along to images that have
   their own. Coordinates are normalized, so the same rect lands proportionally on every
   image, exactly as Paste Settings already does. Geometry::show is a preview toggle, not
   an edit, and stays per-image. */
constexpr const char *kCropField       = "crop";
constexpr const char *kStraightenField = "straighten";
constexpr const char *kWarpField       = "warp";
constexpr const char *kKaleidoField    = "kaleido";

QSet<QString> diffGeometryFields(const Geometry &a, const Geometry &b)
{
    QSet<QString> changed;
    if (a.cropX != b.cropX || a.cropY != b.cropY || a.cropW != b.cropW || a.cropH != b.cropH)
        changed.insert(QString::fromLatin1(kCropField));
    if (a.straighten != b.straighten) changed.insert(QString::fromLatin1(kStraightenField));
    if (a.hasWarp != b.hasWarp ||
        !std::equal(std::begin(a.quad), std::end(a.quad), std::begin(b.quad)))
        changed.insert(QString::fromLatin1(kWarpField));
    if (!a.kaleidoEquals(b)) changed.insert(QString::fromLatin1(kKaleidoField));
    return changed;
}

void copyGeometryFields(const Geometry &src, Geometry &dst, const QSet<QString> &fields)
{
    if (fields.contains(QString::fromLatin1(kCropField))) {
        dst.cropX = src.cropX;
        dst.cropY = src.cropY;
        dst.cropW = src.cropW;
        dst.cropH = src.cropH;
    }
    if (fields.contains(QString::fromLatin1(kStraightenField)))
        dst.straighten = src.straighten;
    if (fields.contains(QString::fromLatin1(kWarpField))) {
        dst.hasWarp = src.hasWarp;
        std::copy(std::begin(src.quad), std::end(src.quad), std::begin(dst.quad));
    }
    if (fields.contains(QString::fromLatin1(kKaleidoField))) {
        dst.kaleidoSegments = src.kaleidoSegments;
        dst.kaleidoX = src.kaleidoX;
        dst.kaleidoY = src.kaleidoY;
        dst.kaleidoAngle = src.kaleidoAngle;
        dst.kaleidoSource = src.kaleidoSource;
    }
}

}   // namespace

QSet<QString> DevelopProperties::diffParamFields(const EditParams &a, const EditParams &b)
{
    QSet<QString> changed;
    for (const FloatField &f : kFloatFields)
        if (a.*(f.m) != b.*(f.m)) changed.insert(QString::fromLatin1(f.name));
    for (const IntField &f : kIntFields)
        if (a.*(f.m) != b.*(f.m)) changed.insert(QString::fromLatin1(f.name));
    if (curvesDiffer(a, b)) changed.insert(QString::fromLatin1(kCurvesField));
    if (pointColorsDiffer(a, b)) changed.insert(QString::fromLatin1(kPointColorField));
    if (a.cameraProfile != b.cameraProfile)
        changed.insert(QString::fromLatin1(kProfileField));
    if (a.lensProfileId != b.lensProfileId)
        changed.insert(QString::fromLatin1(kLensIdField));
    return changed;
}

void DevelopProperties::copyParamFields(const EditParams &src, EditParams &dst,
                                        const QSet<QString> &fields)
{
    for (const FloatField &f : kFloatFields)
        if (fields.contains(QString::fromLatin1(f.name))) dst.*(f.m) = src.*(f.m);
    for (const IntField &f : kIntFields)
        if (fields.contains(QString::fromLatin1(f.name))) dst.*(f.m) = src.*(f.m);
    if (fields.contains(QString::fromLatin1(kCurvesField))) copyCurves(src, dst);
    if (fields.contains(QString::fromLatin1(kPointColorField))) copyPointColors(src, dst);
    if (fields.contains(QString::fromLatin1(kProfileField))) dst.cameraProfile = src.cameraProfile;
    if (fields.contains(QString::fromLatin1(kLensIdField))) dst.lensProfileId = src.lensProfileId;
}

EditStack &DevelopProperties::stackFor(const QString &fPath)
{
    if (!stackCache.contains(fPath)) {
        EditStack::Status st = EditStack::Status::Absent;
        QStringList issues;
        stackCache.insert(fPath, EditStack::fromBase64(
                              Metadata::readDevelopSidecar(fPath), &st, &issues));
        reportStackIssues(fPath, int(st), issues);
        if (!issues.isEmpty()) repairedStacks.insert(fPath);
        else repairedStacks.remove(fPath);
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
        const QString p = idx.data(G::KeyRole).toString();
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

int DevelopProperties::selectedStillCount() const
{
/*
    Developable stills in the selection, independent of the current item. Unlike
    selectedEditCount this does not go through currentImagePath, so it still reports the
    stills when the CURRENT item is a video (MW empties the current path for a video, and
    the count would otherwise collapse to 0 while the user has a dozen JPGs selected).
*/
    if (!mw || !mw->dm || !mw->dm->selectionModel) return 0;
    int n = 0;
    const QModelIndexList rows = mw->dm->selectionModel->selectedRows();
    for (const QModelIndex &idx : rows) {
        if (idx.siblingAtColumn(G::VideoColumn).data().toBool()) continue;
        if (idx.data(G::KeyRole).toString().isEmpty()) continue;
        n++;
    }
    return n;
}

void DevelopProperties::syncPropagateBase()
{
    const EditStack s = stackCache.value(currentImagePath);
    propagateBase = s.scopes.isEmpty() ? EditParams() : s.scopes.at(0).params;
    propagateGeomBase = s.geometry;
    propagateScopesBase = s.scopes;
}

bool DevelopProperties::isPortableScope(const EditScope &l)
{
    if (l.components.isEmpty()) return false;
    for (const MaskComponent &m : l.components)
        if (m.tool != int(MaskTool::Subject) && m.tool != int(MaskTool::Sky) &&
            m.tool != int(MaskTool::Background))
            return false;
    return true;
}

namespace {

bool sameComponents(const EditScope &a, const EditScope &b)
{
    if (a.components.size() != b.components.size()) return false;
    for (int i = 0; i < a.components.size(); ++i) {
        const MaskComponent &x = a.components.at(i), &y = b.components.at(i);
        if (x.tool != y.tool || x.op != y.op || x.enabled != y.enabled ||
            x.inverted != y.inverted || x.feather != y.feather || x.edge != y.edge ||
            x.paramsJson != y.paramsJson)
            return false;
    }
    return true;
}

// the scope-level settings a linked scope carries besides its params and submasks
bool sameScopeState(const EditScope &a, const EditScope &b)
{
    return a.combine == b.combine && a.maskEdge == b.maskEdge &&
           a.maskHalo == b.maskHalo && a.enabled == b.enabled &&
           a.showScope == b.showScope && a.showBasic == b.showBasic &&
           a.showCurves == b.showCurves && a.showColor == b.showColor &&
           a.showCalibrate == b.showCalibrate && a.showColorGrade == b.showColorGrade &&
           a.showPointColor == b.showPointColor && a.showDetail == b.showDetail &&
           a.showEffects == b.showEffects && a.showLens == b.showLens &&
           a.showVignette == b.showVignette && a.showGrain == b.showGrain &&
           a.showKaleido == b.showKaleido;
}

void copyScopeState(const EditScope &src, EditScope &dst)
{
    dst.combine = src.combine;   dst.maskEdge = src.maskEdge;
    dst.maskHalo = src.maskHalo; dst.enabled = src.enabled;
    dst.showScope = src.showScope;           dst.showBasic = src.showBasic;
    dst.showCurves = src.showCurves;         dst.showColor = src.showColor;
    dst.showCalibrate = src.showCalibrate;   dst.showColorGrade = src.showColorGrade;
    dst.showPointColor = src.showPointColor; dst.showDetail = src.showDetail;
    dst.showEffects = src.showEffects;       dst.showLens = src.showLens;
    dst.showVignette = src.showVignette;     dst.showGrain = src.showGrain;
    dst.showKaleido = src.showKaleido;
}

int scopeIndexForLink(const EditStack &s, const QString &link)
{
    for (int i = 1; i < s.scopes.size(); ++i)
        if (s.scopes.at(i).link == link) return i;
    return -1;
}

}  // namespace

QHash<QString, DevelopProperties::ScopeSync> DevelopProperties::diffLinkedScopes(EditStack &s)
{
/*
    Which linked mask scopes this commit changed. A scope is matched to its state at the
    last commit by link; one with no link yet by position, which is right for the only
    case that needs it -- the commit that first gives it a submask, made in place.

    A LINK IS ASSIGNED HERE, and only here: to an AI-only scope (isPortableScope) that
    this commit changed while other images are selected. Assigning it on a commit that
    sends it means the scope it names is created on the targets in the same batch; an
    AI scope edited on one image alone never gets one, so nothing appears elsewhere
    unasked. The selection is read only when a link is wanted, not on every drag tick.
*/
    QHash<QString, ScopeSync> out;
    int multi = -1;                          // otherSelectedPaths, resolved on demand
    QSet<QString> live;
    for (int i = 1; i < s.scopes.size(); ++i) {
        EditScope &sc = s.scopes[i];
        const bool portable = isPortableScope(sc);
        if (!portable && sc.link.isEmpty()) continue;
        const EditScope *was = nullptr;
        if (!sc.link.isEmpty()) {
            for (int j = 1; j < propagateScopesBase.size(); ++j)
                if (propagateScopesBase.at(j).link == sc.link) {
                    was = &propagateScopesBase.at(j);
                    break;
                }
        }
        else if (i < propagateScopesBase.size() && propagateScopesBase.at(i).link.isEmpty())
            was = &propagateScopesBase.at(i);

        ScopeSync d;
        if (was) {
            d.fields = diffParamFields(was->params, sc.params);
            d.mask = portable && !sameComponents(*was, sc);
            d.state = !sameScopeState(*was, sc);
        }
        else d.mask = d.state = true;        // new since the last commit
        if (d.fields.isEmpty() && !d.mask && !d.state) {
            if (!sc.link.isEmpty()) live.insert(sc.link);
            continue;
        }
        if (sc.link.isEmpty()) {
            if (multi < 0) multi = otherSelectedPaths().isEmpty() ? 0 : 1;
            if (!multi) continue;
            sc.link = QUuid::createUuid().toString(QUuid::WithoutBraces);
            d.mask = d.state = true;         // the targets have none of it yet
        }
        live.insert(sc.link);
        out.insert(sc.link, d);
    }
    // a linked scope deleted on this image is deleted on the others
    for (int j = 1; j < propagateScopesBase.size(); ++j) {
        const QString &link = propagateScopesBase.at(j).link;
        if (!link.isEmpty() && !live.contains(link)) out[link].removed = true;
    }
    return out;
}

void DevelopProperties::applyLinkedScopes(const EditStack &src, EditStack &t,
                                          const QHash<QString, ScopeSync> &sync) const
{
    for (auto it = sync.constBegin(); it != sync.constEnd(); ++it) {
        const QString &link = it.key();
        const ScopeSync &d = it.value();
        const int si = scopeIndexForLink(src, link);
        const int ti = scopeIndexForLink(t, link);
        if (d.removed || si < 0) {
            if (ti > 0) t.scopes.removeAt(ti);
            continue;
        }
        const EditScope &from = src.scopes.at(si);
        const bool portable = isPortableScope(from);
        if (ti < 0) {
            /* Not on this image yet: the whole scope, params and all, under a name that
               is unique HERE. Only while it is AI-only: a geometry submask was drawn on
               the current image and means nothing on this one. */
            if (!portable) continue;
            EditScope copy = from;
            QSet<QString> names;
            for (const EditScope &l : t.scopes) names.insert(l.name);
            for (int n = 2; names.contains(copy.name); ++n)
                copy.name = from.name + " " + QString::number(n);
            t.scopes.append(copy);
            continue;
        }
        EditScope &to = t.scopes[ti];
        copyParamFields(from.params, to.params, d.fields);
        if (d.state) copyScopeState(from, to);
        if (d.mask && portable) to.components = from.components;
    }
}

void DevelopProperties::queuePropagation(const QString &action, const QString &value,
                                         const QString &mergeKey,
                                         const QHash<QString, ScopeSync> &scopeChanges)
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
        propagateGeomBase = s.geometry;
        propagateScopesBase = s.scopes;
        return;
    }

    QSet<QString> changed = diffParamFields(propagateBase, now);
    changed += diffGeometryFields(propagateGeomBase, s.geometry);
    propagateBase = now;                 // consumed either way: it is the new baseline
    propagateGeomBase = s.geometry;
    propagateScopesBase = s.scopes;      // already holds any link noteScopeEdit gave
    // anything else -- a brush or gradient mask, a spot -- stays per-image
    if (changed.isEmpty() && scopeChanges.isEmpty()) return;

    /* ONE BATCH PER HISTORY STEP. A batch is linked to the current image's step by a
       syncId, so a commit that is a different step (another gesture, or a non-coalescing
       action) closes the open batch first -- otherwise two steps on this image would map
       onto one step on the others, and reverting between them could not be matched. */
    const bool batchOpen = !pendingFields.isEmpty() || !pendingScopes.isEmpty();
    if (batchOpen && (mergeKey.isEmpty() || mergeKey != propagateMergeKey))
        flushPropagation();

    /* The batch belongs to the selection that was live when it OPENED, so a selection
       change mid-drag cannot redirect edits the user has already made -- and a drag
       (which commits continuously) reads the selection once, not once per tick. */
    if (pendingFields.isEmpty() && pendingScopes.isEmpty()) {
        propagateTargets = otherSelectedPaths();
        if (propagateTargets.isEmpty()) return;   // single image: nothing to fan out to
        propagateSyncId = ++nextSyncId;
    }
    pendingFields += changed;
    for (auto it = scopeChanges.constBegin(); it != scopeChanges.constEnd(); ++it) {
        ScopeSync &p = pendingScopes[it.key()];
        p.fields += it.value().fields;
        p.mask |= it.value().mask;
        p.state |= it.value().state;
        p.removed |= it.value().removed;
    }
    propagateAction = action;
    propagateValue  = value;
    propagateMergeKey = mergeKey;
    /* noteScopeEdit recorded this image's step just before calling here (it records
       whenever action is non-empty and we are not populating/restoring, which returned
       above). Link it to the batch. */
    if (history && !action.isEmpty())
        history->stampTop(currentImagePath, propagateSyncId);
    if (propagateTimer) propagateTimer->start(kPropagateMs);
}

void DevelopProperties::flushPropagation()
{
    if (propagateTimer) propagateTimer->stop();
    if ((pendingFields.isEmpty() && pendingScopes.isEmpty()) || propagateTargets.isEmpty()) {
        pendingFields.clear();
        pendingScopes.clear();
        propagateTargets.clear();
        return;
    }
    const QSet<QString> fields = pendingFields;
    const QHash<QString, ScopeSync> scopes = pendingScopes;
    const QStringList targets = propagateTargets;
    const quint64 syncId = propagateSyncId;
    pendingFields.clear();
    pendingScopes.clear();
    propagateTargets.clear();
    propagateMergeKey.clear();

    const EditStack cur = stackCache.value(currentImagePath);
    if (cur.scopes.isEmpty()) return;
    const EditParams src = cur.scopes.at(0).params;

    if (G::isLogger) G::log("DevelopProperties::flushPropagation",
                            QString::number(targets.count()) + " images");

    /* One history entry per target per settled gesture, coalesced on the field set: a
       long Exposure drag reads as one "Exposure" step on every image, not one per tick.

       EVERY target gets the step, opened or not, linked by syncId to the current image's
       -- that is what makes a multi-image edit undoable (applyHistoryEntry). An image
       with no history is seeded first with its state BEFORE this edit, so there is
       something to revert to. (This used to record only on images already opened, to
       spare the LRU store; the store was enlarged instead -- DevelopHistory::kMaxImages.)

       The key is SORTED: a QSet's iteration order is not stable, and an unstable key
       would defeat the coalescing it exists for. */
    QStringList names = fields.values();
    names.sort();
    QString mergeKey = "sync/" + names.join(',');
    /* A linked mask scope's step is labelled with that scope, as it is on the current
       image, and coalesces only with edits to the same scope and fields. */
    QString scopeLabel = QStringLiteral("Global");
    QStringList links = scopes.keys();
    links.sort();
    for (const QString &link : links) {
        const ScopeSync &d = scopes.value(link);
        QStringList f = d.fields.values();
        f.sort();
        mergeKey += "|" + link + ":" + f.join(',') + (d.mask ? "+m" : "")
                  + (d.state ? "+s" : "") + (d.removed ? "+x" : "");
        const int si = scopeIndexForLink(cur, link);
        if (fields.isEmpty() && si > 0) scopeLabel = cur.scopes.at(si).name;
    }
    for (const QString &p : targets) {
        EditStack &t = stackFor(p);
        if (history) history->seed(p, t);          // no-op once it has history
        copyParamFields(src, t.scopes[0].params, fields);
        copyGeometryFields(cur.geometry, t.geometry, fields);
        applyLinkedScopes(cur, t, scopes);
        EditStack::sanitize(t);
        dirty.insert(p);
        if (history)
            history->record(p, scopeLabel, propagateAction, propagateValue, mergeKey, t,
                            syncId);
    }
    /* Seeding and recording touched every target; touch the image on screen last so a
       large selection can never evict ITS history. */
    if (history) history->seed(currentImagePath, cur);
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
    /* Linked like an adjustment batch (see flushPropagation): every target gets a step,
       seeded first if it had no history, and the current image's step -- recorded by the
       caller just before this -- carries the same syncId. */
    const quint64 syncId = ++nextSyncId;
    if (history) history->stampTop(currentImagePath, syncId);
    for (const QString &p : targets) {
        EditStack &t = stackFor(p);
        if (history) history->seed(p, t);
        t = mergePreset(preset, t, 0);
        dirty.insert(p);
        if (history)
            history->record(p, "Global", label, QString(), QString(), t, syncId);
    }
    if (history) history->seed(currentImagePath, stackCache.value(currentImagePath));
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
    setSliderReal("clarity",            p.clarity);
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
    setSliderReal("denoiseLuma",   p.denoiseLumaOr(G::denoiseLumaDefault) * 100.0);   // Global "Denoise raw" (0..1 -> 0..100)
    setSliderReal("denoiseChroma", p.denoiseChromaOr(G::denoiseChromaDefault) * 100.0);
    setSliderReal("sharpenAmount",      p.sharpenAmount      * 100.0);   // "Sharpening"
    setSliderReal("sharpenRadius",      p.sharpenRadius);                    // div 100
    setSliderReal("sharpenDetail",      p.sharpenDetail      * 100.0);
    setSliderReal("sharpenMasking",     p.sharpenMasking     * 100.0);
    setSliderReal("localDenoise",       p.localDenoiseLuma   * 100.0);   // "Denoise"
    setSliderReal("localDenoiseChroma", p.localDenoiseChroma * 100.0);
    setSliderReal("vignetteExposure",   p.vignetteExposure);            // "Vignette" (EV)
    setSliderReal("vignetteFeather",    p.vignetteFeather    * 100.0);
    setSliderReal("grainAmount",        p.grainAmount        * 100.0);   // "Grain"
    setSliderReal("grainSize",          p.grainSize          * 100.0);
    setSliderReal("grainRoughness",     p.grainRoughness     * 100.0);
    syncLensPanel();                  // Global's Remove CA / fringe rows (scope 0 always)
    syncKaleidoSliders();             // Global's Effects > Kaleidoscope (geometry)
    emit kaleidoToolChanged();        // new image / scope: the handles re-evaluate
    if (toneSlider)
        toneSlider->setPositions(p.toneShadowCenter, p.toneCrossover, p.toneHighlightCenter);
    refreshCurveRow();
    setSliderReal("calShadowTint", p.calShadowTint);   // Calibrate (RGB)
    setSliderReal("calRedHue",     p.calRedHue);
    setSliderReal("calRedSat",     p.calRedSat);
    setSliderReal("calGreenHue",   p.calGreenHue);
    setSliderReal("calGreenSat",   p.calGreenSat);
    setSliderReal("calBlueHue",    p.calBlueHue);
    setSliderReal("calBlueSat",    p.calBlueSat);
    refreshColorGradeRow();           // push grade values into the wheel + Lum slider
    refreshPointColorRow();           // points -> swatches/field + the selected sliders
    isPopulating = false;
    refreshWbRow();                 // Temp/Tint resolved + the preset dropdown
    refreshViewTransformRow();      // the Calibrate view-transform combo
    refreshCameraProfileRow();      // the Profile combo (its LIST is per-camera)
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

/* Each band's own help page, opened from the "<section> help" item at the foot of its
   [:] menu. Separate pages, not one long module page (howThisWorks), because each panel
   has its own grammar -- what the sliders mean, how they interact, when to reach for
   them -- and the help a user wants is the help for the band they are standing on. */
void DevelopProperties::sectionHelp(int group)
{
    if (G::isLogger) G::log("DevelopProperties::sectionHelp");
    QString doc;
    switch (group) {
    case PV_Basic:      doc = "developbasichelp";      break;
    case PV_Curves:     doc = "developcurveshelp";     break;
    case PV_Color:      doc = "developcolorhelp";      break;
    case PV_Calibrate:  doc = "developcalibratehelp";  break;
    case PV_ColorGrade: doc = "developcolorgradehelp"; break;
    case PV_PointColor: doc = "developpointcolorhelp"; break;
    case PV_ColorPanel: sectionHelp(colorPanelModeGroup()); return;   // the shown mode
    case PV_Detail:     doc = "developdetailhelp";     break;
    case PV_Effects:    doc = "developeffectshelp";    break;
    case PV_Lens:       doc = "developlenshelp";       break;
    default:            howThisWorks();  return;       // PV_Scope: the module page
    }
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - " + groupLabel(group),
                   ":/Docs/" + doc + ".html",
                   QSize(700, 600), r, window());
}

void DevelopProperties::editsHelp()
{
    if (G::isLogger) G::log("DevelopProperties::editsHelp");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Edits",
                   ":/Docs/developeditshelp.html",
                   QSize(700, 600), r, window());
}

void DevelopProperties::submasksHelp()
{
    if (G::isLogger) G::log("DevelopProperties::submasksHelp");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Submasks",
                   ":/Docs/developsubmaskshelp.html",
                   QSize(700, 600), r, window());
}

void DevelopProperties::rawHelp()
{
    if (G::isLogger) G::log("DevelopProperties::rawHelp");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Raw",
                   ":/Docs/developrawhelp.html",
                   QSize(700, 600), r, window());
}

void DevelopProperties::transformHelp()
{
    if (G::isLogger) G::log("DevelopProperties::transformHelp");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Transform",
                   ":/Docs/developtransformhelp.html",
                   QSize(700, 600), r, window());
}

void DevelopProperties::howThisWorks()
{
    if (G::isLogger) G::log("DevelopProperties::howThisWorks");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Develop Module",
                   ":/Docs/develophelp.html",
                   QSize(700, 600), r, window());
}
