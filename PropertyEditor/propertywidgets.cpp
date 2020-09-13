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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->idx = idx;
    int lineEditWidth = idx.data(UR_LabelFixedWidth).toInt();
    int min = idx.data(UR_Min).toInt();
    int max = idx.data(UR_Max).toInt();
    source = idx.data(UR_Source).toString();

    slider = new QSlider(Qt::Horizontal);
    slider->setObjectName("DisableGoActions");  // used in MW::focusChange
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setStyleSheet(
         "QSlider {background: transparent;}"
         "QSlider::groove:horizontal {border:1px solid gray; height:1px;}"
         "QSlider::handle:horizontal {background:silver; width:4px; margin:-4px 0; height:8px;}"
         "QSlider::handle:focus, QSlider::handle:hover{background:red;}"
    );
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return slider->value();
}

void SliderEditor::setValue(QVariant value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    slider->setValue(value.toInt());
}

void SliderEditor::change(int value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = value;
    emit editorValueChanged(this);
    lineEdit->setText(QString::number(value));
}

void SliderEditor::updateSliderWhenLineEdited(QString value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    slider->setValue(value.toInt());
}

void SliderEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* LABEL EDITOR ******************************************************************************/

LabelEditor::LabelEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    label = new QLabel;

    label->setObjectName("DisableGoActions");  // used in MW::focusChange
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QString clr = idx.data(UR_Color).toString();
    label->setStyleSheet("QLabel "
                            "{"
                            "background:transparent;"
                            "border:none;"
                            "padding:0px;"
                            "margin-left:-4;"
//                            "border-radius:0px;"
                            "color:" + clr + ";"
                            "}"
                            "Qlabel:disabled {color:gray}"
                            );
    label->setWindowFlags(Qt::FramelessWindowHint);
    label->setAttribute(Qt::WA_TranslucentBackground);

//    connect(label, &Qlabel::textChanged, this, &labelor::change);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(label, Qt::AlignLeft);
    layout->setContentsMargins(G::propertyWidgetMarginLeft - 2, 0, G::propertyWidgetMarginRight, 0);
    setLayout(layout);

    label->setText(idx.data(Qt::EditRole).toString());
}

QString LabelEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return label->text();
}

void LabelEditor::setValue(QVariant value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    label->setText(value.toString());
}

void LabelEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}


/* LINEEDIT EDITOR ***************************************************************************/

LineEditor::LineEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    lineEdit = new QLineEdit;

    lineEdit->setObjectName("DisableGoActions");  // used in MW::focusChange
    lineEdit->setAlignment(Qt::AlignLeft);
    QString clr = idx.data(UR_Color).toString();
    lineEdit->setStyleSheet("QLineEdit "
                            "{"
                                "background:transparent;"
                                "border:none;"
                                "padding:0px;"
                                "border-radius:0px;"
                                "margin-left:0px;"
                                "color:" + clr +";"
                            "}"
                            "QlineEdit:disabled"
                            "{"
                                "color:gray;"
                            "}"
                            );
    lineEdit->setWindowFlags(Qt::FramelessWindowHint);
    lineEdit->setAttribute(Qt::WA_TranslucentBackground);

    connect(lineEdit, &QLineEdit::textChanged, this, &LineEditor::change);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(lineEdit, Qt::AlignLeft);
    layout->setContentsMargins(G::propertyWidgetMarginLeft - 2, 0, G::propertyWidgetMarginRight, 0);
    setLayout(layout);

    lineEdit->setText(idx.data(Qt::EditRole).toString());
}

QString LineEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return lineEdit->text();
}

void LineEditor::setValue(QVariant value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    lineEdit->setText(value.toString());
}

void LineEditor::change(QString value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = value;
    emit editorValueChanged(this);
}

void LineEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* SPINBOX EDITOR ****************************************************************************/

