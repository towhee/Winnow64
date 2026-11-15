#include "Develop/Replace/replacepanel.h"
#include "Develop/fillspot.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"
#include "PropertyEditor/propertywidgets.h"   // Slider
#include "Utilities/gradientheader.h"

#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <cmath>

ReplacePanel::ReplacePanel(QWidget *parent)
    : QWidget(parent)
{
    if (G::isLogger) G::log("ReplacePanel::ReplacePanel");
    buildUi();
}

QString ReplacePanel::modeName(int mode)
{
    switch (mode) {
    case FillSpotGeom::Heal:   return tr("Heal");
    case FillSpotGeom::Clone:  return tr("Clone");
    case FillSpotGeom::Fill:   return tr("Fill");
    case FillSpotGeom::Object: return tr("Inpaint");
    default:                   return tr("Spot");
    }
}

QString ReplacePanel::modeHint(int mode)
{
    switch (mode) {
    case FillSpotGeom::Heal:
        return tr("Brush over a flaw: a source area's texture is blended into the "
                  "surrounding tone. Drag the source pin to choose another source.");
    case FillSpotGeom::Clone:
        return tr("Brush over an area to cover it with an exact copy of a source area. "
                  "Drag the source pin to choose another source.");
    case FillSpotGeom::Fill:
        return tr("Paint over something thin or scattered -- a wire, a twig, a leash -- "
                  "to fill it from its surroundings.");
    case FillSpotGeom::Object:
        return tr("Paint over an object, with a little margin, to remove it with a "
                  "regenerative (AI) fill.");
    default:
        return tr("Click a blemish: it is healed from a nearby source picked for you.");
    }
}

QString ReplacePanel::panelTitle(int mode)
{
    switch (mode) {
    case FillSpotGeom::Fill:   return tr("Fill");
    case FillSpotGeom::Object: return tr("Inpaint");
    default:                   return tr("Replace");
    }
}

/* The Size slider's log ramp: kSizeSteps positions from kSizeMin to kSizeMax % of the
   long edge. Log, because a spot is usually 0.2 - 3 % and a linear 0..100 track would
   squeeze that into its first few pixels. ImageView clamps the low end further
   (spotSizeMin, ~3 px). */
static constexpr double kSizeMin   = 0.02;
static constexpr double kSizeMax   = 100.0;
static constexpr int    kSizeSteps = 1000;

double ReplacePanel::sizeFromPos(int pos)
{
    const double t = double(pos) / kSizeSteps;
    return std::exp(std::log(kSizeMin) + t * (std::log(kSizeMax) - std::log(kSizeMin)));
}

int ReplacePanel::posFromSize(double size)
{
    const double s = qBound(kSizeMin, size, kSizeMax);
    const double t = (std::log(s) - std::log(kSizeMin))
                   / (std::log(kSizeMax) - std::log(kSizeMin));
    return qRound(t * kSizeSteps);
}

QString ReplacePanel::paintAreaKeysHint()
{
    return tr("Make as many strokes as you need, then press Return or Enter to execute, "
              "or Esc to cancel.");
}

