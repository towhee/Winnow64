#include "filters.h"

Filters::Filters(QWidget *parent) : QTreeWidget(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Filters::Filters";
    #endif
    }
    setRootIsDecorated(false);
    setColumnCount(2);
    setHeaderHidden(true);
    setColumnWidth(0, 200);
    setColumnWidth(1, 50);
    hideColumn(1);
//    setAlternatingRowColors(true);

    QLinearGradient categoryBackground(0, 0, 0, 18);
    categoryBackground.setColorAt(0, QColor(88,88,88));
    categoryBackground.setColorAt(1, QColor(66,66,66));
//    QColor categoryBackground(75,75,75,255);
    QFont categoryFont = this->font();
    categoryFont.setBold(true);

//    int transparency = 35;
//    QColor red(255,0,0,transparency);
//    QColor yellow(255,255,0,transparency);
//    QColor green(0,255,0,transparency);
//    QColor blue(0,0,255,transparency);
//    QColor purple(255,0,255,transparency);

    picks = new QTreeWidgetItem(this);
    picks->setText(0, "Picks");
    picks->setFont(0, categoryFont);
    picks->setBackground(0, categoryBackground);
    picks->setBackground(1, categoryBackground);
    picks->setData(0, G::ColumnRole, G::PickedColumn);
    picksFalse = new QTreeWidgetItem(picks);
    picksFalse->setText(0, "False");
    picksFalse->setCheckState(0, Qt::Unchecked);
    picksFalse->setData(1, Qt::EditRole, "false");
    picksTrue = new QTreeWidgetItem(picks);
    picksTrue->setText(0, "True");
    picksTrue->setCheckState(0, Qt::Unchecked);
    picksTrue->setData(1, Qt::EditRole, "true");

    ratings = new QTreeWidgetItem(this);
    ratings->setText(0, "Ratings");
    ratings->setFont(0, categoryFont);
    ratings->setBackground(0, categoryBackground);
    ratings->setBackground(1, categoryBackground);
    ratings->setData(0, G::ColumnRole, G::RatingColumn);

    ratingsNone = new QTreeWidgetItem(ratings);
    ratingsNone->setText(0, "Unrated");
    ratingsNone->setCheckState(0, Qt::Unchecked);
    ratingsNone->setData(1, Qt::EditRole, "");
    ratings1 = new QTreeWidgetItem(ratings);
    ratings1->setText(0, "One");
    ratings1->setCheckState(0, Qt::Unchecked);
    ratings1->setData(1, Qt::EditRole, 1);
    ratings2 = new QTreeWidgetItem(ratings);
    ratings2->setText(0, "Two");
    ratings2->setCheckState(0, Qt::Unchecked);
    ratings2->setData(1, Qt::EditRole, 2);
    ratings3 = new QTreeWidgetItem(ratings);
    ratings3->setText(0, "Three");
    ratings3->setCheckState(0, Qt::Unchecked);
    ratings3->setData(1, Qt::EditRole, 3);
    ratings4 = new QTreeWidgetItem(ratings);
    ratings4->setText(0, "Four");
    ratings4->setCheckState(0, Qt::Unchecked);
    ratings4->setData(1, Qt::EditRole, 4);
    ratings5 = new QTreeWidgetItem(ratings);
    ratings5->setText(0, "Five");
    ratings5->setCheckState(0, Qt::Unchecked);
    ratings5->setData(1, Qt::EditRole, 5);

    labels = new QTreeWidgetItem(this);
    labels->setText(0, "Color class");
    labels->setFont(0, categoryFont);
    labels->setBackground(0, categoryBackground);
    labels->setBackground(1, categoryBackground);
    labels->setData(0, G::ColumnRole, G::LabelColumn);

    labelsNone = new QTreeWidgetItem(labels);
    labelsNone->setText(0, "None");
    labelsNone->setCheckState(0, Qt::Unchecked);
    labelsNone->setData(1, Qt::EditRole, "");
    labelsRed = new QTreeWidgetItem(labels);
    labelsRed->setText(0, "Red");
    labelsRed->setCheckState(0, Qt::Unchecked);
    labelsRed->setData(1, Qt::EditRole, "Red");
    labelsYellow = new QTreeWidgetItem(labels);
    labelsYellow->setText(0, "Yellow");
    labelsYellow->setCheckState(0, Qt::Unchecked);
    labelsYellow->setData(1, Qt::EditRole, "Yellow");
    labelsGreen = new QTreeWidgetItem(labels);
    labelsGreen->setText(0, "Green");
    labelsGreen->setCheckState(0, Qt::Unchecked);
    labelsGreen->setData(1, Qt::EditRole, "Green");
    labelsBlue = new QTreeWidgetItem(labels);
    labelsBlue->setText(0, "Blue");
    labelsBlue->setCheckState(0, Qt::Unchecked);
    labelsBlue->setData(1, Qt::EditRole, "Blue");
    labelsPurple = new QTreeWidgetItem(labels);
    labelsPurple->setText(0, "Purple");
    labelsPurple->setCheckState(0, Qt::Unchecked);
    labelsPurple->setData(1, Qt::EditRole, "Purple");

//    types = new QTreeWidgetItem(this);
//    types->setText(0, "File type");
//    types->setFont(0, categoryFont);
//    types->setBackground(0, categoryBackground);
//    types->setBackground(1, categoryBackground);
//    types->setData(0, G::ColumnRole, G::TypeColumn);

//    models = new QTreeWidgetItem(this);
//    models->setText(0, "Camera model");
//    models->setFont(0, categoryFont);
//    models->setBackground(0, categoryBackground);
//    models->setBackground(1, categoryBackground);
//    models->setData(0, G::ColumnRole, G::CameraModelColumn);

    setStyleSheet("QTreeView::item { height: 18px;}");

    this->expandAll();    
}

