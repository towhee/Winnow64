#include "propertywidgets.h"
#include "Main/global.h"

/*
The property editor has four components:

PropertyEditor - A subclass of QTreeView, with a data model that holds the property
    information.

PropertyDelegate - Creates the editor widgets, sets the editor and model data and does some
    custom painting to each row in the treeview.

PropertyWidgets - A collection of custom widget editors (slider, combobox etc) that are
    inserted into column 1 in the treeview (PropertyEditor) by PropertyDelegate.  The editors
    are built based on information passed in the datamodel UserRole such as the DelegateType
    (Spinbox, Checkbox, Combo, Slider, PlusMinusBtns etc).  Other information includes the
    source, datamodel index, mim value, max value, a stringlist for combos etc).

PropertyEditor subclass ie Preferences.  All the property items are defined and added to the
    treeview in addItems().  When an item value has been changed in an editor itemChanged(idx)
    is signaled and the variable is updated in Winnow and any necessary actions are executed.
    For example, if the thumbnail size is increased then the IconView function to do this is
    called.
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
    layout->setContentsMargins(G::propertyWidgetMarginLeft,0,G::propertyWidgetMarginRight,0);
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
    emit editorValueChanged(this);
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
    spinBox->setAlignment(Qt::AlignLeft);
    spinBox->setMinimum(min);
    spinBox->setMaximum(max);
    spinBox->setStyleSheet("QSpinBox {background:transparent; border:none;"
                           "padding:0px; border-radius:0px;}");
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
    layout->setContentsMargins(G::propertyWidgetMarginLeft - 2, 0, G::propertyWidgetMarginRight, 0);
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
    emit editorValueChanged(this);
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
    layout->setContentsMargins(G::propertyWidgetMarginLeft,0,G::propertyWidgetMarginRight,0);
    setLayout(layout);

    checkBox->setChecked(idx.data(Qt::EditRole).toBool());
}

bool CheckBoxEditor::value()
{
    return checkBox->isChecked();
}

void CheckBoxEditor::setValue(QVariant value)
{
    checkBox->setChecked(value.toBool());
}

void CheckBoxEditor::change()
{
    emit editorValueChanged(this);
}

/* COMBOBOX EDITOR ***************************************************************************/

ComboBoxEditor::ComboBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    comboBox = new QComboBox;
    comboBox->setObjectName("DisableGoActions");  // used in MW::focusChange
//    comboBox->setView(new QListView);

    comboBox->setStyleSheet("QComboBox {"
                                "background: transparent;"
                                "border:none; "
                                "padding: 0px 0px 0px 0px;"
                            "}"
                            "QComboBox::item:selected {"
                                "background-color: rgb(68,95,118);"
                            "}"
//                            "QComboBox QAbstractItemView::item {"
//                                "height: 20px;"
//                                "background-color: rgb(77,77,77);"
//                            "}"
                            "QComboBox::drop-down {"
                                "border:none;"
                            "}"
                            );

    QPalette palette = comboBox->palette();
    QPalette view_palette = comboBox->view()->palette();
    view_palette.setColor(QPalette::Active, QPalette::Background, QColor(77,77,77));
    comboBox->view()->setPalette(view_palette);

    comboBox->setWindowFlags(Qt::FramelessWindowHint);
//    comboBox->setAttribute(Qt::WA_TranslucentBackground);

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [=](int index){change(index);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(comboBox, Qt::AlignLeft);
    layout->setContentsMargins(G::propertyWidgetMarginLeft,0,G::propertyWidgetMarginRight,0);
    setLayout(layout);
    comboBox->addItems(idx.data(UR_StringList).toStringList());
    comboBox->setCurrentText(idx.data(Qt::EditRole).toString());
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
    emit editorValueChanged(this);
}

/* PLUSMINUS EDITOR **************************************************************************/

PlusMinusEditor::PlusMinusEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    minusBtn = new QPushButton;
    minusBtn->setStyleSheet("QPushButton {color: white;}");
    minusBtn->setObjectName("DisableGoActions");  // used in MW::focusChange
    minusBtn->setText("-");

    plusBtn = new QPushButton;
    plusBtn->setObjectName("DisableGoActions");  // used in MW::focusChange
    plusBtn->setText("+");

    connect(minusBtn, &QPushButton::clicked, this, &PlusMinusEditor::minusChange);
    connect(plusBtn, &QPushButton::clicked, this, &PlusMinusEditor::plusChange);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(minusBtn, Qt::AlignLeft);
    layout->addWidget(plusBtn, Qt::AlignLeft);
    layout->addSpacing(55);
    layout->setContentsMargins(G::propertyWidgetMarginLeft,2,G::propertyWidgetMarginRight,2);
    setLayout(layout);
}

int PlusMinusEditor::value()
{
    return plusMinus;
}

void PlusMinusEditor::minusChange()
{
    plusMinus = -1;
    emit editorValueChanged(this);
}

void PlusMinusEditor::plusChange()
{
    plusMinus = 1;
    emit editorValueChanged(this);
}
