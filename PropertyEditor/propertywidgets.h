#ifndef PROPERTYWIDGETS_H
#define PROPERTYWIDGETS_H

#include <QtWidgets>

enum DelegateType
{
    DT_None,
    DT_Text,
    DT_Spinbox,
    DT_Checkbox,
    DT_Combo,
    DT_Slider
};

enum UserRole
{
    UR_DelegateType = Qt::UserRole + 1, // the PropertyWidget (custom widget)
    UR_Source,                          // name of property/variable being edited
    UR_Type,                            // the data type required by the delegate
    UR_Min,                             // validate minimum value
    UR_Max,                             // validate maximum value
    UR_LabelFixedWidth,                 // fixed label width in custom widget
    UR_LineEditFixedWidth,              // fixed lineedit width in custom widget
    UR_StringList                       // list of items for comboBox
};

class SliderEditor : public QWidget
{
    Q_OBJECT
public:
    SliderEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    int value();

signals:
    void editorValueChanged(QVariant value, QString source);
    void enableGoKeyActions(bool ok);

private:
    void change(int value);
    void updateSliderWhenLineEdited(QString value);
    QSlider *slider;
    QLineEdit *lineEdit;
    QString source;
};

class SpinBoxEditor : public QWidget
{
    Q_OBJECT
public:
    SpinBoxEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    int value();

signals:
    void editorValueChanged(QVariant value, QString source);
    void enableGoKeyActions(bool ok);

private:
    void change(int value);
    QSpinBox *spinBox;
    QString source;
};

class CheckBoxEditor : public QWidget
{
    Q_OBJECT
public:
    CheckBoxEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    bool value();

signals:
    void editorValueChanged(QVariant value, QString source);
    void enableGoKeyActions(bool ok);

private:
    void change(int value);
//    void updateSliderWhenLineEdited(QString value);
    QCheckBox *checkBox;
    QString source;
};

class ComboBoxEditor : public QWidget
{
    Q_OBJECT
public:
    ComboBoxEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    QString value();

signals:
    void editorValueChanged(QVariant value, QString source);
    void enableGoKeyActions(bool ok);
    
private:
    void change(int index);
    QComboBox *comboBox;
    QString source;
};

#endif // PROPERTYWIDGETS_H
