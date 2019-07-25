#ifndef PROPERTYDELEGATE_H
#define PROPERTYDELEGATE_H

#include <QtWidgets>
#include "Main/global.h"
#include "propertywidgets.h"
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
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QString displayText(const QVariant &value, const QLocale &locale) const override;

protected:
//    bool eventFilter(QObject *editor, QEvent *event) override;

signals:
    void itemChanged(QModelIndex idx) const;
//    void editorValueChanged(QVariant value, QString source, QModelIndex index);
//    void commitAndCloseEditor();
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor) const;

private slots:
//    void editorValueChange(QVariant value, QString source, QModelIndex index);
};


#endif // PROPERTYDELEGATE_H
