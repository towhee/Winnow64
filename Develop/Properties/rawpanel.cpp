#include "Develop/Properties/rawpanel.h"
#include "Develop/Properties/paneleditor.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"
#include "ImageFormats/Raw/applerawdecode.h"
#include "Dialogs/applecamerasdlg.h"

#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QLinearGradient>
#include <QSignalBlocker>
#include <QSettings>
#include <QMouseEvent>
#include <QPixmap>
#include <QIcon>

/*
    RawPanel (see rawpanel.h): the raw-decode strip lifted out of the Global "Core" tree
    rows. A dumb view -- controls emit, DevelopProperties pushes state back.
*/

RawPanel::RawPanel(QWidget *parent, QSettings *settings) : QWidget(parent), setting(settings)
{
    if (G::isLogger) G::log("RawPanel::RawPanel");
    if (setting) collapsed = setting->value("Develop/rawCollapsed", false).toBool();
    buildUi();
}

/*
    A rule matching the ones the property tree draws between the Basic / Color / Effects
    groups: a G::groupSeparatorWidth line centred in G::groupSeparatorHeight, inset
    G::groupSeparatorInset, in G::groupSeparatorColor (see
    DevelopProperties::addGroupDivider), with an extra 3px of breathing room above it.
    Translucent like every other container here: a plain QWidget takes the app
    stylesheet's opaque background, which slabs a dark band across the panel's lifted
    content background instead of just showing the rule.
*/
static QWidget *makeGroupRule(QWidget *parent)
{
    QWidget *divider = new QWidget(parent);
    divider->setAttribute(Qt::WA_TranslucentBackground);
    const int sepH = G::groupSeparatorHeight;
    const int sepW = qMin(G::groupSeparatorWidth, sepH);
    const int sepAbove = (sepH - sepW) / 2;
    divider->setFixedHeight(3 + sepH);
    QVBoxLayout *l = new QVBoxLayout(divider);
    l->setContentsMargins(G::groupSeparatorInset, 3 + sepAbove,
                          G::groupSeparatorInset, sepH - sepW - sepAbove);
    l->setSpacing(0);
    QFrame *rule = new QFrame(divider);
    rule->setFixedHeight(sepW);
    rule->setStyleSheet(QString("background: %1; border: none;")
                            .arg(G::groupSeparatorColor.name()));
    l->addWidget(rule);
    return divider;
}

