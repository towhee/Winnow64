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
    /* Accept click and Tab focus so the Develop arrow-nudge keys (Left/Right, plus
       PageUp/PageDown for the larger step) land on this slider. Image navigation is
       gated on the main window holding focus, so a focused slider keeps the arrows. */
    setFocusPolicy(Qt::StrongFocus);
}

void Slider::mousePressEvent(QMouseEvent *event)
{
    QSlider::mousePressEvent(event);
    /* Take keyboard focus on click so arrows immediately nudge this slider. */
    setFocus(Qt::MouseFocusReason);
    int min = minimum();
    int max = maximum();
    int value = event->pos().x() * 1.0 / width() * (max - min) + min;
    setValue(value);
    /*
    qDebug() << "Slider::mousePressEvent"
             << event->pos().x()
             << width()
             << minimum()
             << maximum()
             << div
             << min
             << max
             << value
                ;
                //*/
}

void Slider::keyPressEvent(QKeyEvent *event)
{
    int s = singleStep() ? singleStep() : 1;
    int ps = pageStep() ? pageStep() : s * 10;
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        setValue(value() - s);   event->accept(); return;
    case Qt::Key_Right:
    case Qt::Key_Up:
        setValue(value() + s);   event->accept(); return;
    case Qt::Key_PageDown:
        setValue(value() - ps);  event->accept(); return;
    case Qt::Key_PageUp:
        setValue(value() + ps);  event->accept(); return;
    }
    QSlider::keyPressEvent(event);
}

void Slider::focusInEvent(QFocusEvent *event)
{
    QSlider::focusInEvent(event);
    if (parentWidget()) parentWidget()->update();   // draw the SliderEditor focus cue
}

void Slider::focusOutEvent(QFocusEvent *event)
{
    QSlider::focusOutEvent(event);
    if (parentWidget()) parentWidget()->update();   // clear the SliderEditor focus cue
}

/* SLIDER EDITOR *****************************************************************************/

SliderEditor::SliderEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
/*
    The SliderEditor works within a range of integer values defined by min and max. The
    lineEdit control shows the slider value, but it can be edited to enter a number
    outside the slider range.

    When a real value is required, the range is increased by factor "div" and the
    lineEdit shows the slider value divided by "div". The value in the lineEdit is
    returned as the sliderEditor value.

    Integer mode: div == 0
    Double mode : div != 0
*/
    if (G::isLogger) G::log("SliderEditor::SliderEditor");
    this->idx = idx;
    int lineEditWidth = idx.data(UR_FixedWidth).toInt();
    int min = idx.data(UR_Min).toInt();
    int max = idx.data(UR_Max).toInt();
    QString sCol = idx.data(UR_Color).toString();
    QString eCol = idx.data(UR_Color1).toString();
    // qDebug() << "sCol = " << sCol;
    if (sCol.isEmpty()) sCol = "red";
    if (eCol.isEmpty()) eCol = "white";
    // slider groove background
    QString bg = QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop: 0.0 %1, stop: 1.0 %2);")
                     .arg(sCol.trimmed(), eCol.trimmed());
    // divisor if converting integer slider value to double
    div = idx.data(UR_Div).toInt();
    int step = idx.data(UR_DivPx).toInt();
    QString type = idx.data(UR_Type).toString();
    type == "int" || div == 0 ? isInt = true : isInt = false;
    if (div == 0) div = 1;
    source = idx.data(UR_Source).toString();

    slider = new Slider(Qt::Horizontal, div);
    slider->setObjectName("DisableGoActions");  // used in MW::focusChange
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setSingleStep(step);
    slider->setPageStep(step * 10);
    slider->setStyleSheet(
         "QSlider {"
            "background: transparent;"
            "margin-left:2;"
         "}"
         "QSlider::sub-page:horizontal {"
            // "background:#1571d3;"   // apple blue, left side to the slider handle
            "height:3px;"
         "}"
         "QSlider::add-page:horizontal {"
             "background:transparent;"
             "height:3px;"
         "}"
         "QSlider::groove:horizontal {" +
            bg +
            // "background:qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 green, stop:1 magenta);"  // not working
            // "background: #646464;" // gray
            "border:none;"
            "height:3px;"
         "}"
         "QSlider::handle:horizontal {"
            "background:solid gray;"
            "width:14px;"
            "margin:-6px 0;"
            "height:8px;"
            "border-radius: 7px;"
         "}"
         "QSlider::handle:focus, QSlider::handle:hover{"
            "background:#1571d3;"
         "}"
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

    /* Route focus given to the editor container onto the slider, so clicking or tabbing
       into the row lets the arrow keys nudge the slider (and paints the focus cue). */
    setFocusProxy(slider);

    outOfRange = false;
    int sliderValue = static_cast<int>(idx.data(Qt::EditRole).toDouble() * div);
    slider->setValue(sliderValue);
    emit slider->valueChanged(sliderValue);
}

