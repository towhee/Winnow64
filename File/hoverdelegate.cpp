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
        // qDebug() << "HoverDelegate::paint idx0 =" << idx0;
        opt.state |= QStyle::State_MouseOver;
        painter->fillRect(opt.rect, hoverBackground);  // Highlight color
    }
    QStyledItemDelegate::paint(painter, opt, index);
}
