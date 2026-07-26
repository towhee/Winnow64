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
    MaskPanel (see maskpanel.h): the thin mask build-up strip -- title + commit buttons.
    The tool's settings render in the property tree (addToolRow), so they match the tree.
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

    /* Three combine ops (2nd+ tool). */
    combineRow = new QWidget(body);
    QHBoxLayout *cl = new QHBoxLayout(combineRow);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    QPushButton *addBtn = new QPushButton(tr("Add"), combineRow);
    QPushButton *subBtn = new QPushButton(tr("Subtract"), combineRow);
    QPushButton *intBtn = new QPushButton(tr("Intersect"), combineRow);
    addBtn->setToolTip("Add this tool's area to the layer mask");
    subBtn->setToolTip("Remove this tool's area from the layer mask");
    intBtn->setToolTip("Keep only where this tool overlaps the layer mask");
    /* MaskOp: Add=0, Subtract=1, Intersect=2 (see editstack.h). */
    connect(addBtn, &QPushButton::clicked, this, [this]{ emit committed(0); });
    connect(subBtn, &QPushButton::clicked, this, [this]{ emit committed(1); });
    connect(intBtn, &QPushButton::clicked, this, [this]{ emit committed(2); });
    cl->addWidget(addBtn);
    cl->addWidget(subBtn);
    cl->addWidget(intBtn);
    bw->addWidget(combineRow);

    /* Done (first tool -- already the mask). */
    doneBtn = new QPushButton(tr("Done"), btnWrap);
    doneBtn->setToolTip("Finish editing this mask");
    connect(doneBtn, &QPushButton::clicked, this, [this]{ emit finished(); });
    bw->addWidget(doneBtn);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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
}

void MaskPanel::showForTool(const QString &toolName, bool first)
{
    if (titleLabel) titleLabel->setText(tr("Mask: %1").arg(toolName));
    /* First tool is already the (red) mask -> [Done]; a later tool is a (blue) preview to
       fold in -> the three combine buttons. */
    if (combineRow) combineRow->setVisible(!first);
    if (doneBtn)    doneBtn->setVisible(first);
    setVisible(true);
}
