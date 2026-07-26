#include "Develop/Properties/rawpanel.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
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
    RawPanel (see rawpanel.h): the raw-decode strip lifted out of the Base "Core" tree
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
    const QString capCss = QString("color: %1; font-size: %2pt; background: transparent;")
                               .arg(G::textColor.name()).arg(G::strFontSize.toInt());

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    /* Header band (collapse arrow + "Raw" + [?]); transparent so paintEvent draws the
       gradient behind. The arrow sits in a 10px gutter and the caption lands at x=10,
       matching the tree's section headers (same idiom as the old LayerHeader). */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(0, 3, 6, 3);
    hb->setSpacing(0);
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show the raw decode controls");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(10, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0; background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleCollapsed(); });
    titleLabel = new QLabel(tr("Raw"), headerBand);
    titleLabel->setCursor(Qt::PointingHandCursor);
    titleLabel->installEventFilter(this);        // caption click toggles collapse
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2pt; background: transparent;")
                                  .arg(G::header2Color.name()).arg(G::strFontSize.toInt()));
    tipBtn = new BarBtn();
    tipBtn->setToolTip("About the raw decode controls");
    tipBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    connect(tipBtn, &BarBtn::clicked, this, [this]{ emit tipsRequested(); });
    hb->addWidget(collapseBtn);
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(tipBtn);
    outer->addWidget(headerBand);

    /* Body container (normal background); hidden when collapsed. */
    body = new QWidget(this);
    QVBoxLayout *bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 4, 10, 6);
    bl->setSpacing(4);
    outer->addWidget(body);

    /* Edit source: Raw / Embedded Preview (always shown while the panel is visible). */
    QWidget *editRow = new QWidget(body);
    QHBoxLayout *el = new QHBoxLayout(editRow);
    el->setContentsMargins(0, 0, 0, 0);
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
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(6);
    QLabel *demLbl = new QLabel(tr("Demosaic"), demRow);
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

    /* Run row: "Denoise"/"Denoised" + "Auto run". */
    QWidget *runRow = new QWidget(denoiseBlock);
    QHBoxLayout *nl = new QHBoxLayout(runRow);
    nl->setContentsMargins(0, 0, 0, 0);
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

    /* Lum / Color amount sliders (0..100). Enabled only once a denoised base is ready. */
    auto addSliderRow = [&](const QString &cap, QSlider *&slider, QLabel *&val,
                            void (RawPanel::*sig)(int)) {
        QWidget *row = new QWidget(denoiseBlock);
        QHBoxLayout *sl = new QHBoxLayout(row);
        sl->setContentsMargins(12, 0, 0, 0);
        sl->setSpacing(6);
        QLabel *lbl = new QLabel(cap, row);
        lbl->setStyleSheet(capCss);
        lbl->setMinimumWidth(44);
        slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(0, 100);
        val = new QLabel("0", row);
        val->setStyleSheet(capCss);
        val->setMinimumWidth(26);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(slider, &QSlider::valueChanged, this, [this, val, sig](int v){
            val->setText(QString::number(v));
            (this->*sig)(v);
        });
        sl->addWidget(lbl);
        sl->addWidget(slider, 1);
        sl->addWidget(val);
        nb->addWidget(row);
    };
    addSliderRow(tr("Lum"),   lumaSlider,   lumaValue,   &RawPanel::denoiseLumaChanged);
    addSliderRow(tr("Color"), chromaSlider, chromaValue, &RawPanel::denoiseChromaChanged);

    body->setVisible(!collapsed);        // honour the persisted collapse state
    updateCollapseIcon();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void RawPanel::toggleCollapsed()
{
    collapsed = !collapsed;
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
    if (watched == titleLabel && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && titleLabel->rect().contains(me->pos())) {
            toggleCollapsed();
            return true;
        }
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
    /* The amount sliders only scale a computed PMRID base -> inert until it exists. */
    if (lumaSlider)   lumaSlider->setEnabled(denoised);
    if (chromaSlider) chromaSlider->setEnabled(denoised);
    if (lumaValue)    lumaValue->setEnabled(denoised);
    if (chromaValue)  chromaValue->setEnabled(denoised);
}

void RawPanel::setAutoRun(bool on)
{
    if (!autoRunCheck) return;
    QSignalBlocker b(autoRunCheck);
    autoRunCheck->setChecked(on);
}

void RawPanel::setDenoiseValues(int luma, int chroma)
{
    if (lumaSlider) {
        QSignalBlocker b(lumaSlider);
        lumaSlider->setValue(luma);
        if (lumaValue) lumaValue->setText(QString::number(luma));
    }
    if (chromaSlider) {
        QSignalBlocker b(chromaSlider);
        chromaSlider->setValue(chroma);
        if (chromaValue) chromaValue->setText(QString::number(chroma));
    }
}