void ReplacePanel::buildUi()
{
    /* -------- Header: gradient band, panelTitle left, [?] [E] [X] trailing -------- */
    header = new GradientHeader(panelTitle(FillSpotGeom::Spot), this);
    QHBoxLayout *headerRow = qobject_cast<QHBoxLayout *>(header->layout());

    tipBtn = new BarBtn();
    tipBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    tipBtn->setToolTip(tr("Replace tools help"));
    connect(tipBtn, &BarBtn::clicked, this, &ReplacePanel::tipsRequested);

    /* The eye shows/hides every repair on the image (non-destructive, the Replace menu's
       Show repairs). */
    previewBtn = new BarBtn();
    connect(previewBtn, &BarBtn::clicked, this, [this]{
        previewShown = !previewShown;
        updatePreviewButton();
        emit previewToggled(previewShown);
    });
    updatePreviewButton();

    /* Turn the Replace tool off -- which hides this panel. Last, at the extreme right. */
    closeBtn = new BarBtn();
    closeBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    closeBtn->setToolTip(tr("Turn off the Replace tool  (Esc on the image)"));
    connect(closeBtn, &BarBtn::clicked, this, &ReplacePanel::closeRequested);

    if (headerRow) {
        headerRow->setSpacing(6);
        headerRow->addWidget(tipBtn);
        headerRow->addWidget(previewBtn);
        headerRow->addWidget(closeBtn);
    }

    /* -------- Body: the tool's name and how to use it -------- */
    modeLbl = new QLabel(this);
    modeLbl->setStyleSheet(G::labelCss(G::header2Color));
    QFont mf = modeLbl->font();
    mf.setBold(true);
    modeLbl->setFont(mf);

    hintLbl = new QLabel(this);
    hintLbl->setStyleSheet(G::labelCss(G::textColor));
    hintLbl->setWordWrap(true);

    /* -------- Size: every tool's brush (ImageView::spotBrushSize) -------- */
    QWidget *sizeRow = new QWidget(this);
    sizeRow->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *sr = new QHBoxLayout(sizeRow);
    sr->setContentsMargins(0, 4, 0, 2);
    sr->setSpacing(8);
    QLabel *sizeLbl = new QLabel(tr("Size"), sizeRow);
    sizeLbl->setStyleSheet(G::labelCss(G::textColor));
    /* Slider (PropertyEditor): click jumps the handle, arrows nudge, and a vertical
       trackpad swipe scrolls the dock rather than moving it -- as the Develop sliders. */
    sizeSlider = new Slider(Qt::Horizontal, 1, sizeRow);
    sizeSlider->setRange(0, kSizeSteps);
    sizeSlider->setSingleStep(5);
    sizeSlider->setPageStep(50);
    sizeSlider->setToolTip(tr("Brush size. The mouse wheel and [ ] on the image change "
                              "it too."));
    sizeSlider->setStyleSheet(
        "QSlider { background: transparent; }"
        "QSlider::groove:horizontal { background: " + G::disabledColor.name() + ";"
            " border: none; height: 3px; }"
        "QSlider::handle:horizontal { background: gray; width: 14px; margin: -6px 0;"
            " height: 8px; border-radius: 7px; }"
        "QSlider::handle:focus, QSlider::handle:hover { background: "
            + G::appleBlue.name() + "; }"
        "QSlider::groove:horizontal:disabled { background: #4b4b4b; }"
        "QSlider::handle:horizontal:disabled { background: #5a5a5a; }");
    connect(sizeSlider, &QSlider::valueChanged, this, [this](int pos) {
        brushSize = sizeFromPos(pos);
        updateSizeLabel();
        emit brushSizeChanged(brushSize);
    });
    sizeValLbl = new QLabel(sizeRow);
    sizeValLbl->setStyleSheet(G::labelCss(G::textColor));
    sizeValLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeValLbl->setMinimumWidth(sizeValLbl->fontMetrics().horizontalAdvance("100.00 %"));
    sr->addWidget(sizeLbl);
    sr->addWidget(sizeSlider, 1);
    sr->addWidget(sizeValLbl);
    setBrushSize(brushSize);

    /* -------- Fill / Object: the painted area waits for Return/Enter or Esc -------- */
    paintKeys = new QWidget(this);
    paintKeys->setAttribute(Qt::WA_TranslucentBackground);
    QGridLayout *pk = new QGridLayout(paintKeys);
    pk->setContentsMargins(0, 6, 0, 0);
    pk->setHorizontalSpacing(12);
    pk->setVerticalSpacing(4);
    pk->setColumnStretch(1, 1);
    QLabel *strokesLbl = new QLabel(tr("Make as many strokes as you need. Hold Opt/Alt "
                                       "while painting to erase part of the area."),
                                    paintKeys);
    strokesLbl->setStyleSheet(G::labelCss(G::textColor));
    strokesLbl->setWordWrap(true);
    pk->addWidget(strokesLbl, 0, 0, 1, 2);
    auto addKeyRow = [&](int row, const QString &key, const QString &what) {
        QLabel *k = new QLabel(key, paintKeys);
        k->setStyleSheet(G::labelCss(G::header2Color));
        QFont kf = k->font();
        kf.setBold(true);
        k->setFont(kf);
        QLabel *w = new QLabel(what, paintKeys);
        w->setStyleSheet(G::labelCss(G::textColor));
        w->setWordWrap(true);
        pk->addWidget(k, row, 0, Qt::AlignTop);
        pk->addWidget(w, row, 1, Qt::AlignTop);
    };
    addKeyRow(1, tr("Return or Enter"), tr("execute: replace the painted area"));
    addKeyRow(2, tr("Esc"), tr("cancel: clear the painting (a second Esc turns the "
                               "tool off)"));

    /* ---- Spot tool only: copy the image's spots, paste them on the selection ---- */
    spotTools = new QWidget(this);
    spotTools->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *sl = new QVBoxLayout(spotTools);
    sl->setContentsMargins(0, 6, 0, 0);
    sl->setSpacing(6);

    countLbl = new QLabel(spotTools);
    countLbl->setStyleSheet(G::labelCss(G::textColor));
    countLbl->setWordWrap(true);
    sl->addWidget(countLbl);

    /* Transparent on the panel's lifted background, keeping the app's border and
       padding; the hover fill is restated because a widget sheet outranks the app's
       QPushButton:hover. min-width: 0 defeats the global 100px button minimum, which Qt
       applies as an EXPLICIT minimum that would floor the dock's width
       (widgetcss.cpp). */
    const QString btnCss =
        "QPushButton { background: transparent; min-width: 0; padding: 2px 12px; }"
        "QPushButton:hover { background: " + G::groupSeparatorColor.name() + "; }";
    copyBtn = new QPushButton(tr("Copy all spots"), spotTools);
    copyBtn->setStyleSheet(btnCss);
    copyBtn->setToolTip(tr("Copy every spot on this image, ready to paste onto other "
                           "images -- sensor dust sits in the same place on every "
                           "frame."));
    connect(copyBtn, &QPushButton::clicked, this, &ReplacePanel::copySpotsRequested);

    pasteBtn = new QPushButton(tr("Paste all spots"), spotTools);
    pasteBtn->setStyleSheet(btnCss);
    pasteBtn->setToolTip(tr("Add the copied spots to every selected image. A spot an "
                            "image already has is not added twice."));
    connect(pasteBtn, &QPushButton::clicked, this, &ReplacePanel::pasteSpotsRequested);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(8);
    btnRow->addWidget(copyBtn);
    btnRow->addWidget(pasteBtn);
    btnRow->addStretch(1);
    sl->addLayout(btnRow);

    copiedLbl = new QLabel(spotTools);
    copiedLbl->setStyleSheet(G::labelCss(G::disabledColor));
    copiedLbl->setWordWrap(true);
    sl->addWidget(copiedLbl);

    /* -------- Assemble -------- */
    /* The bottom margin reserves the panel separator rule drawn in paintEvent. */
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, G::panelBorderHeight);
    lay->setSpacing(4);
    lay->addWidget(header);
    QVBoxLayout *body = new QVBoxLayout;
    body->setContentsMargins(10, 4, 10, 8);
    body->setSpacing(4);
    body->addWidget(modeLbl);
    body->addWidget(hintLbl);
    body->addWidget(sizeRow);
    body->addWidget(paintKeys);
    body->addWidget(spotTools);
    lay->addLayout(body);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMode(FillSpotGeom::Spot);
}

