#ifndef PROPERTYDELEGATE_H
#define PROPERTYDELEGATE_H

#include <QtWidgets>
#include "Main/global.h"
#include "propertywidgets.h"
#include "Main/widgetcss.h"
#include <QStyledItemDelegate>

enum Fields
{
    CapColumn,      // Caption / Name
    ValColumn,      // Value
    OrdColumn       // Sort order
};

class PropertyDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    PropertyDelegate(QWidget *parent = nullptr);
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
    bool submitted;
    bool isAlternatingRows;

signals:
    void itemChanged(QModelIndex idx) const;
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor) const;
    void drawBranchesAgain(QPainter *painter, QRect rect, QModelIndex index) const;
    void fontSizeChange(int fontSize);

public slots:
    void commit(QWidget *editor);
    void fontSizeChanged(int fontSize);

protected:
//    bool eventFilter(QObject *editor, QEvent *event) override;

private:
    bool isDebug = false;
};


#endif // PROPERTYDELEGATE_H
