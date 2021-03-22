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

/* SLIDER ************************************************************************************/
Slider::Slider(Qt::Orientation orientation, int div, QWidget *parent) : QSlider(parent)
{
    setOrientation(orientation);
    this->div = div;
}

void Slider::mousePressEvent(QMouseEvent *event)
{
    QSlider::mousePressEvent(event);
    int min = minimum();
    int max = maximum();
    int value = event->pos().x() * 1.0 / width() * (max - min) + min;
    setValue(value);
//    qDebug() << __FUNCTION__
//             << event->pos().x()
//             << width()
//             << minimum()
//             << maximum()
//             << div
//             << min
//             << max
//             << value
//                ;
}

/* SLIDER EDITOR *****************************************************************************/

SliderEditor::SliderEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
/*
The SliderEditor works within a range of integer values defined by min and max.  The
lineEdit control shows the slider value, but it can be edited to enter a number outside
the slider range.

When a real value is required, the range is increased by factor "div" and the lineEdit
shows the slider value divided by "div".  The value in the lineEdit is returned as the
sliderEditor value.

Integer mode: div == 0
Double mode : div != 0
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->idx = idx;
    int lineEditWidth = idx.data(UR_FixedWidth).toInt();
    int min = idx.data(UR_Min).toInt();
    int max = idx.data(UR_Max).toInt();
    // divisor if converting integer slider value to double
    div = idx.data(UR_Div).toInt();
    int step = idx.data(UR_DivPx).toInt();
    QString type = idx.data(UR_Type).toString();
    type == "int" || div == 0 ? isInt = true : isInt = false;
    if (div == 0) div = 1;
    source = idx.data(UR_Source).toString();

    slider = new Slider(Qt::Horizontal, div);
//    slider = new QSlider(Qt::Horizontal);
    slider->setObjectName("DisableGoActions");  // used in MW::focusChange
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setSingleStep(step);
    slider->setPageStep(step * 10);
    slider->setStyleSheet(
         "QSlider {background: transparent;}"
         "QSlider::groove:horizontal {border:1px solid gray; height:1px;}"
         "QSlider::handle:horizontal {background:silver; width:6px; margin:-6px 0; height:8px;}"
         "QSlider::handle:focus, QSlider::handle:hover{background:red;}"
    );
    slider->setWindowFlags(Qt::FramelessWindowHint);
    slider->setAttribute(Qt::WA_TranslucentBackground);

    lineEdit = new QLineEdit;
    lineEdit->setObjectName("DisableGoActions");    // used in MW::focusChange
    lineEdit->setMaximumWidth(lineEditWidth);
    lineEdit->setAlignment(Qt::AlignRight);
    lineEdit->setStyleSheet("QLineEdit {background: transparent; border:none;}");
    lineEdit->setWindowFlags(Qt::FramelessWindowHint);
    lineEdit->setAttribute(Qt::WA_TranslucentBackground);

    connect(slider, &QSlider::valueChanged, this, &SliderEditor::change);
    connect(slider, &QSlider::sliderMoved, this, &SliderEditor::sliderMoved);
    connect(lineEdit, &QLineEdit::editingFinished, this, &SliderEditor::updateSliderWhenLineEdited);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(slider, Qt::AlignLeft);
    layout->addWidget(lineEdit, Qt::AlignRight);
    layout->setContentsMargins(G::propertyWidgetMarginLeft,0,G::propertyWidgetMarginRight,0);
    setLayout(layout);

    outOfRange = false;
    int sliderValue = static_cast<int>(idx.data(Qt::EditRole).toDouble() * div);
    slider->setValue(sliderValue);
    emit slider->valueChanged(sliderValue);
}

double SliderEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return lineEdit->text().toDouble();
//    return slider->value();
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

void SliderEditor::sliderMoved()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    outOfRange = false;
}

void SliderEditor::change(double value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (outOfRange) return;
    double v = static_cast<double>(value) / div;
    if (isInt) lineEdit->setText(QString::number(v));
    else lineEdit->setText(QString::number(v, 'f', 2));
    emit editorValueChanged(this);
}

void SliderEditor::updateSliderWhenLineEdited()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int sliderValue = static_cast<int>(lineEdit->text().toDouble() * div);
    if (sliderValue >= slider->minimum() && sliderValue <= slider->maximum()) {
        outOfRange = false;
        slider->setValue(sliderValue);
        slider->setValue(static_cast<int>(sliderValue));
        return;
    }
    outOfRange = true;
    if (sliderValue < slider->minimum()) slider->setValue(slider->minimum());
    else slider->setValue(slider->maximum());
//    emit editorValueChanged(this);
}


void SliderEditor::fontSizeChanged(int fontSize)
{
    qDebug() << __FUNCTION__ << fontSize;
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
}
void SliderEditor::paintEvent(QPaintEvent *event)
{
    QColor textColor = QColor(G::textShade,G::textShade,G::textShade);
    if (outOfRange)
        setStyleSheet("QLineEdit {color:red;}");
    else
        setStyleSheet("QLineEdit {color:" + textColor.name() + ";}");
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

void LabelEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
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

    connect(lineEdit, &QLineEdit::editingFinished, this, &LineEditor::change);
//    connect(lineEdit, &QLineEdit::textChanged, this, &LineEditor::change);

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

void LineEditor::change(/*QString value*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    QVariant v = value;
//    emit editorValueChanged(this);
    emit editorValueChanged(this);
}

void LineEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
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

    label = new QLabel;
    label->setStyleSheet("QLabel {}");
    label->setText("(" + QString::number(min) + "-" + QString::number(max) + ")");
    label->setDisabled(true);
    label->setWindowFlags(Qt::FramelessWindowHint);
    label->setAttribute(Qt::WA_TranslucentBackground);

    QPushButton *btn = new QPushButton;
    btn->setText("...");
    btn->setVisible(false);

    connect(spinBox, &QSpinBox::editingFinished, this, &SpinBoxEditor::change);
//    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), [=](int i){change(i);});

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

void SpinBoxEditor::change(/*int value*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    QVariant v = value;
    emit editorValueChanged(this);
}

void SpinBoxEditor::fontSizeChanged(int fontSize)
{
//    setStyleSheet(G::css);
    qDebug() << __FUNCTION__ << fontSize;
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
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

void DoubleSpinBoxEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
}

void DoubleSpinBoxEditor::paintEvent(QPaintEvent *event)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
}

// note that *object is reqd to instantiate event filter using "this"
bool DoubleSpinBoxEditor::eventFilter(QObject *object, QEvent *event)
{
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

void CheckBoxEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
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
    comboBox->setMaxVisibleItems(30);
    comboBox->setStyleSheet("QComboBox {"
                                "color:" + clr + ";"
                                "background: transparent;"
                                "border: none;"
                                "padding: 0px 0px 0px 0px;"
                            "}"
                            "QComboBox:focus {"
                                "color: white;"
                                "background: transparent;"
                                "padding: 0px 0px 0px 0px;"
                                "border: none;"
                            "}"
                            "QComboBox::item:selected {"
                                "background-color: rgb(68,95,118);"
                            "}"
                            "QComboBox::drop-down {"
                                "border:none;"
                            "}"
                            "QComboBox::QAbstractItemView {"
                            "}"
                            );

    QPalette palette = comboBox->palette();
    QPalette view_palette = comboBox->view()->palette();
    view_palette.setColor(QPalette::Active, QPalette::Background, QColor(77,77,77));
    comboBox->view()->setPalette(view_palette);

    comboBox->setWindowFlags(Qt::FramelessWindowHint);
    comboBox->setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [=](int index){change(index);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(comboBox, Qt::AlignLeft);
    layout->setContentsMargins(G::propertyWidgetMarginLeft,0,G::propertyWidgetMarginRight,0);
    setLayout(layout);

    if (idx.data(UR_IconList).toStringList().length() == 0)
        comboBox->addItems(idx.data(UR_StringList).toStringList());
    else {
        QStringList iconList = idx.data(UR_IconList).toStringList();
        QStringList textList = idx.data(UR_StringList).toStringList();
        qDebug() << __FUNCTION__ << textList << iconList;
        for (int i = 0; i < iconList.length(); ++i) {
            comboBox->addItem(QIcon(iconList.at(i)), textList.at(i));
        }
    }
    comboBox->setCurrentText(idx.data(Qt::EditRole).toString());
}

QString ComboBoxEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // crash here when rename in token editor sometimes
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

void ComboBoxEditor::addItem(QString item, QIcon icon)
{
/*
This is used to add new templates to the template, style, general,
border, text and graphic dropdown lists
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (icon.isNull()) comboBox->addItem(item);
    else comboBox->addItem(icon, item);
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

void ComboBoxEditor::renameItem(QString oldText, QString newText)
{
/*
    This is used to when a style is renamed and all the style lists need to be updated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__ << oldText << newText;
    int i = comboBox->findText(oldText);
    comboBox->setItemText(i, newText);
    change(i);
}

void ComboBoxEditor::refresh(QStringList items)
{
/*
    Called when a template is renamed or deleted to rebuild the dropdown list
*/
    comboBox->clear();
