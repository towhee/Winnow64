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
    UR_DelegateType = Qt::UserRole + 1,
    UR_Source,
    UR_Type,
    UR_Min,
    UR_Max
};

class SliderEditor : public QWidget
{
    Q_OBJECT
public:
    SliderEditor(const QModelIndex &idx, QWidget *parent = nullptr/*, int &value, int lineEditWidth, int min, int max*/);
    void setValue(QVariant value);
    int value();

signals:
    void update(QVariant value, QString source);

protected:
//    void paintEvent(QPaintEvent *event);

private:
    void change(int value);
    QSlider *slider;
    QLabel *label;
    QModelIndex idx;
};

#endif // PROPERTYWIDGETS_H