double SliderEditor::value()
{
    if (G::isLogger) G::log("SliderEditor::value");
    return lineEdit->text().toDouble();
}

void SliderEditor::setValue(QVariant value)
{
    if (G::isLogger) G::log("SliderEditor::setValue");
    slider->setValue(value.toInt());
}

void SliderEditor::sliderMoved()
{
    if (G::isLogger) G::log("SliderEditor::sliderMoved");
    outOfRange = false;
}

void SliderEditor::change(double value)
{
    if (G::isLogger) G::log("SliderEditor::change");
    if (outOfRange) return;
    double v = static_cast<double>(value) / div;
    if (isInt) {
        lineEdit->setText(QString::number(v));
    }
    else {
        lineEdit->setText(QString::number(v, 'f', 2));
    }
    emit editorValueChanged(this);
}

void SliderEditor::updateSliderWhenLineEdited()
{
    if (G::isLogger) G::log("SliderEditor::updateSliderWhenLineEdited");
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
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void SliderEditor::paintEvent(QPaintEvent *event)
{
    /* Only re-polish when the desired stylesheet actually changes (setStyleSheet is
       expensive and would otherwise churn on every repaint). */
    const QString want = outOfRange ? G::cssError : QString();
    if (styleSheet() != want) setStyleSheet(want);
    QWidget::paintEvent(event);

    /* Focus cue: mark the slider the Develop arrow-nudge keys will act on. The slider and
       lineEdit are translucent, so the fill drawn here shows around them. */
    if (slider->hasFocus()) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x15, 0x71, 0xd3, 40));   // subtle accent fill
        p.drawRoundedRect(rect().adjusted(0, 1, -1, -1), 3, 3);
        p.setBrush(QColor(0x15, 0x71, 0xd3));        // solid accent bar, left edge
        p.drawRect(0, 0, 2, height());
    }
}

/* LABEL EDITOR ******************************************************************************/

LabelEditor::LabelEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("LabelEditor::LabelEditor");
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
    if (G::isLogger) G::log("LabelEditor::value");
    return label->text();
}

void LabelEditor::setValue(QVariant value)
{
    if (G::isLogger) G::log("LabelEditor::setValue");
    label->setText(value.toString());
}

void LabelEditor::fontSizeChanged(int fontSize)
{
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void LabelEditor::paintEvent(QPaintEvent */*event*/)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}


/* LINEEDIT EDITOR ***************************************************************************/

LineEditor::LineEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("LineEditor::LineEditor");
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
    if (G::isLogger) G::log("LineEditor::value");
    return lineEdit->text();
}

void LineEditor::setValue(QVariant value)
{
    if (G::isLogger) G::log("LineEditor::setValue");
    lineEdit->setText(value.toString());
}

void LineEditor::change(/*QString value*/)
{
    if (G::isLogger) G::log("LineEditor::change");
//    QVariant v = value;
//    emit editorValueChanged(this);
    emit editorValueChanged(this);
}

void LineEditor::fontSizeChanged(int fontSize)
{
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void LineEditor::paintEvent(QPaintEvent */*event*/)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* SPINBOX EDITOR ****************************************************************************/

SpinBoxEditor::SpinBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("SpinBoxEditor::SpinBoxEditor");
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
    if (G::isLogger) G::log("SpinBoxEditor::value");
    return spinBox->value();
}

void SpinBoxEditor::setValue(QVariant value)
{
    if (G::isLogger) G::log("SpinBoxEditor::setValue");
    spinBox->setValue(value.toInt());
}

void SpinBoxEditor::change(/*int value*/)
{
    if (G::isLogger) G::log("SpinBoxEditor::change");
//    QVariant v = value;
    emit editorValueChanged(this);
}