//    items.sort(Qt::CaseInsensitive);
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

void ComboBoxEditor::fontSizeChanged(int fontSize)
{
//    setStyleSheet(G::css);
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
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
    minusBtn->setStyleSheet(G::css);
    minusBtn->setObjectName("DisableGoActions");  // used in MW::focusChange
    minusBtn->setMaximumHeight(16);
    minusBtn->setText("-");

    plusBtn = new QPushButton;
    plusBtn->setStyleSheet(G::css);
    plusBtn->setObjectName("DisableGoActions");  // used in MW::focusChange
    plusBtn->setMaximumHeight(16);
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

void PlusMinusEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
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
    layout->setContentsMargins(0,2,0,2);
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
    /*
    btn->setStyleSheet(
        "QPushButton"
        "{"
            "background-color:#3f5f53;"
            "border: none;"
            "border-radius: 0px;"
            "padding: 0,0,0,0;"
            "margin-right: 4px;"
            "max-width: 50px;"
            "max-height: 10px;"
            "min-height: 10px;"
        "}"
        );
//    */
    btn->setMaximumHeight(10);

    colorDlg = new QColorDialog;
//    colorDlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(btn, &QPushButton::clicked, this, &ColorEditor::setValueFromColorDlg);
    connect(lineEdit, &QLineEdit::textChanged, this, &ColorEditor::updateLabelWhenLineEdited);
    connect(colorDlg, &QColorDialog::currentColorChanged, this, &ColorEditor::dlgColorChanged);
    connect(colorDlg, &QColorDialog::finished, this, &ColorEditor::dlgColorClose);

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

void ColorEditor::dlgColorChanged(const QColor &color)
{
/*
    colorDlg signals to dlgColorChanged when the color changes so we can we the changes
    without closing the dialog or an more interactive experience.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    lineEdit->setText(color.name());
}

void ColorEditor::dlgColorClose(int result)
{
    colorDlg->close();
}

void ColorEditor::setValueFromColorDlg()
{
/*
    colorDlg signals to dlgColorChanged when the color changes so we can we the changes
    without closing the dialog or an more interactive experience.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    colorDlg->setCurrentColor(QColor(lineEdit->text()));
    colorDlg->open();
}

void ColorEditor::updateLabelWhenLineEdited(QString value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    btn->setStyleSheet("QPushButton"
                        "{"
                            "background-color:" + value + ";"
                            "margin-right: 4px;"
                            "padding-left: 0px;"
                            "padding-right: 0px;"
                            "padding-top: 0px;"
                            "padding-bottom: 0px;"
                            "max-width: 50px;"
                            "min-width: 50px;"
                            "max-height: 10px;"
                            "min-height: 10px;"
                        "}"
                        "QPushButton:hover {"
                            "border: 1px solid yellow;"
                        "}"
                        );
    emit editorValueChanged(this);
}

void ColorEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
}

void ColorEditor::paintEvent(QPaintEvent *event)
{
}

/* SELECTFOLDER EDITOR ***********************************************************************/

SelectFolderEditor::SelectFolderEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    lineEdit = new QLineEdit;
    lineEdit->setObjectName("DisableGoActions");                // used in MW::focusChange
    lineEdit->setAlignment(Qt::AlignLeft);
    lineEdit->setStyleSheet("QLineEdit"
                            "{"
                                "background: transparent;"      // this works
                                "border:none;"                  // nada
                                "padding:0px;"
                            "}");
    lineEdit->setWindowFlags(Qt::FramelessWindowHint);
    lineEdit->setAttribute(Qt::WA_TranslucentBackground);
    lineEdit->setContextMenuPolicy(Qt::ActionsContextMenu);

    btn = new BarBtn;
    btn->setText("...");
    btn->setToolTip("Open browse folders dialog to create / select a folder.");

    connect(lineEdit, &QLineEdit::textChanged, this, &SelectFolderEditor::change);
    connect(btn, &QPushButton::clicked, this, &SelectFolderEditor::setValueFromSaveFileDlg);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(lineEdit, Qt::AlignLeft);
    layout->addWidget(btn);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);

    lineEdit->setText(idx.data(Qt::EditRole).toString());
}

