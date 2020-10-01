#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Utilities/utilities.h"
#include "PropertyEditor/propertyeditor.h"
#include "Effects/graphicseffect.h"
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
    QString styleName;
    int styleId;
    int globalLightDirection;       // 0-360 degrees used for shadows etc

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
        double rotation;            // rotation in degrees
        QString alignToCorner;
        int alignTo_BorderId;
        int alignTo_CornerId;
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

    // effect structs are defined in graphicseffect.h

    // a style is a list of effects
    QList<winnow_effects::Effect> effects;
    // map of styles
    QMap<QString, QList<winnow_effects::Effect>> styleMap;

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
//    void mouseMoveEvent(QMouseEvent *event) override;   // does not work here
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void initialize();
    void readTileList();
    void renameSettingKey(QString path, QString oldName, QString newName);
    void updateBorderLists();
    void readTemplateList();
    void readTile(QStringList tileName);
    void renameCurrentStyle();
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
    void addStyles();
    void addStyle(QString name, int n);
    void addBlurEffect(QModelIndex parIdx, QString effectName = "");
    void addHighlightEffect(QModelIndex parIdx, QString effectName = "");
    void addShadowEffect(QModelIndex parIdx, QString effectName = "");

    void newBorder();
    void newText();
    void newRectangle();
    void newGraphic();
    void newStyle();

    void deleteTremplate();
    void deleteBorder();
    void deleteText();
    void deleteRectangle();
    void deleteGraphic();
    void deleteItem();

    void updateEffectOrder(QStandardItem *styleItem, int row, int swapRow);
    void moveEffectUp();
    void moveEffectDown();
    void sortEffectList(QString style);

    void itemChangeTemplate(QVariant v);
    void itemChangeFile(QVariant v, QString source);
    void itemChangeImage(QVariant v, QString source);
    void itemChangeBorder(QModelIndex idx);
    void itemChangeText(QModelIndex idx);
    void itemChangeRectangle(QVariant v, QString source, QString parent);
    void itemChangeGraphic(QVariant v, QString source, QString parent);
    void itemChangeGlobalLight(QVariant v, QString source, QString parent);
    void itemChangeShadowEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeBlurEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeHighlightEffect(QVariant v, QString source, QString effectName, QString style);
    void strokeItemChange(QVariant v, QString source, QString effectName, QString style);

    void treeChange(QModelIndex idx);
    bool okToSelect(QModelIndex idx, QString selName);
    void diagnostics(QModelIndex idx);
    void diagnosticStyles();
    void diagnosticVectors();
    void parseAlignToCorner(QString alignTo, int &iBorder, int &iCorner);

    QModelIndex bordersIdx;
    QModelIndex textsIdx;
    QModelIndex rectanglesIdx;
    QModelIndex graphicsIdx;
    QModelIndex stylesIdx;
    QModelIndex effectParentIdx;
    QModelIndexList styleIdxList;
    QModelIndexList effectIdxList;

    // expand/collapse in context menu
    void expandAllRows();
    void collapseAllRows();
    void solo();
    QAction *soloAction;
    bool isSolo;

    void effectContextMenu();
    void effectActionClicked();
    QString uniqueEffectName(QString styleName, int effectType, QString effectName);
    int effectIndex(QString style, QString effectName);

    QMenu *effectMenu;
    QAction *shadowEffectAction;
    QAction *bevelEffectAction;
    QAction *embossEffectAction;
    QAction *blurEffectAction;
    QAction *sharpenEffectAction;
    QAction *colorizeEffectAction;
    QAction *edgeEffectAction;
    QAction *highlightEffectAction;
    QAction *strokeEffectAction;

    QAction *expandAllAction;
    QAction *collapseAllAction;

    bool isTemplateChange = false;

    void addEffectHeaderButtons();
    BarBtn *borderDeleteBtn;
    BarBtn *textDeleteBtn;
    BarBtn *rectangleDeleteBtn;
    BarBtn *graphicDeleteBtn;
    BarBtn *styleDeleteBtn;
    BarBtn *effectDeleteBtn;
    BarBtn *effectUpBtn;
    BarBtn *effectDownBtn;

    ItemInfo i;
    int templateCount;
    QString templatePath;
    QSettings *setting;
    ComboBoxEditor *templateListEditor;
    QVector<ComboBoxEditor*> textAlignToCornerObjectEditor;
    QVector<ComboBoxEditor*> textAnchorObjectEditor;
    QVector<ComboBoxEditor*> styleListObjectEditor;
    QVector<BarBtnEditor*> styleEditor;

    // must be in correct order
    enum roots {
        _templates,
        _styles,
        _file,
        _image,
        _borders,
        _texts,
        _rectangles,
        _graphics
    };
    enum corners {TopLeft, TopRight, BottomLeft, BottomRight};
    QStringList anchorPoints;
    QStringList borderList;
    QStringList borderCorners;
    QStringList alignToCornerList;
    QStringList anchorObjectList;
    QStringList anchorContainerList;
    QStringList fontWeights;
    QStringList tileList;
    QStringList metadataTemplatesList;
    QStringList styleList;
    QStringList effectList;
};

#endif // EMBELPROPERTIES_H