void SpinBoxEditor::fontSizeChanged(int fontSize)
{
//    setStyleSheet(G::css);
    qDebug() << "SpinBoxEditor::fontSizeChanged" << fontSize;
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void SpinBoxEditor::paintEvent(QPaintEvent */*event*/)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* DOUBLESPINBOX EDITOR **********************************************************************/

DoubleSpinBoxEditor::DoubleSpinBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("DoubleSpinBoxEditor::DoubleSpinBoxEditor");
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

   // connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double i){change(i);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addSpacing(3);
    layout->addWidget(doubleSpinBox/*, Qt::AlignLeft*/);
    // layout->addSpacing(20);
    // layout->addStretch();
    layout->addWidget(label/*, Qt::AlignRight*/);
    // layout->addWidget(btn);
    layout->setContentsMargins(G::propertyWidgetMarginLeft - 2, 0, G::propertyWidgetMarginRight, 0);
    setLayout(layout);

    doubleSpinBox->setValue(idx.data(Qt::EditRole).toDouble());
    submitted = false;
}

double DoubleSpinBoxEditor::value()
{
    if (G::isLogger) G::log("DoubleSpinBoxEditor::value");
    return doubleSpinBox->value();
}

void DoubleSpinBoxEditor::setValue(QVariant value)
{
    if (G::isLogger) G::log("DoubleSpinBoxEditor::setValue");
    qDebug() << "DoubleSpinBoxEditor::setValue" << value;
    doubleSpinBox->setValue(value.toDouble());
}

//void DoubleSpinBoxEditor::change(double value)
//{
//    QVariant v = value;
//    emit editorValueChanged(this);
//}

void DoubleSpinBoxEditor::fontSizeChanged(int fontSize)
{
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void DoubleSpinBoxEditor::paintEvent(QPaintEvent */*event*/)
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
    if (G::isLogger) G::log("CheckBoxEditor::CheckBoxEditor");
    this->idx = idx;
    source = idx.data(UR_Source).toString();
    checkBox = new QCheckBox(parent);
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
    if (G::isLogger) G::log("CheckBoxEditor::value");
    return checkBox->isChecked();
}

void CheckBoxEditor::setValue(QVariant value)
{
    if (G::isLogger) G::log("CheckBoxEditor::setValue");
    checkBox->setChecked(value.toBool());
}

void CheckBoxEditor::change()
{
    if (G::isLogger) G::log("CheckBoxEditor::change");
    emit editorValueChanged(this);
}

void CheckBoxEditor::fontSizeChanged(int fontSize)
{
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void CheckBoxEditor::paintEvent(QPaintEvent */*event*/)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* COMBOBOX EDITOR ***************************************************************************/

ComboBoxEditor::ComboBoxEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("ComboBoxEditor::ComboBoxEditor");
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
    view_palette.setColor(QPalette::Active, QPalette::Window, QColor(77,77,77));
//    view_palette.setColor(QPalette::Active, QPalette::Background, QColor(77,77,77));  // rgh 2022-01-22
    comboBox->view()->setPalette(view_palette);

    comboBox->setWindowFlags(Qt::FramelessWindowHint);
    comboBox->setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [=](int index){change(index);});

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(comboBox, Qt::AlignLeft);
    layout->setContentsMargins(G::propertyWidgetMarginLeft,0,G::propertyWidgetMarginRight,0);

    /* Optional trailing buttons: any BarBtns queued in the global `btns` vector before this editor
       is created are placed after the combo (e.g. the Develop "Select layer" combo's [M] mask
       menu button). Same drain-and-clear idiom as BarBtnEditor, so only the editor created
       immediately after the queue picks them up. */
    for (int b = 0; b < btns.size(); ++b) layout->addWidget(btns.at(b));
    btns.clear();

    setLayout(layout);

    if (idx.data(UR_IconList).toStringList().length() == 0)
        comboBox->addItems(idx.data(UR_StringList).toStringList());
    else {
        QStringList iconList = idx.data(UR_IconList).toStringList();
        QStringList textList = idx.data(UR_StringList).toStringList();
        qDebug() << "ComboBoxEditor::ComboBoxEditor" << textList << iconList;
        for (int i = 0; i < iconList.length(); ++i) {
            comboBox->addItem(QIcon(iconList.at(i)), textList.at(i));
        }
    }
    comboBox->setCurrentText(idx.data(Qt::EditRole).toString());
}

QString ComboBoxEditor::value()
{
    if (G::isLogger) G::log("ComboBoxEditor::value");
    // crash here when rename in token editor sometimes
    return comboBox->currentText();
}

void ComboBoxEditor::setValue(QVariant value)
{
    if (G::isLogger) G::log("ComboBoxEditor::setValue");
    int i = comboBox->findText(value.toString());
    comboBox->setCurrentIndex(i);
}

