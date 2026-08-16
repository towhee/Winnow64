#include "Develop/Transform/transformpanel.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QToolButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QSettings>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QLinearGradient>
#include <QInputDialog>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QKeyEvent>

namespace {
/* Padlock icon for the aspect-lock toggle, painted rather than loaded, so it needs no
   asset, stays crisp on retina and can be tinted per state. The emoji glyphs it replaces
   were too small and too low-contrast to read at a glance. Drawn in a nominal 16x16 box:
   filled body, stroked shackle, punched-out keyhole; the open shackle is hinged on the
   left leg and swung up to the right. */
QIcon padlockIcon(bool closed, const QColor &color)
{
    const int px = 16;
    const qreal dpr = 2.0;              // paint at 2x and tag it, so it downsamples
    QImage img(QSize(px * dpr, px * dpr), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    img.setDevicePixelRatio(dpr);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRoundedRect(QRectF(2.5, 7.0, 11.0, 7.5), 1.5, 1.5);

    QPainterPath shackle;
    shackle.moveTo(5.0, 7.0);
    shackle.arcTo(QRectF(5.0, 3.0, 6.0, 6.0), 180.0, -180.0);
    shackle.lineTo(11.0, closed ? 7.0 : 6.0);
    if (!closed) {
        /* Swing the whole shackle open about its left leg: the sprung-up tilt is what
           makes locked and unlocked tell apart at 16px. */
        QTransform t;
        t.translate(5.0, 7.0);
        t.rotate(-32.0);
        t.translate(-5.0, -7.0);
        shackle = t.map(shackle);
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(shackle);

    /* Keyhole: cleared out of the body so it reads at 16px on any background. */
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.setPen(QPen(Qt::black, 1.8, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(8.0, 9.5), QPointF(8.0, 12.0));
    p.end();

    return QIcon(QPixmap::fromImage(img));
}

/* Parse a free-typed custom aspect ("21 x 9", "21:9", "21/9", "1.85x1") into w and h.
   Returns false if it is not two positive numbers separated by x / : or /. */
bool parseAspect(const QString &text, double &w, double &h)
{
    static const QRegularExpression re(
        R"(^\s*([0-9]*\.?[0-9]+)\s*[x:/X]\s*([0-9]*\.?[0-9]+)\s*$)");
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) return false;
    w = m.captured(1).toDouble();
    h = m.captured(2).toDouble();
    return w > 0.0 && h > 0.0;
}

/* A section-header band that paints the same top-to-bottom gradient as the property-tree
   headers (see PropertyDelegate::paint: backgroundShade +5 -> -15). Plain QWidget subclass with no
   _Q_OBJECT (needs no moc) -- it only overrides paintEvent. */
class GradientHeader : public QWidget
{
public:
    explicit GradientHeader(QWidget *parent = nullptr) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        const int a = G::backgroundShade + 5;
        const int b = G::backgroundShade - 15;
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, QColor(a, a, a));
        g.setColorAt(1, QColor(b, b, b));
        p.fillRect(rect(), g);
    }
};
}

TransformPanel::TransformPanel(QWidget *parent, QSettings *settings)
    : QWidget(parent), setting(settings)
{
    if (G::isLogger) G::log("TransformPanel::TransformPanel");
    loadCustomAspects();
    buildUi();
}