SpinBoxEditor::SpinBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
                           "padding:0px; border-radius:0px;}"
                           "margin-left:-7;"
                           "QSpinBox:disabled {color:gray}");
    spinBox->setWindowFlags(Qt::FramelessWindowHint);
    spinBox->setAttribute(Qt::WA_TranslucentBackground);

    QLabel *label = new QLabel;
    label->setStyleSheet("QLabel {}");
    label->setText("(" + QString::number(min) + "-" + QString::number(max) + ")");
    label->setDisabled(true);
    label->setWindowFlags(Qt::FramelessWindowHint);
    label->setAttribute(Qt::WA_TranslucentBackground);

    QPushButton *btn = new QPushButton;
    btn->setText("...");
    btn->setVisible(false);

    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), [=](int i){change(i);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(spinBox, Qt::AlignLeft);
    layout->addSpacing(20);
    layout->addWidget(label);
    layout->addWidget(btn);
    layout->setContentsMargins(G::propertyWidgetMarginLeft - 2, 0, G::propertyWidgetMarginRight, 0);
    setLayout(layout);

    spinBox->setValue(idx.data(Qt::EditRole).toInt());
}

int SpinBoxEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return spinBox->value();
}

void SpinBoxEditor::setValue(QVariant value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    spinBox->setValue(value.toInt());
}

void SpinBoxEditor::change(int value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = value;
    emit editorValueChanged(this);
}

void SpinBoxEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* DOUBLESPINBOX EDITOR **********************************************************************/

DoubleSpinBoxEditor::DoubleSpinBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
   this->idx = idx;
    double min = idx.data(UR_Min).toDouble();
    double max = idx.data(UR_Max).toDouble();
    source = idx.data(UR_Source).toString();

    doubleSpinBox = new QDoubleSpinBox;
    doubleSpinBox->setObjectName("DisableGoActions");  // used in MW::focusChange
//    doubleSpinBox->setAlignment(Qt::AlignLeft);
    doubleSpinBox->setMinimum(min);
    doubleSpinBox->setMaximum(max);
    // some tricky stuff to keep spinbox controls but our background and match alignment
    // of the other custom widgets
    doubleSpinBox->setStyleSheet("QDoubleSpinBox "
                                 "{"
                                     "background:transparent;"
                                     "border:none;"
//                                     "margin-top:-1;"                 // nada
//                                     "margin-bottom:-2;"
                                     "margin-left:-7;"
                                 "}"
                                  "QDoubleSpinBox:disabled {color:gray}"
                                 );
    doubleSpinBox->setWindowFlags(Qt::FramelessWindowHint);
    doubleSpinBox->setAttribute(Qt::WA_TranslucentBackground);
    doubleSpinBox->installEventFilter(this);
//    doubleSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    QLabel *label = new QLabel;
    label->setStyleSheet("QLabel {}");
    label->setText("(" + QString::number(min) + "-" + QString::number(max) + ")");
    label->setDisabled(true);
    label->setWindowFlags(Qt::FramelessWindowHint);
    label->setAttribute(Qt::WA_TranslucentBackground);
    label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    QPushButton *btn = new QPushButton;
    btn->setText("...");
    btn->setVisible(false);

//    connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double i){change(i);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addSpacing(3);
    layout->addWidget(doubleSpinBox/*, Qt::AlignLeft*/);
//    layout->addSpacing(20);
//    layout->addStretch();
    layout->addWidget(label/*, Qt::AlignRight*/);
//    layout->addWidget(btn);
    layout->setContentsMargins(G::propertyWidgetMarginLeft - 2, 0, G::propertyWidgetMarginRight, 0);
    setLayout(layout);

    doubleSpinBox->setValue(idx.data(Qt::EditRole).toDouble());
    submitted = false;
}

double DoubleSpinBoxEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return doubleSpinBox->value();
}

void DoubleSpinBoxEditor::setValue(QVariant value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__ << value;
    doubleSpinBox->setValue(value.toDouble());
}

//void DoubleSpinBoxEditor::change(double value)
//{
//    QVariant v = value;
//    emit editorValueChanged(this);
//}

void DoubleSpinBoxEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
}

// note that *object is reqd to instantiate event filter using "this"
bool DoubleSpinBoxEditor::eventFilter(QObject *object, QEvent *event)
{
    {
    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
    #endif
    }
    if (event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Enter || key == Qt::Key_Return || key == Qt::Key_Tab)
            emit editorValueChanged(this);
    }
    else if (event->type() == QEvent::FocusAboutToChange &&
             static_cast<QFocusEvent*>(event)->reason() == Qt::MouseFocusReason)
    {
        emit editorValueChanged(this);
    }
    return QWidget::eventFilter(object, event);
}

/* CHECKBOX EDITOR ***************************************************************************/

CheckBoxEditor::CheckBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return checkBox->isChecked();
}

void CheckBoxEditor::setValue(QVariant value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    checkBox->setChecked(value.toBool());
}