void RawPanel::buildUi()
{
    const QString capCss = G::labelCss(G::textColor, G::strFontSize.toInt());

    /* The bottom margin reserves the 2px separator rule paintEvent draws under every
       Develop panel (G::panelBorderHeight). */
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, G::panelBorderHeight);
    outer->setSpacing(0);

    /* Header band (collapse arrow + "Raw" + [?]); transparent so paintEvent draws the
       gradient behind. The arrow sits in the tree's gutter and the caption follows
       G::decorationTitleGap clear of it, matching the tree's section headers (same idiom
       as the old ScopeHeader). */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    headerBand->setCursor(Qt::PointingHandCursor);
    headerBand->installEventFilter(this);        // a header click toggles collapse
    /* Same height as a property-tree row, and the caption centred on the band LESS
       G::headerCaptionTrim at the bottom -- exactly what PropertyDelegate does with a
       section header (its r4) -- so "Raw" sits at the same height in its band as
       "Basic" and "Color" do in theirs. Without the fixed height the band also grows
       with any spare space its parent layout hands it, dropping the caption further. */
    headerBand->setFixedHeight(G::propertyRowHeight());
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(G::headerLeftInset, 0, 6, G::headerCaptionTrim);
    hb->setSpacing(0);
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show the raw decode controls");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(9, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0; background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleCollapsed(); });
    titleLabel = new QLabel(tr("Raw"), headerBand);
    titleLabel->setStyleSheet(G::labelCss(G::header2Color, G::strFontSize.toInt()));
    tipBtn = new BarBtn();
    tipBtn->setToolTip("About the raw decode controls");
    tipBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    connect(tipBtn, &BarBtn::clicked, this, [this]{ emit tipsRequested(); });
    hb->addWidget(collapseBtn);
    hb->addSpacing(G::decorationTitleGap);
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(tipBtn);
    outer->addWidget(headerBand);

    /* Body container (normal background); hidden when collapsed. Zero horizontal margins
       so the embedded denoise editor spans full width and aligns with the property tree;
       the custom rows below re-add the 10px inset so their captions stay indented. */
    body = new QWidget(this);
    /* Translucent, like every other container in the Develop panels: under the app
       stylesheet a plain QWidget fills its background opaquely, which would cover the
       panel's content background (G::panelContentBg, painted in paintEvent). The rows
       built below are parented to it and get the same treatment. */
    body->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *bl = new QVBoxLayout(body);
    /* Top margin: the panel's own 4px inset, plus the 5px spacer the property tree puts
       under every section header (DevelopProperties::addHeader), plus 4px of extra
       clearance between the "Raw" band and the Edit row. */
    bl->setContentsMargins(0, 13, 0, 6);
    bl->setSpacing(4);
    outer->addWidget(body);

    /* Edit source: Raw / Embedded Preview (always shown while the panel is visible). */
    QWidget *editRow = new QWidget(body);
    editRow->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *el = new QHBoxLayout(editRow);
    el->setContentsMargins(10, 0, 10, 0);
    el->setSpacing(12);
    QLabel *editLbl = new QLabel(tr("Edit:"), editRow);
    editLbl->setStyleSheet(capCss);
    rawRadio  = new QRadioButton(tr("Raw"), editRow);
    prevRadio = new QRadioButton(tr("Embedded Preview"), editRow);
    /* Transparent like the captions beside them: an opaque control would slab the panel's
       lifted background. */
    for (QRadioButton *r : {rawRadio, prevRadio})
        r->setStyleSheet("QRadioButton { background: transparent; }");
    QButtonGroup *grp = new QButtonGroup(editRow);
    grp->setExclusive(true);
    grp->addButton(rawRadio);
    grp->addButton(prevRadio);
    connect(rawRadio, &QRadioButton::toggled, this, [this](bool on){
        /* toggled fires for both halves; report the "Raw" button's new state. */
        emit editSourceChanged(on);
    });
    el->addWidget(editLbl);
    el->addWidget(rawRadio);
    el->addWidget(prevRadio);
    el->addStretch(1);
    bl->addWidget(editRow);

    /* Demosaic + Denoise block (hidden when editing the Embedded Preview). */
    rawBlock = new QWidget(body);
    rawBlock->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *rb = new QVBoxLayout(rawBlock);
    rb->setContentsMargins(0, 0, 0, 0);
    rb->setSpacing(4);
    bl->addWidget(rawBlock);

    /* Rule between the Edit row and "Render using". Inside rawBlock so it hides with
       the block on Embedded Preview rather than dangling under the Edit row. */
    rb->addWidget(makeGroupRule(rawBlock));

    /* Demosaic engine. */
    QWidget *demRow = new QWidget(rawBlock);
    demRow->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *dl = new QHBoxLayout(demRow);
    dl->setContentsMargins(10, 0, 10, 0);
    dl->setSpacing(6);
    QLabel *demLbl = new QLabel(tr("Render using"), demRow);
    demLbl->setStyleSheet(capCss);
    engineCombo = new QComboBox(demRow);
    engineCombo->setToolTip("Which decoder turns this raw file into a picture: Apple's, "
                            "or Winnow's own. Saved with this image's edits, so each "
                            "image -- and each version -- keeps its own. Images you have "
                            "not set follow the default in Preferences.");
    rebuildEngineCombo();
    /*  The TOP Apple entry means "newest" (pin 0), not "this number": it keeps following
        OS updates. A lower entry pins that version. Either is THIS IMAGE's choice, kept
        in its recipe (Develop/rawrender.h). */
    connect(engineCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx){
        const int v = engineCombo->itemData(idx).toInt();
        if (v < 0) emit demosaicChanged(false);
        else       emit appleVersionChanged(idx == 0 ? 0 : v);
    });
    dl->addWidget(demLbl);
    dl->addWidget(engineCombo, 1);
    rb->addWidget(demRow);

    /* Apple block: shown only while an Apple decoder renders -- the counterpart of the
       Winnow-only denoise block below. A rule, then "View cameras supported". */
    appleBlock = new QWidget(rawBlock);
    appleBlock->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *ab = new QVBoxLayout(appleBlock);
    ab->setContentsMargins(0, 0, 0, 0);
    ab->setSpacing(4);
    ab->addWidget(makeGroupRule(appleBlock));
    QWidget *camRow = new QWidget(appleBlock);
    camRow->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *cl = new QHBoxLayout(camRow);
    cl->setContentsMargins(10, 0, 10, 0);
    cl->setSpacing(0);
    QPushButton *camBtn = new QPushButton(tr("View cameras supported"), camRow);
    /* Transparent on the panel's lifted background, keeping the app's border and
       padding. A widget stylesheet outranks the app's QPushButton:hover too, so the hover
       fill is restated; min-width: 0 defeats the global 100px minimum, which Qt applies
       as an EXPLICIT minimum that would floor the dock's width (widgetcss.cpp). */
    camBtn->setStyleSheet(
        "QPushButton { background: transparent; min-width: 0; }"
        "QPushButton:hover { background: " + G::groupSeparatorColor.name() + "; }");
    camBtn->setToolTip(tr("List the cameras Apple supports for each Apple Raw decoder "
                          "version."));
    connect(camBtn, &QPushButton::clicked, this, [this]{
        /* Open on the version this image renders with, else the newest it supports. */
        const int v = appleUsed > 0 ? appleUsed : appleVersions.value(0);
        AppleCamerasDlg dlg(v, window());
        dlg.exec();
    });
    cl->addWidget(camBtn);
    cl->addStretch(1);
    ab->addWidget(camRow);
    rb->addWidget(appleBlock);

    /* Denoise block (hidden on the Apple engine -- PMRID needs the CFA mosaic). */
    denoiseBlock = new QWidget(rawBlock);
    denoiseBlock->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *nb = new QVBoxLayout(denoiseBlock);
    nb->setContentsMargins(0, 0, 0, 0);
    nb->setSpacing(4);
    rb->addWidget(denoiseBlock);

    /* Rule below the "Render using" row. It lives in denoiseBlock so it hides with the
       denoise group on the Apple engine rather than dangling under the last visible
       row. */
    nb->addWidget(makeGroupRule(denoiseBlock));

    /* Group caption for the raw denoise controls that follow. */
    QWidget *nrRow = new QWidget(denoiseBlock);
    nrRow->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *nrl = new QHBoxLayout(nrRow);
    nrl->setContentsMargins(10, 0, 10, 0);
    nrl->setSpacing(0);
    QLabel *nrLbl = new QLabel(tr("Noise Reduction:"), nrRow);
    nrLbl->setStyleSheet(capCss);
    nrl->addWidget(nrLbl);
    nrl->addStretch(1);
    nb->addWidget(nrRow);
    nb->addSpacing(4);              // extra space between the caption and its controls

    /* Run row: "Auto run", then "Run on this image" -- the per-image choice, shown only
       while Auto run is off (with it on every raw is denoised, so there is nothing to
       choose; see EditParams::wantsDenoiseRaw). */
    QWidget *runRow = new QWidget(denoiseBlock);
    runRow->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *nl = new QHBoxLayout(runRow);
    nl->setContentsMargins(10, 0, 10, 0);
    nl->setSpacing(20);
    autoRunCheck = new QCheckBox(tr("Auto run"), runRow);
    autoRunCheck->setToolTip("On: every raw image is denoised as you come to it.\n"
                             "Off: choose per image with \"Run on this image\".");
    connect(autoRunCheck, &QCheckBox::toggled, this, [this](bool on){
        updateRunOnImageVisible();
        emit autoRunToggled(on);
    });
    denoiseCheck = new QCheckBox(tr("Run on this image"), runRow);
    denoiseCheck->setToolTip("Denoise this image. Lum and Color can be used once it is "
                             "ready.");
    connect(denoiseCheck, &QCheckBox::toggled, this,
            [this](bool on){ emit denoiseRunToggled(on); });
    for (QCheckBox *c : {autoRunCheck, denoiseCheck})
        c->setStyleSheet("QCheckBox { background: transparent; }");
    nl->addWidget(autoRunCheck);
    nl->addWidget(denoiseCheck);
    nl->addStretch(1);
    nb->addWidget(runRow);
    updateRunOnImageVisible();

    /* Lum / Color amount sliders (0..100), rendered by an embedded PanelEditor so they
       look/align exactly like the property tree. In both Auto run states they SHOW only
       once this image has been denoised (setDenoiseRunState): they scale the blend of a
       computed base, so before it exists there is nothing for them to do. Their
       appearing is also the "denoise finished" cue. */
    denoiseEditor = new PanelEditor(denoiseBlock);
    denoiseEditor->addSlider("denoiseLuma", "Lum",
                             "Reduce grainy noise.", 0, 100);
    denoiseEditor->addSlider("denoiseChroma", "Color",
                             "Raw colour (chroma) noise reduction.", 0, 100);
    denoiseEditor->fitHeight();
    connect(denoiseEditor, &PanelEditor::settingChanged, this,
            [this](const QString &key, const QVariant &v){
                if      (key == "denoiseLuma")   emit denoiseLumaChanged(v.toInt());
                else if (key == "denoiseChroma") emit denoiseChromaChanged(v.toInt());
            });
    nb->addWidget(denoiseEditor);
    updateAmountsVisible();

    /* Why the group above is dead, in its place -- shown only when PMRID cannot run here
       (setDenoiseAvailable). Wraps, indented to the row text, and dimmed so it reads as an
       explanation rather than another control. */
    denoiseNote = new QLabel(denoiseBlock);
    denoiseNote->setWordWrap(true);
    denoiseNote->setContentsMargins(10, 0, 10, 2);
    denoiseNote->setStyleSheet(G::labelCss(G::disabledColor, G::strFontSize.toInt()));
    denoiseNote->setVisible(false);
    nb->addWidget(denoiseNote);

    body->setVisible(!collapsed);        // honour the persisted collapse state
    updateCollapseIcon();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void RawPanel::toggleCollapsed()
{
    setCollapsed(!collapsed);
    /* User-driven only: DevelopProperties folds the tree sections / Scope row in when
       Solo mode is on. Programmatic setCollapsed calls stay silent so the solo handler
       cannot bounce back into the panel that triggered it. */
    emit collapseToggled(collapsed);
}

