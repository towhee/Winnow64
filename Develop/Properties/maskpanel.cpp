#include "Develop/Properties/maskpanel.h"
#include "Develop/Properties/maskeditor.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QLinearGradient>

/*
    MaskPanel (see maskpanel.h): the thin submask build-up strip -- title, settings,
    overlay-colour swatches and ONE commit button whose label follows the held modifier.
*/

MaskPanel::MaskPanel(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("MaskPanel::MaskPanel");
    buildUi();
}

void MaskPanel::buildUi()
{
    /* Bottom margin reserves the 2px panel separator drawn in paintEvent. */
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, G::panelBorderHeight);
    outer->setSpacing(0);

    /* Header band ("Mask: <tool>" + cancel); transparent so paintEvent draws gradient. */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(10, 3, 6, 3);
    hb->setSpacing(6);
    titleLabel = new QLabel(tr("Mask"), headerBand);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2pt; background: transparent;")
                                  .arg(G::header2Color.name()).arg(G::strFontSize.toInt()));
    cancelBtn = new BarBtn();
    cancelBtn->setToolTip("Cancel (Esc): discard this tool");
    cancelBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    connect(cancelBtn, &BarBtn::clicked, this, [this]{ emit cancelled(); });
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(cancelBtn);
    outer->addWidget(headerBand);

    /* Settings tree (tree-rendered rows), then the commit buttons. */
    QWidget *body = new QWidget(this);
    QVBoxLayout *bl = new QVBoxLayout(body);
    bl->setContentsMargins(0, 2, 10, 6);
    bl->setSpacing(4);
    outer->addWidget(body);

    /* The tool's settings render here (Size/Feather/etc.), identical to the main tree.
       DevelopProperties populates + wires it. Full left margin (0) so its own caption
       column lines up with the tree below. */
    maskEditor = new MaskEditor(body);
    bl->addWidget(maskEditor);

    /* Commit buttons live under the settings, inset like a normal control row. */
    QWidget *btnWrap = new QWidget(body);
    QVBoxLayout *bw = new QVBoxLayout(btnWrap);
    bw->setContentsMargins(10, 4, 0, 0);
    bw->setSpacing(4);
    bl->addWidget(btnWrap);

    /* Overlay colour: the mask overlay speaks ONE colour (the veil previews the OUTCOME
       of the op, so no colour key is needed), and this row picks it -- red reads badly
       over a red subject. Persisted by DevelopProperties under Develop/maskOverlayColor;
       G::maskOverlayColor is updated here so the repaint is immediate. */
    swatchRow = new QWidget(body);
    QHBoxLayout *sl = new QHBoxLayout(swatchRow);
    sl->setContentsMargins(0, 0, 0, 2);
    sl->setSpacing(6);
    swatchColors = {QColor(220, 40, 40),   QColor(70, 200, 90),  QColor(60, 150, 255),
                    QColor(240, 200, 40),  QColor(225, 70, 210), QColor(240, 240, 240)};
    for (const QColor &c : swatchColors) {
        QPushButton *sw = new QPushButton(swatchRow);
        sw->setFixedSize(18, 18);
        sw->setToolTip("Mask overlay colour");
        sw->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(sw, &QPushButton::clicked, this, [this, c]{
            G::maskOverlayColor = c;
            refreshSwatches();
            emit overlayColourChanged();
        });
        swatches.append(sw);
        sl->addWidget(sw);
    }
    /* Grayscale toggle, on the same row as the swatches: no overlay colour reads well
       over every subject, so the alternative to hunting for a colour is to take the
       colour out of the picture. Purely a view effect (ImageView desaturates what it
       draws under the veil) -- the render and the export are untouched. */
    sl->addSpacing(8);
    grayBtn = new QPushButton(tr("Gray"), swatchRow);
    grayBtn->setCheckable(true);
    grayBtn->setChecked(G::maskOverlayGrayscale);
    grayBtn->setFixedHeight(18);
    grayBtn->setToolTip("Show the image in grayscale while the mask overlay is on,\n"
                        "so the overlay colour is easier to see (view only)");
    grayBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(grayBtn, &QPushButton::clicked, this, [this]{
        G::maskOverlayGrayscale = grayBtn->isChecked();
        refreshGrayBtn();
        emit overlayGrayscaleChanged();
    });
    sl->addWidget(grayBtn);

    sl->addStretch(1);
    bw->addWidget(swatchRow);
    refreshSwatches();
    refreshGrayBtn();

    /* ONE commit button. Its label follows the op the overlay is previewing, so the words
       Subtract/Intersect stay visible instead of hiding behind undocumented keys. */
    commitBtn = new QPushButton(tr("Add and Commit"), btnWrap);
    commitBtn->setToolTip("Commit this submask into the mask (Return)\n"
                          "Opt: subtract    Shift+Opt: intersect");
    connect(commitBtn, &QPushButton::clicked, this, [this]{ emit committed(); });
    /* The global stylesheet (widgetcss.cpp) sets "QPushButton { min-width: 100px }",
       which Qt applies as an EXPLICIT minimum width (~112px at this DPI) and which floors
       the whole develop dock's width (a size policy alone can't undo an explicit
       minimum). Override it here so the panel never widens the dock. */
    commitBtn->setStyleSheet("QPushButton { min-width: 0; }");
    commitBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    bw->addWidget(commitBtn);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void MaskPanel::refreshSwatches()
{
    /* Flat colour chips; the active one gets a light border. Styled rather than painted
       so they inherit the dock's button metrics. */
    for (int k = 0; k < swatches.size(); ++k) {
        const bool on = (swatchColors.at(k).rgb() == G::maskOverlayColor.rgb());
        swatches[k]->setStyleSheet(
            QString("QPushButton { min-width: 0; border-radius: 3px; background: %1;"
                    " border: %2; }")
                .arg(swatchColors.at(k).name(),
                     on ? "2px solid #f0f0f0" : "1px solid #303030"));
    }
}

