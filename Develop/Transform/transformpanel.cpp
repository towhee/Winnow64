#include "Develop/Transform/transformpanel.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QSettings>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QLinearGradient>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QKeyEvent>

namespace {
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
   headers (see PropertyDelegate::paint: G::headerGradientTop -> Bottom). Plain QWidget subclass with no
   _Q_OBJECT (needs no moc) -- it only overrides paintEvent. */
class GradientHeader : public QWidget
{
public:
    explicit GradientHeader(QWidget *parent = nullptr) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, G::headerGradientTop);
        g.setColorAt(1, G::headerGradientBottom);
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

    /* Captions: transparent (no slab on the panel fill). The modifier hints are grey -- they
       name a key, they are not a control. */
    const QString capCss = G::labelCss(G::textColor);
    const QString hintCss = G::labelCss(G::disabledColor);
    auto makeCaption = [&](const QString &text) {
        QLabel *l = new QLabel(text, this);
        l->setStyleSheet(capCss);
        return l;
    };
    auto makeHint = [&](const QString &text) {
        QLabel *l = new QLabel(text, this);
        l->setStyleSheet(hintCss);
        return l;
    };

    /* -------- Crop group: aspect combo, lock, flip -------- */
    aspectCombo = new QComboBox(this);
    aspectCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    populateAspectCombo();
    connect(aspectCombo, QOverload<int>::of(&QComboBox::activated),
            this, &TransformPanel::onAspectActivated);

    lockCheck = new QCheckBox(this);
    lockCheck->setStyleSheet("QCheckBox { background: transparent; }");
    connect(lockCheck, &QCheckBox::toggled, this, &TransformPanel::setAspectLocked);

    /* Landscape / Portrait: the flip as an explicit choice of orientation. Styled like
       RawPanel's radios: transparent, and plain caption text when checked (the indicator
       already shows the choice; the app-wide checked colour would only add noise). */
    landscapeRadio = new QRadioButton(tr("Landscape"), this);
    portraitRadio  = new QRadioButton(tr("Portrait"),  this);
    for (QRadioButton *r : {landscapeRadio, portraitRadio})
        r->setStyleSheet("QRadioButton { background: transparent; color: "
                         + G::textColor.name() + "; }"
                         "QRadioButton:disabled { color: "
                         + G::disabledColor.name() + "; }");
    QButtonGroup *flipGroup = new QButtonGroup(this);
    flipGroup->setExclusive(true);
    flipGroup->addButton(landscapeRadio);
    flipGroup->addButton(portraitRadio);
    /* toggled fires for both halves; the Portrait button's state is the flip. */
    connect(portraitRadio, &QRadioButton::toggled, this, &TransformPanel::setAspectFlipped);

    QHBoxLayout *flipRow = new QHBoxLayout;
    flipRow->setContentsMargins(0, 0, 0, 0);
    flipRow->setSpacing(12);
    flipRow->addWidget(landscapeRadio);
    flipRow->addWidget(portraitRadio);
    flipRow->addWidget(makeHint(tr("X")));
    flipRow->addStretch(1);

    /* -------- Straighten group: angle field + the modifier that draws a level line ---- */
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

    /* The platform's own names for the keys ImageView reads as ControlModifier (Cmd on
       macOS) and AltModifier (Opt on macOS). */
#ifdef Q_OS_MAC
    const QString cmdKey = tr("Cmd"), optKey = tr("Opt");
#else
    const QString cmdKey = tr("Ctrl"), optKey = tr("Alt");
#endif
    QLabel *levelHint = makeHint(tr("%1 modifier").arg(cmdKey));
    levelHint->setToolTip(tr("Hold %1 and drag on the image along something that should be "
                             "level or plumb").arg(cmdKey));

    /* -------- Warp group: only the modifier that drags the corners -------- */
    QLabel *warpHint = makeHint(tr("%1 modifier").arg(optKey));
    warpHint->setToolTip(tr("Hold %1 and drag a corner of the crop onto something "
                            "rectangular; Return or a double-click squares it up").arg(optKey));
    QLabel *panHint = makeHint(tr("Space bar to pan"));
    panHint->setToolTip(tr("Hold Space and drag to move the picture under the crop frame"));

