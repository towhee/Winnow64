#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Utilities/utilities.h"
#include "PropertyEditor/propertyeditor.h"
#include "Effects/graphicseffect.h"
#include "Views/infostring.h"
#include "Dialogs/copystyledlg.h"
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
    int horizontalFitPx;
    int verticalFitPx;

    // Export parameters
    QString exportFileType;
    QString saveMethod;
    QString exportFolderPath;
    QString exportSubfolder;
    bool overwriteFiles;

    struct Image {
        QString style;
    };
    Image image;

    struct Border {
        QString name;
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

    struct Graphic {
        int index;
        QString name;
        QString caption;
        QString parent;
        QString filePath;
        QString anchorObject;
        QString anchorContainer;
        double x;                   // container coordinate
        double y;                   // container coordinate
        QString anchorPoint;
        double size;                // % of long dimension
        double rotation;            // rotation in degrees
        int opacity;
        QString style;
    } graphic;
    QVector<Graphic>g;

    // effect structs are defined in graphicseffect.h

    // a style is a list of effects
    QList<winnow_effects::Effect> effects;
    // map of styles
    QMap<QString, QList<winnow_effects::Effect>> styleMap;

    void newEmbelTemplate();
    void doNotEmbellish();
    QString metaString(QString key);
    QString metaString(QString key, QString fPath);
    void test1();
    void test2();

public slots:
    void itemChange(QModelIndex idx) override;
//    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
    void invokeFromAction(QAction *embelAction);
    void setCurrentTemplate(QString name);
    void diagnostic(QModelIndex parent = QModelIndex());
    void coordHelp();

signals:
    void templateChanged(int id);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;   // does not work here
//    void focusInEvent(QFocusEvent  *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void initialize();
    void renameSettingKey(QString path, QString oldName, QString newName);
    void updateBorderLists();
    void updateAnchorObjects();
    void updateBorderOrderAfterDeletion();
    void sortTemplateList();
    void readTemplateList();
    void readTileList();
    void readTile(QStringList tileName);
    void renameCurrentStyle();
    void renameCurrentTemplate();
    void copyStyle();
    void copyTemplate();
    QString uniqueTemplateName(QString name);

    void addTemplateHeader();
    void addTemplateItems();
    void addExport();
    void addGeneral();
    void addBorders();
    void addBorder(int count, QString borderName);
    void addTexts();
    void addText(int count);
    void addGraphics();
    void addGraphic(int count);
    void addStyles();
    void addStyle(QString name, int n);
    void addBlurEffect(QModelIndex parIdx, QString effectName = "");
    void addBrightenEffect(QModelIndex parIdx, QString effectName = "");
    void addSharpenEffect(QModelIndex parIdx, QString effectName = "");
    void addHighlightEffect(QModelIndex parIdx, QString effectName = "");
    void addShadowEffect(QModelIndex parIdx, QString effectName = "");
    void addEmbossEffect(QModelIndex parIdx, QString effectName = "");

    void newBorderFromBtn();
    void newBorder(QString name = "");
    void newText();
    void newRectangle();
    void newGraphic();
    void newStyle();

    void deleteTemplate();
    void deleteItem();

    void flash(QModelIndex idx);
    // redirect from action to relevent function (use currentIdx)
    void rename();
    void moveUp();
    void moveDown();
    void copy();

    void swapBorders(QStandardItem *borders, int row, int swapRow);
    void moveBorderUp();
    void moveBorderDown();
    void syncBorderVector();

    void swapEffects(QStandardItem *styleItem, int row, int swapRow);
    void moveEffectUp();
    void moveEffectDown();
    bool sortEffectList(QString style);

    void itemChangeTemplate(QVariant v);
    void itemChangeExport(QModelIndex idx, QVariant v, QString source);
    void itemChangeGeneral(QVariant v, QString source);
    void itemChangeBorder(QModelIndex idx);
    void itemChangeText(QModelIndex idx);
    void itemChangeRectangle(QVariant v, QString source, QString parent);
    void itemChangeGraphic(QModelIndex idx, QVariant v, QString source, QString parent);
    void itemChangeShadowEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeBlurEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeSharpenEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeHighlightEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeBrightenEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeEmbossEffect(QModelIndex idx, QVariant v, QString source, QString effectName, QString style);
    void itemChangeStroke(QVariant v, QString source, QString effectName, QString style);

    void treeChange(QModelIndex idx);
    bool okToSelect(QModelIndex idx, QString selName);
    void diagnostics(QModelIndex idx);
    void diagnosticStyles();
    void diagnosticVectors();

    QModelIndex root;                   // invisible root of treeview
    QModelIndex templateIdx;
    QModelIndex currentIdx;
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

    void effectContextMenu();
    void newEffectActionClicked();
    QString uniqueEffectName(QString styleName, int effectType, QString effectName);
    int effectIndex(QString style, QString effectName);

    QMenu *effectMenu;
    QAction *brightenEffectAction;
    QAction *shadowEffectAction;
    QAction *bevelEffectAction;
    QAction *embossEffectAction;
    QAction *blurEffectAction;
    QAction *sharpenEffectAction;
    QAction *colorizeEffectAction;
    QAction *edgeEffectAction;
    QAction *highlightEffectAction;
    QAction *strokeEffectAction;

    // context menu
    QAction *expandAllAction;
    QAction *collapseAllAction;
    QAction *renameAction;
    QAction *copyTemplateAction;
    QAction *copyStyleAction;
    QAction *moveUpAction;
    QAction *moveDownAction;
    QAction *separatorAction0;
    QAction *separatorAction1;
    QAction *separatorAction2;

    bool isTemplateChange = false;

    void addBorderHeaderButtons();
    void addEffectBtns();
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
    QVector<ComboBoxEditor*> textAnchorObjectEditor;
    QVector<ComboBoxEditor*> graphicAnchorObjectEditor;
    QVector<ComboBoxEditor*> styleListObjectEditor;
    QVector<BarBtnEditor*> styleEditor;

//    QString copyStyleSettingsPath = "";

    // must be in correct order
    enum roots {
        _templates,
        _styles,
        _file,
        _image,
        _borders,
        _texts,
        _graphics
    };
    enum corners {TopLeft, TopRight, BottomLeft, BottomRight};
    QStringList anchorPoints;
    QStringList borderList;
    QStringList borderCorners;
    QStringList anchorObjectList;
    QStringList anchorContainerList;
    QStringList fontWeights;
    QStringList tileList;
    QStringList metadataTemplatesList;
    QStringList styleList;
    QStringList effectList;

    // req'd?
    QStringList embossContourList;
    QStringList embossContourIconList;
};

#endif // EMBELPROPERTIES_H
