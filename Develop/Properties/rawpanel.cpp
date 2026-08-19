#include "Develop/Properties/rawpanel.h"
#include "Develop/Properties/paneleditor.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
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
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(0, 3, 6, 3);
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
    QVBoxLayout *bl = new QVBoxLayout(body);
    bl->setContentsMargins(0, 4, 0, 6);
    bl->setSpacing(4);
    outer->addWidget(body);

    /* Edit source: Raw / Embedded Preview (always shown while the panel is visible). */
    QWidget *editRow = new QWidget(body);
    QHBoxLayout *el = new QHBoxLayout(editRow);
    el->setContentsMargins(10, 0, 10, 0);
    el->setSpacing(12);
    QLabel *editLbl = new QLabel(tr("Edit:"), editRow);
    editLbl->setStyleSheet(capCss);
    rawRadio  = new QRadioButton(tr("Raw"), editRow);
    prevRadio = new QRadioButton(tr("Embedded Preview"), editRow);
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
    QVBoxLayout *rb = new QVBoxLayout(rawBlock);
    rb->setContentsMargins(0, 0, 0, 0);
    rb->setSpacing(4);
    bl->addWidget(rawBlock);

    /* Demosaic engine. */
    QWidget *demRow = new QWidget(rawBlock);
    QHBoxLayout *dl = new QHBoxLayout(demRow);
    dl->setContentsMargins(10, 0, 10, 0);
    dl->setSpacing(6);
    QLabel *demLbl = new QLabel(tr("Render using"), demRow);
    demLbl->setStyleSheet(capCss);
    engineCombo = new QComboBox(demRow);
    engineCombo->addItem(tr("Apple"));      // index 0 = Apple
    engineCombo->addItem(tr("Winnow"));     // index 1 = Winnow
    engineCombo->setToolTip("Demosaic engine (Apple Core Image or in-house Winnow).");
    connect(engineCombo, QOverload<int>::of(&QComboBox::activated), this,
            [this](int idx){ emit demosaicChanged(idx == 0); });
    dl->addWidget(demLbl);
    dl->addWidget(engineCombo, 1);
    rb->addWidget(demRow);

    /* Denoise block (hidden on the Apple engine -- PMRID needs the CFA mosaic). */
    denoiseBlock = new QWidget(rawBlock);
    QVBoxLayout *nb = new QVBoxLayout(denoiseBlock);
    nb->setContentsMargins(0, 0, 0, 0);
    nb->setSpacing(4);
    rb->addWidget(denoiseBlock);

    /* Divider below the "Render using" row, matching the rules the property tree draws
       between the Basic / Color / Effects groups: a centred 1px line, inset 6px, in
       G::backgroundShade + 20 (see DevelopProperties::addDivider), with an extra 3px of
       breathing room above it. It lives in denoiseBlock so it hides with the denoise
       group on the Apple engine rather than dangling under the last visible row. */
    QWidget *demDivider = new QWidget(denoiseBlock);
    demDivider->setFixedHeight(8);
    QVBoxLayout *ddl = new QVBoxLayout(demDivider);
    ddl->setContentsMargins(6, 5, 6, 2);
    ddl->setSpacing(0);
    QFrame *demRule = new QFrame(demDivider);
    demRule->setFixedHeight(1);
    const int divShade = G::backgroundShade + 20;
    demRule->setStyleSheet(QString("background: %1; border: none;")
                               .arg(QColor(divShade, divShade, divShade).name()));
    ddl->addWidget(demRule);
    nb->addWidget(demDivider);

    /* Group caption for the raw denoise controls that follow. */
    QWidget *nrRow = new QWidget(denoiseBlock);
    QHBoxLayout *nrl = new QHBoxLayout(nrRow);
    nrl->setContentsMargins(10, 0, 10, 0);
    nrl->setSpacing(0);
    QLabel *nrLbl = new QLabel(tr("Noise Reduction:"), nrRow);
    nrLbl->setStyleSheet(capCss);
    nrl->addWidget(nrLbl);
    nrl->addStretch(1);
    nb->addWidget(nrRow);

    /* Run row: "Denoise"/"Denoised" + "Auto run". */
    QWidget *runRow = new QWidget(denoiseBlock);
    QHBoxLayout *nl = new QHBoxLayout(runRow);
    nl->setContentsMargins(10, 0, 10, 0);
    nl->setSpacing(6);
    denoiseCheck = new QCheckBox(tr("Denoise"), runRow);
    denoiseCheck->setToolTip("Run the raw denoise; shows \"Denoised\" when complete.");
    connect(denoiseCheck, &QCheckBox::toggled, this,
            [this](bool on){ emit denoiseRunToggled(on); });
    autoRunCheck = new QCheckBox(tr("Auto run"), runRow);
    autoRunCheck->setToolTip("On: denoise runs automatically. Off: use the Denoise box.");
    connect(autoRunCheck, &QCheckBox::toggled, this,
            [this](bool on){ emit autoRunToggled(on); });
    nl->addWidget(denoiseCheck);
    nl->addStretch(1);
    nl->addWidget(autoRunCheck);
    nb->addWidget(runRow);

    /* Lum / Color amount sliders (0..100), rendered by an embedded PanelEditor so they
       look/align exactly like the property tree. Enabled only once a denoised base is
       ready (setDenoiseRunState). */
    denoiseEditor = new PanelEditor(denoiseBlock);
    denoiseEditor->addSlider("denoiseLuma", "Lum",
                             "Raw luminance noise reduction (AI, whole image).", 0, 100);
    denoiseEditor->addSlider("denoiseChroma", "Color",
                             "Raw colour (chroma) noise reduction.", 0, 100);
    denoiseEditor->fitHeight();
    connect(denoiseEditor, &PanelEditor::settingChanged, this,
            [this](const QString &key, const QVariant &v){
                if      (key == "denoiseLuma")   emit denoiseLumaChanged(v.toInt());
                else if (key == "denoiseChroma") emit denoiseChromaChanged(v.toInt());
            });
    nb->addWidget(denoiseEditor);

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
    const int a = G::backgroundShade + 5;
    const int b = G::backgroundShade - 15;
    const QRect r = headerBand->geometry();
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, QColor(a, a, a));
    g.setColorAt(1, QColor(b, b, b));
    p.fillRect(r, g);
    /* Separator rule across the bottom edge (space reserved by the layout margin). */
    p.fillRect(0, height() - G::panelBorderHeight, width(), G::panelBorderHeight,
               G::tabWidgetBorderColor);
}