void TransformPanel::buildUi()
{
    /* -------- Header: gradient band, caption left, [?] [E] [R] [X] trailing -------- */
    GradientHeader *header = new GradientHeader(this);

    QLabel *title = new QLabel(tr("Transform"), header);
    title->setStyleSheet(G::labelCss(G::header2Color));
    QFont hf = title->font();
    hf.setPointSize(G::strFontSize.toInt());
    title->setFont(hf);

    tipBtn = new BarBtn();
    tipBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    tipBtn->setToolTip(tr("Crop and transform tips"));
    connect(tipBtn, &BarBtn::clicked, this, &TransformPanel::tipsRequested);

    /* The eye shows/ignores the whole transform (non-destructive). */
    previewBtn = new BarBtn();
    connect(previewBtn, &BarBtn::clicked, this, [this]{
        previewShown = !previewShown;
        updatePreviewButton();
        emit previewToggled(previewShown);
    });
    updatePreviewButton();

    headerResetBtn = new BarBtn();
    headerResetBtn->setIcon(":/images/icon16/reset.png", G::iconOpacity);
    headerResetBtn->setToolTip(tr("Reset crop, straighten and perspective to the full frame"));
    connect(headerResetBtn, &BarBtn::clicked, this, &TransformPanel::resetRequested);

    /* Close the panel (same as the action-row Transform button / "R"): commits the crop
       session and hides the panel. Kept last so it sits at the extreme right. */
    closeBtn = new BarBtn();
    closeBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    closeBtn->setToolTip(tr("Close the Transform panel (R)"));
    connect(closeBtn, &BarBtn::clicked, this, &TransformPanel::closeRequested);

    QHBoxLayout *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(6, 3, 6, 3);
    headerRow->setSpacing(6);
    headerRow->addWidget(title);
    headerRow->addStretch(1);
    headerRow->addWidget(tipBtn);
    headerRow->addWidget(previewBtn);
    headerRow->addWidget(headerResetBtn);
    headerRow->addWidget(closeBtn);

    /* -------- Mode toggle (Crop / Level / Warp): mutually exclusive, teal-when-selected -------- */
    const int bs = G::backgroundShade;
    const QString select = G::selectionColor.name();
    const QString border  = QColor(bs + 30, bs + 30, bs + 30).name();
    const QString normalBg = QColor(bs + 8,  bs + 8,  bs + 8).name();
    const QString hoverBg  = QColor(bs + 18, bs + 18, bs + 18).name();
    /* The disabled state has to be spelled out here: a per-widget stylesheet overrides
       the app one, and ":checked" with no ":enabled" qualifier keeps painting the armed
       mode at full strength on a greyed panel. Disabled = dimmed text, and a checked
       button keeps a MUTED teal so "which mode is armed" survives greying. */
    const QString mutedSel = G::dimmed(G::selectionColor).name();
    const QString dim = G::disabledColor.name();
    const QString modeQss = QString(
        "QToolButton{border:1px solid %1; border-radius:3px; padding:2px 6px; background:%2;}"
        "QToolButton:hover:!checked{background:%3;}"
        "QToolButton:checked{background-color:%4; border:1px solid %3; color:white;}"
        "QToolButton:disabled{color:%5;}"
        "QToolButton:checked:disabled{background-color:%6; border:1px solid %1; color:%5;}")
        .arg(border, normalBg, hoverBg, select, dim, mutedSel);

    // auto makeModeBtn = [&](const QString &text, const QString &tip) {
    //     QToolButton *b = new QToolButton(this);
    //     b->setText(text);
    //     b->setToolTip(tip);
    //     b->setCheckable(true);
    //     b->setStyleSheet(modeQss);
    //     b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    //     b->setFocusPolicy(Qt::StrongFocus);   // so C/L/W reach the eventFilter after a click
    //     b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    //     return b;
    // };

    auto makeModeBtn = [&](const QString &letter, const QString &rest, const QString &tip) {
        QToolButton *b = new QToolButton(this);
        b->setToolTip(tip);
        b->setCheckable(true);
        b->setStyleSheet(modeQss);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        b->setFocusPolicy(Qt::StrongFocus);

        /* 1. A QLabel for the caption: the shortcut letter is accented (white, bold) and
              QToolButton text cannot render rich text. setAccentCaption keeps the halves
              on the label so changeEvent can re-render them dimmed when disabled. */
        QLabel *label = new QLabel(b);
        G::setAccentCaption(label, letter, rest, QColor(Qt::white));
        label->setAttribute(Qt::WA_TransparentForMouseEvents); // Clicks pass through to the button
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("background-color: transparent;");

        // 2. Put the label inside a layout inside the button
        QHBoxLayout *layout = new QHBoxLayout(b);
        layout->setContentsMargins(6, 2, 6, 2); // Match your QSS padding
        layout->addWidget(label);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        layout->setContentsMargins(6, 2, 6, 2);
        b->setLayout(layout);

        return b;
    };

    // 3. Shortcut letter + the rest of the caption; the factory accents the letter.
    cropModeBtn = makeModeBtn(tr("C"), tr("rop"),
                              tr("Crop and set an aspect ratio (C)"));
    levelModeBtn = makeModeBtn(tr("L"), tr("evel"),
                              tr("Straighten: draw a level line or type an angle (L)"));
    warpModeBtn = makeModeBtn(tr("W"), tr("arp"),
                              tr("Perspective correction: drag the corners (W)"));

    // cropModeBtn  = makeModeBtn(tr("Crop"),  tr("Crop and set an aspect ratio (C)"));
    // levelModeBtn = makeModeBtn(tr("Level"), tr("Straighten: draw a level line or type an angle (L)"));
    // warpModeBtn  = makeModeBtn(tr("Warp"),  tr("Perspective correction: drag the corners (W)"));

    modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(cropModeBtn,  CropMode);
    modeGroup->addButton(levelModeBtn, LevelMode);
    modeGroup->addButton(warpModeBtn,  WarpMode);
    connect(modeGroup, &QButtonGroup::idClicked, this, &TransformPanel::selectMode);

    /* Uniform mode-button width so the whole toggle column lines up. */
    int modeW = 0;
    for (QToolButton *b : {cropModeBtn, levelModeBtn, warpModeBtn})
        modeW = qMax(modeW, b->sizeHint().width());
    for (QToolButton *b : {cropModeBtn, levelModeBtn, warpModeBtn})
        b->setFixedWidth(modeW);

    /* -------- Crop row controls: aspect combo + aspect-lock padlock -------- */
    QLabel *aspectLbl = new QLabel(tr("Aspect"), this);
    aspectCombo = new QComboBox(this);
    aspectCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    populateAspectCombo();
    connect(aspectCombo, QOverload<int>::of(&QComboBox::activated),
            this, &TransformPanel::onAspectActivated);

    /* Chrome-free like the other bar buttons: toggling only swaps the padlock symbol
       (closed and bright vs open and dim), never a border or background. Not checkable
       for the same reason -- a checked QToolButton would draw a pressed background. */
    lockBtn = new BarBtn();
    lockBtn->setIconSize(QSize(16, 16));
    connect(lockBtn, &QToolButton::clicked, this, &TransformPanel::toggleAspectLock);

    /* Flip the aspect between landscape and portrait (swap w/h). Checkable so the
       pressed state shows the current orientation; also driven by the "F" key. */
    flipBtn = new BarBtn();
    flipBtn->setCheckable(true);
    flipBtn->setIcon(":/images/icon16/swap.png", G::iconOpacity);
    flipBtn->setToolTip(tr("Flip the aspect ratio between landscape and portrait (F)"));
    connect(flipBtn, &BarBtn::clicked, this, &TransformPanel::toggleAspectFlip);

    /* -------- Level row controls: straighten-angle field -------- */
    QLabel *angleLbl = new QLabel(tr("Angle"), this);
    angleEdit = new QLineEdit(this);
    angleEdit->setToolTip(tr("Straighten angle in degrees (-45 to 45)"));
    angleEdit->setPlaceholderText("0.0");
    QDoubleValidator *av = new QDoubleValidator(-45.0, 45.0, 2, angleEdit);
    av->setNotation(QDoubleValidator::StandardNotation);
    angleEdit->setValidator(av);
    angleEdit->setFixedWidth(60);
    connect(angleEdit, &QLineEdit::editingFinished, this, [this]{
        emit levelAngleEntered(angleEdit->text().toDouble());
    });

    /* -------- Warp row: a hint (no control yet) -------- */
    QLabel *warpHint = new QLabel(tr("Drag corners"), this);
    // warpHint->setStyleSheet(QString("color: %1;").arg(G::disabledColor.name()));

    /* -------- Per-row reset buttons (all in the last column so they align) -------- */
    auto makeRowReset = [&](int m, const QString &tip) {
        BarBtn *b = new BarBtn();
        b->setIcon(":/images/icon16/reset.png", G::iconOpacity);
        b->setToolTip(tip);
        connect(b, &BarBtn::clicked, this, [this, m]{ emit resetModeRequested(m); });
        return b;
    };
    BarBtn *cropResetBtn  = makeRowReset(CropMode,  tr("Reset the crop to the full frame"));
    BarBtn *levelResetBtn = makeRowReset(LevelMode, tr("Reset the straighten angle to 0"));
    BarBtn *warpResetBtn  = makeRowReset(WarpMode,  tr("Reset the perspective correction"));

    /* -------- Grid: col0 mode | col1 label | col2 control (stretch) | col3 lock | col4 reset ---- */
    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(2, 1);

    /* Lock + flip share the padlock column so the reset buttons stay aligned in col4. */
    QHBoxLayout *lockFlipRow = new QHBoxLayout;
    lockFlipRow->setContentsMargins(0, 0, 0, 0);
    lockFlipRow->setSpacing(0);
    lockFlipRow->addWidget(lockBtn);
    lockFlipRow->addSpacing(10);
    lockFlipRow->addWidget(flipBtn);

    grid->addWidget(cropModeBtn,  0, 0);
    grid->addWidget(aspectLbl,    0, 1);
    grid->addWidget(aspectCombo,  0, 2);
    grid->addLayout(lockFlipRow,  0, 3);
    grid->addWidget(cropResetBtn, 0, 4);

    grid->addWidget(levelModeBtn, 1, 0);
    grid->addWidget(angleLbl,     1, 1);
    grid->addWidget(angleEdit,    1, 2, Qt::AlignLeft);
    grid->addWidget(levelResetBtn,1, 4);

    grid->addWidget(warpModeBtn,  2, 0);
    grid->addWidget(warpHint,     2, 1, 1, 2);
    grid->addWidget(warpResetBtn, 2, 4);

    /* -------- Assemble -------- */
    /* The bottom margin reserves the panel separator rule drawn in paintEvent. */
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, G::panelBorderHeight);
    lay->setSpacing(6);
    lay->addWidget(header);
    QVBoxLayout *bodyWrap = new QVBoxLayout;
    bodyWrap->setContentsMargins(6, 0, 6, 6);
    bodyWrap->addLayout(grid);
    lay->addLayout(bodyWrap);

    /* Restore the persisted lock state, aspect selection and mode. */
    aspectLocked = setting && setting->value("Develop/Transform/aspectLocked", false).toBool();
    updateLockButton();
    aspectFlipped = setting && setting->value("Develop/Transform/aspectFlipped", false).toBool();
    updateFlipButton();
    if (setting) {
        const QString key = setting->value("Develop/Transform/aspectKey", "asShot").toString();
        for (int i = 0; i < aspectCombo->count(); ++i) {
            if (aspectCombo->itemData(i, KeyRole).toString() == key) {
                QSignalBlocker block(aspectCombo);
                aspectCombo->setCurrentIndex(i);
                lastAspectIndex = i;
                break;
            }
        }
    }
    currentMode = setting ? setting->value("Develop/Transform/mode", CropMode).toInt() : CropMode;
    setMode(currentMode);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    /* Let A/F/C/L/W act on the focused panel (see eventFilter). The aspect combo and the
       angle field are excluded so their text entry / type-ahead still works. */
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
    QWidget *filtered[] = {cropModeBtn, levelModeBtn, warpModeBtn, lockBtn, flipBtn};
    for (QWidget *w : filtered)
        w->installEventFilter(this);
}

