#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include <QtWidgets>
#include "Main/global.h"
#include "propertywidgets.h"


//const int DelegateTypeRole = Qt::UserRole + 1;

class PropertyEditor : public QTreeView
{
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget *parent = nullptr);

signals:

public slots:
    void editorUpdate(QVariant v, QString source);
//    void editCheck(QTreeWidgetItem *item, int column);
};

#include <QStyledItemDelegate>

class PropertyDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    PropertyDelegate(QObject *parent = nullptr);
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
    void update(QVariant value, QString source);

private slots:
    void editorUpdate(QVariant value, QString source);
};

#endif // PROPERTYEDITOR_H








//#ifndef PROPERTYEDITOR_H
//#define PROPERTYEDITOR_H

//#include <QtWidgets>
//#include "Main/global.h"

//#include "propertywidgets.h"

//class PropertyEditor : public QTreeWidget
//{
//    Q_OBJECT
//public:
//    explicit PropertyEditor(QWidget *parent = nullptr);

//    QTreeWidgetItem *general;
//    QTreeWidgetItem *generalFontSize;

//signals:

//public slots:
//    void editCheck(QTreeWidgetItem *item, int column);
//};

//#include <QStyledItemDelegate>

//class PropertyDelegate : public QStyledItemDelegate
//{
//    Q_OBJECT

//public:
//    PropertyDelegate(QObject *parent = nullptr);
//    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
//                          const QModelIndex &index) const override;
//    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override;
//    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
//    void setModelData(QWidget *editor, QAbstractItemModel *model,
//                      const QModelIndex &index) const override;
//    void updateEditorGeometry(QWidget *editor,
//        const QStyleOptionViewItem &option, const QModelIndex &index) const override;
//};

//#endif // PROPERTYEDITOR_H
