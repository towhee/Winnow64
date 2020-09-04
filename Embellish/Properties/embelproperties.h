#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Utilities/utilities.h"
#include "PropertyEditor/propertyeditor.h"
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

    struct Blur {
        qreal radius;
        bool quality;
        int transposed;
    };

    struct Highlight {
    //  QColor color;    // default constructor is implicitly deleted because variant field has a non-trivial default constructor
    //  QPointF offset;  // default constructor is implicitly deleted because variant field has a non-trivial default constructor
        int r, g, b;
        int x, y;
    };

    // when add an effect must add to enum and union
    enum EffectType {blur, highlight};
    struct Effect {
        enum EffectType effectType;
        union {
            Blur blur;
            Highlight highlight;
        };
    };
    QList<Effect> effects;

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
    void addShadowEffect(QModelIndex parIdx);

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

    void templateChange(QVariant v);
    void fileItemChange(QVariant v, QString source);
    void imageItemChange(QVariant v, QString source);
    void borderItemChange(QModelIndex idx);
    void textItemChange(QModelIndex idx);
    void rectangleItemChange(QVariant v, QString source, QString parent);
    void graphicItemChange(QVariant v, QString source, QString parent);

    void treeChange(QModelIndex idx);
    bool okToSelect(QModelIndex idx, QString selName);
    void diagnostics(QModelIndex idx);
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

    QMenu *effectMenu;
    QAction *shadowEffectAction;
    QAction *bevelEffectAction;
    QAction *embossEffectAction;
    QAction *blurEffectAction;
    QAction *sharpenEffectAction;
    QAction *colorizeEffectAction;
    QAction *edgeEffectAction;

    bool isTemplateChange = false;

    BarBtn *borderDeleteBtn;
    BarBtn *textDeleteBtn;
    BarBtn *rectangleDeleteBtn;
    BarBtn *graphicDeleteBtn;
    BarBtn *styleDeleteBtn;
    BarBtn *effectDeleteBtn;

    ItemInfo i;
    int templateCount;
    QString templatePath;
    QSettings *setting;
    ComboBoxEditor *templateListEditor;
    QVector<ComboBoxEditor*> textAlignToCornerObjectEditor;
    QVector<ComboBoxEditor*> textAnchorObjectEditor;
    QVector<BarBtnEditor*> styleEditor;

    enum roots {
        _templates,
        _file,
        _image,
        _borders,
        _texts,
        _rectangles,
        _graphics,
        _styles
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
