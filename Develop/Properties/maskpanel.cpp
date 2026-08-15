#include "Develop/Properties/maskpanel.h"
#include "Develop/Properties/maskeditor.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

/*
    MaskPanel (see maskpanel.h): the active mask's editor, nested under its scope row --
    the submask list, the selected submask's settings, the overlay-colour row and ONE
    commit button whose label follows the held modifier (or reads "Done" when the submask
    being edited is already committed).
*/

MaskPanel::MaskPanel(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("MaskPanel::MaskPanel");
    buildUi();
}

void MaskPanel::buildUi()
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    /* The mask's contents, first: what the mask is made of is the thing the user needs to
       see, and every submask in it re-opens from here. */
    submaskList = new SubmaskList(this);
    connect(submaskList, &SubmaskList::collapsedChanged, this, [this]{ syncAttrVisible(); });
    outer->addWidget(submaskList);

    /* Settings + commit for the SELECTED submask. Wrapped so the whole block hides when
       nothing is selected (the list alone is then the panel). */
    attrWrap = new QWidget(this);
    QVBoxLayout *bl = new QVBoxLayout(attrWrap);
    bl->setContentsMargins(0, 2, 10, 6);
    bl->setSpacing(4);
    outer->addWidget(attrWrap);

    /* Header row: what a change to the settings below will AFFECT. A brush's attributes
       are baked per stroke, so the same slider reaches the submask's strokes when it is
       SELECTED, the next stroke while one is being ADDED, and the last stroke when Shift
       is held there -- with no way to tell which, and a slider that silently does nothing
       reads as broken. DevelopProperties keeps the text current, Shift included. */
    scopeLabel = new QLabel(attrWrap);
    scopeLabel->setWordWrap(true);
    scopeLabel->setContentsMargins(10, 2, 0, 2);
    scopeLabel->setStyleSheet(QString("color: %1; font-size: %2pt; background: transparent;")
                                  .arg(G::disabledColor.name())
                                  .arg(qMax(7, G::strFontSize.toInt() - 1)));
    scopeLabel->setVisible(false);
    bl->addWidget(scopeLabel);

    /* The tool's settings render here (Size/Feather/etc.), identical to the main tree.
       DevelopProperties populates + wires it. Full left margin (0) so its own caption
       column lines up with the tree below. */
    maskEditor = new MaskEditor(attrWrap);
    bl->addWidget(maskEditor);

    /* Commit buttons live under the settings, inset like a normal control row. */
    QWidget *btnWrap = new QWidget(attrWrap);
    QVBoxLayout *bw = new QVBoxLayout(btnWrap);
    bw->setContentsMargins(10, 4, 0, 0);
    bw->setSpacing(4);
    bl->addWidget(btnWrap);

    /* Overlay colour: the mask overlay speaks ONE colour (the veil previews the OUTCOME
       of the op, so no colour key is needed), and this row picks it -- red reads badly
       over a red subject. Persisted by DevelopProperties under Develop/maskOverlayColor;
       G::maskOverlayColor is updated here so the repaint is immediate. */
    swatchRow = new QWidget(btnWrap);
    QHBoxLayout *sl = new QHBoxLayout(swatchRow);
    sl->setContentsMargins(0, 0, 0, 2);
    sl->setSpacing(6);
    swatchColors = overlayColours();
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
       Subtract/Intersect stay visible instead of hiding behind undocumented keys. The
       [x] beside it discards a submask that is still being built; a re-opened submask has
       nothing to discard, so it is hidden there (see refreshCommitBtn). */
    QWidget *commitRow = new QWidget(btnWrap);
    QHBoxLayout *cl = new QHBoxLayout(commitRow);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    commitBtn = new QPushButton(tr("Add and Commit"), commitRow);
    commitBtn->setToolTip("Commit this submask into the mask (Return)\n"
                          "Opt: subtract    Shift+Opt: intersect");
    connect(commitBtn, &QPushButton::clicked, this, [this]{ emit committed(); });
    /* The global stylesheet (widgetcss.cpp) sets "QPushButton { min-width: 100px }",
       which Qt applies as an EXPLICIT minimum width (~112px at this DPI) and which floors
       the whole develop dock's width (a size policy alone can't undo an explicit
       minimum). Override it here so the panel never widens the dock. */
    commitBtn->setStyleSheet("QPushButton { min-width: 0; }");
    commitBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    cancelBtn = new BarBtn();
    cancelBtn->setToolTip("Cancel (Esc): discard this submask");
    cancelBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    connect(cancelBtn, &BarBtn::clicked, this, [this]{ emit cancelled(); });
    cl->addWidget(commitBtn);
    cl->addWidget(cancelBtn);
    bw->addWidget(commitRow);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

