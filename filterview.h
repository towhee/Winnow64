#ifndef FILTERVIEW_H
#define FILTERVIEW_H

#include <QtWidgets>
#include "global.h"
#include "thumbview.h"

class FilterView : public QTreeWidget
{
    Q_OBJECT
public:
    explicit FilterView(QWidget *parent, ThumbView *thumbView);

signals:

public slots:

private:
    ThumbView *thumbView;
};

#endif // FILTERVIEW_H