void ComboBoxEditor::addItem(QString item, QIcon icon)
{
/*
This is used to add new templates to the template, style, general,
border, text and graphic dropdown lists
*/
    if (G::isLogger) G::log("ComboBoxEditor::addItem");
    if (icon.isNull()) comboBox->addItem(item);
    else comboBox->addItem(icon, item);
}

void ComboBoxEditor::removeItem(QString item)
{
/*
    This is used to remove deleted templates from the template drop list, styles from
    the style dropdowns
*/
    if (G::isLogger) G::log("ComboBoxEditor::removeItem");
    int i = comboBox->findText(item);
    comboBox->removeItem(i);
}

void ComboBoxEditor::renameItem(QString oldText, QString newText)
{
/*
    This is used to when a style is renamed and all the style lists need to be updated.
*/
    if (G::isLogger) G::log("ComboBoxEditor::renameItem");
    qDebug() << "ComboBoxEditor::renameItem" << oldText << newText;
    int i = comboBox->findText(oldText);
    comboBox->setItemText(i, newText);
    change(i);
}

void ComboBoxEditor::setRenamable(bool on)
{
/*
    Makes the combo's text editable so the user can rename the CURRENTLY SELECTED item in place.
    A committed edit (Enter / focus-out) that actually changes the text emits itemRenamed(old, new);
    the owner decides whether to accept it (and refreshes the list). Selecting a different item from
    the dropdown leaves the text matching that item, so it never reads as a rename.
*/
    if (G::isLogger) G::log("ComboBoxEditor::setRenamable");
    isRenamable = on;
    comboBox->setEditable(on);
    if (!on) return;

    comboBox->setInsertPolicy(QComboBox::NoInsert);     // Enter must not append a new item
    const QString clr = idx.data(UR_Color).toString();
    comboBox->lineEdit()->setStyleSheet("QLineEdit { background: transparent; border: none;"
                                        "color:" + clr + "; padding: 0px; }");

    connect(comboBox->lineEdit(), &QLineEdit::editingFinished, this, [=]() {
        const int i = comboBox->currentIndex();
        if (i < 0) return;
        const QString oldText = comboBox->itemText(i);
        const QString newText = comboBox->lineEdit()->text().trimmed();
        if (newText.isEmpty() || newText == oldText) {
            comboBox->lineEdit()->setText(oldText);     // restore the displayed name
            return;
        }
        /* Defer so the owner can rebuild the combo (refresh) outside this widget's own signal. */
        QTimer::singleShot(0, this, [=]() { emit itemRenamed(oldText, newText); });
    });
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
    if (G::isLogger) G::log("ComboBoxEditor::change");
    QVariant v = comboBox->itemText(index);
    emit editorValueChanged(this);
}

void ComboBoxEditor::fontSizeChanged(int fontSize)
{
//    setStyleSheet(G::css);
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void ComboBoxEditor::paintEvent(QPaintEvent */*event*/)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* PLUSMINUS EDITOR **************************************************************************/

PlusMinusEditor::PlusMinusEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("PlusMinusEditor::PlusMinusEditor");
    this->idx = idx;
    source = idx.data(UR_Source).toString();

    int maxHt = G::strFontSize.toInt() * G::ptToPx + 4;  // font size plus space in button

    QString pushBtnStyle =
        "QPushButton {"
            "min-width: 25px;"
        "}"
        ;
    minusBtn = new QPushButton;
    minusBtn->setStyleSheet(pushBtnStyle);
    minusBtn->setObjectName("DisableGoActions");  // used in MW::focusChange
    minusBtn->setMaximumHeight(maxHt);
    minusBtn->setText("-");

    plusBtn = new QPushButton;
    plusBtn->setStyleSheet(pushBtnStyle);
    plusBtn->setObjectName("DisableGoActions");  // used in MW::focusChange
    plusBtn->setMaximumHeight(maxHt);
    plusBtn->setText("+");

    connect(minusBtn, &QPushButton::clicked, this, &PlusMinusEditor::minusChange);
    connect(plusBtn, &QPushButton::clicked, this, &PlusMinusEditor::plusChange);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(minusBtn, Qt::AlignLeft);
    layout->addWidget(plusBtn, Qt::AlignLeft);
    layout->addSpacing(55);
    layout->setContentsMargins(G::propertyWidgetMarginLeft,0,G::propertyWidgetMarginRight,0);
    setLayout(layout);
}

int PlusMinusEditor::value()
{
    if (G::isLogger) G::log("PlusMinusEditor::value");
    return plusMinus;
}

