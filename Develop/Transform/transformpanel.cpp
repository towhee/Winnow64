#include "Develop/Transform/transformpanel.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QToolButton>
#include <QCheckBox>
#include <QSettings>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QKeyEvent>

namespace {
/* Padlock glyphs for the aspect-lock toggle (closed = ratio locked). Kept as UTF-8 so the
   button needs no icon asset; both render on macOS and Windows. */
const QString kLockClosed = QString::fromUtf8("\xF0\x9F\x94\x92");   // U+1F512
const QString kLockOpen   = QString::fromUtf8("\xF0\x9F\x94\x93");   // U+1F513

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
    /* Row 1: section caption on the left, the [?] tips button pinned top-right. */
    QLabel *title = new QLabel(tr("Transform"), this);
    title->setStyleSheet("font-weight: bold;");

    tipBtn = new BarBtn();
    tipBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    tipBtn->setToolTip(tr("Crop and transform tips"));
    connect(tipBtn, &BarBtn::clicked, this, &TransformPanel::tipsRequested);

    QHBoxLayout *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    titleRow->addWidget(tipBtn);

    /* Row 2: aspect ratio combo + the lock toggle. */
    QLabel *aspectLbl = new QLabel(tr("Aspect"), this);
    aspectCombo = new QComboBox(this);
    aspectCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    populateAspectCombo();
    connect(aspectCombo, QOverload<int>::of(&QComboBox::activated),
            this, &TransformPanel::onAspectActivated);

    /* The lock toggle. The "A" key also drives it (handled in eventFilter, not a QAction, to avoid
       an ambiguous overload with the window-level "A"). */
    lockBtn = new QToolButton(this);
    lockBtn->setCheckable(true);
    connect(lockBtn, &QToolButton::clicked, this, &TransformPanel::toggleAspectLock);

    QHBoxLayout *aspectRow = new QHBoxLayout;
    aspectRow->setContentsMargins(0, 0, 0, 0);
    aspectRow->addWidget(aspectLbl);
    aspectRow->addWidget(aspectCombo, 1);
    aspectRow->addWidget(lockBtn);

    /* Row 3: the three tools. Crop is a toggle (overlay on/off); Straighten and Rectify are
       one-shot actions. Rectify is enabled once a polygon crop has been drawn (Alt-drag); for now
       it stays available and simply emits its signal. */
    straightenBtn = new QToolButton(this);
    straightenBtn->setText(tr("Level"));
    straightenBtn->setToolTip(tr("Draw a line on the image to straighten it"));
    connect(straightenBtn, &QToolButton::clicked, this, &TransformPanel::straightenToolRequested);

    cropBtn = new QToolButton(this);
    cropBtn->setText(tr("Crop"));
    cropBtn->setCheckable(true);
    cropBtn->setToolTip(tr("Drag a crop rectangle. Hold Alt/Option to drag a 4-point polygon for "
                           "perspective correction."));
    connect(cropBtn, &QToolButton::toggled, this, &TransformPanel::cropToolToggled);

    rectifyBtn = new QToolButton(this);
    rectifyBtn->setText(tr("Rectify"));
    rectifyBtn->setToolTip(tr("Warp the image so the 4-point polygon becomes square, then fit the "
                              "largest crop that lies inside the corrected canvas"));
    connect(rectifyBtn, &QToolButton::clicked, this, &TransformPanel::rectifyRequested);

    QHBoxLayout *toolRow = new QHBoxLayout;
    toolRow->setContentsMargins(0, 0, 0, 0);
    toolRow->addWidget(straightenBtn);
    toolRow->addWidget(cropBtn);
    toolRow->addWidget(rectifyBtn);
    toolRow->addStretch(1);

    /* Row 4: fill extra canvas (content-aware edge-extend). */
    fillCanvasChk = new QCheckBox(tr("Fill extra canvas"), this);
    fillCanvasChk->setToolTip(tr("Content-aware fill of empty corners left by a straighten or "
                                 "perspective correction"));
    if (setting)
        fillCanvasChk->setChecked(setting->value("Develop/Transform/fillCanvas", false).toBool());
    connect(fillCanvasChk, &QCheckBox::toggled, this, [this](bool on){
        if (setting) setting->setValue("Develop/Transform/fillCanvas", on);
        emit fillCanvasToggled(on);
    });

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(4);
    lay->addLayout(titleRow);
    lay->addLayout(aspectRow);
    lay->addLayout(toolRow);
    lay->addWidget(fillCanvasChk);

    /* Restore the persisted lock state and aspect selection. */
    aspectLocked = setting && setting->value("Develop/Transform/aspectLocked", false).toBool();
    updateLockButton();
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

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    /* Let "A" toggle the lock whenever the panel or one of its non-text controls has focus. The
       aspect combo is intentionally excluded so its type-ahead keeps working. */
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
    QWidget *filtered[] = {lockBtn, straightenBtn, cropBtn, rectifyBtn, fillCanvasChk};
    for (QWidget *w : filtered)
        w->installEventFilter(this);
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

void TransformPanel::updateLockButton()
{
    if (!lockBtn) return;
    const QSignalBlocker block(lockBtn);
    lockBtn->setChecked(aspectLocked);
    lockBtn->setText(aspectLocked ? kLockClosed : kLockOpen);
    lockBtn->setToolTip(aspectLocked ? tr("Aspect ratio locked (A)")
                                     : tr("Aspect ratio unlocked (A)"));
}

bool TransformPanel::eventFilter(QObject *watched, QEvent *event)
{
    /* Claim a bare "A" before the window-level Run-Droplet shortcut can: accept its
       ShortcutOverride so the following KeyPress is delivered normally, then toggle on it. */
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_A && ke->modifiers() == Qt::NoModifier) {
            if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
            toggleAspectLock();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QString TransformPanel::aspectKey() const
{
    return aspectCombo ? aspectCombo->currentData(KeyRole).toString() : QString("asShot");
}

double TransformPanel::aspectRatio() const
{
    return aspectCombo ? aspectCombo->currentData(RatioRole).toDouble() : 0.0;
}

bool TransformPanel::isFillCanvas() const
{
    return fillCanvasChk && fillCanvasChk->isChecked();
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
