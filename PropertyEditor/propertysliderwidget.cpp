#include "propertysliderwidget.h"

SliderLineEdit::SliderLineEdit(QWidget *parent/*,
                               int &value,
                               int lineEditWidth,
                               int min,
                               int max*/)     : QWidget(parent)
{
    int lineEditWidth = 50;
    int min = 6;
    int max = 20;
    slider = new QSlider(Qt::Horizontal);
    slider->setMinimum(min);
    slider->setMaximum(max);
//    slider->setValue(value);

    lineEdit = new QLineEdit;
    lineEdit->setStyleSheet("QLineEdit {"
                            "background-color:blue;"
                            "}"
                            );
    lineEdit->setMaximumWidth(lineEditWidth);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(slider, Qt::AlignLeft);
    layout->addWidget(lineEdit, Qt::AlignRight);
    layout->addSpacing(15);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);
}

void SliderLineEdit::setValue(int value)
{
    slider->setValue(value);
}

SliderLineEditDelegate::SliderLineEditDelegate(QObject *parent/*,
                                               QModelIndex targetIdx,
                                               int &value,
                                               int lineEditWidth,
                                               int min,
                                               int max*/)
                                        : QStyledItemDelegate(parent)
{
//    this->value = value;
//    this->targetIdx = targetIdx;
//    this->lineEditWidth = lineEditWidth;
//    this->min = min;
//    this->max = max;
}

QWidget *SliderLineEditDelegate::createEditor(QWidget *parent,
                                              const QStyleOptionViewItem &option,
                                              const QModelIndex &index ) const
{
    /*if (index == targetIdx) */return new SliderLineEdit(parent/*, value, lineEditWidth, min, max*/);
}

QSize SliderLineEditDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return SliderLineEdit(/*nullptr, value, lineEditWidth,  min, max*/).sizeHint();
}

void SliderLineEditDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    static_cast<SliderLineEdit*>(editor)->setValue(value);
//    SliderLineEdit *w = static_cast<SliderLineEdit*>(editor);
//    w->setValue(value);
}

void SliderLineEditDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
//    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
//    spinBox->interpretText();
//    int value = spinBox->value();

    int value = 17;

    model->setData(index, value, Qt::EditRole);
}

void SliderLineEditDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}

void SliderLineEditDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);
//    QSlider::paint
}