QString SelectFolderEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return lineEdit->text();
}

void SelectFolderEditor::setValue(QVariant value)
{
    lineEdit->setText(value.toString());
}

void SelectFolderEditor::change(QString value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = value;
    emit editorValueChanged(this);
}

void SelectFolderEditor::setValueFromSaveFileDlg()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = QFileDialog::getExistingDirectory
            (this, tr("Select or create folder"), "/home",
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    lineEdit->setText(path);
}

void SelectFolderEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
}

void SelectFolderEditor::paintEvent(QPaintEvent *event)
{
}

/* SELECTFILE EDITOR *************************************************************************/

SelectFileEditor::SelectFileEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
    }
    lineEdit = new QLineEdit;
    lineEdit->setObjectName("DisableGoActions");                // used in MW::focusChange
    lineEdit->setAlignment(Qt::AlignLeft);
    lineEdit->setStyleSheet("QLineEdit"
                            "{"
                            "background: transparent;"      // this works
                            "border:none;"                  // nada
                            "padding:0px;"
                            "}");
    lineEdit->setWindowFlags(Qt::FramelessWindowHint);
    lineEdit->setAttribute(Qt::WA_TranslucentBackground);
    lineEdit->setContextMenuPolicy(Qt::ActionsContextMenu);

    btn = new BarBtn;
    btn->setText("...");
    btn->setToolTip("Open browse folders dialog to create / select a folder.");

    connect(lineEdit, &QLineEdit::textChanged, this, &SelectFileEditor::change);
    connect(btn, &QPushButton::clicked, this, &SelectFileEditor::setValueFromSaveFileDlg);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(lineEdit, Qt::AlignLeft);
    layout->addWidget(btn);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);

    lineEdit->setText(idx.data(Qt::EditRole).toString());
    lineEdit->setCursorPosition(0);
}

QString SelectFileEditor::value()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return lineEdit->text();
}

void SelectFileEditor::change(QString value)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = value;
    emit editorValueChanged(this);
}

void SelectFileEditor::setValue(QVariant value)
{
    lineEdit->setText(value.toString());
}

void SelectFileEditor::setValueFromSaveFileDlg()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = QFileDialog::getOpenFileName(this, tr("Select file"), "/home");
    lineEdit->setText(path);
}

void SelectFileEditor::fontSizeChanged(int fontSize)
{
    setStyleSheet("QWidget {font-size:" + QString::number(fontSize) + "pt;}");
}

void SelectFileEditor::paintEvent(QPaintEvent *event)
{
}
