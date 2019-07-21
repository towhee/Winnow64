#ifndef PROPERTYWIDGETS_H
#define PROPERTYWIDGETS_H

#include <QtWidgets>

enum DelegateType
{
    DT_None,
    DT_Text,
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
    UR_LineEditFixedWidth               // fixed lineedit width in custom widget
};

class SliderEditor : public QWidget
{
    Q_OBJECT
public:
    SliderEditor(const QModelIndex &idx, QWidget *parent = nullptr);
    void setValue(QVariant value);
    int value();
    QString parentObjectName;

signals:
    void editorValueChanged(QVariant value, QString source);

private:
    void change(int value);
    QSlider *slider;
    QLabel *label;
    QString source;
    bool isInitializing;
};

#endif // PROPERTYWIDGETS_H
