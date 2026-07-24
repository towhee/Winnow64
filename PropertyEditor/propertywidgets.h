#ifndef PROPERTYWIDGETS_H
#define PROPERTYWIDGETS_H

#include <QtWidgets>
#include <Main/dockwidget.h>
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
    UR_okToCollapseRoot,                // collapse root item when collapseAll
    UR_isDecoration,                    // show expand/collapse decoration
    UR_isBackgroundGradient,            // make the root rows dark gray gradiant
    UR_isHidden,                        // flag to hide/show row in tree
    UR_isEnabled,                       // flag to show disabled in tree
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
    UR_Color,                           // QColor for text in LineEdit, LabelEdit, Start slider
    UR_Color1,                          // QColor end in Slider
    UR_LeafSingleLine,                  // header-style row drawn single-line but in LEAF text colour
    UR_DeleteBtn,                       // draw a delete [-] glyph at the row's right (delegate-drawn)
    UR_AddBtn,                          // draw an add [+] glyph left of the [-] (delegate-drawn)
    UR_ShowDecoration,                  // force expand/collapse arrow, no children
    UR_ExtraIndent,                     // one extra indent level for a leaf caption
    UR_ExtraRowHeight,                  // extra px added to a row height (top+bottom pad)
    UR_isDivider,                       // spacer/divider row (see addDivider)
    UR_DividerHeight,                   // divider: total row height in px
    UR_DividerLineHeight,               // divider: line thickness in px (0 = gap only)
    UR_DividerColor,                    // divider: line QColor (alpha 0 = gap only)
    UR_LogScale,                        // Slider: UR_Min/UR_Max are a LOG value range
    UR_FlashLevel                       // caption: 0..1 flash tint feedback
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
    /* Nudge on Left/Right (PageUp/PageDown = larger step) and ACCEPT the key, so it does
       not propagate up to MW::eventFilter and move image selection. Needed because the
       Develop sliders use singleStep 0 (QAbstractSlider would then step by 0 and ignore). */
    void keyPressEvent(QKeyEvent *event) override;
    /* Repaint the host SliderEditor when focus arrives/leaves so its focus cue
       (accent bar) tracks which slider the Develop arrow-nudge keys will act on. */
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void syncFocusRepaint();    // repaint the value + caption cells on focus in/out
    int div;
};

class SliderEditor : public QWidget
{
    Q_OBJECT
public:
    SliderEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    double value();
    void focusSlider();     // give keyboard focus to the inner slider (nudge target)
    bool sliderHasFocus() const;   // true while the inner slider owns keyboard focus

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
    /* Log-scale mode (UR_LogScale): the slider's integer position is a point on a
       LOGARITHMIC ramp between UR_Min and UR_Max, not the value itself. Needed by the
       white-balance Temp slider, whose 2000..50000 K range would otherwise cram every
       useful temperature into the first sixth of the track. valueFromPos/posFromValue
       convert; everything outside this class still sees the real value. */
    bool logScale;
    double valueFromPos(int pos) const;
    int    posFromValue(double v) const;
    double logMin, logMax;
    static constexpr int kLogSteps = 1000;
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
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

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
    void setRenamable(bool on);     // make the combo's text editable so the current item can be renamed
    WidgetCSS css;

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void editorValueChanged(QWidget *);
    void enableGoKeyActions(bool ok);
    void itemRenamed(QString oldText, QString newText);

public slots:
    void fontSizeChanged(int fontSize);

private:
    void change(int index);
    QComboBox *comboBox;
    QString source;
    QModelIndex idx;
    bool isRenamable = false;
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
    void dlgColorClose(int result);

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