void RawPanel::setCollapsed(bool collapse)
{
    collapsed = collapse;
    if (body) body->setVisible(!collapsed);
    updateCollapseIcon();
    if (setting) setting->setValue("Develop/rawCollapsed", collapsed);
}

void RawPanel::updateCollapseIcon()
{
    if (!collapseBtn) return;
    /* Match the tree's branch arrows: open (down) when shown, closed (right) when hidden,
       full opacity at 9x9 -- BarBtn::setIcon(path, opacity) would dim it. */
    const QString path = collapsed ? ":/images/branch-closed-winnow.png"
                                   : ":/images/branch-open-winnow.png";
    collapseBtn->setIcon(QIcon(QPixmap(path)));
}

bool RawPanel::eventFilter(QObject *watched, QEvent *event)
{
    /* A single left click anywhere on the header band -- the caption (which ignores the
       press so it propagates here) or the empty area -- toggles collapse. The [?] tip and
       collapse-arrow buttons consume their own clicks, so they keep working.
       MouseButtonDblClick is included because Qt sends it INSTEAD of the second press:
       without it a double click would toggle once (leaving the panel in the opposite
       state) rather than toggling twice back to where it started. */
    if (watched == headerBand && (event->type() == QEvent::MouseButtonPress ||
                                  event->type() == QEvent::MouseButtonDblClick)) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) { toggleCollapsed(); return true; }
    }
    return QWidget::eventFilter(watched, event);
}