void CheckBoxEditor::change()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    emit editorValueChanged(this);
}

void CheckBoxEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* COMBOBOX EDITOR ***************************************************************************/

ComboBoxEditor::ComboBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    comboBox = new QComboBox;
    comboBox->setObjectName("DisableGoActions");  // used in MW::focusChange
    QString clr = idx.data(UR_Color).toString();
    comboBox->setStyleSheet("QComboBox {"
                                "color:" + clr + ";"
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return comboBox->currentText();
}

void ComboBoxEditor::setValue(QVariant value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int i = comboBox->findText(value.toString());
    comboBox->setCurrentIndex(i);
}

void ComboBoxEditor::addItem(QString item)
{
/*
This is used to add new templates to the template drop list, new styles to
the style dropdowns
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    comboBox->addItem(item);
}

void ComboBoxEditor::removeItem(QString item)
{
/*
This is used to remove deleted templates from the template drop list, styles from
the style dropdowns
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int i = comboBox->findText(item);
    comboBox->removeItem(i);
}

void ComboBoxEditor::refresh(QStringList items)
{
/*
Called when a template is renamed or deleted to rebuild the dropdown list
*/
    comboBox->clear();
    comboBox->addItems(items);
}

void ComboBoxEditor::change(int index)
{    
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = comboBox->itemText(index);
    emit editorValueChanged(this);
}

void ComboBoxEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* PLUSMINUS EDITOR **************************************************************************/

PlusMinusEditor::PlusMinusEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return plusMinus;
}

void PlusMinusEditor::minusChange()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    plusMinus = -1;
    emit editorValueChanged(this);
}

void PlusMinusEditor::plusChange()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    plusMinus = 1;
    emit editorValueChanged(this);
}

void PlusMinusEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* BARBTN EDITOR **************************************************************************/
QVector<BarBtn*> btns;
BarBtnEditor::BarBtnEditor(const QModelIndex, QWidget *parent)
    : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignRight);
    for (int i = 0; i < btns.size(); ++i) {
        layout->addWidget(btns.at(i));
    }
    setLayout(layout);
    btns.clear();
}

void BarBtnEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* COLOR EDITOR ******************************************************************************/

ColorEditor::ColorEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    lineEdit = new QLineEdit;
    lineEdit->setObjectName("DisableGoActions");    // used in MW::focusChange
    lineEdit->setAlignment(Qt::AlignLeft);
    lineEdit->setStyleSheet("QLineEdit"
                            "{"
                                "background: transparent;"      // this works
                                "border:none;"       // nada
                                "padding:0px;"
                            "}");
    lineEdit->setWindowFlags(Qt::FramelessWindowHint);
    lineEdit->setAttribute(Qt::WA_TranslucentBackground);

    btn = new QPushButton;
//    btn->setAutoFillBackground(true);
//    btn->setFlat(true);
    btn->setToolTip("Click here to open the color select dialog.");
    btn->setStyleSheet(
        "QPushButton, QPushButton:pressed, QPushButton:hover, QPushButton:flat"
        "{background-color:#3f5f53;"
        "margin-right: 4px;"
//        "max-width: 50px;"
        "max-height: 10px;"
        "min-height: 10px;"
        "}"
        );

    connect(btn, &QPushButton::clicked, this, &ColorEditor::setValueFromColorDlg);
    connect(lineEdit, &QLineEdit::textChanged, this, &ColorEditor::updateLabelWhenLineEdited);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(lineEdit, Qt::AlignLeft);
    layout->addWidget(btn);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);

    lineEdit->setText(idx.data(Qt::EditRole).toString());
}

QString ColorEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return lineEdit->text();
}

void ColorEditor::setValue(QVariant value)
{
    lineEdit->setText(value.toString());
}

void ColorEditor::setValueFromColorDlg()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QColor color = QColorDialog::getColor(QColor(lineEdit->text()));
    lineEdit->setText(color.name());
}

void ColorEditor::updateLabelWhenLineEdited(QString value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    btn->setStyleSheet("QPushButton, QPushButton:pressed, QPushButton:hover, QPushButton:flat"
                        "{background-color:" + value + ";"
                        "margin-right: 4px;"
//                        "max-width: 50px;"
                        "max-height: 10px;"
                        "min-height: 10px;"
                        "}"
                        );
    emit editorValueChanged(this);
}

void ColorEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

