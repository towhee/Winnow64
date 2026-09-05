#include "Views/catalogscopetree.h"
#include "Cache/catalog.h"
#include "Main/global.h"

#include <QHeaderView>
#include <QPainter>
#include <QPointer>
#include <QThreadPool>

namespace {
/*  A FOLDER, IN A COLOUR NO FOLDER IS. The rows are read as a continuation of the folder
    tree beneath them, so the glyph is the same shape -- anything else (a magnifying
    glass was tried) reads as a different kind of thing and breaks the list in two. The
    COLOUR is what says these are not folders on disk: teal against the blue folders
    macOS draws and the yellow ones Windows draws. */
const QColor kCatalogColor(78, 176, 166);
const QColor kYearColor(78, 176, 166);
}  // namespace

CatalogScopeTree::CatalogScopeTree(const QString &countMetric, int countMargin,
                                   QWidget *parent)
    : QTreeWidget(parent), countMetric(countMetric), countMargin(countMargin)
{
    if (G::isLogger) G::log("CatalogScopeTree::CatalogScopeTree");

    catalogIcon = tintedIcon(kCatalogColor);
    yearIcon = tintedIcon(kYearColor.lighter(115));

    setColumnCount(2);
    setHeaderHidden(true);
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFrameShape(QFrame::NoFrame);

    catalogItem = new QTreeWidgetItem(this);
    catalogItem->setIcon(0, catalogIcon);
    catalogItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    catalogItem->setToolTip(0,
        "Every image Winnow has catalogued, across all folders.\n"
        "Filter it the same way you filter a folder.\n\n"
        "Expand to pick one year, which opens the catalog with that\n"
        "year already checked in the Filters panel.");
    refreshText();

    /*  The years are a GROUP BY over the whole index, so they are not read until
        somebody asks to see them -- and then only once per catalog change. */
    connect(this, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *) {
        yearsWanted = true;
        refreshYears();
        fitToContents();
    });
    connect(this, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *) {
        fitToContents();
    });
    connect(this, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int) {
        if (item == catalogItem) emit catalogChosen();
        else emit catalogYearChosen(item->text(0));
    });

    updateStyle();
    fitToContents();
}

QIcon CatalogScopeTree::tintedIcon(const QColor &c)
{
/*
    The shipped glyph is white, which is invisible against a light theme and identical to
    every other white glyph against a dark one. Tinting it here keeps ONE asset and lets
    the colour be a decision in code rather than a second png to keep in step.
*/
    QPixmap pm(":/images/icon16/foldertree_white.png");
    if (pm.isNull()) return QIcon();
    QPixmap tinted(pm.size());
    tinted.setDevicePixelRatio(pm.devicePixelRatio());
    tinted.fill(Qt::transparent);
    QPainter p(&tinted);
    p.drawPixmap(0, 0, pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(tinted.rect(), c);
    p.end();
    return QIcon(tinted);
}

void CatalogScopeTree::setImageCount(qint64 count)
{
    if (count == imageCount) return;
    imageCount = count;
    refreshText();
    resizeColumns();
    /*  The catalog grew or shrank, so the years it holds may have too -- but only
        re-query if somebody is looking at them. */
    if (yearsWanted) refreshYears();
}

void CatalogScopeTree::refreshText()
{
    if (!catalogItem) return;
    catalogItem->setText(0, "Catalog");
    /*  A count of -1 is "the index is not open", which is not the same fact as "the
        library is empty" -- so say nothing rather than say zero. */
    /*  NO THOUSANDS SEPARATOR, here or on the years: the count column is narrow, and a
        six-figure library elides to "43,0..." -- a number that has been made unreadable
        to fit punctuation in. The folder counts beneath are written the same way
        (FSModel::updateCount), so the column reads as one column. */
    catalogItem->setText(1, imageCount < 0 ? QString()
                                           : QString::number(imageCount));
    /*  Nothing to expand into when there is no index. */
    catalogItem->setChildIndicatorPolicy(imageCount < 0
        ? QTreeWidgetItem::DontShowIndicator
        : QTreeWidgetItem::ShowIndicator);
}

void CatalogScopeTree::refreshYears()
{
/*
    Read the years OFF THE GUI THREAD. It is a GROUP BY over every indexed image, and
    this widget lives in the Folders panel where the user is clicking -- the one place
    a stall would be felt. One query at a time: a burst of catalog commits during a
    folder load would otherwise queue up a query per commit for the same answer.
*/
    if (!yearsWanted || yearsPending) return;
    if (!Catalog::instance().isAvailable()) return;
    yearsPending = true;
    QPointer<CatalogScopeTree> self(this);
    QThreadPool::globalInstance()->start([self]{
        const QMap<QString, int> years =
            Catalog::instance().categoryItems(G::YearColumn);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, years]{
            if (!self) return;
            self->yearsPending = false;
            self->setYears(years);
        }, Qt::QueuedConnection);
    });
}

