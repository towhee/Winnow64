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
        double outlineWidth;
        QString outlineColor;
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
    } border;
    QVector<Border>b;

    void newEmbelTemplate();
    void test1();
    void test2();

public slots:
    void itemChange(QModelIndex idx) override;
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
    void invokeFromAction(QAction *embelAction);
    void diagnostic(QModelIndex parent = QModelIndex());
    void coordHelp();

signals:
    void templateChanged(int id);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

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

    void deleteTremplate();
    void deleteBorder();
    void deleteText();
    void deleteRectangle();
    void deleteGraphic();
    void deleteItem();
    void showRelevantDeleteBtn(QString btnToShow = "");

    void templateChange(QVariant v);
    void fileItemChange(QVariant v, QString source);
    void imageItemChange(QVariant v, QString source);
    void borderItemChange(QModelIndex idx);
    void textItemChange(QVariant v, QString source, QString parent);
    void rectangleItemChange(QVariant v, QString source, QString parent);
    void graphicItemChange(QVariant v, QString source, QString parent);

    void treeChange(QModelIndex idx);
    bool okToSelect(QModelIndex idx);
    void diagnostics(QModelIndex idx);

    BarBtn *borderDeleteBtn;
    BarBtn *textDeleteBtn;
    BarBtn *rectangleDeleteBtn;
    BarBtn *graphicDeleteBtn;

    ItemInfo i;
    int templateCount;
    QString templatePath;
    ComboBoxEditor *templateListEditor;
    QSettings *setting;

    enum roots {
        _templates,
        _file,
        _image,
        _borders,
        _texts,
        _rectangles,
        _graphics
    };
};

#endif // EMBELPROPERTIES_H