void RawPanel::setEditSource(bool raw)
{
    if (rawRadio)  { QSignalBlocker b(rawRadio);  rawRadio->setChecked(raw); }
    if (prevRadio) { QSignalBlocker b(prevRadio); prevRadio->setChecked(!raw); }
    /* Demosaic + Denoise apply to the raw decode only; hidden on Embedded Preview. */
    if (rawBlock) rawBlock->setVisible(raw);
}

void RawPanel::setEngine(bool apple)
{
    if (engineCombo) {
        QSignalBlocker b(engineCombo);
        engineCombo->setCurrentIndex(apple ? 0 : 1);
    }
    /* PMRID denoise needs the CFA mosaic -> only meaningful on the Winnow engine. */
    if (denoiseBlock) denoiseBlock->setVisible(!apple);
}

void RawPanel::setDenoiseRunState(bool denoised)
{
    if (denoiseCheck) {
        QSignalBlocker b(denoiseCheck);
        denoiseCheck->setChecked(denoised);
        denoiseCheck->setText(denoised ? tr("Denoised") : tr("Denoise"));
    }
    /* The amount sliders only scale a computed PMRID base -> inert until it exists, and
       stay inert when the denoise cannot run at all (that base is never coming). */
    if (denoiseEditor) {
        const bool on = denoised && denoiseAvailable;
        denoiseEditor->setRowEnabled("denoiseLuma", on);
        denoiseEditor->setRowEnabled("denoiseChroma", on);
    }
}

void RawPanel::setDenoiseAvailable(bool available, const QString &reason)
{
    denoiseAvailable = available;
    if (denoiseCheck) {
        QSignalBlocker b(denoiseCheck);           // unticking must not emit a "clear" run
        if (!available) {
            denoiseCheck->setChecked(false);
            denoiseCheck->setText(tr("Denoise"));
        }
        denoiseCheck->setEnabled(available);
    }
    if (autoRunCheck) autoRunCheck->setEnabled(available);
    if (!available && denoiseEditor) {
        denoiseEditor->setRowEnabled("denoiseLuma", false);
        denoiseEditor->setRowEnabled("denoiseChroma", false);
    }
    if (denoiseNote) {
        denoiseNote->setText(reason);
        denoiseNote->setVisible(!available && !reason.isEmpty());
    }
}

void RawPanel::setAutoRun(bool on)
{
    if (!autoRunCheck) return;
    QSignalBlocker b(autoRunCheck);
    autoRunCheck->setChecked(on);
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
