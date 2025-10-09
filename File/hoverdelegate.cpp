#include "HoverDelegate.h"

HoverDelegate::HoverDelegate(QObject *parent)
    : QStyledItemDelegate(parent), hoveredIndex(QModelIndex())
{
    int b = G::backgroundShade + 20;
    hoverBackground = QColor(b,b,b);
}

void HoverDelegate::setHoveredIndex(QModelIndex idx0) {
    if (hoveredIndex != idx0) {
        hoveredIndex = idx0;
        // qDebug() << "HoverDelegate::setHoveredIndex index =" << idx0;
        emit hoverChanged();
    }
}

void HoverDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItem opt = option;
    QModelIndex idx0 = index.sibling(index.row(), 0);
    if (idx0 == hoveredIndex) {
        opt.state |= QStyle::State_MouseOver;
        painter->fillRect(opt.rect, hoverBackground);  // Highlight color
    }

    // --- Handle over-limit highlight ---
    // FSTree::markFolderOverLimit sets OverLimitRole = true on FSModel items
    const bool isOverLimit = index.data(OverLimitRole).toBool();
    if (isOverLimit) {
        // Optional pale background tint to distinguish
        // painter->fillRect(opt.rect, overLimitBackground);

        // Change text color
        opt.palette.setColor(QPalette::Text, overLimitTextColor);
        // Override default text color when item is selected
        opt.palette.setColor(QPalette::HighlightedText, overLimitTextColor);
    }

    // --- Paint normally using the modified option ---
    QStyledItemDelegate::paint(painter, opt, index);
}
