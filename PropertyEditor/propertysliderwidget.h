#ifndef PROPERTYSLIDERWIDGET_H
#define PROPERTYSLIDERWIDGET_H

#include <QtWidgets>

class SliderLineEdit : public QWidget
{
    Q_OBJECT
public:
    SliderLineEdit(QWidget *parent = nullptr/*, int &value, int lineEditWidth, int min, int max*/);
    void setValue(int value);

protected:
//    void paintEvent(QPaintEvent *event);

private:
    QSlider *slider;
    QLineEdit *lineEdit;
};

#include <QStyledItemDelegate>

class SliderLineEditDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    SliderLineEditDelegate(QObject *parent = nullptr/*,
                           QModelIndex targetIdx,
                           int &value,
                           int lineEditWidth,
                           int min,
                           int max*/);
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QModelIndex targetIdx;
    int value;
    int lineEditWidth;
    int min;
    int max;
};

#endif // PROPERTYSLIDERWIDGET_H