    /* -------- Per-group reset buttons (all in the last column so they align) -------- */
    auto makeRowReset = [&](int part, const QString &tip) {
        BarBtn *b = new BarBtn();
        b->setIcon(":/images/icon16/reset.png", G::iconOpacity);
        b->setToolTip(tip);
        connect(b, &BarBtn::clicked, this, [this, part]{ emit resetPartRequested(part); });
        return b;
    };
    BarBtn *cropResetBtn  = makeRowReset(CropPart,  tr("Reset the crop to the full frame"));
    BarBtn *levelResetBtn = makeRowReset(LevelPart, tr("Reset the straighten angle to 0"));
    BarBtn *warpResetBtn  = makeRowReset(WarpPart,  tr("Reset the perspective correction"));

    /* -------- Grid: col0 caption | col1 control | col2 tip (stretch) | col3 reset --------
       The tips (A, the modifiers, Space) share col2 so they line up down the panel; the
       aspect combo and the Flip radios span col1-2, and Flip's X tip rides after them. */
    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(2, 1);

    /* The control-group divider every Develop panel uses (see
       DevelopProperties::addGroupDivider) above the Angle and Warp groups. The slot is
       G::groupSeparatorHeight less the grid's row spacing on either side (the spacing a
       divider row in the property tree takes), plus 12px of extra room above and below
       the line, which paintEvent centres in the slot. */
    const int ruleExtra = 12;
    auto addRule = [&](int row) {
        QWidget *rule = new QWidget(this);
        rule->setAttribute(Qt::WA_TranslucentBackground);
        rule->setFixedHeight(qMax(G::groupSeparatorWidth,
                                  G::groupSeparatorHeight - 2 * grid->verticalSpacing())
                             + 2 * ruleExtra);
        rowRules.append(rule);
        grid->addWidget(rule, row, 0, 1, 4);
    };

    grid->addWidget(makeCaption(tr("Aspect")), 0, 0);
    grid->addWidget(aspectCombo,               0, 1, 1, 2);
    grid->addWidget(cropResetBtn,              0, 3);
    grid->addWidget(makeCaption(tr("Lock")),   1, 0);
    grid->addWidget(lockCheck,                 1, 1, Qt::AlignLeft);
    grid->addWidget(makeHint(tr("A")),         1, 2);
    grid->addWidget(makeCaption(tr("Flip")),   2, 0);
    grid->addLayout(flipRow,                   2, 1, 1, 2);

    addRule(3);
    grid->addWidget(makeCaption(tr("Angle")),  4, 0);
    grid->addWidget(angleEdit,                 4, 1, Qt::AlignLeft);
    grid->addWidget(levelHint,                 4, 2);
    grid->addWidget(levelResetBtn,             4, 3);

    addRule(5);
    grid->addWidget(makeCaption(tr("Warp")),   6, 0);
    grid->addWidget(warpHint,                  6, 2);
    grid->addWidget(warpResetBtn,              6, 3);
    grid->addWidget(panHint,                   7, 2);

    /* -------- Assemble -------- */
    /* The bottom margin reserves the panel separator rule drawn in paintEvent. */
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, G::panelBorderHeight);
    lay->setSpacing(6);
    lay->addWidget(header);
    QVBoxLayout *bodyWrap = new QVBoxLayout;
    bodyWrap->setContentsMargins(6, 0, 6, 6);
    bodyWrap->setSpacing(0);
    bodyWrap->addSpacing(12);       // breathing room above the Aspect row
    bodyWrap->addLayout(grid);
    bodyWrap->addSpacing(12);       // and below the Warp row
    lay->addLayout(bodyWrap);

    /* Restore the persisted lock state, flip and aspect selection. */
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

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    /* Let A/X act on the focused panel (see eventFilter). The aspect combo and the angle
       field are excluded so their text entry / type-ahead still works. */
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
    QWidget *filtered[] = {lockCheck, landscapeRadio, portraitRadio};
    for (QWidget *w : filtered)
        w->installEventFilter(this);
}

void TransformPanel::resetCropState()
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

    aspectLocked = false;
    aspectFlipped = false;
    updateLockButton();
    updateFlipButton();

    if (setting) {
        setting->setValue("Develop/Transform/aspectKey", "asShot");
        setting->setValue("Develop/Transform/aspectLocked", false);
        setting->setValue("Develop/Transform/aspectFlipped", false);
    }
}