void TransformPanel::setMode(int mode)
{
    currentMode = mode;
    QToolButton *b = (mode == LevelMode) ? levelModeBtn
                   : (mode == WarpMode)  ? warpModeBtn
                                         : cropModeBtn;
    if (b) {
        const QSignalBlocker block(b);
        b->setChecked(true);
    }
    /* The aspect controls only make sense in Crop mode; the angle field only in Level mode. */
    if (aspectCombo) aspectCombo->setEnabled(mode == CropMode);
    if (lockBtn)     lockBtn->setEnabled(mode == CropMode);
    if (flipBtn)     flipBtn->setEnabled(mode == CropMode);
    if (angleEdit)   angleEdit->setEnabled(mode == LevelMode);
}

void TransformPanel::selectMode(int mode)
{
    if (mode == currentMode) { setMode(mode); return; }   // still refresh enabled-states
    setMode(mode);
    if (setting) setting->setValue("Develop/Transform/mode", mode);
    emit modeChanged(mode);
}

void TransformPanel::setAspectAsShot()
{
    if (!aspectCombo) return;
    /* "As shot" is the first entry, but find it by key so a reorder cannot break this. */
    int idx = 0;
    for (int i = 0; i < aspectCombo->count(); ++i) {
        if (aspectCombo->itemData(i, KeyRole).toString() == "asShot") { idx = i; break; }
    }
    const QSignalBlocker block(aspectCombo);
    aspectCombo->setCurrentIndex(idx);
    lastAspectIndex = idx;
    if (setting) setting->setValue("Develop/Transform/aspectKey", "asShot");
}

