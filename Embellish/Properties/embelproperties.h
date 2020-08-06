#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Utilities/utilities.h"
#include "PropertyEditor/propertyeditor.h"
#include "PropertyEditor/propertywidgets.h"
#include "Views/infostring.h"
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
        int horizontalFitPx;
        int verticalFitPx;
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
        QByteArray tile;
        QString color;
        double opacity;
        QString style;
        double outlineWidth;
        QString outlineColor;
    } border;
    QVector<Border>b;

    struct Text {
        int index;
        QString name;
        QString caption;
        QString parent;
        QString anchorObject;
        QString anchorContainer;
        double x;                   // container coordinate
        double y;                   // container coordinate
        QString anchorPoint;
        QString source;
        QString text;
        QString metadataTemplate;
        QString metadataText;
        double size;                // percent of long side pixels
        QString font;
        bool isBold;
        bool isItalic;
        QString color;
        int opacity;
        QString style;
    } text;
    QVector<Text>t;


    void newEmbelTemplate();
    QString metaString(QString key);
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
//    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void initialize();
    void readTileList();
    void rename(QString path, QString from, QString to);
    void updateBorderList();
    void readTemplateList();
    void readTile(QStringList tileName);
    void renameCurrentTemplate();
    void setCurrentTemplate();

    void addTemplateHeader();
    void addTemplateItems();
    void addFile();
    void addImage();
    void addBorders();
    void addBorder(int count);
    void addTexts();
    void addText(int count);
    void addRectangles();
    void addGraphics();

    void newBorder();
    void newText();
    void newRectangle();
    void newGraphic();

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
    void textItemChange(QModelIndex idx);
    void rectangleItemChange(QVariant v, QString source, QString parent);
    void graphicItemChange(QVariant v, QString source, QString parent);

    void treeChange(QModelIndex idx);
    bool okToSelect(QModelIndex idx);
    void diagnostics(QModelIndex idx);

    bool isTemplateChange = false;

    BarBtn *borderDeleteBtn;
    BarBtn *textDeleteBtn;
    BarBtn *rectangleDeleteBtn;
    BarBtn *graphicDeleteBtn;

    ItemInfo i;
    int templateCount;
    QString templatePath;
    QSettings *setting;
    ComboBoxEditor *templateListEditor;
    QVector<ComboBoxEditor*> textAnchorObjectEditor;

    enum roots {
        _templates,
        _file,
        _image,
        _borders,
        _texts,
        _rectangles,
        _graphics
    };
    QStringList anchorPoints;
    QStringList borderList;
    QStringList anchorObjectList;
    QStringList anchorContainerList;
    QStringList fontWeights;
    QStringList tileList;
    QStringList metadataTemplatesList;
};

#endif // EMBELPROPERTIES_H
