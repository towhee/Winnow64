#ifndef THUMBVIEWDELEGATE_H
#define THUMBVIEWDELEGATE_H

#include <QtGui>
#include <QtWidgets>
#include <QStyleOptionViewItem>
#include <QFont>
#include "global.h"
//#include "thumbview.h"

class ThumbViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    ThumbViewDelegate(QObject *parent);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index ) const;

    void setThumbDimensions(int thumbWidth, int thumbHeight,
          int thumbPadding, int labelFontSize, bool showThumbLabels);

    QSize getThumbCell();
    void reportThumbAttributes();

    QModelIndex currentIndex;

signals:
    void update(const QModelIndex index, QRect iconRect) const;

public slots:
    void onCurrentChanged(QModelIndex current, QModelIndex previous);

private:
    QObject parent;
    bool delegateShowThumbLabels;
    QFont font;
    QSize thumbSize;
    QSize thumbSpace;
    int iconPadding;
    int itemBorderThickness;
    int thumbBorderThickness;
    int thumbBorderGap;
//    int maxThumbNailWidth = 160;
//    int maxThumbNailHeight = 120;
    int fontHt;
};

#endif // THUMBVIEWDELEGATE_H
