#include "propertywidgets.h"

SliderEditor::SliderEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    isInitializing = true;
//    qDebug() << __FUNCTION__ << parent;
//    if (parent->is) parentObjectName = parent->objectName();

    int lineEditWidth = idx.data(UR_LabelFixedWidth).toInt();
    int min = idx.data(UR_Min).toInt();
    int max = idx.data(UR_Max).toInt();
    source = idx.data(UR_Source).toString();

    slider = new QSlider(Qt::Horizontal);
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setStyleSheet("QSlider {background: transparent;}"
         "QSlider::groove:horizontal {border:1px solid gray; height:1px;}"
         "QSlider::handle:horizontal {background:silver; width:4px; margin:-4px 0; height:10px;}"
         "QSlider::handle:focus {background:yellow;}");
    slider->setWindowFlags(Qt::FramelessWindowHint);
    slider->setAttribute(Qt::WA_TranslucentBackground);

    label = new QLabel;
    label->setMaximumWidth(lineEditWidth);
    label->setWindowFlags(Qt::FramelessWindowHint);
    label->setAttribute(Qt::WA_TranslucentBackground);

    connect(slider, &QSlider::valueChanged, this, &SliderEditor::change);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(slider, Qt::AlignLeft);
    layout->addWidget(label, Qt::AlignRight);
    layout->addSpacing(15);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);

    isInitializing = false;
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
    QVariant v = value;
//    qDebug() << __FUNCTION__ /*<< parentObjectName*/ << isInitializing << v << source;
    emit editorValueChanged(v, source);
    label->setText(QString::number(value));
}