void TransformPanel::setCustomAspect()
{
    if (!aspectCombo) return;
    if (aspectKey() != "free") selectAspectKey("free");
    if (setting) setting->setValue("Develop/Transform/aspectKey", "free");
    if (aspectFlipped) {
        aspectFlipped = false;
        updateFlipButton();
        if (setting) setting->setValue("Develop/Transform/aspectFlipped", false);
    }
}

void TransformPanel::populateAspectCombo()
{
    QSignalBlocker block(aspectCombo);
    aspectCombo->clear();

    addAspectItem(tr("As shot"),  "asShot", 0.0);
    /* The ratio the user made by dragging the frame freely (see setCustomAspect). Keyed
       "free", not "custom", so it can never collide with a "custom:" user aspect. */
    addAspectItem(tr("Custom"),   "free",   kCustomRatio);
    aspectCombo->setItemData(aspectCombo->count() - 1,
                             tr("The crop's own ratio, as dragged with the aspect unlocked"),
                             Qt::ToolTipRole);
    addAspectItem("1 x 1",        "1:1",    1.0);
    addAspectItem("3 x 2",        "3:2",    3.0 / 2.0);
    addAspectItem("4 x 3",        "4:3",    4.0 / 3.0);
    addAspectItem("5 x 7",        "5:7",    5.0 / 7.0);
    addAspectItem("4 x 5",        "4:5",    4.0 / 5.0);
    addAspectItem("8.5 x 11",     "8.5:11", 8.5 / 11.0);
    addAspectItem("16 x 9",       "16:9",   16.0 / 9.0);
    addAspectItem("16 x 10",      "16:10",  16.0 / 10.0);

    /* The user's own ratios, in their own block below a separator: the built-in list is
       fixed and the customs are the only entries Edit can touch, so the break says which
       is which. Only drawn when there are customs -- an empty block would stack two
       separators. A renamed entry ("Cinemascope") no longer states its ratio, so each
       carries it as a tooltip. */
    if (!customAspects.isEmpty()) {
        aspectCombo->insertSeparator(aspectCombo->count());
        for (const auto &c : customAspects) {
            addAspectItem(c.first, "custom:" + c.first, c.second);
            aspectCombo->setItemData(aspectCombo->count() - 1,
                                     tr("Ratio %1 : 1").arg(QString::number(c.second, 'g', 5)),
                                     Qt::ToolTipRole);
        }
    }

    aspectCombo->insertSeparator(aspectCombo->count());

    /* Action items are tagged so onAspectActivated intercepts them instead of treating
       them as ratios. Edit appears only when there is something to edit. */
    addActionItem(tr("Add custom aspect..."), AddCustomAction);
    if (!customAspects.isEmpty())
        addActionItem(tr("Edit custom aspects..."), ManageCustomAction);
}

void TransformPanel::addActionItem(const QString &caption, int action)
{
    aspectCombo->addItem(caption);
    aspectCombo->setItemData(aspectCombo->count() - 1, action, ActionRole);
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
    const int action = aspectCombo->itemData(index, ActionRole).toInt();
    if (action != NoAction) {
        /* Don't leave an action item selected: restore the prior choice, then act. */
        {
            QSignalBlocker block(aspectCombo);
            aspectCombo->setCurrentIndex(lastAspectIndex);
        }
        if (action == AddCustomAction)         promptAddCustomAspect();
        else if (action == ManageCustomAction) manageCustomAspects();
        return;
    }

    applyAspect(index, /*refit*/ true);
}