void PlusMinusEditor::minusChange()
{
    if (G::isLogger) G::log("PlusMinusEditor::minusChange");
    plusMinus = -1;
    emit editorValueChanged(this);
}

void PlusMinusEditor::plusChange()
{
    if (G::isLogger) G::log("PlusMinusEditor::plusChange");
    plusMinus = 1;
    emit editorValueChanged(this);
}

void PlusMinusEditor::fontSizeChanged(int fontSize)
{
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void PlusMinusEditor::paintEvent(QPaintEvent *event)
{
}

/* BARBTN EDITOR **************************************************************************/
QVector<BarBtn*> btns;
BarBtnEditor::BarBtnEditor(const QModelIndex, QWidget *parent)
    : QWidget(parent)
{
    if (G::isLogger) G::log("BarBtnEditor::BarBtnEditor");
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0,2,6,2);        // small right inset so buttons don't touch the edge
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignRight);
    for (int i = 0; i < btns.size(); ++i) {
        layout->addWidget(btns.at(i));
    }
    setLayout(layout);
    btns.clear();
}

void BarBtnEditor::paintEvent(QPaintEvent */*event*/)
{
//    setStyleSheet("font-size: " + G::fontSize + "pt;");
//    QWidget::paintEvent(event);
}

/* COLOR EDITOR ******************************************************************************/

ColorEditor::ColorEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("ColorEditor::ColorEditor");
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
    if (G::isLogger) G::log("ColorEditor::value");
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
    if (G::isLogger) G::log("ColorEditor::dlgColorChanged");
    lineEdit->setText(color.name());
}

void ColorEditor::dlgColorClose(int /*result*/)
{
    colorDlg->close();
}

void ColorEditor::setValueFromColorDlg()
{
/*
    colorDlg signals to dlgColorChanged when the color changes so we can we the changes
    without closing the dialog or an more interactive experience.
*/
    if (G::isLogger) G::log("ColorEditor::setValueFromColorDlg");
    colorDlg->setCurrentColor(QColor(lineEdit->text()));
    colorDlg->open();
}

void ColorEditor::updateLabelWhenLineEdited(QString value)
{
    if (G::isLogger) G::log("ColorEditor::updateLabelWhenLineEdited");
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
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void ColorEditor::paintEvent(QPaintEvent */*event*/)
{
}

/* SELECTFOLDER EDITOR ***********************************************************************/

SelectFolderEditor::SelectFolderEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("SelectFolderEditor::SelectFolderEditor");
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
    if (G::isLogger) G::log("SelectFolderEditor::value");
    return lineEdit->text();
}

void SelectFolderEditor::setValue(QVariant value)
{
    lineEdit->setText(value.toString());
}

void SelectFolderEditor::change(QString value)
{
    if (G::isLogger) G::log("SelectFolderEditor::change");
    QVariant v = value;
    emit editorValueChanged(this);
}

void SelectFolderEditor::setValueFromSaveFileDlg()
{
    if (G::isLogger) G::log("SelectFolderEditor::setValueFromSaveFileDlg");
    QString path = QFileDialog::getExistingDirectory
            (this, tr("Select or create folder"), "/home",
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    lineEdit->setText(path);
}

void SelectFolderEditor::fontSizeChanged(int fontSize)
{
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void SelectFolderEditor::paintEvent(QPaintEvent */*event*/)
{
}

/* SELECTFILE EDITOR *************************************************************************/

SelectFileEditor::SelectFileEditor(const QModelIndex &idx, QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("SelectFileEditor::SelectFileEditor");
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
    if (G::isLogger) G::log("SelectFileEditor::value");
    return lineEdit->text();
}

void SelectFileEditor::change(QString value)
{
    if (G::isLogger) G::log("SelectFileEditor::change");
    QVariant v = value;
    emit editorValueChanged(this);
}

void SelectFileEditor::setValue(QVariant value)
{
    lineEdit->setText(value.toString());
}

void SelectFileEditor::setValueFromSaveFileDlg()
{
    if (G::isLogger) G::log("SelectFileEditor::setValueFromSaveFileDlg");
    QString path = QFileDialog::getOpenFileName(this, tr("Select file"), "/home");
    lineEdit->setText(path);
}

void SelectFileEditor::fontSizeChanged(int fontSize)
{
    QFont f = font();
    f.setPointSize(fontSize);
    setFont(f);
}

void SelectFileEditor::paintEvent(QPaintEvent */*event*/)
{
}
