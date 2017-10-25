#include "filterview.h"

FilterView::FilterView(QWidget *parent, ThumbView *thumbView) : QTreeWidget(parent)
{
    this->thumbView = thumbView;
}