void TransformPanel::populateAspectCombo()
{
    QSignalBlocker block(aspectCombo);
    aspectCombo->clear();

    addAspectItem(tr("As shot"),  "asShot", 0.0);
    addAspectItem("1 x 1",        "1:1",    1.0);
    addAspectItem("3 x 2",        "3:2",    3.0 / 2.0);
    addAspectItem("4 x 3",        "4:3",    4.0 / 3.0);
    addAspectItem("5 x 7",        "5:7",    5.0 / 7.0);
    addAspectItem("4 x 5",        "4:5",    4.0 / 5.0);
    addAspectItem("8.5 x 11",     "8.5:11", 8.5 / 11.0);
    addAspectItem("16 x 9",       "16:9",   16.0 / 9.0);
    addAspectItem("16 x 10",      "16:10",  16.0 / 10.0);

    /* Persisted custom aspects (if any) live between the presets and the action item. */
    for (const auto &c : customAspects)
        addAspectItem(c.first, "custom:" + c.first, c.second);

    aspectCombo->insertSeparator(aspectCombo->count());

    /* The "Add custom aspect..." action item is tagged so onAspectActivated intercepts it
       instead of treating it as a ratio. */
    aspectCombo->addItem(tr("Add custom aspect..."));
    const int last = aspectCombo->count() - 1;
    aspectCombo->setItemData(last, AddCustomAction, ActionRole);
}