void MaskPanel::refreshGrayBtn()
{
    /* Chip-sized like the swatches beside it (the global stylesheet's 100px minimum
       would otherwise widen the whole dock -- see the commit button), with the checked
       state drawn as a light border so it matches the selected swatch. */
    if (!grayBtn) return;
    grayBtn->setChecked(G::maskOverlayGrayscale);
    grayBtn->setStyleSheet(
        QString("QPushButton { min-width: 0; padding: 0 6px; border-radius: 3px;"
                " background: %1; border: %2; }")
            .arg(G::maskOverlayGrayscale ? "#585858" : "#3a3a3a",
                 G::maskOverlayGrayscale ? "2px solid #f0f0f0" : "1px solid #303030"));
}

void MaskPanel::setPendingOp(int op)
{
    /* Modifiers are inert on the first submask -- there is nothing to subtract from or
       intersect with an empty mask. */
    if (firstMask) op = 0;
    if (op == pendingOp) return;
    pendingOp = op;
    /* The label names the OPERATION and the ACT: with the render already live, a bare
       "Update" read as "refresh the view" rather than "fold this submask in". */
    if (commitBtn)
        commitBtn->setText(op == 1 ? tr("Subtract and Commit")
                         : op == 2 ? tr("Intersect and Commit")
                                   : tr("Add and Commit"));
}

void MaskPanel::paintEvent(QPaintEvent *)
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

void MaskPanel::showForTool(const QString &toolName, bool first)
{
    if (titleLabel) titleLabel->setText(tr("Mask: %1").arg(toolName));
    firstMask = first;
    pendingOp = -1;                 // force the relabel below
    setPendingOp(0);                // every submask opens as Add ("Add and Commit")
    refreshSwatches();
    refreshGrayBtn();               // the flag is persistent, so re-sync on every show
    setVisible(true);
}