void CatalogScopeTree::setYears(const QMap<QString, int> &years)
{
/*
    Replace the year children, newest first -- the order a person looks for a recent
    year in. Images with no capture date group under the blank key; they are not a year,
    so they are dropped rather than shown as an unnamed row that filters to something
    the label does not name.
*/
    if (!catalogItem) return;
    const QString selected = (currentItem() && currentItem() != catalogItem)
                                 ? currentItem()->text(0) : QString();

    while (catalogItem->childCount()) delete catalogItem->takeChild(0);

    QList<QString> keys = years.keys();
    std::sort(keys.begin(), keys.end(), std::greater<QString>());
    for (const QString &year : keys) {
        if (year.isEmpty()) continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(catalogItem);
        item->setIcon(0, yearIcon);
        item->setText(0, year);
        item->setText(1, QString::number(years.value(year)));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setToolTip(0, QString("Open the catalog filtered on %1.").arg(year));
        if (year == selected) setCurrentItem(item);
    }
    resizeColumns();
    fitToContents();
}

void CatalogScopeTree::setScopeIsCatalog(bool isCatalog)
{
/*
    MW pushes the scope back here rather than this widget deciding it, so the two
    instances and the Filter panel cannot disagree. Leaving a chosen YEAR selected is the
    point of the second clause: it is still the catalog, and it says which year.
*/
    if (!isCatalog) {
        clearSelection();
        setCurrentItem(nullptr);
        return;
    }
    if (!currentItem() || !currentItem()->isSelected()) {
        setCurrentItem(catalogItem);
        catalogItem->setSelected(true);
    }
}

void CatalogScopeTree::fitToContents()
{
/*
    The tree takes exactly the rows it has: one when collapsed, and ALL of the years when
    expanded. An earlier version capped it at twelve rows and scrolled inside itself,
    which meant expanding the catalog of a twenty-year library showed twelve years and
    hid the rest behind a scrollbar three rows tall -- a list of years that stops partway
    through is not a list of the catalog's years.

    THE ONLY LIMIT IS THE PANEL. However many years there are, kMinFolderRows of the dock
    are left for the folder tree beneath, so it can never be squeezed out of existence;
    past that this scrolls. In a dock of any ordinary height that limit is never reached.
*/
    int rows = 1;
    if (catalogItem && catalogItem->isExpanded()) rows += catalogItem->childCount();
    const int rowHeight = qMax(sizeHintForRow(0), fontMetrics().height() + 6);

    int maxHeight = rows * rowHeight;
    if (parentWidget()) {
        const int forFolders = kMinFolderRows * rowHeight;
        maxHeight = qMax(rowHeight, parentWidget()->height() - forFolders);
    }
    setFixedHeight(qMin(rows * rowHeight, maxHeight) + 2 * frameWidth());
}

bool CatalogScopeTree::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) fitToContents();
    return QTreeWidget::eventFilter(watched, event);
}

void CatalogScopeTree::showEvent(QShowEvent *event)
{
    QTreeWidget::showEvent(event);
    /*  The parent exists by now (it does not at construction), and its height is real
        rather than the layout's initial guess. */
    if (parentWidget()) parentWidget()->installEventFilter(this);
    fitToContents();
}

void CatalogScopeTree::resizeColumns()
{
/*
    The count column is the tree-below's width OR the width of the widest count in it,
    whichever is larger.

    Matching the neighbour is what lines the two columns up, and it is right until the
    catalog outgrows it: the neighbour sizes for a folder's count, and the whole library
    is an order of magnitude bigger. When it no longer fits, the number wins -- an elided
    total ("43,0...") is worse than a column an few pixels wider than the folder counts
    below, which is the one thing a person actually reads this row for.
*/
    QFont f = font();
    f.setPointSize(G::strFontSize.toInt());
    const QFontMetrics fm(f);
    int countWidth = fm.boundingRect(countMetric).width();

    /*  Measured with the WIDGET's own metrics, not the neighbour's font: the tree draws
        in its own font, and sizing to a smaller one is how the ellipsis got there. */
    const QFontMetrics wfm = fontMetrics();
    int widest = 0;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        widest = qMax(widest, wfm.horizontalAdvance((*it)->text(1)));
        ++it;
    }
    /*  The item's own left/right text margins, which are not in the string's width. */
    countWidth = qMax(countWidth, widest + 8);

    setColumnWidth(1, countWidth);
    setColumnWidth(0, width() - G::scrollBarThickness - countWidth - countMargin);
}

void CatalogScopeTree::resizeEvent(QResizeEvent *event)
{
    QTreeWidget::resizeEvent(event);
    resizeColumns();
}

void CatalogScopeTree::updateStyle()
{
/*
    Painted from G::backgroundShade rather than left to the app stylesheet, for the same
    reason the row it replaced was: this is a SELECTOR, and it has to look selected when
    it is the current scope. The CHECKED colour is G::selectionColor so the row reads as
    selected in the same language as a selected folder in the tree beneath it -- the two
    are alternatives, and they should look like alternatives.
*/
    const QString hover = QColor(G::backgroundShade + 14, G::backgroundShade + 14,
                                 G::backgroundShade + 14).name();
    const QString css = QString(
        "QTreeWidget { background:transparent; border:none; color:%1;"
        "  outline:none; }"
        "QTreeWidget::item { padding:2px 0px; }"
        "QTreeWidget::item:selected { background:%2; color:%3; font-weight:bold; }"
        "QTreeWidget::item:hover:!selected { background:%4; }"
    ).arg(G::textColor.name(), G::selectionColor.name(),
          G::textColor.name(), hover);
    setStyleSheet(css);
    fitToContents();
}
