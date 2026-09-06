#ifndef FLOWLAYOUT_H
#define FLOWLAYOUT_H

#include <QLayout>
#include <QList>
#include <QRect>
#include <QStyle>

/*
    A layout that wraps its items onto as many rows as they need, like words in a
    paragraph. Qt ships one as an example but not as a class, so here it is.

    THE KEYWORDS DOCK NEEDS IT for the chip zone: an image's keywords are a handful of
    variable-width labels whose number and widths are not known until the selection
    changes, in a dock the user can make any width. A grid would need a column count
    nobody can choose correctly and a horizontal box would clip.

    heightForWidth IS THE WHOLE POINT and is why this is a layout rather than a widget
    doing its own geometry: the containing scroll area has to know how tall the chips will
    be at the width it is about to give them, and only the layout can answer that.
*/
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr, int margin = -1,
                        int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> itemList;
    int m_hSpace;
    int m_vSpace;
};

#endif // FLOWLAYOUT_H
