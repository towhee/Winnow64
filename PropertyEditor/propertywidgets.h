#ifndef PROPERTYWIDGETS_H
#define PROPERTYWIDGETS_H

#include <QtWidgets>
#include <Main/dockwidget.h>
#include "Main/global.h"

enum DelegateType
{
    DT_None,
    DT_Label,
    DT_LineEdit,
    DT_Spinbox,
    DT_DoubleSpinbox,
    DT_Checkbox,
    DT_Combo,
    DT_Slider,
    DT_PlusMinus,
    DT_BarBtns,
    DT_Color
};

enum UserRole
{
    UR_Name = Qt::UserRole + 1,         // unique name to identify the item
    UR_DelegateType,                    // the PropertyWidget (custom widget)
    UR_ItemIndex,                       // the index for the border, text, rectangle or graphic
    UR_isDecoration,                    // show expand/collaple decoration
    UR_DecorateGradient,                // make the root rows dark gray gradiant
    UR_Source,                          // name of property/variable being edited
    UR_QModelIndex,                     // index from another model ie infoView->ok
    UR_Type,                            // the data type required by the delegate
    UR_Min,                             // validate minimum value
    UR_Max,                             // validate maximum value
    UR_LabelFixedWidth,                 // fixed label width in custom widget
    UR_LineEditFixedWidth,              // fixed lineedit width in custom widget
    UR_StringList,                      // list of items for comboBox
    UR_IsBtn,                           // button to run a secondary widget or help
    UR_BtnText,                         // button text
    UR_Color                            // QColor for text in LineEdit, LabelEdit
};

// reqd as can only pass QVariant convertable type through StandardItemModel
extern QVector<BarBtn*> btns;

class SliderEditor : public QWidget
{
    Q_OBJECT
public:
    SliderEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    int value();

protected:
    void paintEvent(QPaintEvent *event);

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

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    void change(int value);
    QSpinBox *spinBox;
    QString source;
    QModelIndex idx;
};

class DoubleSpinBoxEditor : public QWidget
{
    Q_OBJECT
public:
    DoubleSpinBoxEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    double value();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    QDoubleSpinBox *doubleSpinBox;
    QString source;
    QModelIndex idx;
    bool submitted;
};

class LineEditor : public QWidget
{
    Q_OBJECT
public:
    LineEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    QString value();

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    void change(QString value);
    QLineEdit *lineEdit;
    QString source;
    QModelIndex idx;
};

class LabelEditor : public QWidget
{
    Q_OBJECT
public:
    LabelEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    QString value();

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void enableGoKeyActions(bool ok);

private:
    QLabel *label;
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

protected:
    void paintEvent(QPaintEvent *event);

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
    void addItem(QString item);

protected:
    void paintEvent(QPaintEvent *event);

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

protected:
    void paintEvent(QPaintEvent *event);

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

class BarBtnEditor : public QWidget
{
    Q_OBJECT
public:
    BarBtnEditor(const QModelIndex, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event);

private:
    QModelIndex idx;
};

class ColorEditor : public QWidget
{
    Q_OBJECT
public:
    ColorEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    void setValueFromColorDlg();
    QString value();

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

private:
    void updateLabelWhenLineEdited(QString value);
    QLineEdit *lineEdit;
    QPushButton *btn;
};

#endif // PROPERTYWIDGETS_H
