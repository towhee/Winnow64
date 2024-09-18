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
};
#endif // HOVERDELEGATE_H
