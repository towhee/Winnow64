#include "Develop/Properties/submaskdialog.h"

#include "Develop/Properties/developproperties.h"
#include "Develop/Properties/submasklist.h"
#include "Develop/editstack.h"
#include "Main/global.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

/*
    SubmaskDialog (see submaskdialog.h): op + type, asked together, before the submask
    exists.
*/

namespace {
/* The role carrying the MaskTool on a selectable row. Group headings have no data, which
   is what tells them apart from tools. */
constexpr int UR_Tool = Qt::UserRole + 1;
}

bool SubmaskDialog::choose(QWidget *parent, bool firstMask, int &op, int &tool)
{
    SubmaskDialog dlg(firstMask, parent);
    if (dlg.exec() != QDialog::Accepted) return false;
    if (dlg.chosenTool() < 0) return false;      // OK is disabled without one, belt+braces
    op   = dlg.chosenOp();
    tool = dlg.chosenTool();
    return true;
}

SubmaskDialog::SubmaskDialog(bool first, QWidget *parent)
    : QDialog(parent), firstMask(first)
{
    if (G::isLogger) G::log("SubmaskDialog::SubmaskDialog");
    buildUi();
}

int SubmaskDialog::chosenOp() const
{
    if (firstMask) return int(MaskOp::Add);
    if (subtractBtn  && subtractBtn->isChecked())  return int(MaskOp::Subtract);
    if (intersectBtn && intersectBtn->isChecked()) return int(MaskOp::Intersect);
    return int(MaskOp::Add);
}

int SubmaskDialog::chosenTool() const
{
    const QListWidgetItem *it = toolList ? toolList->currentItem() : nullptr;
    if (!it || !it->data(UR_Tool).isValid()) return -1;
    return it->data(UR_Tool).toInt();
}

void SubmaskDialog::buildUi()
{
    setWindowTitle(tr("Add submask"));

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(20, 18, 20, 16);
    v->setSpacing(8);

    /* ---- Section 1: what it does to the mask ----------------------------------------
       Radio buttons rather than a combo: there are three, they are the whole point of
       the dialog, and each needs a line of its own saying what it DOES -- the words the
       modifier shortcut never had room for. */
    QLabel *opHeading = new QLabel(tr("What it does to the mask"), this);
    QFont hf = opHeading->font();
    hf.setBold(true);
    opHeading->setFont(hf);
    v->addWidget(opHeading);

    /* The glyph matches the one on the submask's row in the list (SubmaskList::opGlyph),
       so the choice made here is recognisable there afterwards. */
    auto opButton = [&](int op, const QString &what) {
        QRadioButton *b = new QRadioButton(
            QString("%1  %2 — %3").arg(SubmaskList::opGlyph(op),
                                            SubmaskList::opName(op), what), this);
        v->addWidget(b);
        return b;
    };
    addBtn       = opButton(int(MaskOp::Add),
                            tr("the new area joins the mask"));
    subtractBtn  = opButton(int(MaskOp::Subtract),
                            tr("the new area is cut out of the mask"));
    intersectBtn = opButton(int(MaskOp::Intersect),
                            tr("only where the new area and the mask overlap survives"));
    addBtn->setChecked(true);

    /* An empty mask has nothing to combine with, so Subtract and Intersect are not
       choices yet. Disabled AND explained: a control that silently does nothing reads as
       broken, and the reason ("there is no mask yet") is not guessable from three greyed
       radio buttons. */
    if (firstMask) {
        subtractBtn->setEnabled(false);
        intersectBtn->setEnabled(false);
        opNote = new QLabel(tr("This is the first submask, so there is nothing yet to "
                               "subtract from or intersect with."), this);
        opNote->setWordWrap(true);
        opNote->setEnabled(false);
        v->addWidget(opNote);
    }

    v->addSpacing(6);

    /* ---- Section 2: what kind of submask --------------------------------------------
       The same list the pop-up menu offered, in the same order, with the menu's two
       separators promoted to headings -- a separator says "these differ" without saying
       HOW, and the three groups answer different questions (draw it / pick it by
       content / let the model find it). */
    QLabel *toolHeading = new QLabel(tr("Submask type"), this);
    toolHeading->setFont(hf);
    v->addWidget(toolHeading);

    toolList = new QListWidget(this);
    toolList->setSelectionMode(QAbstractItemView::SingleSelection);
    toolList->setUniformItemSizes(false);
    v->addWidget(toolList, 1);

    addToolGroup(tr("Drawn"),     int(MaskTool::LinearGradient), int(MaskTool::Brush));
    addToolGroup(tr("Content"),   int(MaskTool::ColorRange),     int(MaskTool::LuminanceRange));
    addToolGroup(tr("Automatic"), int(MaskTool::Subject),        int(MaskTool::Object));

    /* Tall enough for every row: the list is short and scrolling it hides choices. Row
       heights are summed (headings are bold, so not uniform) rather than assumed. */
    int listH = 2 * toolList->frameWidth();
    for (int r = 0; r < toolList->count(); ++r)
        listH += toolList->sizeHintForRow(r) + 2 * toolList->spacing();
    toolList->setMinimumHeight(listH);
    toolList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    /* Nothing preselected: the tool is the one thing the dialog cannot guess, and a
       default row would be added by a stray Return. OK stays off until a row is picked;
       a double-click is the shortcut for people who know which one they want. */
    toolList->setCurrentRow(-1);
    connect(toolList, &QListWidget::itemSelectionChanged, this, [this]{ syncOkEnabled(); });
    connect(toolList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it){
        if (it && it->data(UR_Tool).isValid()) accept();
    });

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    okBtn = box->addButton(tr("Add submask"), QDialogButtonBox::AcceptRole);
    okBtn->setDefault(true);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(box);

    syncOkEnabled();
    resize(sizeHint());
}

void SubmaskDialog::addToolGroup(const QString &heading, int firstTool, int lastTool)
{
    QListWidgetItem *h = new QListWidgetItem(heading, toolList);
    h->setFlags(Qt::NoItemFlags);                // a caption, not a choice
    QFont f = h->font();
    f.setBold(true);
    h->setFont(f);

    for (int t = firstTool; t <= lastTool; ++t) {
        QListWidgetItem *it = new QListWidgetItem(
            "    " + DevelopProperties::maskToolName(t), toolList);
        it->setData(UR_Tool, t);
    }
}

void SubmaskDialog::syncOkEnabled()
{
    if (okBtn) okBtn->setEnabled(chosenTool() >= 0);
}
