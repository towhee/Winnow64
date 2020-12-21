#ifndef PROPERTYWIDGETS_H
#define PROPERTYWIDGETS_H

#include <QtWidgets>
#include <Main/dockwidget.h>
#include "Main/global.h"
#include "Main/widgetcss.h"

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
    DT_SelectFolder,
    DT_SelectFile,
    DT_Color
};

enum UserRole
{
    UR_Name = Qt::UserRole + 1,         // unique name to identify the item
    UR_ItemIndex,                       // unique, consistent index for all items
    UR_SortOrder,                       // override appended order ie for effects, borders
    UR_DefaultValue,                    // when created or double click
    UR_SettingsPath,                    // settings path
    UR_Editor,                          // pointer to custom editor created
    UR_DelegateType,                    // type of custom widget
    UR_hasValue,                        // if no value then value column is empty
    UR_CaptionIsEditable,               // can edit caption
    UR_isIndent,                        // indent column 0 in QTreeView
    UR_isHeader,                        // header item in QTreeView
    UR_isDecoration,                    // show expand/collapse decoration
    UR_isBackgroundGradient,            // make the root rows dark gray gradiant
    UR_isHidden,                        // flag to hide/show row in tree
    UR_Source,                          // name of property/variable being edited = i.key
    UR_QModelIndex,                     // index from another model ie infoView->ok
    UR_Type,                            // the data type required by the delegate
    UR_Min,                             // validate minimum value
    UR_Max,                             // validate maximum value
    UR_Div,                             // Divide slider value by amount to get double from int
    UR_DivPx,                           // Slider singlestep = one pixel
    UR_FixedWidth,                      // fixed label width in custom widget
    UR_StringList,                      // list of items for comboBox
    UR_IconList,                        // list of icons for comboBox
    UR_IsBtn,                           // button to run a secondary widget or help
    UR_BtnText,                         // button text
    UR_Color                            // QColor for text in LineEdit, LabelEdit
};

// reqd as can only pass QVariant convertable type through StandardItemModel
extern QVector<BarBtn*> btns;

class Slider : public QSlider
{
    Q_OBJECT
public:
    Slider(Qt::Orientation orientation, int div, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    int div;
};

class SliderEditor : public QWidget
{
    Q_OBJECT
public:
    SliderEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    double value();

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

public slots:
    void fontSizeChanged(int fontSize);

private:
    void change(double value);
    void sliderMoved();
    void updateSliderWhenLineEdited();
    bool outOfRange;
    bool isInt;
    int div;
    QSlider *slider;
    QLineEdit *lineEdit;
    QString source;
    QModelIndex idx;
};
//Q_DECLARE_METATYPE(SliderEditor*);

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

public slots:
    void fontSizeChanged(int fontSize);

private:
    void change();
    QSpinBox *spinBox;
    QLabel *label;
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

public slots:
    void fontSizeChanged(int fontSize);

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

public slots:
    void fontSizeChanged(int fontSize);

private:
    void change();
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

public slots:
    void fontSizeChanged(int fontSize);

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

public slots:
    void fontSizeChanged(int fontSize);

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
    void addItem(QString item, QIcon = QIcon());
    void removeItem(QString item);
    void renameItem(QString oldText, QString newText);
    void refresh(QStringList items);
    WidgetCSS css;

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

public slots:
    void fontSizeChanged(int fontSize);

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

public slots:
    void fontSizeChanged(int fontSize);

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

public slots:
    void dlgColorChanged(const QColor &color);

public slots:
    void fontSizeChanged(int fontSize);

private:
    void updateLabelWhenLineEdited(QString value);
    QLineEdit *lineEdit;
    QPushButton *btn;
    QColorDialog *colorDlg;
};

class SelectFolderEditor : public QWidget
{
    Q_OBJECT
public:
    SelectFolderEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    void setValueFromSaveFileDlg();
    QString value();

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

public slots:
    void fontSizeChanged(int fontSize);

private:
    void change(QString value);
    QLineEdit *lineEdit;
    BarBtn *btn;
};

class SelectFileEditor : public QWidget
{
    Q_OBJECT
public:
    SelectFileEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    void setValueFromSaveFileDlg();
    QString value();

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);

public slots:
    void fontSizeChanged(int fontSize);

private:
    void change(QString value);
    QLineEdit *lineEdit;
    BarBtn *btn;
};

#endif // PROPERTYWIDGETS_H
