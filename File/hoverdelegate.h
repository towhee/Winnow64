#ifndef HOVERDELEGATE_H
#define HOVERDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDebug>
#include "main/global.h"

class HoverDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    HoverDelegate(QObject *parent = nullptr);

    void setHoveredIndex(QModelIndex idx0);

signals:
    void hoverChanged();

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QModelIndex hoveredIndex;
    int b = G::backgroundShade + 20;
    QColor hoverBackground;

    // --- New additions ---
    QColor overLimitTextColor = QColor(255,165,0);   // orange
    QColor overLimitBackground = QColor(255,245,220); // light background tint (optional)

    // Shared custom role for "too many subfolders" highlight
    enum { OverLimitRole = Qt::UserRole + 2 };
};
#endif // HOVERDELEGATE_H