void ReplacePanel::setMode(int mode)
{
    currentMode = mode;
    const bool paintArea = FillSpotGeom::isPaintArea(mode);
    header->setText(panelTitle(mode));
    /* Replace hosts three tools, so it names the one in use; Fill and Inpaint are
       one tool each and the header already names it. */
    modeLbl->setText(modeName(mode));
    modeLbl->setVisible(!paintArea);
    hintLbl->setText(modeHint(mode));
    paintKeys->setVisible(paintArea);
    spotTools->setVisible(mode == FillSpotGeom::Spot);
    updateSpotTools();
}

void ReplacePanel::setSpotCounts(int onImage, int copied)
{
    spotsOnImage = onImage;
    spotsCopied = copied;
    updateSpotTools();
}

void ReplacePanel::updateSpotTools()
{
    auto spots = [](int n) {
        return n == 1 ? tr("1 spot") : tr("%1 spots").arg(n);
    };
    /* Copy is greyed with its reason on the line above it, rather than left live to
       report "nothing to copy" after the click. */
    countLbl->setText(spotsOnImage > 0
                      ? tr("%1 on this image").arg(spots(spotsOnImage))
                      : tr("No spots on this image to copy yet."));
    countLbl->setStyleSheet(G::labelCss(spotsOnImage > 0 ? G::textColor
                                                          : G::disabledColor));
    copyBtn->setEnabled(spotsOnImage > 0);
    /* Paste appears only once something has been copied this session. */
    pasteBtn->setVisible(spotsCopied > 0);
    copiedLbl->setVisible(spotsCopied > 0);
    copiedLbl->setText(tr("%1 copied. Paste adds them to every selected image.")
                       .arg(spots(spotsCopied)));
}

void ReplacePanel::setBrushSize(double size)
{
    brushSize = size;
    {
        const QSignalBlocker block(sizeSlider);
        sizeSlider->setValue(posFromSize(size));
    }
    updateSizeLabel();
}

void ReplacePanel::updateSizeLabel()
{
    /* Two decimals below 1 %, where the spots live; one below 10; whole above. */
    const int dec = brushSize < 1.0 ? 2 : brushSize < 10.0 ? 1 : 0;
    sizeValLbl->setText(QString::number(brushSize, 'f', dec) + " %");
    sizeValLbl->setToolTip(tr("Brush diameter as a percentage of the image's long "
                              "edge"));
}

void ReplacePanel::setPreviewShown(bool shown)
{
    previewShown = shown;
    updatePreviewButton();
}

void ReplacePanel::updatePreviewButton()
{
    if (!previewBtn) return;
    previewBtn->setIcon(previewShown ? ":/images/icon16/eye.png"
                                     : ":/images/icon16/eye_off.png", G::iconOpacity);
    previewBtn->setToolTip(previewShown
        ? tr("Showing the repairs (click to see the photo without them)")
        : tr("Repairs hidden (click to show them)"));
}

void ReplacePanel::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    /* Subpanel content background. The GradientHeader is a CHILD, and children paint
       after their parent, so its band covers this fill with the header gradient. */
    p.fillRect(rect(), G::panelContentBg);
    /* Separator rule across the bottom edge (space reserved by the layout margin). */
    p.fillRect(0, height() - G::panelBorderHeight, width(), G::panelBorderHeight,
               G::panelSeparatorColor);
}