const QVector<QColor> &MaskPanel::overlayColours()
{
    /* The overlay speaks ONE colour and this is the choice: red first (the default),
       then colours far enough apart in hue to survive most subjects, ending in white
       for dark ones. Shared with the Develop action-row tint button's context menu. */
    static const QVector<QColor> colours = {
        QColor(220, 40, 40),   QColor(70, 200, 90),  QColor(60, 150, 255),
        QColor(240, 200, 40),  QColor(225, 70, 210), QColor(240, 240, 240)};
    return colours;
}

const QStringList &MaskPanel::overlayColourNames()
{
    static const QStringList names = {
        tr("Red"), tr("Green"), tr("Blue"), tr("Yellow"), tr("Magenta"), tr("White")};
    return names;
}

void MaskPanel::syncOverlayControls()
{
    refreshSwatches();
    refreshGrayBtn();
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

void MaskPanel::refreshCommitBtn()
{
    if (!commitBtn) return;
    if (editingExisting) {
        /* Already folded into the mask: there is nothing to commit and nothing to throw
           away -- the button just closes the editing session. */
        commitBtn->setText(tr("Done"));
        commitBtn->setToolTip("Finish editing this submask (Return).\n"
                              "Its edits are already part of the mask.");
    }
    else {
        /* The label names the OPERATION and the ACT: with the render already live, a bare
           "Update" read as "refresh the view" rather than "fold this submask in". */
        commitBtn->setText(pendingOp == 1 ? tr("Subtract and Commit")
                         : pendingOp == 2 ? tr("Intersect and Commit")
                                          : tr("Add and Commit"));
        commitBtn->setToolTip("Commit this submask into the mask (Return)\n"
                              "Opt: subtract    Shift+Opt: intersect");
    }
    if (cancelBtn) cancelBtn->setVisible(!editingExisting);
}

void MaskPanel::setPendingOp(int op)
{
    /* Modifiers are inert on the first submask -- there is nothing to subtract from or
       intersect with an empty mask -- and on a submask that is already committed, whose
       op is changed on its own row in the list. */
    if (editingExisting) return;
    if (firstMask) op = 0;
    if (op == pendingOp) return;
    pendingOp = op;
    refreshCommitBtn();
}

void MaskPanel::setEditingExisting(bool existing)
{
    editingExisting = existing;
    if (existing) pendingOp = 0;
    refreshCommitBtn();
}

void MaskPanel::showAttributes(bool show)
{
    attrShown = show;
    syncAttrVisible();
}

void MaskPanel::syncAttrVisible()
{
    /* The settings block belongs to a submask in the list above it, so a collapsed
       Submasks section takes it down too -- otherwise collapsing left the selected
       submask's settings (and the commit row) floating under a closed header. */
    if (!attrWrap) return;
    const bool collapsed = submaskList && submaskList->isCollapsed();
    attrWrap->setVisible(attrShown && !collapsed);
}

void MaskPanel::setAttributeScope(const QString &text)
{
    if (!scopeLabel) return;
    if (scopeLabel->text() == text) return;      // called on every Shift press/release
    scopeLabel->setText(text);
    scopeLabel->setVisible(!text.isEmpty());
}

void MaskPanel::beginPending(bool first)
{
    firstMask = first;
    editingExisting = false;
    pendingOp = 0;                  // every submask opens as Add ("Add and Commit")
    refreshCommitBtn();
    refreshSwatches();
    refreshGrayBtn();               // the flag is persistent, so re-sync on every show
    /* A new submask is being built: re-open the section, or its settings and the
       commit button would be invisible (the [+] that starts one is on the header). */
    if (submaskList) submaskList->setCollapsed(false);
    showAttributes(true);
    setVisible(true);
}
