#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "PropertyEditor/propertyeditor.h"
#include "Utilities/utilities.h"
#include "Utilities/htmlwindow.h"
#include "Utilities/foldercompressor.h"
#include "Effects/graphicseffect.h"
//#include "Views/infostring.h"
#include "Dialogs/copystyledlg.h"
#include "Dialogs/patterndlg.h"
#include "Dialogs/managetilesdlg.h"
#include "Dialogs/managegraphicsdlg.h"
#include "Dialogs/manageimagesdlg.h"
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
    int lightDirection;             // 0-360 degrees (0 = north) used for shadows etc
    int horizontalFitPx;
    int verticalFitPx;
    int longSidePx;

    // Export parameters
    QString exportFileType;
    int exportFileQuality;
    QString saveMethod;
    QString exportFolderPath;
    QString exportSubfolder;
    QString exportSuffix;
    bool copyMetadata;
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
        QString anchorPoint;        // text anchor pt to justify upon
        bool align;                 // align text to nearest image edge
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
        QByteArray graphic;
        QString caption;
        QString parent;
        QString filePath;
        QString anchorObject;
        QString anchorContainer;
        double x;                   // container coordinate
        double y;                   // container coordinate
        QString anchorPoint;
        bool align;                 // align graphic to nearest image edge
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

    void newTemplate();
    void newTemplateFromImport(QString name);
    void doNotEmbellish();
    QString metaString(QString key);
    QString metaString(QString key, QString fPath);
    void test1(QIcon icon = QIcon());
    void test2();

public slots:
    void itemChange(QModelIndex idx) override;
    void manageTiles();
    void manageGraphics();
    void invokeFromAction(QAction *embelAction);
    void setCurrentTemplate(QString name);
    void coordHelp();
    QString diagnostics();
    void renameMetadataTemplateList(QString oldName, QString newName);
    void updateMetadataTemplateList();
    bool saveTemplateToFile();
    bool readTemplateFromFile();

signals:
    void templateChanged(int id);
    void syncEmbellishMenu();
    void updateGraphicsList();

protected:
    void mouseMoveEvent(QMouseEvent *event) override;   // does not work here
//    void focusInEvent(QFocusEvent  *event) override;
    void mousePressEvent(QMouseEvent *event) override;
//    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void initialize();
    void renameSettingKey(QString path, QString oldName, QString newName);
    void updateBorderLists();
    void updateAnchorObjects();
    void updateBorderOrderAfterDeletion();
    void updateEffectOrderAfterDeletion(QModelIndex parIdx);
    void sortTemplateList();
    void readTemplateList();
    void readTileList();
    void readGraphicsList();
    void updateTileList(QString oldName = "", QString newName = "");
    void updateGraphicList(QString oldName = "", QString newName = "");
    void readTile(QStringList tileName);
    void readMetadataTemplateList();
    void renameCurrentStyle();
    void renameCurrentTemplate();
    void copyStyle();
    void copyTemplate();
    QString uniqueTemplateName(QString name);
    QString uniqueTileName(QString name);
    QString uniqueGraphicName(QString name);
    QString uniqueTokenName(QString name);
    void syncWinnets();

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
    void addBrightnessEffect(QModelIndex parIdx, QString effectName = "");
    void addSharpenEffect(QModelIndex parIdx, QString effectName = "");
    void addHighlighterEffect(QModelIndex parIdx, QString effectName = "");
    void addShadowEffect(QModelIndex parIdx, QString effectName = "");
    void addEmbossEffect(QModelIndex parIdx, QString effectName = "");
    void addStrokeEffect(QModelIndex parIdx, QString effectName = "");
    void addGlowEffect(QModelIndex parIdx, QString effectName = "");

    void newBorderFromBtn();
    void newBorder(QString name = "");
    void newText();
    void newRectangle();
    void newGraphic();
    void newStyle();

    void deleteTemplate();
    void deleteItem();
//    void deleteTile();

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
    void itemChangeHighlighterEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeBrightnessEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeEmbossEffect(QModelIndex idx, QVariant v, QString source, QString effectName, QString style);
    void itemChangeStrokeEffect(QVariant v, QString source, QString effectName, QString style);
    void itemChangeGlowEffect(QVariant v, QString source, QString effectName, QString style);

    void treeChange(QModelIndex idx);
    bool okToSelect(QModelIndex idx, QString selName);
    void diagnosticStyles(QTextStream &rpt);
    void diagnosticVectors(QTextStream &rpt);
    void diagnosticModel(QTextStream &rpt, QModelIndex parent = QModelIndex());

    PatternDlg *patternDlg;

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
    void expandRecursively();

    void effectContextMenu();
    void newEffect();
    QString uniqueEffectName(QString styleName, int effectType, QString effectName);
    int effectIndex(QString style, QString effectName);

    QMenu *effectMenu;
    QAction *brightnessEffectAction;
    QAction *shadowEffectAction;
    QAction *embossEffectAction;
    QAction *blurEffectAction;
    QAction *sharpenEffectAction;
    QAction *colorizeEffectAction;
    QAction *highlighterEffectAction;
    QAction *strokeEffectAction;
    QAction *glowEffectAction;

    // context menu
    QAction *expandAllAction;
    QAction *collapseAllAction;
    QAction *soloAction;
    QAction *expandRecursivelyAction;
    QAction *renameAction;
    QAction *readTemplateFromFileAction;
    QAction *saveTemplateToFileAction;
    QAction *newTemplateAction;
    QAction *copyTemplateAction;
    QAction *copyStyleAction;
    QAction *tokenEditorAction;
    QAction *manageTilesAction;
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
    QVector<ComboBoxEditor*> borderTileObjectEditor;
    QVector<ComboBoxEditor*> graphicsObjectEditor;
    QVector<ComboBoxEditor*> graphicAnchorObjectEditor;
    QVector<ComboBoxEditor*> textAnchorObjectEditor;
    QVector<ComboBoxEditor*> textTokenObjectEditor;
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
    QStringList graphicList;
    QStringList metadataTemplatesList;
    QStringList styleList;
    QStringList effectList;

    // req'd?
    QStringList embossContourList;
    QStringList embossContourIconList;
};

#endif // EMBELPROPERTIES_H
