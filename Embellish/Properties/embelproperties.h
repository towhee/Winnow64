#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "PropertyEditor/propertyeditor.h"
#include "PropertyEditor/propertywidgets.h"
#include "ui_embelCoord.h"

class EmbelProperties : public PropertyEditor
{
    Q_OBJECT
public:
    EmbelProperties(QWidget *parent, QSettings *setting);

    QStringList templateList;
    QString templateName;
    int templateId;                 // 0 == Do not embellish

    struct File {
        int horizontalFit;
        int verticalFit;
        QString fileType;
        QString saveMethod;
        QString folderPath;
        QString suffix;
        bool overwriteFiles;
    } f;

    struct Image {
        QString style;
    };
    Image image;

    struct Border {
        int index;
        QString name;
        QString caption;
        QString parent;
        double top;
        double left;
        double right;
        double bottom;
        QString tile;
        QString color;
        int opacity;
        QString style;
        double outlineWidth;
        QString outlineColor;
        // coord
//        QPointF tl, tc, tr, cl, cc, cr, bl, bc, br;
//        double w, h;
    } border;
    QVector<Border>b;

    void newEmbelTemplate();
    void test1();
    void test2();

public slots:
    void itemChange(QModelIndex idx);
    void invokeFromAction(QAction *embelAction);
    void diagnostic(QModelIndex parent = QModelIndex());
    void coordHelp();

signals:
    void templateChanged(int id);

private:
    void readTemplateList();
    void renameCurrentTemplate();
    void setCurrentTemplate();
    void addTemplateHeader();
    void addTemplateItems();
    void addFile();
    void addImage();
    void addBorders();
    void newBorder();
    void addBorder(int count);
    void addTexts();
    void addRectangles();
    void addGraphics();

    void templateChange(QVariant v);
    void fileItemChange(QVariant v, QString source);
    void imageItemChange(QVariant v, QString source);
    void borderItemChange(QModelIndex idx);
    void textItemChange(QVariant v, QString source, QString parent);
    void rectangleItemChange(QVariant v, QString source, QString parent);
    void graphicItemChange(QVariant v, QString source, QString parent);

    void diagnostics(QModelIndex idx);


    ItemInfo i;
    int templateCount;
    QString templatePath;
    ComboBoxEditor *templateListEditor;
    QSettings *setting;
};

#endif // EMBELPROPERTIES_H
