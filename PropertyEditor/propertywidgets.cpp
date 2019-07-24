#include "propertywidgets.h"
#include "Main/mainwindow.h"
#include "Main/global.h"

CONST int propertyWidgetMargin = 10;

/*
Slider
Combo
LineEdit
Check
Text
*/

/* SLIDER EDITOR *****************************************************************************/

SliderEditor::SliderEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    this->idx = idx;
    int lineEditWidth = idx.data(UR_LabelFixedWidth).toInt();
    int min = idx.data(UR_Min).toInt();
    int max = idx.data(UR_Max).toInt();
    source = idx.data(UR_Source).toString();

    slider = new QSlider(Qt::Horizontal);
    slider->setObjectName("DisableGoActions");  // used in MW::focusChange
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setStyleSheet("QSlider {background: transparent;}"
         "QSlider::groove:horizontal {border:1px solid gray; height:1px;}"
         "QSlider::handle:horizontal {background:silver; width:6px; margin:-6px 0; height:10px;}"
         "QSlider::handle:focus, QSlider::handle:hover{background:red;}");
    slider->setWindowFlags(Qt::FramelessWindowHint);
    slider->setAttribute(Qt::WA_TranslucentBackground);

    lineEdit = new QLineEdit;
    lineEdit->setObjectName("DisableGoActions");    // used in MW::focusChange
    lineEdit->setMaximumWidth(lineEditWidth);
    lineEdit->setStyleSheet("QLineEdit {background: transparent; border:none;}");
    lineEdit->setWindowFlags(Qt::FramelessWindowHint);
    lineEdit->setAttribute(Qt::WA_TranslucentBackground);
    QValidator *validator = new QIntValidator(min, max, this);
    lineEdit->setValidator(validator);

    connect(slider, &QSlider::valueChanged, this, &SliderEditor::change);
    connect(lineEdit, &QLineEdit::textEdited, this, &SliderEditor::updateSliderWhenLineEdited);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(slider, Qt::AlignLeft);
    layout->addWidget(lineEdit, Qt::AlignRight);
    layout->setContentsMargins(propertyWidgetMargin,0,propertyWidgetMargin,0);
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
    QVariant v = value;
    emit editorValueChanged(v, source, idx);
    lineEdit->setText(QString::number(value));
}

void SliderEditor::updateSliderWhenLineEdited(QString value)
{
    slider->setValue(value.toInt());
}

/* SPINBOX EDITOR ****************************************************************************/

SpinBoxEditor::SpinBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    this->idx = idx;
    int min = idx.data(UR_Min).toInt();
    int max = idx.data(UR_Max).toInt();
    source = idx.data(UR_Source).toString();

    spinBox = new QSpinBox;
    spinBox->setObjectName("DisableGoActions");  // used in MW::focusChange
    spinBox->setMinimum(min);
    spinBox->setMaximum(max);
    spinBox->setStyleSheet("QSpinBox {background:transparent; border:none;"
                           "padding:0px;border-radius:0px;}");
    spinBox->setWindowFlags(Qt::FramelessWindowHint);
    spinBox->setAttribute(Qt::WA_TranslucentBackground);

    QLabel *label = new QLabel;
    label->setStyleSheet("QLabel {}");
    label->setText("(" + QString::number(min) + "-" + QString::number(max) + ")");
    label->setDisabled(true);
    label->setWindowFlags(Qt::FramelessWindowHint);
    label->setAttribute(Qt::WA_TranslucentBackground);

    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), [=](int i){change(i);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(spinBox, Qt::AlignLeft);
    layout->addSpacing(20);
    layout->addWidget(label);
    layout->setContentsMargins(propertyWidgetMargin - 2, 0, propertyWidgetMargin, 0);
    setLayout(layout);

    spinBox->setValue(idx.data(Qt::EditRole).toInt());
}

int SpinBoxEditor::value()
{
    return spinBox->value();
}

void SpinBoxEditor::setValue(QVariant value)
{
    spinBox->setValue(value.toInt());
}

void SpinBoxEditor::change(int value)
{
    QVariant v = value;
    emit editorValueChanged(v, source, idx);
}

/* CHECKBOX EDITOR ***************************************************************************/

CheckBoxEditor::CheckBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    checkBox = new QCheckBox;
    checkBox->setObjectName("DisableGoActions");  // used in MW::focusChange
    checkBox->setStyleSheet("QCheckBox {background: transparent; border:none;}");
    checkBox->setWindowFlags(Qt::FramelessWindowHint);
    checkBox->setAttribute(Qt::WA_TranslucentBackground);

    connect(checkBox, &QCheckBox::stateChanged, this, &CheckBoxEditor::change);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(checkBox, Qt::AlignLeft);
    layout->setContentsMargins(propertyWidgetMargin,0,propertyWidgetMargin,0);
    setLayout(layout);

    checkBox->setChecked(idx.data(Qt::EditRole).toBool());
}

bool CheckBoxEditor::value()
{
    if (checkBox->checkState() == Qt::Checked) return true;
    else return false;
}

void CheckBoxEditor::setValue(QVariant value)
{
    checkBox->setChecked(value.toBool());
}

void CheckBoxEditor::change(int value)
{
    QVariant v;
    if (value == Qt::Checked) v = true;
    else v = false;
    emit editorValueChanged(v, source, idx);
}

/* COMBOBOX EDITOR ***************************************************************************/

ComboBoxEditor::ComboBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    comboBox = new QComboBox;
    comboBox->setObjectName("DisableGoActions");  // used in MW::focusChange
    comboBox->setStyleSheet("QComboBox {background: transparent;"
                            "border:none; padding: 0px 0px 0px 0px;}"
                            "QComboBox::drop-down {border:none;}");
    comboBox->setWindowFlags(Qt::FramelessWindowHint);
    comboBox->setAttribute(Qt::WA_TranslucentBackground);

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [=](int index){change(index);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(comboBox, Qt::AlignLeft);
    layout->setContentsMargins(propertyWidgetMargin,0,propertyWidgetMargin,0);
    setLayout(layout);

    comboBox->addItems(idx.data(UR_StringList).toStringList());
}

QString ComboBoxEditor::value()
{
    return comboBox->currentText();
}

void ComboBoxEditor::setValue(QVariant value)
{
    int i = comboBox->findText(value.toString());
    comboBox->setCurrentIndex(i);
}

void ComboBoxEditor::change(int index)
{    
    QVariant v = comboBox->itemText(index);
    emit editorValueChanged(v, source, idx);
}