void TransformPanel::addAspectItem(const QString &caption, const QString &key, double ratio)
{
    aspectCombo->addItem(caption);
    const int i = aspectCombo->count() - 1;
    aspectCombo->setItemData(i, key,      KeyRole);
    aspectCombo->setItemData(i, ratio,    RatioRole);
    aspectCombo->setItemData(i, NoAction, ActionRole);
}

void TransformPanel::onAspectActivated(int index)
{
    if (aspectCombo->itemData(index, ActionRole).toInt() == AddCustomAction) {
        /* Don't leave "Add custom aspect..." selected: restore the prior choice, then prompt. */
        QSignalBlocker block(aspectCombo);
        aspectCombo->setCurrentIndex(lastAspectIndex);
        promptAddCustomAspect();
        return;
    }

    lastAspectIndex = index;
    const QString key = aspectCombo->itemData(index, KeyRole).toString();
    const double ratio = aspectCombo->itemData(index, RatioRole).toDouble();
    if (setting) setting->setValue("Develop/Transform/aspectKey", key);
    emit aspectChanged(key, ratio);
}

void TransformPanel::promptAddCustomAspect()
{
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, tr("Add custom aspect"),
        tr("Enter an aspect ratio (e.g. 21 x 9):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    double w = 0.0, h = 0.0;
    if (!parseAspect(text, w, h)) return;

    /* Normalise the caption to "W x H" using the numbers the user typed. */
    const QString caption = QString("%1 x %2")
        .arg(QString::number(w, 'g', 6), QString::number(h, 'g', 6));
    const double ratio = w / h;

    /* Skip duplicates; otherwise append and persist. */
    for (const auto &c : customAspects)
        if (qFuzzyCompare(c.second, ratio)) { return; }
    customAspects.append({caption, ratio});
    saveCustomAspects();

    populateAspectCombo();
    for (int i = 0; i < aspectCombo->count(); ++i) {
        if (aspectCombo->itemData(i, KeyRole).toString() == "custom:" + caption) {
            aspectCombo->setCurrentIndex(i);
            onAspectActivated(i);
            break;
        }
    }
}

void TransformPanel::toggleAspectLock()
{
    aspectLocked = !aspectLocked;
    updateLockButton();
    if (setting) setting->setValue("Develop/Transform/aspectLocked", aspectLocked);
    emit aspectLockToggled(aspectLocked);
}

void TransformPanel::toggleAspectFlip()
{
    aspectFlipped = !aspectFlipped;
    updateFlipButton();
    if (setting) setting->setValue("Develop/Transform/aspectFlipped", aspectFlipped);
    emit aspectFlipToggled(aspectFlipped);
}

void TransformPanel::updateFlipButton()
{
    if (!flipBtn) return;
    const QSignalBlocker block(flipBtn);
    flipBtn->setChecked(aspectFlipped);
}

void TransformPanel::updatePreviewButton()
{
    if (!previewBtn) return;
    previewBtn->setIcon(previewShown ? ":/images/icon16/eye.png"
                                     : ":/images/icon16/eye_off.png", G::iconOpacity);
    previewBtn->setToolTip(previewShown ? tr("Showing the crop result (click to keep editing)")
                                        : tr("Preview the crop result (click to apply and view)"));
}

void TransformPanel::setPreviewShown(bool shown)
{
    previewShown = shown;
    updatePreviewButton();
}

void TransformPanel::setLevelAngle(double degrees)
{
    if (!angleEdit) return;
    const QSignalBlocker block(angleEdit);
    angleEdit->setText(degrees == 0.0 ? QString() : QString::number(degrees, 'f', 2));
}

void TransformPanel::updateLockButton()
{
    if (!lockBtn) return;
    /* Closed and bright when locked, open and dim when not: the symbol alone carries
       the state (no border or background change). */
    lockBtn->setIcon(padlockIcon(aspectLocked, aspectLocked ? QColor(Qt::white)
                                                            : QColor(110, 110, 110)));
    lockBtn->setToolTip(aspectLocked
        ? tr("Aspect ratio LOCKED: dragging the crop keeps the ratio (A)")
        : tr("Aspect ratio unlocked: dragging the crop is free (A)"));
}

void TransformPanel::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    /* Separator rule across the bottom edge (space reserved by the layout margin). */
    QPainter p(this);
    p.fillRect(0, height() - G::panelBorderHeight, width(), G::panelBorderHeight,
               G::tabWidgetBorderColor);
}

