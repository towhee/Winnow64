#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "PropertyEditor/propertyeditor.h"
#include "PropertyEditor/propertywidgets.h"

class EmbelProperties : public PropertyEditor
{
    Q_OBJECT
public:
    EmbelProperties(QWidget *parent, QSettings *setting);
    struct File {
        int horizontalFit;
        int verticalFit;
        QString fileType;
        QString saveMethod;
        QString folderPath;
        QString suffix;
        bool overwriteFiles;
    } f;

    struct Border {
        QString name;
        QString caption;
        QString parent;
        int top;
        int left;
        int right;
        int bottom;
        QString tile;
        QString color;
        int opacity;
        QString style;
    } border;

    QStringList templateList;
    QVector<Border>b;

public slots:
    void itemChange(QModelIndex idx);
    void diagnostic(QModelIndex parent = QModelIndex());

private:
    void readTemplateList();
    void renameCurrentTemplate();
    void setCurrentTemplate();
    void newTemplate();
    void addBorders();
    void newBorder();
    void addBorder();
    void addTemplateHeader();
    void addTemplateItems();

    ItemInfo i;
    int templateCount;
    QString templateName;
    QString templatePath;
    QWidget *templateListEditor;
    QSettings *setting;
};

#endif // EMBELPROPERTIES_H