void RawPanel::paintEvent(QPaintEvent *)
{
    if (!headerBand) return;
    QPainter p(this);
    /* Subpanel content background (G::panelContentBg): everything BELOW the header band,
       which keeps the gradient painted just after this. */
    p.fillRect(0, headerBand->geometry().bottom() + 1, width(),
               height() - headerBand->geometry().bottom() - 1, G::panelContentBg);
    const QRect r = headerBand->geometry();
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, G::headerGradientTop);
    g.setColorAt(1, G::headerGradientBottom);
    p.fillRect(r, g);
    /* Separator rule across the bottom edge (space reserved by the layout margin). */
    p.fillRect(0, height() - G::panelBorderHeight, width(), G::panelBorderHeight,
               G::panelSeparatorColor);
}

void RawPanel::setEditSource(bool raw)
{
    if (rawRadio)  { QSignalBlocker b(rawRadio);  rawRadio->setChecked(raw); }
    if (prevRadio) { QSignalBlocker b(prevRadio); prevRadio->setChecked(!raw); }
    /* Demosaic + Denoise apply to the raw decode only; hidden on Embedded Preview. */
    if (rawBlock) rawBlock->setVisible(raw);
}

void RawPanel::setAppleVersions(const QList<int> &newestFirst, int used)
{
    if (newestFirst == appleVersions && used == appleUsed) return;
    appleVersions = newestFirst;
    appleUsed = used;
    rebuildEngineCombo();
}