void TransformPanel::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    /* The mode captions accent their shortcut letter with INLINE html, which no
       ":disabled" rule can reach, so re-render them whenever the panel is greyed or
       re-enabled (Develop greys wholesale on a video selection). */
    if (event->type() == QEvent::EnabledChange) G::restyleAccentLabels(this);
}

bool TransformPanel::eventFilter(QObject *watched, QEvent *event)
{
    /* Claim a bare A/F/C/L/W before any window-level shortcut can: accept its
       ShortcutOverride so the following KeyPress is delivered normally, then act. */
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->modifiers() == Qt::NoModifier) {
            int key = ke->key();
            const bool ours = (key == Qt::Key_A || key == Qt::Key_C ||
                               key == Qt::Key_L || key == Qt::Key_W ||
                               key == Qt::Key_F);
            if (ours) {
                if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
                handleTransformShortcut(key);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool TransformPanel::handleTransformShortcut(int key)
{
    switch (key) {
    case Qt::Key_A: toggleAspectLock();   return true;
    case Qt::Key_F: toggleAspectFlip();   return true;
    case Qt::Key_C: selectMode(CropMode); return true;
    case Qt::Key_L: selectMode(LevelMode);return true;
    case Qt::Key_W: selectMode(WarpMode); return true;
    default:        return false;
    }
}

QString TransformPanel::aspectKey() const
{
    return aspectCombo ? aspectCombo->currentData(KeyRole).toString() : QString("asShot");
}

double TransformPanel::aspectRatio() const
{
    return aspectCombo ? aspectCombo->currentData(RatioRole).toDouble() : 0.0;
}

void TransformPanel::loadCustomAspects()
{
    customAspects.clear();
    if (!setting) return;
    /* Stored as a "caption;ratio" string list so it round-trips without an array cursor. */
    const QStringList list =
        setting->value("Develop/Transform/customAspects").toStringList();
    for (const QString &s : list) {
        const int sep = s.lastIndexOf(';');
        if (sep <= 0) continue;
        const QString caption = s.left(sep);
        const double ratio = s.mid(sep + 1).toDouble();
        if (ratio > 0.0) customAspects.append({caption, ratio});
    }
}

void TransformPanel::saveCustomAspects()
{
    if (!setting) return;
    QStringList list;
    for (const auto &c : customAspects)
        list << QString("%1;%2").arg(c.first, QString::number(c.second, 'g', 10));
    setting->setValue("Develop/Transform/customAspects", list);
}