void TransformPanel::applyAspect(int index, bool refit)
{
    if (index < 0 || index >= aspectCombo->count()) return;
    lastAspectIndex = index;
    const QString key = aspectCombo->itemData(index, KeyRole).toString();
    const double ratio = aspectCombo->itemData(index, RatioRole).toDouble();
    if (setting) setting->setValue("Develop/Transform/aspectKey", key);

    /* A released ratio takes the lock with it: the lock was holding the aspect that has
       just been deleted, and "As shot" would silently re-point it at the native ratio --
       so the next drag would re-shape a crop the user never asked to change. */
    if (!refit && aspectLocked) {
        aspectLocked = false;
        updateLockButton();
        if (setting) setting->setValue("Develop/Transform/aspectLocked", false);
    }

    /* "As shot" carries the capture's ORIENTATION as well as its ratio, so a flip left over
       from a previous aspect must not turn it portrait. Clear it (the button follows) before
       the signal, so the handler reads the new state. The user can still press F afterwards
       to deliberately stand the native ratio on end. */
    if (key == "asShot" && aspectFlipped) {
        aspectFlipped = false;
        updateFlipButton();
        if (setting) setting->setValue("Develop/Transform/aspectFlipped", false);
    }

    emit aspectChanged(key, ratio, refit);
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

void TransformPanel::selectAspectKey(const QString &key)
{
    if (!aspectCombo) return;
    int idx = 0;                            // "As shot" is the fallback when key has gone
    for (int i = 0; i < aspectCombo->count(); ++i) {
        if (aspectCombo->itemData(i, KeyRole).toString() == key) { idx = i; break; }
    }
    const QSignalBlocker block(aspectCombo);
    aspectCombo->setCurrentIndex(idx);
    lastAspectIndex = idx;
}

void TransformPanel::manageCustomAspects()
{
/*
    Rename or delete the user's own aspect entries. The presets are fixed, so the list
    holds only the customs -- which is also why the combo item that opens this is hidden
    until there is at least one.

    The dialog edits customAspects in place and rebuilds both itself and the combo from
    it after every change, so the two can never drift apart, and Close needs to undo
    nothing: each change was already persisted when it was made.
*/
    if (customAspects.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit custom aspects"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(tr("Custom aspect ratios:"), &dlg));

    QListWidget *list = new QListWidget(&dlg);
    lay->addWidget(list);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    QPushButton *renameBtn = box->addButton(tr("Rename..."), QDialogButtonBox::ActionRole);
    QPushButton *deleteBtn = box->addButton(tr("Delete"),    QDialogButtonBox::ActionRole);
    lay->addWidget(box);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto fill = [&]{
        list->clear();
        for (const auto &c : customAspects) {
            QListWidgetItem *it = new QListWidgetItem(c.first, list);
            it->setToolTip(tr("Ratio %1 : 1").arg(QString::number(c.second, 'g', 5)));
        }
        if (list->count()) list->setCurrentRow(0);
        renameBtn->setEnabled(list->count() > 0);
        deleteBtn->setEnabled(list->count() > 0);
    };
    fill();

    /* Double-click is the conventional shortcut for the primary action on a list row. */
    connect(list, &QListWidget::itemDoubleClicked, &dlg, [&](QListWidgetItem *){
        if (renameCustomAspect(list->currentRow(), &dlg)) fill();
    });
    connect(renameBtn, &QPushButton::clicked, &dlg, [&]{
        if (renameCustomAspect(list->currentRow(), &dlg)) fill();
    });
    connect(deleteBtn, &QPushButton::clicked, &dlg, [&]{
        if (!deleteCustomAspect(list->currentRow(), &dlg)) return;
        if (customAspects.isEmpty()) dlg.accept();   // nothing left to manage
        else fill();
    });

    dlg.exec();
}

bool TransformPanel::renameCustomAspect(int i, QWidget *parent)
{
    if (i < 0 || i >= customAspects.size()) return false;
    const QString was = customAspects.at(i).first;

    bool ok = false;
    const QString name = QInputDialog::getText(
        parent, tr("Rename custom aspect"), tr("Name for this aspect ratio:"),
        QLineEdit::Normal, was, &ok).trimmed();
    if (!ok || name.isEmpty() || name == was) return false;

    /* The caption IS the entry's identity -- the combo key is "custom:" + caption and that
       key is what the selection is persisted as -- so two entries may not share one. */
    for (int k = 0; k < customAspects.size(); ++k) {
        if (k != i && customAspects.at(k).first == name) {
            QMessageBox::warning(parent, tr("Rename custom aspect"),
                                 tr("There is already a custom aspect named \"%1\".").arg(name));
            return false;
        }
    }

    const QString oldKey = "custom:" + was;
    const QString newKey = "custom:" + name;
    const bool wasSelected = (aspectKey() == oldKey);
    const QString keepKey = wasSelected ? newKey : aspectKey();

    customAspects[i].first = name;
    saveCustomAspects();
    /* The key moved with the caption, so move the persisted selection with it -- else the
       renamed entry would not be the one restored next launch. */
    if (setting && setting->value("Develop/Transform/aspectKey").toString() == oldKey)
        setting->setValue("Develop/Transform/aspectKey", newKey);

    populateAspectCombo();
    selectAspectKey(keepKey);
    return true;   // the ratio did not change: nothing to re-render
}

bool TransformPanel::deleteCustomAspect(int i, QWidget *parent)
{
    if (i < 0 || i >= customAspects.size()) return false;
    const QString caption = customAspects.at(i).first;

    if (QMessageBox::question(parent, tr("Delete custom aspect"),
                              tr("Delete the custom aspect \"%1\"?").arg(caption),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) return false;

    const bool wasSelected = (aspectKey() == "custom:" + caption);
    const QString keepKey = aspectKey();

    customAspects.removeAt(i);
    saveCustomAspects();

    populateAspectCombo();
    if (wasSelected) {
        /* The selected ratio just ceased to exist, so fall back to "As shot" -- announced
           as a RELEASE (refit == false), so the constraint is dropped without the crop
           being re-fitted. Deleting an entry from a list is housekeeping: it must not
           enlarge the frame the user has composed, which a plain "As shot" selection
           would do (it grows the crop back to the full image). */
        selectAspectKey("asShot");
        applyAspect(aspectCombo->currentIndex(), /*refit*/ false);
    }
    else selectAspectKey(keepKey);
    return true;
}

void TransformPanel::toggleAspectLock()
{
    setAspectLocked(!aspectLocked);
}

void TransformPanel::toggleAspectFlip()
{
    setAspectFlipped(!aspectFlipped);
}

void TransformPanel::setAspectLocked(bool locked)
{
    if (locked == aspectLocked) return;
    aspectLocked = locked;
    updateLockButton();             // the "A" key path: the checkbox follows
    if (setting) setting->setValue("Develop/Transform/aspectLocked", aspectLocked);
    emit aspectLockToggled(aspectLocked);
}

void TransformPanel::setAspectFlipped(bool flipped)
{
    if (flipped == aspectFlipped) return;
    aspectFlipped = flipped;
    updateFlipButton();             // the "X" key path: the radios follow
    if (setting) setting->setValue("Develop/Transform/aspectFlipped", aspectFlipped);
    emit aspectFlipToggled(aspectFlipped);
}

void TransformPanel::updateFlipButton()
{
    if (!landscapeRadio || !portraitRadio) return;
    const QSignalBlocker b1(landscapeRadio);
    const QSignalBlocker b2(portraitRadio);
    (aspectFlipped ? portraitRadio : landscapeRadio)->setChecked(true);
    const QString tip = tr("Crop orientation: landscape honours the aspect as written, "
                           "portrait stands it on end (X)");
    landscapeRadio->setToolTip(tip);
    portraitRadio->setToolTip(tip);
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
    if (!lockCheck) return;
    const QSignalBlocker block(lockCheck);
    lockCheck->setChecked(aspectLocked);
    lockCheck->setToolTip(aspectLocked
        ? tr("Aspect ratio LOCKED: dragging the crop keeps the ratio (A)")
        : tr("Aspect ratio unlocked: dragging the crop is free (A)"));
}

void TransformPanel::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    /* Subpanel content background (G::panelContentBg). The whole panel lifts: the
       GradientHeader is a CHILD, and children paint after their parent, so its band
       covers this fill with the header gradient. */
    p.fillRect(rect(), G::panelContentBg);
    /* Group dividers above Level and Warp: a G::groupSeparatorWidth line centred in each
       translucent placeholder, inset G::groupSeparatorInset from the panel edges like
       the property tree's dividers. */
    const int sepW = G::groupSeparatorWidth;
    const int inset = G::groupSeparatorInset;
    for (QWidget *rule : rowRules) {
        const QRect g = rule->geometry();
        p.fillRect(inset, g.top() + (g.height() - sepW) / 2, width() - 2 * inset, sepW,
                   G::groupSeparatorColor);
    }
    /* Separator rule across the bottom edge (space reserved by the layout margin). */
    p.fillRect(0, height() - G::panelBorderHeight, width(), G::panelBorderHeight,
               G::panelSeparatorColor);
}

bool TransformPanel::eventFilter(QObject *watched, QEvent *event)
{
    /* Claim a bare A/X before any window-level shortcut can: accept its
       ShortcutOverride so the following KeyPress is delivered normally, then act. */
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->modifiers() == Qt::NoModifier) {
            int key = ke->key();
            const bool ours = (key == Qt::Key_A || key == Qt::Key_X);
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
    case Qt::Key_X: toggleAspectFlip();   return true;
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
