#include "propertywidgets.h"

SliderEditor::SliderEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    this->idx = idx;
    int lineEditWidth = 50;
    int min = idx.data(UR_Min).toInt();
    int max = idx.data(UR_Max).toInt();
    slider = new QSlider(Qt::Horizontal);
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setStyleSheet("QSlider::groove:horizontal {"
                         "border: 1px solid gray;"
                          "height: 1px;"
                         "}"
                         "QSlider::handle:horizontal {"
                         "background: silver;"
                         "width: 4px;"
                         "margin: -4px 0;"
                         "height: 10px;"
                         "}"
                         "QSlider::handle:focus {"
                         "background: yellow;"
                         "}"
                          );

    label = new QLabel;
    label->setStyleSheet("QLabel {"
                         "background-color:rgb(77,77,77);"
                         "}"
                         );
    label->setMaximumWidth(lineEditWidth);

    connect(slider, &QSlider::valueChanged, this, &SliderEditor::change);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(slider, Qt::AlignLeft);
    layout->addWidget(label, Qt::AlignRight);
    layout->addSpacing(15);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);

    slider->setValue(idx.data(Qt::EditRole).toInt());
}

int SliderEditor::value()
{
    return slider->value();
}

void SliderEditor::setValue(QVariant value)
{
    slider->setValue(value.toInt());
}

void SliderEditor::change(int value)
{
    QString source = idx.data(UR_Source).toString();
    QVariant v = value;
    emit update(v, source);
    label->setText(QString::number(value));
}
