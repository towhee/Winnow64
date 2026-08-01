#include "Develop/Replace/replacepanel.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QSettings>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QLinearGradient>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <QMouseEvent>

namespace {
/* Same gradient band as TransformPanel's header (PropertyDelegate shades). Plain QWidget
   subclass with no _Q_OBJECT (needs no moc) -- it only overrides paintEvent. */
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

/* Row caption with the shortcut letter bold in G::header2Color, e.g. (S)pot -> "Spot"
   with a bold blue S. QRadioButton text cannot render rich text, so the caption is a
   separate QLabel; clicking it selects the row's radio (see labelClickFilter). */
QString captionHtml(const QString &shortcutLetter, const QString &rest)
{
    return QString("<span style=\"color:%1; font-weight:bold;\">%2</span>%3")
        .arg(G::header2Color.name(), shortcutLetter, rest);
}
}

ReplacePanel::ReplacePanel(QWidget *parent, QSettings *settings)
    : QWidget(parent), setting(settings)
{
    if (G::isLogger) G::log("ReplacePanel::ReplacePanel");
    buildUi();
}

void ReplacePanel::buildUi()
{
    /* -------- Header: gradient band, "Fill Replace" left, [?] [E] trailing -------- */
    GradientHeader *header = new GradientHeader(this);

    QLabel *title = new QLabel(tr("Fill Replace"), header);
    title->setStyleSheet(QString("color: %1;").arg(G::header2Color.name()));
    QFont hf = title->font();
    hf.setPointSize(G::strFontSize.toInt());
    title->setFont(hf);

    tipBtn = new BarBtn();
    tipBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    tipBtn->setToolTip(tr("Fill replace tips"));
    connect(tipBtn, &BarBtn::clicked, this, &ReplacePanel::tipsRequested);

    /* The eye shows/ignores every committed replace (non-destructive). */
    previewBtn = new BarBtn();
    connect(previewBtn, &BarBtn::clicked, this, [this]{
        previewShown = !previewShown;
        updatePreviewButton();
        emit previewToggled(previewShown);
    });
    updatePreviewButton();

    QHBoxLayout *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(6, 3, 6, 3);
    headerRow->setSpacing(6);
    headerRow->addWidget(title);
    headerRow->addStretch(1);
    headerRow->addWidget(tipBtn);
    headerRow->addWidget(previewBtn);

    /* -------- Mode rows: radio + rich-text caption + dimmed hint -------- */
    modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);

    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(2, 1);

    auto addRow = [&](int row, QRadioButton *&radio, const QString &letter,
                      const QString &rest, const QString &hint, const QString &tip) {
        radio = new QRadioButton(this);
        radio->setToolTip(tip);
        radio->setFocusPolicy(Qt::StrongFocus);   // so S/F/O reach the eventFilter
        modeGroup->addButton(radio, row);
        QLabel *caption = new QLabel(captionHtml(letter, rest), this);
        caption->setToolTip(tip);
        /* Clicking the caption selects its radio (rich text keeps QLabel, not the
           radio's own text). The label is tagged with the mode for the event filter. */
        caption->setProperty("replaceMode", row);
        caption->installEventFilter(this);
        QLabel *hintLbl = new QLabel(hint, this);
        hintLbl->setStyleSheet(QString("color: %1;").arg(G::disabledColor.name()));
        grid->addWidget(radio,   row, 0);
        grid->addWidget(caption, row, 1);
        grid->addWidget(hintLbl, row, 2);
    };
    addRow(SpotMode, spotBtn, "S", tr("pot"), tr("click a blemish"),
           tr("Spot: click a blemish to heal it with cloned surroundings (S)"));
    addRow(FillMode, fillBtn, "F", tr("ill"), tr("paint area, Enter fills"),
           tr("Fill: paint the area to replace with cloned surroundings; Opt/Alt "
              "erases, Enter fills, Esc clears (F)"));
    addRow(ObjectMode, objectBtn, "O", tr("bject"), tr("brush over an object"),
           tr("Object: brush over an object to remove it with a regenerative fill (O)"));
    connect(modeGroup, &QButtonGroup::idClicked, this, &ReplacePanel::selectMode);

    /* -------- Assemble -------- */
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    lay->addWidget(header);
    QVBoxLayout *bodyWrap = new QVBoxLayout;
    bodyWrap->setContentsMargins(6, 0, 6, 6);
    bodyWrap->addLayout(grid);
    lay->addLayout(bodyWrap);

    currentMode = setting ? setting->value("Develop/Replace/mode", SpotMode).toInt()
                          : int(SpotMode);
    setMode(currentMode);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    /* Let S/F/O act on the focused panel (see eventFilter). */
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
    QWidget *filtered[] = {spotBtn, fillBtn, objectBtn};
    for (QWidget *w : filtered)
        w->installEventFilter(this);
}

void ReplacePanel::setMode(int mode)
{
    currentMode = qBound(int(SpotMode), mode, int(ObjectMode));
    QRadioButton *b = (currentMode == FillMode)   ? fillBtn
                    : (currentMode == ObjectMode) ? objectBtn
                                                  : spotBtn;
    if (b) {
        const QSignalBlocker block(b);
        b->setChecked(true);
    }
}

void ReplacePanel::selectMode(int mode)
{
    if (mode == currentMode) { setMode(mode); return; }
    setMode(mode);
    if (setting) setting->setValue("Develop/Replace/mode", currentMode);
    emit modeChanged(currentMode);
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
        ? tr("Showing the replaces (click to view the original)")
        : tr("Replaces hidden (click to show them)"));
}

bool ReplacePanel::eventFilter(QObject *watched, QEvent *event)
{
    /* Caption click selects its row's mode. */
    if (event->type() == QEvent::MouseButtonPress) {
        const QVariant m = watched->property("replaceMode");
        if (m.isValid()) { selectMode(m.toInt()); return true; }
    }
    /* Claim a bare S/F/O before any window-level shortcut can: accept its
       ShortcutOverride so the following KeyPress is delivered normally, then act.
       Only while the Fill/Object modes are live -- shelved, the panel is never shown and
       S belongs to the Develop mode spot-tool toggle (MW::loadDevelopShortcuts). */
    if (G::useReplaceFillModes &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->modifiers() == Qt::NoModifier) {
            const int key = ke->key();
            const bool ours = (key == Qt::Key_S || key == Qt::Key_F || key == Qt::Key_O);
            if (ours) {
                if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
                switch (key) {
                case Qt::Key_S: selectMode(SpotMode);   break;
                case Qt::Key_F: selectMode(FillMode);   break;
                case Qt::Key_O: selectMode(ObjectMode); break;
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
