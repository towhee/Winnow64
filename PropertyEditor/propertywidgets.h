#ifndef PROPERTYWIDGETS_H
#define PROPERTYWIDGETS_H

#include <QtWidgets>

enum DelegateType
{
    DT_None,        // 0
    DT_Text,        // 1
    DT_Spinbox,     // 2
    DT_Checkbox,    // 3
    DT_Combo,       // 4
    DT_Slider,      // 5
    DT_PlusMinus    // 6
};

enum UserRole
{
    UR_DelegateType = Qt::UserRole + 1, // the PropertyWidget (custom widget)
    UR_Source,                          // name of property/variable being edited
    UR_QModelIndex,                     // index from another model ie infoView->ok
    UR_Type,                            // the data type required by the delegate
    UR_Min,                             // validate minimum value
    UR_Max,                             // validate maximum value
    UR_LabelFixedWidth,                 // fixed label width in custom widget
    UR_LineEditFixedWidth,              // fixed lineedit width in custom widget
    UR_StringList,                      // list of items for comboBox
    UR_IsBtn,                           // button to run a secondary widget or help
    UR_BtnText                          // button text
};

//int propertyWidgetMarginLeft = 10;
//int propertyWidgetMarginRight = 15;

class SliderEditor : public QWidget
{
    Q_OBJECT
public:
    SliderEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    int value();

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    void change(int value);
    void updateSliderWhenLineEdited(QString value);
    QSlider *slider;
    QLineEdit *lineEdit;
    QString source;
    QModelIndex idx;
};

class SpinBoxEditor : public QWidget
{
    Q_OBJECT
public:
    SpinBoxEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    int value();

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    void change(int value);
    QSpinBox *spinBox;
    QString source;
    QModelIndex idx;
};

class CheckBoxEditor : public QWidget
{
    Q_OBJECT
public:
    CheckBoxEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    bool value();

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    void change();
    QCheckBox *checkBox;
    QString source;
    QModelIndex idx;
};

class ComboBoxEditor : public QWidget
{
    Q_OBJECT
public:
    ComboBoxEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    QString value();

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);
    
private:
    void change(int index);
    QComboBox *comboBox;
    QString source;
    QModelIndex idx;
};

class PlusMinusEditor : public QWidget
{
    Q_OBJECT
public:
    PlusMinusEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    int value();

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    void minusChange();
    void plusChange();
    int plusMinus;
    QPushButton *minusBtn;
    QPushButton *plusBtn;
    QString source;
    QModelIndex idx;
};

#endif // PROPERTYWIDGETS_H