void RawPanel::rebuildEngineCombo()
{
/*
    One Apple entry per decoder version THIS IMAGE supports, newest first, then Winnow.
    Item data is the version, 0 for the unnumbered entry shown for the moment it takes
    Core Image to open the file and say (AppleRawDecode::RequestVersions, 50-200 ms on a
    pool thread, whichever engine is rendering -- the camera model alone is not a
    reliable source, see notes/Documentation.txt "Apple decoder version"), and -1 for
    Winnow.

    The selection is the version this image's choice picks from its list
    (DevelopProperties::syncAppleDecoderVersions), which differs from the pin when the
    image does not support the pinned version.
*/
    if (!engineCombo) return;
    QSignalBlocker b(engineCombo);
    engineCombo->clear();
#ifdef Q_OS_MAC
    const QString appleName = tr("Apple Raw");
#else
    const QString appleName = tr("Apple");
#endif
    if (appleVersions.isEmpty()) {
        engineCombo->addItem(appleName, 0);
        engineCombo->setItemData(0, tr("Apple's decoder. Checking which versions "
                                       "support this image..."),
                                 Qt::ToolTipRole);
    }
    for (int i = 0; i < appleVersions.size(); ++i) {
        const int v = appleVersions.at(i);
        engineCombo->addItem(QString("%1 %2").arg(appleName).arg(v), v);
        engineCombo->setItemData(i, i == 0
            ? tr("Apple's newest decoder (version %1). Always uses the newest version "
                 "each image supports.").arg(v)
            : tr("Apple's decoder version %1 for this image (if the image does not "
                 "support it, the newest version it does support).").arg(v), Qt::ToolTipRole);
    }
    engineCombo->addItem(tr("Winnow"), -1);

    int sel = engineCombo->count() - 1;                 // Winnow
    if (engineApple) {
        sel = 0;
        const int at = engineCombo->findData(appleUsed);
        if (appleUsed > 0 && at >= 0) sel = at;
    }
    engineCombo->setCurrentIndex(sel);
}

void RawPanel::setEngine(bool apple)
{
    engineApple = apple;
    rebuildEngineCombo();
    /* PMRID denoise needs the CFA mosaic -> only meaningful on the Winnow engine. */
    if (denoiseBlock) denoiseBlock->setVisible(!apple);
    if (appleBlock) appleBlock->setVisible(apple);
}

void RawPanel::setDenoiseRunState(bool denoised)
{
    denoiseReady = denoised;
    updateAmountsVisible();
}

void RawPanel::setRunOnImage(bool on)
{
    if (!denoiseCheck) return;
    QSignalBlocker b(denoiseCheck);
    denoiseCheck->setChecked(on);
}

void RawPanel::updateAmountsVisible()
{
    /* The amount sliders only scale a computed PMRID base, so they are hidden until it
       exists -- and while the denoise cannot run at all (that base is never coming; the
       reason line takes their place). */
    if (denoiseEditor) denoiseEditor->setVisible(denoiseReady && denoiseAvailable);
}

void RawPanel::setDenoiseAvailable(bool available, const QString &reason)
{
    denoiseAvailable = available;
    /* The box keeps showing the saved choice; greyed, with the reason below, it reads as
       "wanted but cannot run here" rather than pretending the choice was never made. */
    if (denoiseCheck) denoiseCheck->setEnabled(available);
    if (autoRunCheck) autoRunCheck->setEnabled(available);
    updateAmountsVisible();
    if (denoiseNote) {
        denoiseNote->setText(reason);
        denoiseNote->setVisible(!available && !reason.isEmpty());
    }
}

void RawPanel::setAutoRun(bool on)
{
    if (!autoRunCheck) return;
    {
        QSignalBlocker b(autoRunCheck);
        autoRunCheck->setChecked(on);
    }
    updateRunOnImageVisible();
}

void RawPanel::updateRunOnImageVisible()
{
    if (denoiseCheck && autoRunCheck)
        denoiseCheck->setVisible(!autoRunCheck->isChecked());
}

void RawPanel::setDenoiseValues(int luma, int chroma)
{
    if (!denoiseEditor) return;
    denoiseEditor->setSliderReal("denoiseLuma", luma);     // seeds silently (no echo)
    denoiseEditor->setSliderReal("denoiseChroma", chroma);
}

void RawPanel::setCaptionWidth(int w)
{
    if (denoiseEditor) denoiseEditor->setCaptionWidth(w);
}
