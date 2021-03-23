#include "embelproperties.h"

/*
This class holds the template information used by Embel to create borders, add text,
rectangles and graphics to any image in the loupe view (ImageView). Template0 is an empty
template, which means no changes are made to the loupe image. The information is stored in
QSettings. QSettings are loaded into a StandardItemModel for the QTreeView. The model is
translated into structs and QVectors of struct such as QVector<Border>b. The structs make it
easier to use the information in Embel, where the graphics are build in the scene, which is
contained in ImageView.

Thus the embellish information is maintained in three information structures:

- QSettings         Save and load data between sessions
- Model             The property editor treeview (for one template + Styles)
- Local structs     Embel GraphicsView

QSettings structure:

Embel
    Styles
        Style1
            Effect1
            ...
        Style2
        ...
    Templates
        Do not Embellish (template0)
        Template1                   isCurrent
            File
            General
                Global light direction
                Image style
            Borders
                Border1
                ...
            Texts
                Text1
                ...
            Graphics
                Graphic1
                ...
        Template2
        ...
    Tiles

Each of the above in turn includes a series of fields the contain the information required by
Embel. The information is read from QSettings by additemsToRenumber, addBorders ... As each item
is read, it is sent to PropertyEditor::addItem, which builds a StandardItemModel to mirror the
QSettings. A custom widget is created in the treeview that is used to display and edit the
information.

When items change in the datamodel this->itemChanged is signalled.  The change is saved in
QSettings and the local structs.  Embel::build is called to update the QGraphicsScene in
ImageView.

Associated classes

    MW::embelDockWidget (container for EmbelProperties - this treeview)
        Embel
            ImageView (contains the QGraphicsScene)
        EmbelProperties
          - PropertyEditor
             - PropertyDelegate
                - PropertyWidgets

*/

#include "Main/mainwindow.h"
extern MW *mw3;
MW *mw3;
#define e mw3->embel

EmbelProperties::EmbelProperties(QWidget *parent, QSettings* setting): PropertyEditor(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    mw3 = qobject_cast<MW*>(parent);
    this->setting = setting;

    setSolo(false);
    setIndentation(10);
    setAlternatingRowColors(false);
    setMouseTracking(false);
    setHeaderHidden(true);
//    header()->setFixedHeight(0);

    ignoreFontSizeChangeSignals = false;

    /* Set column widths.  Note that the minimum width of the container dock widget is set in
       MW::createEmbelDock embelDock->setMinimumWidth(275) to make sure the - and + buttons
       in the header rows are always visible.  */
    stringToFitCaptions = "===captions column===";
    stringToFitValues   = "====values column====";
    resizeColumns();

    QModelIndex root = model->invisibleRootItem()->index();

    initialize();
    // get the list of templates and which one was last active
    readTemplateList();
    // add the template header, which then selects the last active template
    addTemplateHeader();

    if (templateId > 0) setRowHidden(1, root, false);
    else setRowHidden(1, root, true);

    // add the Styles, General, Image, Borders, Texts, and Graphics items for the template
    if (templateId != 0) addTemplateItems();
    // default state
    collapseAll();
    expand(model->index(_templates,0));
    updateHiddenRows(QModelIndex());
    setMouseTracking(true);
}

void EmbelProperties::initialize()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (longSidePx < 1) longSidePx = 500;
    anchorPoints << "Top Left" << "Top Center" << "Top Right"
                 << "Middle Left" << "Middle Center" << "Middle Right"
                 << "Bottom Left" << "Bottom Center" << "Bottom Right";
    anchorObjectList << "Image";
    anchorContainerList << "Top" << "Left" << "Right" << "Bottom";
    borderCorners << "TopLeft" << "TopRight" << "BottomLeft" << "BottomRight";
    embossContourList << "Flat"
                      << "Ridge"
                      << "Trough"
                      << "Gradient brighter"
                      << "Gradient darker"
                      << "Offset ridge"
                      << "Offset trough"
                         ;
    embossContourIconList << ":/images/icon16/emboss_flat.png"
                          << ":/images/icon16/emboss_ridge.png"
                          << ":/images/icon16/emboss_trough.png"
                          << ":/images/icon16/emboss_gradient_brighter.png"
                          << ":/images/icon16/emboss_gradient_darker.png"
                          << ":/images/icon16/emboss_offset_ridge.png"
                          << ":/images/icon16/emboss_offset_trough.png"
                             ;
    readGraphicsList();
    readTileList();
    readMetadataTemplateList();
    // EFFECTS
    effectList
                << "Shadow"
                << "Emboss"
                << "Stroke"
                << "Highlighter"
                << "Blur"
                << "Sharpen"
                << "Brightness"
//                << "Glow"
                   ;

    shadowEffectAction = new QAction(tr("Shadow"), this);
    shadowEffectAction->setObjectName("shadowEffectAction");
    connect(shadowEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    embossEffectAction = new QAction(tr("Emboss"), this);
    embossEffectAction->setObjectName("embossEffectAction");
    connect(embossEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    blurEffectAction = new QAction(tr("Blur"), this);
    blurEffectAction->setObjectName("blurEffectAction");
    connect(blurEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    sharpenEffectAction = new QAction(tr("Sharpen"), this);
    sharpenEffectAction->setObjectName("sharpenEffectAction");
    connect(sharpenEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    brightnessEffectAction = new QAction(tr("Brightness"), this);
    brightnessEffectAction->setObjectName("brightnessEffectAction");
    connect(brightnessEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    highlighterEffectAction = new QAction(tr("Highlighter"), this);
    highlighterEffectAction->setObjectName("highlighterEffectAction");
    connect(highlighterEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    strokeEffectAction = new QAction(tr("Stroke"), this);
    strokeEffectAction->setObjectName("strokeEffectAction");
    connect(strokeEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    glowEffectAction = new QAction(tr("Glow"), this);
    glowEffectAction->setObjectName("glowEffectAction");
    connect(glowEffectAction, &QAction::triggered, this, &EmbelProperties::newEffect);

    effectMenu = new QMenu(this);
    effectMenu->addAction(shadowEffectAction);
    effectMenu->addAction(embossEffectAction);
    effectMenu->addAction(strokeEffectAction);
    effectMenu->addAction(highlighterEffectAction);
    effectMenu->addAction(brightnessEffectAction);
    effectMenu->addAction(blurEffectAction);
//    effectMenu->addAction(sharpenEffectAction);
//    effectMenu->addAction(glowEffectAction);

    // CONTEXT MENU
    expandAllAction = new QAction(tr("Expand all"), this);
    expandAllAction->setShortcutVisibleInContextMenu(true);
    expandAllAction->setShortcut(QKeySequence("Ctrl+>"));
    addAction(expandAllAction);
    connect(expandAllAction, &QAction::triggered, this, &EmbelProperties::expandAllRows);

    collapseAllAction = new QAction(tr("Collapse all"), this);
    collapseAllAction->setShortcutVisibleInContextMenu(true);
    collapseAllAction->setShortcut(QKeySequence("Ctrl+<"));
    addAction(collapseAllAction);
    connect(collapseAllAction, &QAction::triggered, this, &EmbelProperties::collapseAllRows);

    soloAction = new QAction(tr("Solo"), this);
    soloAction->setShortcutVisibleInContextMenu(true);
//    soloAction->setShortcut(QKeySequence("Ctrl+*"));
    soloAction->setCheckable(true);
    soloAction->setVisible(true);
    addAction(soloAction);
    soloAction->setChecked(setting->value("Embel/isSolo").toBool());
    connect(soloAction, &QAction::triggered, this, &EmbelProperties::solo);
    solo();

    expandRecursivelyAction = new QAction(tr("Expand recursively"), this);
    expandRecursivelyAction->setShortcutVisibleInContextMenu(true);
//    expandRecursivelyAction->setShortcut(QKeySequence("Ctrl+*"));
    expandRecursivelyAction->setCheckable(true);
    expandRecursivelyAction->setVisible(true);
    addAction(expandRecursivelyAction);
    expandRecursivelyAction->setChecked(setting->value("Embel/expandRecursively").toBool());
    connect(expandRecursivelyAction, &QAction::triggered, this, &EmbelProperties::expandRecursively);
    expandRecursively();

    separatorAction0 = new QAction(this);
    separatorAction0->setSeparator(true);
    addAction(separatorAction0);

    renameAction = new QAction(tr("Rename template"), this);
    addAction(renameAction);
    connect(renameAction, &QAction::triggered, this, &EmbelProperties::rename);

    copyTemplateAction = new QAction(tr("Copy template"), this);
    addAction(copyTemplateAction);
    connect(copyTemplateAction, &QAction::triggered, this, &EmbelProperties::copyTemplate);

    newTemplateAction = new QAction(tr("New template"), this);
    addAction(newTemplateAction);
    connect(newTemplateAction, &QAction::triggered, this, &EmbelProperties::newTemplate);

    saveTemplateToFileAction = new QAction(tr("Save current template to file"), this);
    addAction(saveTemplateToFileAction);
    connect(saveTemplateToFileAction, &QAction::triggered, this, &EmbelProperties::saveTemplateToFile);

    readTemplateFromFileAction = new QAction(tr("Read template from file"), this);
    addAction(readTemplateFromFileAction);
    connect(readTemplateFromFileAction, &QAction::triggered, this, &EmbelProperties::readTemplateFromFile);

    copyStyleAction = new QAction(tr("Copy style"), this);
    addAction(copyStyleAction);
    connect(copyStyleAction, &QAction::triggered, this, &EmbelProperties::copyStyle);

    tokenEditorAction = new QAction(tr("Token Editor"), this);
    addAction(tokenEditorAction);
    connect(tokenEditorAction, &QAction::triggered, mw3, &MW::tokenEditor);

    manageTilesAction = new QAction(tr("Manage tiles"), this);
    addAction(manageTilesAction);
    connect(manageTilesAction, &QAction::triggered, this, &EmbelProperties::manageTiles);

    separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    addAction(separatorAction1);

    moveUpAction = new QAction(tr("Move up"), this);
    addAction(moveUpAction);
    connect(moveUpAction, &QAction::triggered, this, &EmbelProperties::moveUp);

    moveDownAction = new QAction(tr("Move down"), this);
    addAction(moveDownAction);
    connect(moveDownAction, &QAction::triggered, this, &EmbelProperties::moveDown);

    separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    addAction(separatorAction2);

    setContextMenuPolicy(Qt::ActionsContextMenu);
}

void EmbelProperties::effectContextMenu()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
//    effectParentIdx = btn->index;
    effectParentIdx = getItemIndex(btn->itemIndex);
    effectMenu->exec(QCursor::pos());
}

void EmbelProperties::newEffect()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString effect = (static_cast<QAction*>(sender()))->text();
    qDebug() << __FUNCTION__ << "effect =" << effect;
    if (effect == "Blur") addBlurEffect(effectParentIdx);
    if (effect == "Sharpen") addSharpenEffect(effectParentIdx);
    if (effect == "Highlighter") addHighlighterEffect(effectParentIdx);
    if (effect == "Shadow") addShadowEffect(effectParentIdx);
    if (effect == "Brightness") addBrightnessEffect(effectParentIdx);
    if (effect == "Emboss") addEmbossEffect(effectParentIdx);
    if (effect == "Stroke") addStrokeEffect(effectParentIdx);
    if (effect == "Glow") addGlowEffect(effectParentIdx);
    QModelIndex idx = model->index(model->rowCount(effectParentIdx) - 1, 0, effectParentIdx);
    expand(stylesIdx);
    expand(effectParentIdx);
    expand(idx);
    selectionModel()->clear();
    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    e->build("", __FUNCTION__);
}

void EmbelProperties::updateEffectOrderAfterDeletion(QModelIndex parIdx)
{
/*
After a deletion the sort order will no longer be contiguous. For example:

Before:                 1, 2, 3, 4
Delete the 2nd row:     1, 3, 4
Update to:              1, 2, 3
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // Get a standardItem as it supports sorting of the children, and the borders may be sorted
    QStandardItem *effectItem = new QStandardItem;
    effectItem = model->itemFromIndex(parIdx);
    for (int row = 0; row < effectItem->rowCount(); ++row) {
        QModelIndex idx = effectItem->child(row)->index();
        model->setData(idx, row, UR_SortOrder);
        QString path = idx.data(UR_SettingsPath).toString() + "/sortOrder";
        setting->setValue(path, row);
    }
}

void EmbelProperties::updateBorderOrderAfterDeletion()
{
    /*
After a deletion the sort order will no longer be contiguous. For example:

Before:                 1, 2, 3, 4
Delete the 2nd row:     1, 3, 4
Update to:              1, 2, 3
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // Get a standardItem as it supports sorting of the children, and the borders may be sorted
    QStandardItem *borders = new QStandardItem;
    borders = model->itemFromIndex(bordersIdx);
    for (int row = 0; row < borders->rowCount(); ++row) {
        QModelIndex idx = borders->child(row)->index();
        model->setData(idx, row, UR_SortOrder);
        QString path = idx.data(UR_SettingsPath).toString() + "/sortOrder";
        setting->setValue(path, row);
    }
}

void EmbelProperties::updateBorderLists()
{
/*
    Called from new and delete borders to rebuild the lists that have the borders
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    borderList.clear();
    anchorObjectList.clear();
    int borderCount = model->rowCount(bordersIdx);
    for (int i = 0; i < borderCount; ++i) {
        QString borderName = model->index(i, CapColumn, bordersIdx).data(UR_Name).toString();
        borderList << borderName;
        anchorObjectList << borderName;
    }
    anchorObjectList << "Image";

    borderList.sort();
    anchorObjectList.sort();

    // update texts and graphics that are anchored on a border
    updateAnchorObjects();

}

void EmbelProperties::updateAnchorObjects()
{
/*
    When borders are created or deleted the border and anchor lists are updated. This function
    iterates through all texts and graphics to refresh the anchorObjectList and reassign the
    anchor object if it was a border, since the border list will have changed.

    Note that refreshing the anchorObjectList or each text and graphic removes the previous
    selected value, so even if the anchor object remains the same it still must be reassigned.

    Also note that when the setValue function of the textAnchorObjectEditor and
    graphicAnchorObjectEditor trigger itemChange which in turn updates settings and local
    vectors.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // update texts anchor lists
    for (int i = 0; i < t.size(); ++i) {
        QString oldAnchorName = textAnchorObjectEditor.at(i)->value();
        textAnchorObjectEditor.at(i)->refresh(anchorObjectList);
        // refreshing anchorObjectList removes old value for the text - reassign anchor object
        if (anchorObjectList.contains(oldAnchorName))
            textAnchorObjectEditor.at(i)->setValue(oldAnchorName);
        else
            textAnchorObjectEditor.at(i)->setValue("Image");
    }

    // update graphics anchor lists
    for (int i = 0; i < g.size(); ++i) {
        QString oldAnchorName = graphicAnchorObjectEditor.at(i)->value();
        graphicAnchorObjectEditor.at(i)->refresh(anchorObjectList);
        // refreshing anchorObjectList removes old value for the graphic - reassign anchor object
        if (anchorObjectList.contains(oldAnchorName))
            graphicAnchorObjectEditor.at(i)->setValue(oldAnchorName);
        else
            graphicAnchorObjectEditor.at(i)->setValue("Image");
    }
}

void EmbelProperties::diagnosticStyles(QTextStream &rpt)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    using namespace winnow_effects;
    QMapIterator<QString, QList<Effect>> s(styleMap);
    while (s.hasNext()) {
        s.next();
        int n = s.value().length();
        rpt << "\n\n" "Style " << s.key() << " has " << n << " effect(s).";
        // effects in style
        for (int i = 0; i < n; ++i) {
            const Effect &ef = s.value().at(i);
            rpt << "\n" << "  effectName     = " << ef.effectName;
            rpt << "\n" << "    Effect index = " << i;
            rpt << "\n" << "    effectType   = " << ef.effectType;
            rpt << "\n" << "    effectOrder  = " << ef.effectOrder;

            switch (ef.effectType) {
            case winnow_effects::blur:
                rpt << "\n" << "      Blur: radius            = " << ef.blur.radius;
                break;
            case winnow_effects::brightness:
                rpt << "\n" << "      Brightness: evDelta     = " << ef.brightness.evDelta;
                break;
            case winnow_effects::emboss:
                rpt << "\n" << "      Emboss: size            = " << ef.emboss.size;
                rpt << "\n" << "      Emboss: inflection      = " << ef.emboss.inflection;
                rpt << "\n" << "      Emboss: exposure        = " << ef.emboss.exposure;
                rpt << "\n" << "      Emboss: contrast        = " << ef.emboss.contrast;
                rpt << "\n" << "      Emboss: start           = " << ef.emboss.start;
                rpt << "\n" << "      Emboss: mid             = " << ef.emboss.mid;
                rpt << "\n" << "      Emboss: end             = " << ef.emboss.end;
                rpt << "\n" << "      Emboss: umbra           = " << ef.emboss.umbra;
                rpt << "\n" << "      Emboss: isUmbraGradient = " << ef.emboss.isUmbraGradient;
                break;
            case winnow_effects::sharpen:
                rpt << "\n" << "      Sharpen: radius         = " << ef.sharpen.radius;
                break;
            case winnow_effects::highlighter:
                rpt << "\n" << "      Highlighter: top        = " << ef.highlighter.top;
                rpt << "\n" << "      Highlighter: left       = " << ef.highlighter.left;
                rpt << "\n" << "      Highlighter: right      = " << ef.highlighter.right;
                rpt << "\n" << "      Highlighter: bottom     = " << ef.highlighter.bottom;
                rpt << "\n" << "      Highlighter: r          = " << ef.highlighter.r;
                rpt << "\n" << "      Highlighter: g          = " << ef.highlighter.g;
                rpt << "\n" << "      Highlighter: b          = " << ef.highlighter.b;
                rpt << "\n" << "      Highlighter: a          = " << ef.highlighter.a;
                break;
            case winnow_effects::shadow:
                rpt << "\n" << "      Shadow: size            = " << ef.shadow.length;
                rpt << "\n" << "      Shadow: blurRadius      = " << ef.shadow.blurRadius;
                rpt << "\n" << "      Shadow: r               = " << ef.shadow.r;
                rpt << "\n" << "      Shadow: g               = " << ef.shadow.g;
                rpt << "\n" << "      Shadow: b               = " << ef.shadow.b;
                rpt << "\n" << "      Shadow: a               = " << ef.shadow.a;
                break;
            case winnow_effects::stroke:
                rpt << "\n" << "      Stroke: size            = " << ef.stroke.width;
                rpt << "\n" << "      Stroke: r               = " << ef.stroke.r;
                rpt << "\n" << "      Stroke: g               = " << ef.stroke.g;
                rpt << "\n" << "      Stroke: b               = " << ef.stroke.b;
                rpt << "\n" << "      Stroke: a               = " << ef.stroke.a;
                break;
            case winnow_effects::glow:
                rpt << "\n" << "      Glow: size              = " << ef.glow.width;
                rpt << "\n" << "      Glow: blurRadius        = " << ef.glow.blurRadius;
                rpt << "\n" << "      Glow: r                 = " << ef.glow.r;
                rpt << "\n" << "      Glow: g                 = " << ef.glow.g;
                rpt << "\n" << "      Glow: b                 = " << ef.glow.b;
                rpt << "\n" << "      Glow: a                 = " << ef.glow.a;
                break;
            }
        }
    }
}

void EmbelProperties::diagnosticVectors(QTextStream &rpt)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    rpt << "\n" << "\nVectors:";
    if (b.length()) rpt << "\n" << "Border(s)";
    for (int i = 0; i < b.length(); ++i) {
        rpt << "\n"
            << i
            << ". name = " << b[i].name
            << ", top = " << b[i].top
            << ", left = " << b[i].left
            << ", right = " << b[i].right
            << ", bottom = " << b[i].bottom
            << ", tile = " << b[i].tile
            << ", color = " << b[i].color
            << ", opacity = " << b[i].opacity
            << ", style = " << b[i].style;
    }
    if (t.length()) rpt << "\n" << "Text(s)";
    for (int i = 0; i < t.length(); ++i) {
        rpt << "\n"
            << i
            << " name = " << t[i].name
            << " caption = " << t[i].caption
            << " parent = " << t[i].parent
            << " anchorObject = " << t[i].anchorObject
            << " anchorContainer = " << t[i].anchorContainer
            << " anchorPoint = " << t[i].anchorPoint
            << " x = " << t[i].x
            << " y = " << t[i].y
            << " text = " << t[i].text
            << " rotation = " << t[i].rotation
            << " metadataTemplate = " << t[i].metadataTemplate
            << " size = " << t[i].size
            << " font = " << t[i].font
            << " isBold = " << t[i].isBold
            << " isItalic = " << t[i].isItalic
            << " color = " << t[i].color
            << " opacity = " << t[i].opacity
            << " style = " << t[i].style
               ;
    }
    if (g.length()) rpt << "\n" << "Graphic(s)";
    for (int i = 0; i < g.length(); ++i) {
        rpt << "\n"
            << i
            << " name = " << g[i].name
            << " caption = " << g[i].caption
            << " parent = " << g[i].parent
            << " filePath = " << g[i].filePath
            << " anchorObject = " << g[i].anchorObject
            << " anchorContainer = " << g[i].anchorContainer
            << " anchorPoint = " << g[i].anchorPoint
            << " x = " << g[i].x
            << " y = " << g[i].y
            << " rotation = " << g[i].rotation
            << " size = " << g[i].size
            << " opacity = " << g[i].opacity
            << " style = " << g[i].style
               ;
    }
}

void EmbelProperties::diagnosticModel(QTextStream &rpt, QModelIndex parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    static int iteration = 0;
    if (iteration == 0) {
        rpt << "\n\n" "Embellish Properties Model:\n\n";  // << rowCount << " items.\n\n";

        // Report headers
        QString cBlank = "-";

        QString cName = "Name";
        QString cParent = "Parent";
        QString cRow = "Row";
        QString cSortOrder = "Order";
        QString cKey = "Key";
        QString cValue = "Value";
        QString cDefaultValue = "DefaultValue";
        QString cItemIndex = "ItemIdx";
        QString cSettingsPath = "Settings Path";
        QString cDelegateType = "Delegate Type";
        QString cMin = "Min";
        QString cMax = "Max";
        QString cDiv = "Div";

        rpt << cParent.leftJustified(16);
        rpt << cName.leftJustified(16);
        rpt << cRow.leftJustified(5);
        rpt << cSortOrder.leftJustified(6);
        rpt << cItemIndex.leftJustified(8);
        rpt << cKey.leftJustified(15);
        rpt << cValue.leftJustified(15);
        rpt << cDefaultValue.leftJustified(15);
        rpt << cMin.leftJustified(5);
        rpt << cMax.leftJustified(8);
        rpt << cDiv.leftJustified(8);
        rpt << cSettingsPath.leftJustified(70);

        // Underline headers
        rpt << "\n";
        rpt << cBlank.leftJustified(15, '-') << " ";
        rpt << cBlank.leftJustified(15, '-') << " ";
        rpt << cBlank.leftJustified(4, '-') << " ";
        rpt << cBlank.leftJustified(5, '-') << " ";
        rpt << cBlank.leftJustified(7, '-') << " ";
        rpt << cBlank.leftJustified(14, '-') << " ";
        rpt << cBlank.leftJustified(14, '-') << " ";
        rpt << cBlank.leftJustified(14, '-') << " ";
        rpt << cBlank.leftJustified(4, '-') << " ";
        rpt << cBlank.leftJustified(7, '-') << " ";
        rpt << cBlank.leftJustified(7, '-') << " ";
        rpt << cBlank.leftJustified(69, '-') << " ";
    }
    iteration++;

    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx0 = model->index(r, CapColumn, parent);
        QModelIndex idx1 = model->index(r, ValColumn, parent);
        QString parentName = parent.data(UR_Name).toString();
        QString name = idx0.data(UR_Name).toString();
        QString row = QString::number(r);
        QString sortOrder = idx0.data(UR_SortOrder).toString();
        QString key = idx0.data(UR_Source).toString();
        QString value = idx1.data(Qt::EditRole).toString();
        QString defaultValue = idx1.data(UR_DefaultValue).toString();
        QString itemIndex = idx0.data(UR_ItemIndex).toString();
        QString settingsPath = idx0.data(UR_SettingsPath).toString();
        QString delegateType = idx0.data(UR_DelegateType).toString();
        QString min = idx0.data(UR_Min).toString();
        QString max = idx0.data(UR_Max).toString();
        QString div = idx0.data(UR_Div).toString();

        rpt << "\n";
        rpt << parentName.leftJustified(16);
        rpt << name.leftJustified(16);
        rpt << row.leftJustified(5);
        rpt << sortOrder.leftJustified(6);
        rpt << itemIndex.leftJustified(8);
        rpt << key.leftJustified(15);
        rpt << value.leftJustified(15);
        rpt << defaultValue.leftJustified(15);
        rpt << min.leftJustified(5);
        rpt << max.leftJustified(8);
        rpt << div.leftJustified(8);
        rpt << settingsPath.leftJustified(70);

        // iterate children
        if (model->hasChildren(idx0)) {
            diagnosticModel(rpt, idx0);
        }
    }
}

/* void EmbelProperties::diagnosticModel(QModelIndex parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // model
    // Report headers
    QString c1 = "Idx";
    QString c2 = "Header";
    QString c3 = "Key";
    QString c4 = "Value";
    QString c5 = "SettingPath";
    QString c0 = "-";
    qDebug();
    // Underline headers
    qDebug().noquote()
             << c1.leftJustified(4)
             << c2.leftJustified(15)
             << c3.leftJustified(25)
             << c4.leftJustified(40)
             << c5.leftJustified(70);
    qDebug().noquote()
            << c0.leftJustified(4, '-')
            << c0.leftJustified(15, '-')
            << c0.leftJustified(25, '-')
            << c0.leftJustified(40, '-')
            << c0.leftJustified(70, '-');

    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx0 = model->index(r, CapColumn, parent);
        QModelIndex idx1 = model->index(r, ValColumn, parent);
        QString p = parent.data(UR_Name).toString();
        QString n = idx0.data(UR_Name).toString();
        QString u = idx0.data(UR_ItemIndex).toString();
        QString v = idx1.data(Qt::EditRole).toString();
        QString s = idx0.data(UR_Source).toString();
        QString path = idx0.data(UR_SettingsPath).toString();
        qDebug().noquote()
                 << u.leftJustified(4)
                 << p.leftJustified(15)
                 << n.leftJustified(25)
                 << v.leftJustified(40)
                 << path.leftJustified(70);
        // iterate children
        if (model->hasChildren(idx0)) {
            diagnosticModel(idx0);
        }
    }
}
*/

void EmbelProperties::rename() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (currentIdx.parent() == templateIdx) renameCurrentTemplate();
    if (currentIdx == templateIdx) renameCurrentTemplate();
    if (currentIdx.parent() == stylesIdx) renameCurrentStyle();
}

void EmbelProperties::copy() {
    // contextmenu placeholder
}

void EmbelProperties::moveUp() {
    // contextmenu placeholder
}

void EmbelProperties::moveDown() {
    // contextmenu placeholder
}

void EmbelProperties::renameSettingKey(QString path, QString oldName, QString newName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // if name is blank then could remove all child items in settings
    if (oldName == "") {
        qDebug() << __FUNCTION__ << "Error: Blank name";
        return;
    }
    QString path1 = path + "/" + oldName;
    QString path2 = path + "/" + newName;
    setting->beginGroup(path1);
    QStringList keys = setting->allKeys();
    setting->endGroup();
    for (int i = 0; i < keys.length(); ++i) {
        QString key1 = path1 + "/" + keys.at(i);
        QString key2 = path2 + "/" + keys.at(i);
        setting->setValue(key2, setting->value(key1));
    }
    setting->remove(path1);
}

void EmbelProperties::renameCurrentStyle()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
//    BarBtn *btn = qobject_cast<BarBtn*>(sender());
//    QModelIndex idx = btn->index;
    QModelIndex idx = currentIdx;
    QString oldName = idx.data().toString();
    QString newName = Utilities::inputText("Rename Style", "Rename style " + oldName,
                                           styleList, oldName);
    if (newName == "") return;

    // update setting
    renameSettingKey("Embel/Templates/" + templateName + "/Styles", oldName, newName);

    // update styleMap (copy to tmp with name change and swap back)
    using namespace winnow_effects;
    QMap<QString, QList<Effect>> tmp;
    QMapIterator<QString, QList<Effect>> s(styleMap);
    while (s.hasNext()) {
        s.next();
        QString style = s.key();
        if (style == oldName) style = newName;
        tmp[style] = s.value();
    }
    styleMap.swap(tmp);

    // update model
    model->setData(idx, newName);
    // update all stylelist dropdowns in model
    for (int i = 0; i < styleListObjectEditor.length(); ++i) {
        styleListObjectEditor[i]->renameItem(oldName, newName);
        if (styleListObjectEditor[i]->value() == oldName)
            styleListObjectEditor[i]->setValue(newName);
    }

    // cleanup remove oldName from styleMap
    if (styleMap.contains(oldName)) {
        styleMap.remove(oldName);
    }
}

void EmbelProperties::renameCurrentTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    qDebug() << __FUNCTION__;
    QString name = Utilities::inputText("Rename Template",
                                        "Rename template " + templateName,
                                        templateList,
                                        templateName);
    if (name == "") return;
    renameSettingKey("Embel/Templates", templateName, name);
    readTemplateList();
    templateListEditor->refresh(templateList);
    templateListEditor->setValue(name);
    emit syncEmbellishMenu();
}

void EmbelProperties::setCurrentTemplate(QString name)
{
/*
    Sets the current template to templateName, which must be assigned before calling this
    function. The templatePath and isCurrent are defined. The template is selected.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (name == "") return;
    templateName = name;
    for (int i = 0; i < templateList.length(); ++i) {
        QString t = templateList.at(i);
        bool isCurrent = (t == templateName);
        QString path = "Embel/Templates/" + t + "/isCurrent";
        setting->setValue(path, isCurrent);
        if (isCurrent) {
            templateId = i;
            templatePath = "Embel/Templates/" + templateName + "/";
        }
    }
    templateListEditor->setValue(templateName);

//    qDebug() << __FUNCTION__ << templateId << templateName << templatePath;
}

void EmbelProperties::sortTemplateList()
{
/*
The sort order of the templateList has "Do not Embellish" first, followed by the template
items in alphabetical order. The template items are read from settings to a temp list, sorted,
and then appended to templateList.

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (templateList.length() < 2) return;
    QStringList list;
    for (int i = 1; i < templateList.length(); ++i) {
        list << templateList.at(i);
    }
    list.sort(Qt::CaseInsensitive);
    templateList.clear();
    templateList << "Do not Embellish" << list;
}

void EmbelProperties::readTemplateList()
{
/*
    Read the template list using QSettings. The sort order of the templateList has "Do not
    Embellish" first, followed by the template items in alphabetical order. The template items
    are read from settings to a temp list, sorted, and then appended to templateList.

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    templateList.clear();
    templateId = 0;
    templateName = "Do not Embellish";
    setting->beginGroup("Embel/Templates");
    // Add "Do not Embellish" as first template
    templateList << templateName;
    // 1st time, no templates created yet
    if (setting->childGroups().count() == 0) {
        setting->setValue("Do not Embellish/isCurrent", true);
    }
    else {
        // build list of templates
        for (int i = 0; i < setting->childGroups().count(); ++i) {
            if (setting->childGroups().at(i) != "Do not Embellish")
                templateList << setting->childGroups().at(i);
        }
        sortTemplateList();
       // determine which template is current
        for (int i = 1; i < templateList.size(); ++i) {
            QString t = templateList.at(i);
            QString path = t + "/isCurrent";
            if (setting->value(path).toBool()) {
                templateName = t;
                templateId = i;
            }
        }
    }
    setting->endGroup();
    templatePath = "Embel/Templates/" + templateName + "/";
    if (templateId == 0) G::isEmbellish = false;
    else G::isEmbellish = true;

    syncWinnets();
}

QString EmbelProperties::uniqueTemplateName(QString name)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    setting->beginGroup("Embel/Templates");
    QStringList keys = setting->childGroups();
    setting->endGroup();
    bool nameExists = true;
    QString newName = name;
    int count = 0;
    while (nameExists) {
        if (keys.contains(newName)) newName = name + "_" + QString::number(++count);
        else nameExists = false;
    }
    return newName;
}

QString EmbelProperties::uniqueTileName(QString name)
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
#ifdef ISLOGGER
        Utilities::log(__FUNCTION__, "");
#endif
    }
    setting->beginGroup("Embel/Tiles");
    QStringList keys = setting->childKeys();
    setting->endGroup();
    qDebug() << __FUNCTION__ << name << keys;
    bool nameExists = true;
    QString newName = name;
    int count = 0;
    while (nameExists) {
        if (keys.contains(newName)) newName = name + "_" + QString::number(++count);
        else nameExists = false;
    }
    return newName;
}

QString EmbelProperties::uniqueGraphicName(QString name)
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
#ifdef ISLOGGER
        Utilities::log(__FUNCTION__, "");
#endif
    }
    setting->beginGroup("Embel/Graphics");
    QStringList keys = setting->childKeys();
    setting->endGroup();
    bool nameExists = true;
    QString newName = name;
    int count = 0;
    while (nameExists) {
        if (keys.contains(newName)) newName = name + "_" + QString::number(++count);
        else nameExists = false;
    }
    return newName;
}

QString EmbelProperties::uniqueTokenName(QString name)
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
#ifdef ISLOGGER
        Utilities::log(__FUNCTION__, "");
#endif
    }
    setting->beginGroup("InfoTemplates");
    QStringList keys = setting->childKeys();
    setting->endGroup();
    bool nameExists = true;
    QString newName = name;
    int count = 0;
    while (nameExists) {
        if (keys.contains(newName)) newName = name + "_" + QString::number(++count);
        else nameExists = false;
    }
    return newName;
}

void EmbelProperties::copyStyle()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString style = currentIdx.data().toString();
    QString toTemplate = "";
    CopyStyleDlg copyDlg(style, toTemplate, templateList);
    if (!copyDlg.exec()) return;
    QString copyName = style;  //uniqueTemplateName(templateName + " copy");
    qDebug() << __FUNCTION__ << toTemplate << copyName;
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/";
    QString copyPath = "Embel/Templates/" + toTemplate + "/Styles/" + copyName + "/";;
    setting->beginGroup(path);
    QStringList keys = setting->allKeys();
    setting->endGroup();
    for (int i = 0; i < keys.length(); ++i) {
        QString key = path + keys.at(i);
        setting->setValue(copyPath + keys.at(i), setting->value(key));
    }

}

void EmbelProperties::copyTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/";
    QString copyName = uniqueTemplateName(templateName + " copy");
    QString copyPath = "Embel/Templates/" + copyName + "/";
    setting->beginGroup(path);
    QStringList keys = setting->allKeys();
    setting->endGroup();
    for (int i = 0; i < keys.length(); ++i) {
        QString key = path + keys.at(i);
        setting->setValue(copyPath + keys.at(i), setting->value(key));
    }
    templateList << copyName;
    sortTemplateList();
    templateListEditor->refresh(templateList);
    templateName = copyName;
    templateListEditor->setValue(templateName);
}

bool EmbelProperties::saveTemplateToFile()
{
/*
    Saves the current template to a file:

    Load all the template QSettings keys into a QMap.  Convert the QMap to a json document.

    User selects a folder. Create a unique temporary subfolder using the template name. Write
    the json document to templateName.template in the subfolder.

    Write any tiles used by the template as tileName.tile (QByteArray) in the subfolder. Same
    thing for any graphics used by the template as graphicName.graphic (QByteArray).

    Save any tokens used by texts using the QSettings > QMap > json process.

    Compress the subfolder into one binary file.

    Remove the temporary subfolder.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // cannot save "Do not Embellish"
    if (templateName == "Do not Embellish") {
        QString msg = "Cannot save template 'Do not Embellish'";
        G::popUp->showPopup(msg, 2000);
        return false;
    }

    // path to source template in setting
    QString srcPath = "Embel/Templates/" + templateName + "/";

    // get all QSettings keys for the template
    setting->beginGroup(srcPath);
    QStringList keys = setting->allKeys();
    setting->endGroup();

    // QMap to hold key/value pairs
    QMap<QString, QVariant> keysMap;

    // QSettings to QMap
    for (int i = 0; i < keys.length(); ++i) {
        QString key = srcPath + keys.at(i);
        keysMap.insert(key, setting->value(key));
    }

    // QMap to json
    QJsonObject keysJson = QJsonObject::fromVariantMap(keysMap);

    // create a folder to hold the template json, tiles and graphics
    QString msg = "Select folder to save template " + templateName;
    QString locPath = QFileDialog::getExistingDirectory(this, msg,
         "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    // cancel?
    if (locPath == "") return false;
    QString dPath = locPath + "/" + templateName;
    // rename folder (named as template name) does not exist
    Utilities::uniqueFolderPath(dPath);
    QDir dir;
    dir.mkdir(dPath);
    dir.setPath(dPath);

    // create a file for the json encoded template settings
    QString fName = dPath + "/" + templateName + ".template";
    QFile file(fName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Couldn't open save file.");
        return false;
    }

    // write template json to file
    file.write(QJsonDocument(keysJson).toJson());
    file.close();

    // copy tiles (if any)
    for (int i = 0; i < model->rowCount(bordersIdx); ++i) {
        QModelIndex bIdx = model->index(i, 0, bordersIdx);
        QString tileName = getItemValue("tile", bIdx).toString();
        QFile file(dPath + "/" + tileName + ".tile");
        if (tileName != "Do not tile" && !file.exists()) {
            QSaveFile f(dPath + "/" + tileName + ".tile");
            f.open(QIODevice::WriteOnly);
            f.write(b[i].tile);
            f.commit();
        }
    }

    // copy graphics (if any)
    for (int i = 0; i < model->rowCount(graphicsIdx); ++i) {
        QModelIndex gIdx = model->index(i, 0, graphicsIdx);
        QString graphicName = getItemValue("graphic", gIdx).toString();
        QFile file(dPath + "/" + graphicName + ".graphic");
        if (graphicName != "Do not tile" && !file.exists()) {
            QSaveFile f(dPath + "/" + graphicName + ".graphic");
            f.open(QIODevice::WriteOnly);
            f.write(g[i].graphic);
            f.commit();
        }
    }

    // copy tokens (if any)
    keys.clear();
    keysMap.clear();
    for (int i = 0; i < model->rowCount(textsIdx); ++i) {
        QModelIndex tIdx = model->index(i, 0, textsIdx);
        keys << getItemValue("metadataTemplate", tIdx).toString();
    }
//    qDebug() << __FUNCTION__ << keys; return true;
    /*
    setting->beginGroup("InfoTemplates");
        keys = setting->allKeys();
    setting->endGroup();
//    */
    if (keys.length() > 0) {
        for (int i = 0; i < keys.length(); ++i) {
            QString key = "InfoTemplates/" + keys.at(i);
            keysMap.insert(keys.at(i), setting->value(key));
        }
        keysJson = QJsonObject::fromVariantMap(keysMap);
        fName = dPath + "/tokens.json";
        QFile fTokens(fName);
        if (!fTokens.open(QIODevice::WriteOnly)) {
            qWarning("Couldn't open file to save tokens.");
            return false;
        }
        fTokens.write(QJsonDocument(keysJson).toJson());
        fTokens.close();
    }

    // compress folder into one binary file
    QString compressed = locPath + "/" + templateName + ".embeltemplate";
    Utilities::uniqueFilePath(compressed);
    FolderCompressor fc;
    fc.compressFolder(dPath, compressed);
    dir.removeRecursively();

    return true;
}

bool EmbelProperties::readTemplateFromFile()
{
/*
    Read a embeltemplate file that contains all the information required to recreate a
    template in embellish.

    Locate and select a template file, ending in .embeltemplate

    Decompress the .embeltemplate file into a temporary folder with a unique name in the same
    folder. The folder will contain:
        - one xxx.template file where xxx = the template name that was saved
        - one or more .tile files (QByteArrays of tile PNG files)
        - one or more .graphic files (QByteArrays of tile PNG files)

    Make sure the xxx template name does not exist. If it does, get a unique template name.
    Read the template file and replace all references to the xxx template name with the unique
    name.

    Read all the tile files, make sure tile names are unique, and add them to QSettings.

    Read all the graphic files, make sure tile names are unique, and add them to QSettings.

    Decode the QByteArray tokens (from file read) to a json document.  Convert the json
    document to a QMap.  Use the QMap to add the tokens to QSettings if they do not already
    exist.

    Decode the QByteArray template (from file read) to a json document.  Convert the json
    document to a QMap.  Use the QMap to add the template to QSettings.

    Load the new template.

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // select compressed temple file
    QString srcPath = QFileDialog::getOpenFileName(this, tr("Select template"),
                                                 "/home", "*.embeltemplate");
    QFileInfo srcInfo(srcPath);

    // geta unique folder name for the decompressed information
    QString srcFileName = srcInfo.baseName();
    QString dstPath = srcInfo.absolutePath() + "/" + srcFileName;
    Utilities::uniqueFolderPath(dstPath);
    FolderCompressor fc;

    // decompress template file into folder named after src name
    fc.decompressFolder(srcPath, dstPath);

    // Prep for filtering and reading template, tiles and graphics
    QDir dir(dstPath);
    QStringList fileFilters;

    // filter on the template file
    fileFilters << "*.template";
    dir.setNameFilters(fileFilters);
    dir.setFilter(QDir::Files);

    // there should only be one .template file
    if (dir.entryInfoList().length() != 1) return false;

    // the source template name is the base name of the .template file
    QString srcTName = dir.entryInfoList().at(0).baseName();

    // make sure the template does not already exist, get unique name
    QString uniqueTName  = uniqueTemplateName(srcTName);
    bool dupTemplateName = uniqueTName != srcTName;

    // read json template file
    QString tPath = dir.entryInfoList().at(0).absoluteFilePath();
//    QString tPath = dstPath + "/" + srcTemplateName + ".template";
    QFile tFile(tPath);
    if (!tFile.open(QIODevice::ReadOnly)) {
        qDebug() << __FUNCTION__ << "Could not open template file.";
        qWarning("Couldn't open template file.");
        return false;
    }
    QByteArray ba = tFile.readAll();
    tFile.close();

    // rename template in template json file if already exists in embellish
    if (dupTemplateName) {
        QString a = "Templates/" + srcTName;
        QString b = "Templates/" + uniqueTName;
//        qDebug() << __FUNCTION__ << a << b;
        ba.replace(a.toUtf8(), b.toUtf8());
    }

    // checking if tile files already exist in embellish
    fileFilters.clear();
    fileFilters << "*.tile";
    dir.setNameFilters(fileFilters);
    dir.setFilter(QDir::Files);
    for (int i = 0; i < dir.entryInfoList().length(); ++i) {
        // get new tile QByteArray
        QFile f(dir.entryInfoList().at(i).filePath());
        f.open(QIODevice::ReadOnly);
        QByteArray newTileBa = f.readAll();
        f.close();
        // get unique name
        QString name = dir.entryInfoList().at(i).baseName();
        QString uniqueName = uniqueTileName(name);
        if (name != uniqueName) {
            // tile name already exists, check if tile QByteArray is also the same
            QByteArray existingTileBa = setting->value("Embel/Tiles/" + name).toByteArray();
            // if tile name and tile QByteArray same then do not save again
            if (newTileBa == existingTileBa) continue;
            // tiles are diffent, so rename tile in the template settings being read
            QString a = "/tile\": \"" + name;
            QString b = "/tile\": \"" + uniqueName;
            ba.replace(a.toUtf8(), b.toUtf8());
        }
        // add tile to QSettings
        QString settingPath = "Embel/Tiles/" + uniqueName;
        setting->setValue(settingPath, newTileBa);
    }

    // checking if graphic files already exist in embellish
    fileFilters.clear();
    fileFilters << "*.graphic";
    dir.setNameFilters(fileFilters);
    dir.setFilter(QDir::Files);
    for (int i = 0; i < dir.entryInfoList().length(); ++i) {
        // get new tile QByteArray
        QFile f(dir.entryInfoList().at(i).filePath());
        f.open(QIODevice::ReadOnly);
        QByteArray newGraphicBa = f.readAll();
        f.close();
        // get unique name
        QString name = dir.entryInfoList().at(i).baseName();
        QString uniqueName = uniqueGraphicName(name);
        if (name != uniqueName) {
            // graphic name already exists, check if graphic QByteArray is also the same
            QByteArray existingGraphicBa = setting->value("Embel/Graphics/" + name).toByteArray();
            // if tile name and tile QByteArray same then do not save again
            if (newGraphicBa == existingGraphicBa) continue;
            // the graphics are diffent, so rename graphic in the template settings being read
            QString a = "/graphic\": \"" + name;
            QString b = "/graphic\": \"" + uniqueName;
            ba.replace(a.toUtf8(), b.toUtf8());
        }
        // add graphic to QSettings
        QString settingPath = "Embel/Graphics/" + uniqueName;
        setting->setValue(settingPath, newGraphicBa);
    }

    // Get tokens
    QFile tokenFile(dstPath + "/tokens.json");
    if (tokenFile.exists()) {
        // QFile > QByteArray > JsonDocument > QMap > QSettings
        tokenFile.open(QIODevice::ReadOnly);
        QByteArray tokenArray = tokenFile.readAll();
        QJsonDocument tokenJsonDoc = QJsonDocument::fromJson(tokenArray);
        QMap<QString, QVariant> tokensMap;
        tokensMap = tokenJsonDoc.object().toVariantMap();
        // add token templates and save in QSettings
        mw3->infoString->add(tokensMap);
    }
    tokenFile.close();

    // Recover the template settings
    // recreate jsonDoc from QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(ba);

    // QMap to hold key/value pairs
    QMap<QString, QVariant> keysMap;
    keysMap = jsonDoc.object().toVariantMap();

    // QMap to QSettings
    QMapIterator<QString, QVariant> i(keysMap);
    while (i.hasNext()) {
        i.next();
        setting->setValue(i.key(), i.value());
    }

    // new Template
    newTemplateFromImport(uniqueTName);

    // cleanup temp folder created
    dir.removeRecursively();

    return true;
}

void EmbelProperties::readTileList()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    tileList.clear();
    tileList << "Do not tile";
    setting->beginGroup("Embel/Tiles");
    tileList << setting->allKeys();
    setting->endGroup();
}

void EmbelProperties::readGraphicsList()
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
#ifdef ISLOGGER
        Utilities::log(__FUNCTION__, "");
#endif
    }
    graphicList.clear();
    setting->beginGroup("Embel/Graphics");
    graphicList << setting->allKeys();
    setting->endGroup();
}

void EmbelProperties::manageTiles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    ManageImagesDlg dlg("Manage Embellish Tiles", setting, "Embel/Tiles");
    connect(&dlg, &ManageImagesDlg::itemRenamed, this, &EmbelProperties::updateTileList);
    dlg.exec();
    updateTileList();
}


void EmbelProperties::updateTileList(QString oldName, QString newName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    tileList.clear();
    setting->beginGroup("Embel/Tiles");
    tileList << setting->allKeys();
    setting->endGroup();
    tileList.sort();
    tileList.insert(0, "Do not tile");

    // update tileList in each border
    for (int i = 0; i < b.size(); ++i) {
        QString oldTileName = borderTileObjectEditor.at(i)->value();
        if (oldTileName == oldName) oldTileName = newName;
        borderTileObjectEditor.at(i)->refresh(tileList);
        // refreshing anchorObjectList removes old value for the text - reassign anchor object
        if (tileList.contains(oldTileName))
            borderTileObjectEditor.at(i)->setValue(oldTileName);
        else
            borderTileObjectEditor.at(i)->setValue("Do not tile");
    }
}

void EmbelProperties::manageGraphics()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    ManageImagesDlg dlg("Manage Embellish Graphics", setting, "Embel/Graphics");
    connect(&dlg, &ManageImagesDlg::itemRenamed, this, &EmbelProperties::updateGraphicList);
    dlg.exec();
    updateGraphicsList();
}

void EmbelProperties::updateGraphicList(QString oldName, QString newName)
{
    graphicList.clear();
    setting->beginGroup("Embel/Graphics");
    graphicList << setting->allKeys();
    setting->endGroup();
    graphicList.sort();
    qDebug() << __FUNCTION__ << graphicList;
    // update graphicList in each graphic item
    for (int i = 0; i < g.size(); ++i) {
        QString oldGraphicName = graphicsObjectEditor.at(i)->value();
        if (oldGraphicName == oldName) oldGraphicName = newName;
        graphicsObjectEditor.at(i)->refresh(graphicList);
        // refreshing anchorObjectList removes old value for the text - reassign anchor object
        if (graphicList.contains(oldGraphicName))
            graphicsObjectEditor.at(i)->setValue(oldGraphicName);
    }
}

void EmbelProperties::readMetadataTemplateList()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    metadataTemplatesList.clear();
    QMapIterator<QString, QString> i(mw3->infoString->infoTemplates);
    while (i.hasNext()) {
        i.next();
        metadataTemplatesList << i.key();
    }
    metadataTemplatesList.sort(Qt::CaseInsensitive);
}

void EmbelProperties::renameMetadataTemplateList(QString oldName, QString newName)
{
/*
    When a token template is renamed in TokenDlg, InfoString signals this slot to update the
    token template name in the Texts Template field combobox list.
*/
    // update model (triggers itemChangeText which updates QSettings and vector)
    for (int i = 0; i < textTokenObjectEditor.size(); ++i) {
        // guard index
        if (i >= t.size()) break;
        // ignore if the source is a text string
        if (t.at(i).source == "Text") continue;
        textTokenObjectEditor.at(i)->renameItem(oldName, newName);
    }
}

void EmbelProperties::updateMetadataTemplateList()
{
/*
    After the token editor has been invoked and closed the text metadataTemplate lists
    are updated in case the template list has changed (new items or deleted items).
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // reread the metadata template list
    readMetadataTemplateList();

    // make sure everything is in sync
    if (textTokenObjectEditor.size() != t.size()) {
        return;
    }

    // update text metadataTemplate lists
    for (int i = 0; i < textTokenObjectEditor.size(); ++i) {
        // ignore if the source in not a metadata template (cause crash)
        if (t.at(i).source == "Text") continue;
        QString oldTemplateName = textTokenObjectEditor.at(i)->value();
        textTokenObjectEditor.at(i)->refresh(metadataTemplatesList);
        // refreshing metadataTemplatesList removes old value for the text - reassign textMetadataTemplate object
        if (metadataTemplatesList.contains(oldTemplateName))
            textTokenObjectEditor.at(i)->setValue(oldTemplateName);
    }
}

void EmbelProperties::newTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    templateName = Utilities::inputText("New Template",
                                        "Enter new template name",
                                        templateList);
    qDebug() << __FUNCTION__ << templateName;
    if (templateName == "") return;
    templateId = templateList.count();
//    // set all templates except templateName isCurrent = false
    templateList << templateName;
    templateListEditor->addItem(templateName);
    templateListEditor->setValue(templateName);
    syncWinnets();
}

void EmbelProperties::newTemplateFromImport(QString name)
{
    if (name == "") return;
    templateName = name;
    templateId = templateList.count();
    // set all templates except templateName isCurrent = false
    templateList << name;
    // sort so the new template item is in order
    sortTemplateList();
    // refresh dropdown with appended and sorted list
    templateListEditor->refresh(templateList);
    // select the imported template, which triggers itemChange
    qDebug() << __FUNCTION__ << "templateListEditor->setValue(templateName)" << name;
    templateListEditor->setValue(name);
    emit syncEmbellishMenu();
    syncWinnets();
}

void EmbelProperties::syncWinnets()
{
/*
    Winnets are small executables that act like photoshop droplets. They reside in the same
    folder as the Winnow executable. They send a list of files and a template name to Winnow
    to be embellished. For example, in order for Winnow to embellish a series of files that
    have been exported from lightroom, Winnow needs to know which embellish template to use.
    Instead of sending the files directly to Winnow, thay are sent to an intermediary program
    (a Winnet) that is named after the template. The Winnet (ie Zen2048) receives the list of
    files, inserts the strings Embellish" and "Zen2048" and then resends to Winnow.

    Argument options:

    arg[0] = system parameter - usually path to executing program (Winnow)

    if arg[1]  = "Embellish" then
       arg[2]  = templateName (also name on Winnet used to send to Winnow)
       arg[3+] = path to each image being exported to be embellished

    else
       arg[1+] = path to each image to view in Winnow.  Only arg[1] is used to determine the
       directory to open in Winnow.

    The main Winnet is called winnet.exe and it is copied and renamed to the template name
    when a new template is created.  The Winnets need to be synced when templates are created,
    renamed or deleted.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // list of executables that must not be changed
    QStringList criticalExecutables;
    criticalExecutables << "Winnow.exe" << "Winnet.exe";

    // list of all executables in the Winnow folder
    QString appDir = qApp->applicationDirPath();
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dirPath += "/Winnets";
    QDir dir(dirPath);
    if (!dir.exists()) dir.mkpath(dirPath);

    // make sure Qt5Core.dll exists
    QString dllPath = dirPath + "/Qt5Core.dll";
    if (!QFile(dllPath).exists()) {
        QString dllSourcePath = appDir + "/Qt5Core.dll";
        QFile dll(dllSourcePath);
        dll.copy(dllPath);
    }

    QStringList dirFilter("*.exe");
    dir.setNameFilters(dirFilter);
    dir.setFilter(QDir::Files);
    QStringList okToChange;
    okToChange << dir.entryList();

    // strip ".exe"
    okToChange.replaceInStrings(".exe", "");

    // add if missing item in templateList
    QString winnetPath = appDir + "/Winnet.exe";
    QFile winnet(winnetPath);
    for (int i = 0; i < templateList.length(); i++) {
        if (!okToChange.contains(templateList.at(i))) {
            QString newWinnet = dirPath + "/" + templateList.at(i) + ".exe";
            // add a new Winnet
            winnet.copy(newWinnet);
        }
    }

    // remove if existing Winnet is not in templateList
    for (int i = okToChange.length() - 1; i >= 0; i--) {
        if (!templateList.contains(okToChange.at(i))) {
            QString fPath = dirPath + "/" + okToChange.at(i) + ".exe";
            QFile::remove(fPath);
            okToChange.removeAt(i);
        }
    }
}

void EmbelProperties::newStyle()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // rgh prevent duplicate style names
    QString name = Utilities::inputText("New Style",
                                        "Enter new style name (not created until you add an effect)",
                                        styleList);
    if (name == "") return;
    styleName = name;
    styleList << name;
    int n = styleList.length() - 1;
    addStyle(name, n);
    QModelIndex idx = model->index(n, 0, stylesIdx);
    for (int i = 0; i < styleListObjectEditor.length(); ++i) {
        styleListObjectEditor[i]->addItem(name);
    }
    expand(stylesIdx);
    selectionModel()->clear();
//    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void EmbelProperties::invokeFromAction(QAction *embelAction)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (templateName == embelAction->text()) return;
    for (int i = 0; i < templateList.count(); ++i) {
        if (templateList.at(i)  == embelAction->text()) {
            templateListEditor->setValue(templateList.at(i));
        }
    }
}

void EmbelProperties::doNotEmbellish()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    templateListEditor->setValue("Do not Embellish");
}

void EmbelProperties::swapBorders(QStandardItem *borders, int row, int swapRow)
{
/*
When borders are swapped to change their order 3 separate data structures must be kept in sync:
model (properties tree), settings (persistent data between sessions) and border vector (a
vector of Border more convenient to use than model).

The model border items are swapped using QStandardItem::sortChildren, based on UR_SortOrder.

The border vector is rebuilt using syncBorderVector.

The sortOrder is updated in settings.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // swap the current row with the one above
    QStandardItem *item = borders->child(row);
    QStandardItem *swapItem = borders->child(swapRow);
    int swapOrder = swapItem->data(UR_SortOrder).toInt();
    int thisOrder = item->data(UR_SortOrder).toInt();

    // update model
    swapItem->setData(thisOrder, UR_SortOrder);
    item->setData(swapOrder, UR_SortOrder);

    // sort model
    borders->sortChildren(Qt::AscendingOrder);

    // sync border vector with model
    syncBorderVector();

    // update order in settings
    QString borderPath = "Embel/Templates/" + templateName + "/Borders/";
    for (int i = 0; i < borders->rowCount(); ++i) {
        QString borderName = borders->child(i)->data(Qt::DisplayRole).toString();
        QString key = borderPath + borderName + "/sortOrder";
        setting->setValue(key, i);
    }

    // refresh the graphicsScene
    e->build("", __FUNCTION__);
}

void EmbelProperties::moveBorderUp()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
//    QModelIndex idx = btn->index;
    QModelIndex idx = getItemIndex(btn->itemIndex);
    if (!idx.isValid()) return;

    // get current row for this index as it may have been sorted
    QString borderName = btn->name;
    QStandardItem *borders = new QStandardItem;
    borders = model->itemFromIndex(idx.parent());
    int row;
    for (row = 0; row < borders->rowCount(); ++row) {
        QString sortedItemName = borders->child(row)->data(Qt::DisplayRole).toString();
        // if found border then break at this row
        if (sortedItemName == borderName) break;
    }
    if (row == 0) return;

    // swap the current row with the one above
    int swapRow = row - 1;
    swapBorders(borders, row, swapRow);
}

void EmbelProperties::moveBorderDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
//    QModelIndex idx = btn->index;
    QModelIndex idx = getItemIndex(btn->itemIndex);
    if (!idx.isValid()) return;

    // get current row for this index as it may have been sorted already
    QString borderName = btn->name;
    QStandardItem *borders = new QStandardItem;
    borders = model->itemFromIndex(idx.parent());
    int row;
    for (row = 0; row < borders->rowCount(); ++row) {
        QString sortedItemName = borders->child(row)->data(Qt::DisplayRole).toString();
        // if found border then break at this row
        if (sortedItemName == borderName) break;
    }
    if (row == borders->rowCount() - 1) return;

    // swap the current row with the one above
    int swapRow = row + 1;
    swapBorders(borders, row, swapRow);
}

void EmbelProperties::swapEffects(QStandardItem *styleItem, int row, int swapRow)
{
/*
A style may contain multiple effects, and the effect order can be changed using the up and
down arrow command buttons (moveEffectUp and moveEffectDown). When the order is changed, the
settings, model (tree) and styleMap must be updated. QSettings keeps everything in
alphabetical order, so the last sort order must be re-established after the settings have been
read, based on the setting sortOrder. This is done in sortEffectList, which is called by
addStyle, using the model role UR_SortOrder. The sorting is accomplished using
QStandardItem::sortChildren. styleItem is the parent and the effects are the children being
sorted.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // swap the current row with the swap row
    QStandardItem *item = styleItem->child(row);
    QStandardItem *swapItem = styleItem->child(swapRow);

    QModelIndex par = styleItem->index();   // for debugging
//    /*
    qDebug() << __FUNCTION__
             << "item->data() =" << item->data()
             << "swapItem->data() =" << swapItem->data()
                ;
//                */

    for (int i = 0; i < model->rowCount(par); ++i) {
        QModelIndex idx = model->index(i, 0, par);
        QString name = idx.data().toString();
        qDebug() << __FUNCTION__ << "Before swap"
                 << "row =" << i
                 << "name =" << name.leftJustified(8)
                 << "order =" << idx.data(UR_SortOrder).toInt()
                    ;
    }

    int swapOrder = swapItem->data(UR_SortOrder).toInt();
    int thisOrder = item->data(UR_SortOrder).toInt();
    swapItem->setData(thisOrder, UR_SortOrder);
    item->setData(swapOrder, UR_SortOrder);
//    /*
    for (int i = 0; i < model->rowCount(par); ++i) {
        QModelIndex idx = model->index(i, 0, par);
        QString name = idx.data().toString();
        qDebug() << __FUNCTION__ << "After  swap"
                 << "row =" << i
                 << "name =" << name.leftJustified(8)
                 << "order =" << idx.data(UR_SortOrder).toInt()
                    ;
    }
//                */
    styleItem->sortChildren(Qt::AscendingOrder);

    // update order in settings
    QString style = styleItem->data(Qt::DisplayRole).toString();
    QString stylePath = "Embel/Templates/" + templateName + "/Styles/" + style + "/";
    for (int i = 0; i < styleItem->rowCount(); ++i) {
        QString effectName = styleItem->child(i)->data(Qt::DisplayRole).toString();
        QString key = stylePath + effectName + "/sortOrder";
        setting->setValue(key, i);
        for (int a = 0; a < styleMap[style].length(); ++a) {
            if (styleMap[style][a].effectName == effectName) styleMap[style][a].effectOrder = i;
        }
    }

    // update order in styleMap
    sortEffectList(style);

    // refresh the graphicsScene
    e->build("", __FUNCTION__);

    return;
}

void EmbelProperties::moveEffectUp()
{
/*
    Move an effect up one item in a group of effects within a style in the property tree.
    The effects may have been sorted
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    /*
    QWindow *wBtn = new QWindow;
    btn->createWindowContainer(wBtn);
    if (wBtn == nullptr) qDebug() << "NULL";
    qDebug() << __FUNCTION__;
    qDebug() << "btn->window()->pos()            =" << btn->window()->pos();
    qDebug() << "btn->pos()                      =" << btn->pos();
    qDebug() << "wBtn->position();               =" << wBtn->position();
    qDebug() << "window()->pos()                 =" << window()->pos();
    qDebug() << "window()->geometry()            =" << window()->geometry();
    qDebug() << "geometry()                      =" << geometry();
    qDebug() << "pos()                           =" << pos();
    qDebug() << "mw3->embelDock->pos()           =" << mw3->embelDock->pos();
    qDebug() << "mw3->embelDock->window()->pos() =" << mw3->embelDock->window()->pos();
    return;
//    */
    QModelIndex idx = getItemIndex(btn->itemIndex);
//    QModelIndex idx = btn->index;
    if (!idx.isValid()) {
        qDebug() << __FUNCTION__ << "idx.isValid() =" << idx.isValid();
        return;
    }

    // get current row for this index as it may have been sorted already
    QString effectName = btn->name;
    /*
    qDebug() << __FUNCTION__
             << "effectName =" << effectName
             << "idx.data() =" << idx.data()
             << "idx.parent().data() =" << idx.parent().data()
                ;
//                */
    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    QStandardItem *styleItem = new QStandardItem;
    styleItem = model->itemFromIndex(idx.parent());
    int row;
    int rowCount = styleItem->rowCount();
    qDebug() << __FUNCTION__ << rowCount;
    for (row = 0; row < styleItem->rowCount(); ++row) {
        QString sortedItemName = styleItem->child(row)->data(Qt::DisplayRole).toString();
        if (sortedItemName == effectName) break;
    }
    qDebug() << __FUNCTION__ << "row =" << row;
    if (row == 0) return;

    // swap the current row with the one above
    int swapRow = row - 1;
    swapEffects(styleItem, row, swapRow);

    return;
}

void EmbelProperties::moveEffectDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    QModelIndex idx = getItemIndex(btn->itemIndex);
//    QModelIndex idx = btn->index;
    if (!idx.isValid()) return;

    // get current row for this index as it may have been sorted
    QString effectName = btn->name;
    QStandardItem *styleItem = new QStandardItem;
    styleItem = model->itemFromIndex(idx.parent());

    int modelRowCount = model->rowCount(idx.parent());
    int standardItemRowCount = styleItem->rowCount();

    int row;
    for (row = 0; row < styleItem->rowCount(); ++row) {
        QString sortedItemName = styleItem->child(row)->data(Qt::DisplayRole).toString();
//        /*
        qDebug() << __FUNCTION__
                 << "row =" << row
                 << "effectName =" << effectName
                 << "sortedItemName =" << sortedItemName
                 << "effectName =" << effectName;
//                 */
        if (sortedItemName == effectName) break;
    }
    if (row == styleItem->rowCount() - 1) return;

    // swap the current row with the one above
    int swapRow = row + 1;
    swapEffects(styleItem, row, swapRow);

    return;
}

void EmbelProperties::syncBorderVector()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < model->rowCount(bordersIdx); ++i) {
        QModelIndex bIdx = model->index(i, 0, bordersIdx);
        b[i].name = bIdx.data(UR_Name).toString();
        b[i].top = getItemValue("topMargin", bIdx).toDouble();
        b[i].left = getItemValue("leftMargin", bIdx).toDouble();
        b[i].right = getItemValue("rightMargin", bIdx).toDouble();
        b[i].bottom = getItemValue("bottomMargin", bIdx).toDouble();
        QString tile = getItemValue("tile", bIdx).toString();
        if (tile == "Do not tile") b[i].tile.clear();
        else b[i].tile = setting->value("Embel/Tiles/" + tile).toByteArray();
        b[i].color = getItemValue("color", bIdx).toString();
        b[i].opacity = getItemValue("opacity", bIdx).toDouble();
        b[i].style = getItemValue("style", bIdx).toString();
    }

//    std::sort(b.begin(), b.end(),
//              [](Border const &l, Border const &r) {
//              return l.order < r.order; });
}

bool EmbelProperties::sortEffectList(QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (!styleMap.contains(style)) {
        qDebug() << __FUNCTION__ << style << "not in list";
        return false;
    }
    std::sort(styleMap[style].begin(), styleMap[style].end(),
              [](winnow_effects::Effect const &l, winnow_effects::Effect const &r) {
              return l.effectOrder < r.effectOrder; });
    return true;
}

void EmbelProperties::itemChange(QModelIndex idx)
{
/*
When the user changes a value in the editor (the control in the value column of the tree)
this slot is triggered to update the associated value in the application.  The model index
of the value is used to recall:

    v      - the value of the property
    source - the name of the value, which is used to figure out which associated app value
             to update with v.
    index  - is used where the associated value is in another model

The connection is defined in the parent PropertyEditor to fire the virtual function
itemChange, which is subclassed here.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }

//    if (!propertyDelegate->submitted) return;
//    propertyDelegate->submitted = false;
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString parent = idx.parent().data(UR_Name).toString();
    QString grandparent = idx.parent().parent().data(UR_Name).toString();
    QModelIndex grandparentIdx = idx.parent().parent();

    templatePath = "Embel/Templates/" + templateName + "/";
    /*
//    QModelIndex index = idx.data(UR_QModelIndex).toModelIndex();
    qDebug() << __FUNCTION__
             << " =" << templateId
             << "idx.isValid() =" << idx.isValid()
             << "v =" << v
             << "source =" << source
             << "parent =" << parent
             << "grandparent =" << grandparent;
//           */

    if (source == "templateList") itemChangeTemplate(v);
    if (templateId != 0) {
        if (parent == "ExportHeader") itemChangeExport(idx, v, source);
        if (parent == "GeneralHeader") itemChangeGeneral(v, source);
        if (grandparentIdx == bordersIdx) itemChangeBorder(idx);
        if (grandparentIdx == textsIdx) itemChangeText(idx);
        if (grandparentIdx == graphicsIdx) itemChangeGraphic(idx, v, source, parent);
        if (parent.left(6) == "Shadow") itemChangeShadowEffect(v, source, parent, grandparent);
        if (parent.left(4) == "Blur") itemChangeBlurEffect(v, source, parent, grandparent);
        if (parent.left(7) == "Sharpen") itemChangeSharpenEffect(v, source, parent, grandparent);
        if (parent.left(11) == "Highlighter") itemChangeHighlighterEffect(v, source, parent, grandparent);
        if (parent.left(10) == "Brightness") itemChangeBrightnessEffect(v, source, parent, grandparent);
        if (parent.left(6) == "Emboss") itemChangeEmbossEffect(idx, v, source, parent, grandparent);
        if (parent.left(6) == "Stroke") itemChangeStrokeEffect(v, source, parent, grandparent);
        if (parent.left(6) == "Glow") itemChangeGlowEffect(v, source, parent, grandparent);
    }

//    e->build();
}

void EmbelProperties::itemChangeTemplate(QVariant v)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
//    qDebug() << __FUNCTION__ << v;
    isTemplateChange = true;

    // save which template is current and set templateId and templateName
    setCurrentTemplate(v.toString());

    // update global for ImageView
    if (templateId == 0) G::isEmbellish = false;
    else G::isEmbellish = true;

    // update Embellish menu
    emit templateChanged(templateId);

    // clear model except for template name header (row 0)
    model->removeRows(1, model->rowCount(QModelIndex()) - 1, QModelIndex());

    // clear styles
    styleMap.clear();
    styleList.clear();

    // clear comboBoxEditors
    borderTileObjectEditor.clear();
    graphicsObjectEditor.clear();
    graphicAnchorObjectEditor.clear();
    textAnchorObjectEditor.clear();
    textTokenObjectEditor.clear();
    styleListObjectEditor.clear();
    styleEditor.clear();
//    anchorContainerList.clear();

    // other lists
    borderList.clear();
    anchorObjectList.clear();
    anchorObjectList << "Image";

    // clear borders, texts and graphics
    b.clear();
    t.clear();
    g.clear();

    // clear all graphics items
    if (templateId == 0) e->clear();

    // add items for this template
    if (templateId > 0) addTemplateItems();
    collapseAll();
    expand(model->index(_templates, 0));
    isTemplateChange = false;
//    qDebug() << __FUNCTION__ << "loadImage..."
//             << "mw3->dm->currentFilePath =" << mw3->dm->currentFilePath;
    mw3->imageView->loadImage(mw3->dm->currentFilePath, __FUNCTION__);
}

void EmbelProperties::itemChangeExport(QModelIndex idx, QVariant v, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // prevent template "Export" is being mysteriously created
    if (source == "") return;

    QString path = templatePath + "Export/" + source;

    if (source == "fileType") {
        setting->setValue(path, v.toString());
        exportFileType = v.toString();
    }

    if (source == "fileQuality") {
        setting->setValue(path, v.toInt());
        exportFileQuality = v.toInt();
    }

    if (source == "saveMethod") {
        setting->setValue(path, v.toString());
        saveMethod = v.toString();
        qDebug() << __FUNCTION__ << source << saveMethod;
        int row = idx.row();
        // also hidden on creation using updateHiddenRows(QModelIndex parent)
        if (saveMethod == "Another folder somewhere else") {
            setRowHidden(row + 1, idx.parent(), false);
            setRowHidden(row + 2, idx.parent(), true);
        }
        if (saveMethod == "Subfolder") {
            setRowHidden(row + 1, idx.parent(), true);
            setRowHidden(row + 2, idx.parent(), false);
        }
    }

    if (source == "folderPath") {
        setting->setValue(path, v.toString());
        exportFolderPath = v.toString();
    }

    if (source == "subfolder") {
        setting->setValue(path, v.toString());
        exportSubfolder = v.toString();
    }

    if (source == "copyMetadata") {
        setting->setValue(path, v.toBool());
        copyMetadata = v.toBool();
    }

    if (source == "overwriteFiles") {
        setting->setValue(path, v.toBool());
        overwriteFiles = v.toBool();
    }
}

void EmbelProperties::itemChangeGeneral(QVariant v, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = templatePath + "General/" + source;
//    qDebug() << __FUNCTION__ << path;

    if (source == "horizontalFit") {
        setting->setValue(path, v.toInt());
        horizontalFitPx = v.toInt();
        horizontalFitPx > verticalFitPx ? longSidePx = horizontalFitPx : longSidePx = verticalFitPx;
    }

    if (source == "verticalFit") {
        setting->setValue(path, v.toInt());
        verticalFitPx = v.toInt();
        horizontalFitPx > verticalFitPx ? longSidePx = horizontalFitPx : longSidePx = verticalFitPx;
    }

    if (source == "lightDirection") {
        setting->setValue(path, v.toString());
        lightDirection = v.toInt();
    }

    if (source == "imageStyle") {
        setting->setValue(path, v.toString());
        image.style = v.toString();
    }
    e->build("", __FUNCTION__);
}

void EmbelProperties::itemChangeBorder(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
//    qDebug() << __FUNCTION__;
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString parent = idx.parent().data(UR_Name).toString();
    int index = idx.parent().data(UR_SortOrder).toInt();
//    int index = idx.parent().data(UR_ItemIndex).toInt();
    QString path = templatePath + "Borders/" + parent + "/" + source;
//    diagnostics(idx);

    if (source == "topMargin") {
        double x = v.toDouble();
        setting->setValue(path, x);
        b[index].top = x;
        // build embel as change in border dimension changes all coordinates
        e->build("", __FUNCTION__);
        return;
    }

    if (source == "leftMargin") {
        double x = v.toDouble();
        setting->setValue(path, x);
        b[index].left = x;
        // build embel as change in border dimension changes all coordinates
        e->build("", __FUNCTION__);
        return;
    }

    if (source == "rightMargin") {
        double x = v.toDouble();
        setting->setValue(path, x);
        b[index].right = x;
        // build embel as change in border dimension changes all coordinates
        e->build("", __FUNCTION__);
        return;
    }

    if (source == "bottomMargin") {
        double x = v.toDouble();
        setting->setValue(path, x);
        b[index].bottom = x;
        // build embel as change in border dimension changes all coordinates
        e->build("", __FUNCTION__);
        return;
    }

    if (source == "tile") {
        setting->setValue(path, v.toString());
        if (v.toString() == "Do not tile")  b[index].tile.clear();
        else b[index].tile = setting->value("Embel/Tiles/" + v.toString()).toByteArray();
    }

    if (source == "color") {
        setting->setValue(path, v.toString());
        b[index].color = v.toString();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toInt());
        qDebug() << __FUNCTION__ << index;
        b[index].opacity = v.toInt();
    }

    if (source == "style") {
        setting->setValue(path, v.toString());
        b[index].style = v.toString();
    }

    e->updateBorder(index);
}

void EmbelProperties::itemChangeText(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString parent = idx.parent().data(UR_Name).toString();
    // see PropertyEditor::getItemIndex for details on using itemIndex
    int index = getItemIndex(idx.parent().data(UR_ItemIndex).toInt()).row();
    QString path = templatePath + "Texts/" + parent + "/" + source;
    /*
    qDebug() << __FUNCTION__
             << "source =" << source
             << "parent =" << parent;
//    */

    if (source == "anchorObject") {
        // called during refresh operation
        QString anchorObject = v.toString();
        if (anchorObject == "") return;
        setting->setValue(path, anchorObject);
        t[index].anchorObject = anchorObject;

        // show/hide the container row
        int row = idx.row();
        // also hidden on creation using updateHiddenRows(QModelIndex parent)
        if (v.toString() == "Image") {
            setRowHidden(row + 1, idx.parent(), true);  // anchor container
        }
        else {
            setRowHidden(row + 1, idx.parent(), false);
        }
    }

    if (source == "anchorContainer") {
        setting->setValue(path, v.toString());
        t[index].anchorContainer = v.toString();
    }

    if (source == "x") {
        setting->setValue(path, v.toDouble());
        t[index].x = v.toDouble();
    }

    if (source == "y") {
        setting->setValue(path, v.toDouble());
        t[index].y = v.toDouble();
    }

    if (source == "rotation") {
        setting->setValue(path, v.toDouble());
        t[index].rotation = v.toDouble();
    }

    if (source == "anchorPoint") {
        setting->setValue(path, v.toString());
        t[index].anchorPoint = v.toString();
    }

    if (source == "align") {
        setting->setValue(path, v.toBool());
        t[index].align = v.toBool();
    }

    if (source == "source") {
        QString s = v.toString();
        setting->setValue(path, s);
        t[index].source = s;
        int row = idx.row();
        // also hidden on creation using updateHiddenRows(QModelIndex parent)
        if (s == "Text") {
            setRowHidden(row + 1, idx.parent(), false);
            setRowHidden(row + 2, idx.parent(), true);
        }
        if (s == "Metadata template") {
            setRowHidden(row + 1, idx.parent(), true);
            setRowHidden(row + 2, idx.parent(), false);
        }
    }

    if (source == "text") {
        setting->setValue(path, v.toString());
        t[index].text = v.toString();
    }

    if (source == "metadataTemplate") {
        QString value = v.toString();
        qDebug() << __FUNCTION__ << "metadataTemplate" << value;
        setting->setValue(path, value);
        t[index].metadataTemplate = value;
    }

    if (source == "font") {
        setting->setValue(path, v.toString());
        t[index].font = v.toString();
    }

    if (source == "size") {
        setting->setValue(path, v);
        double x = v.toDouble();
        t[index].size = x;
    }

    if (source == "bold") {
        setting->setValue(path, v.toBool());
        t[index].isBold = v.toBool();
    }

    if (source == "italic") {
        setting->setValue(path, v.toBool());
        t[index].isItalic = v.toBool();
    }

    if (source == "color") {
        setting->setValue(path, v.toString());
        t[index].color = v.toString();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toInt());
        t[index].opacity = v.toInt();
    }

    if (source == "style") {
        qDebug() << __FUNCTION__ << source;
        setting->setValue(path, v.toString());
        t[index].style = v.toString();
    }

    e->updateText(index);
}

void EmbelProperties::itemChangeRectangle(QVariant v, QString source, QString parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }

}

void EmbelProperties::itemChangeGraphic(QModelIndex idx, QVariant v, QString source, QString parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // see PropertyEditor::getItemIndex for details on using itemIndex
    int index = getItemIndex(idx.parent().data(UR_ItemIndex).toInt()).row();
    QString path = templatePath + "Graphics/" + parent + "/" + source;
//    qDebug() << __FUNCTION__ << "row =" << index << path << source << v.toInt();

    if (source == "graphic") {
        setting->setValue(path, v.toString());
        g[index].name = v.toString();
        g[index].graphic = setting->value("Embel/Graphics/" + v.toString()).toByteArray();
        e->build("", __FUNCTION__);
        return;
    }

    if (source == "anchorObject") {
        setting->setValue(path, v.toString());
        g[index].anchorObject = v.toString();
        int row = idx.row();
        // also hidden on creation using updateHiddenRows(QModelIndex parent)
        if (v.toString() == "Image") {
            setRowHidden(row + 1, idx.parent(), true);  // anchor container
            g[index].anchorContainer = "";
        }
        else {
            setRowHidden(row + 1, idx.parent(), false);
        }
    }

    if (source == "anchorContainer") {
        setting->setValue(path, v.toString());
        g[index].anchorContainer = v.toString();
    }

    if (source == "x") {
        setting->setValue(path, v.toDouble());
        g[index].x = v.toDouble();
    }

    if (source == "y") {
        setting->setValue(path, v.toDouble());
        g[index].y = v.toDouble();
    }

    if (source == "anchorPoint") {
        setting->setValue(path, v.toString());
        g[index].anchorPoint = v.toString();
    }

    if (source == "align") {
        setting->setValue(path, v.toBool());
        g[index].align = v.toBool();
    }

    if (source == "size") {
        setting->setValue(path, v.toDouble());
        g[index].size = v.toDouble();
        qDebug() << __FUNCTION__ << "g[index].size =" << g[index].size;
    }

    if (source == "rotation") {
        setting->setValue(path, v.toDouble());
        g[index].rotation = v.toDouble();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toInt());
        g[index].opacity = v.toInt();
    }

    if (source == "style") {
        qDebug() << __FUNCTION__ << source;
        setting->setValue(path, v.toString());
        g[index].style = v.toString();
    }

    e->updateGraphic(index);
}

void EmbelProperties::itemChangeShadowEffect(QVariant v, QString source, QString effectName, QString style)
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "length") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].shadow.length = v.toDouble();
    }

    if (source == "blur") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].shadow.blurRadius = v.toInt();
    }

    if (source == "color") {
        setting->setValue(path, v.toString());
        QColor color(v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].shadow.r = color.red();
        styleMap[style][effect].shadow.g = color.green();
        styleMap[style][effect].shadow.b = color.blue();
        styleMap[style][effect].shadow.a = color.alpha();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].shadow.opacity = v.toDouble() * 1.0 / 100;
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].shadow.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeStrokeEffect(QVariant v, QString source, QString effectName, QString style)
{
    /*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
    //    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "width") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].stroke.width = v.toDouble();
    }

    if (source == "color") {
        setting->setValue(path, v.toString());
        QColor color(v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].stroke.r = color.red();
        styleMap[style][effect].stroke.g = color.green();
        styleMap[style][effect].stroke.b = color.blue();
        styleMap[style][effect].stroke.a = color.alpha();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].stroke.opacity = v.toDouble() * 1.0 / 100;
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].stroke.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeGlowEffect(QVariant v, QString source, QString effectName, QString style)
{
    /*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
    //    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "width") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].glow.width = v.toDouble();
    }

    if (source == "blurRadius") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].glow.blurRadius = v.toDouble();
    }

    if (source == "color") {
        setting->setValue(path, v.toString());
        QColor color(v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].glow.r = color.red();
        styleMap[style][effect].glow.g = color.green();
        styleMap[style][effect].glow.b = color.blue();
        styleMap[style][effect].glow.a = color.alpha();
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].glow.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeBlurEffect(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "radius") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].blur.radius = v.toDouble();
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].blur.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeSharpenEffect(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "radius") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].sharpen.radius = v.toDouble();
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].sharpen.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeHighlighterEffect(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "top") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlighter.top = v.toInt();
    }

    if (source == "left") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlighter.left = v.toInt();
    }

    if (source == "right") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlighter.right = v.toInt();
    }

    if (source == "bottom") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlighter.bottom = v.toInt();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlighter.a = v.toInt() * 255 / 100;
    }

    if (source == "color") {
        setting->setValue(path, v.toString());
        QColor color(v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlighter.r = color.red();
        styleMap[style][effect].highlighter.g = color.green();
        styleMap[style][effect].highlighter.b = color.blue();
        styleMap[style][effect].highlighter.a = color.alpha();
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlighter.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeBrightnessEffect(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "evDelta") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].brightness.evDelta = v.toDouble();
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].brightness.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeEmbossEffect(QModelIndex idx, QVariant v, QString source,
                                             QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();
    // item 0
//    if (source == "contour") {
//        int index = embossContourList.indexOf(v.toString());
//        setting->setValue(path, v.toString());
//        int effect = effectIndex(style, effectName);
//        if (effect == -1) return;
//        styleMap[style][effect].emboss.contour = index;
//    }

    if (source == "size") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.size = v.toDouble();
    }

    if (source == "inflection") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.inflection = v.toDouble();
    }

    if (source == "exposure") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.exposure = v.toDouble();
    }

    if (source == "contrast") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.contrast = v.toDouble();
    }

    if (source == "start") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.start = v.toDouble();
    }

    if (source == "mid") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.mid = v.toDouble();
    }

    if (source == "end") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.end = v.toDouble();
    }

    if (source == "umbra") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.umbra = v.toDouble();
    }

    if (source == "isUmbraGradient") {
        setting->setValue(path, v.toBool());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.isUmbraGradient = v.toBool();
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::addTemplateHeader()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // Templates header (Root)
    i.name = "TemplatesHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.okToCollapseRoot = false;
    i.isDecoration = false;
    i.captionText = "Templates";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;

    BarBtn *templateDeleteBtn = new BarBtn();
    templateDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    templateDeleteBtn->setToolTip("Delete the selected template");
    btns.append(templateDeleteBtn);
    connect(templateDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteTemplate);
    BarBtn *templateNewBtn = new BarBtn();
    templateNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    templateNewBtn->setToolTip("Create a new template");
    connect(templateNewBtn, &BarBtn::clicked, this, &EmbelProperties::newTemplate);
    btns.append(templateNewBtn);
    addItem(i);
    templateIdx = model->index(0, 0, root);
    QModelIndex parIdx = capIdx;

    // Templates
    i.name = "templateList";
    i.parIdx = parIdx;
    i.parentName = "TemplatesHeader";
    i.decorateGradient = false;
    i.captionText = "Select template";
    i.tooltip = "Templates contain all the properties to embellish your images.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = templateName;
    i.key = "templateList";
    i.delegateType = DT_Combo;      // no parent, delegateType > 0 -> No header, has value
    i.type = "QString";
    i.color = "#1b8a83";
    i.dropList = templateList;
    templateListEditor = static_cast<ComboBoxEditor*>(addItem(i));

    if (templateId == 0) return;

    // select the active template
    QModelIndex idx = sourceIdx["templateList"];
    model->setData(idx, templateName);
    propertyDelegate->setEditorData(templateListEditor, idx);

    if (templateList.size() == 1) {
        QString msg = "You must create a template as a first step before adding\n"
                      "borders, text, rectangles or graphics.";
        G::popUp->showPopup(msg, 2000);
    }
}

void EmbelProperties::addTemplateItems()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    addExport();
    addStyles();
    addGeneral();
    addBorders();
    addTexts();
    addGraphics();
}

void EmbelProperties::addExport()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // EXPORT File header (Root)
    i.name = "ExportHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "Export";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
//    i.value = templatePath + "Export";
    i.delegateType = DT_None;
    addItem(i);
    QModelIndex parIdx = capIdx;

    QString settingRootPath = templatePath + "Export/";

    // EXPORT File type
    i.name = "fileType";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "File type";
    i.tooltip = "Select file type.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "fileType";
    i.path = settingRootPath + i.key;
    i.defaultValue = "JPG";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else i.value = i.defaultValue;
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "TIF" << "JPG" << "PNG";
    exportFileType = i.value.toString();
    addItem(i);

    // EXPORT File quality
    i.name = "fileQuality";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "File quality";
    i.tooltip = "Set quality for the exported image file.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "fileQuality";
    i.path = settingRootPath + i.key;
    i.defaultValue = 100;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else i.value = i.defaultValue;
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    exportFileQuality = i.value.toInt();
    addItem(i);

    // EXPORT Save method
    i.name = "saveMethod";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Save method";
    i.tooltip = "Select where to save the export file.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "saveMethod";
    i.path = settingRootPath + i.key;
    i.defaultValue= "Subfolder";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Subfolder" << "Another folder somewhere else";
    saveMethod = i.value.toString();
    addItem(i);

    // EXPORT Folder name
    i.name = "folderPath";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Export folder path";
    i.tooltip = "Enter the full export folder path.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "folderPath";
    i.path = settingRootPath + i.key;
    i.defaultValue = "";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = i.defaultValue;
    }
    i.delegateType = DT_SelectFolder;
    i.type = "string";
    exportFolderPath = i.value.toString();
    addItem(i);
    // hide if not selected by saveMethod
    if (saveMethod == "Subfolder") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
        setRowHidden(capIdx.row(), capIdx.parent(), true);
    }

    // EXPORT Subfolder name
    i.name = "subfolder";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Subfolder name";
    i.tooltip = "Enter the name of the subfolder to contain the exported files.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "subfolder";
    i.path = settingRootPath + i.key;
    i.defaultValue = "";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = i.defaultValue;
    }
    i.delegateType = DT_LineEdit;
    i.type = "string";
    exportSubfolder = i.value.toString();
    addItem(i);
    // hide if not selected by saveMethod
    if (saveMethod == "Another folder somewhere else") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
        setRowHidden(capIdx.row(), capIdx.parent(), true);
    }

    // Append text to filename
    i.name = "suffix";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Suffix";
    i.tooltip = "Enter text to be appended to the file name.  For example\n"
                "with a file name = img.jpg and a suffix '_flickr' the exported\n"
                "file name = img_flickr.jpg\n\n"
                "The default value is '_' + the template name.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "suffix";
    i.path = settingRootPath + i.key;
    i.defaultValue = "_" + templateName;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = i.defaultValue;
    }
    i.delegateType = DT_LineEdit;
    i.type = "QString";
    exportSuffix = i.value.toString();
    addItem(i);

    // EXPORT Copy metadata
    i.name = "copyMetadata";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Copy metadata";
    i.tooltip = "Copy metadata from source to export file.\n"
                "Note that this significantly slows down \n"
                "the export process.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "copyMetadata";
    i.path = settingRootPath + i.key;
    i.defaultValue = false;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = i.defaultValue;
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    copyMetadata = i.value.toBool();
    addItem(i);

    // EXPORT Overwrite existing files
    i.name = "overwriteFiles";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Overwrite";
    i.tooltip = "Overwrite existing files.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "overwriteFiles";
    i.path = settingRootPath + i.key;
    i.defaultValue = false;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = i.defaultValue;
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    overwriteFiles = i.value.toBool();
    addItem(i);
}

void EmbelProperties::addGeneral()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // IMAGE Image header (Root)
    i.name = "GeneralHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "General";
    i.tooltip = "Dimensions, light and image style";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.path = templatePath + "General";
    i.delegateType = DT_None;
    addItem(i);
    QModelIndex parIdx = capIdx;

    QString settingRootPath = templatePath + "General/";

    // FILES Horizontal fit
    i.name = "horizontalFit";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Fit horizontal";
    i.tooltip = "The number of pixels in the horizontal axis including the borders";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "horizontalFit";
    i.defaultValue = 1000;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 10000;
    i.fixedWidth = 50;
    horizontalFitPx = i.value.toInt();
    addItem(i);

    // FILES Vertical fit
    i.name = "verticalFit";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Fit vertical";
    i.tooltip = "The number of pixels in the vertical axis including the borders";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "verticalFit";
    i.defaultValue = 1000;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 10000;
    i.fixedWidth = 50;
    verticalFitPx = i.value.toInt();
    addItem(i);

    horizontalFitPx > verticalFitPx ? longSidePx = horizontalFitPx : longSidePx = verticalFitPx;

    // Global light direction
    i.name = "lightDirection";
    i.parIdx = parIdx;
    i.parentName = "General";
    i.captionText = "Light direction";
    i.tooltip = "Light source direction (0 - 360 degrees from north = 0)";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "lightDirection";
    i.defaultValue = 315;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 360;
    i.fixedWidth = 50;
    lightDirection = i.value.toInt();
    addItem(i);

    // IMAGE style
    i.name = "imageStyle";
    i.parIdx = parIdx;
    i.parentName = "GeneralHeader";
    i.captionText = "Image style";
    i.tooltip = "Select style to apply to the image ie a shadow.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "imageStyle";
    i.defaultValue = "No style";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "No style" << styleList;
    image.style = i.value.toString();
    styleListObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));
}

void EmbelProperties::addBorders()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // BORDER header (Borders)
    i.name = "Borders";
    i.parentName = "???";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Borders";
    i.tooltip = "";
    //i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *borderNewBtn = new BarBtn();
    borderNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    borderNewBtn->setToolTip("Create a new border");
    btns.append(borderNewBtn);
    connect(borderNewBtn, &BarBtn::clicked, this, &EmbelProperties::newBorderFromBtn);
    addItem(i);
    bordersIdx = capIdx;

    // QStandardItem reqd to sort borders after they have been read from settings
    QStandardItem *borders = new QStandardItem;
    borders = model->itemFromIndex(bordersIdx);

    QString path = templatePath + "/Borders";
    setting->beginGroup(path);
    QStringList bordersList = setting->childGroups();
    setting->endGroup();
    for (int i = 0; i < bordersList.count(); ++i) {
        newBorder(bordersList.at(i));
    }

    // sort the borders
    borders->sortChildren(Qt::AscendingOrder);
    syncBorderVector();
    /*
    for (int row = 0; row < model->rowCount(bordersIdx); ++row) {
        QModelIndex bIdx = model->index(row, 0, bordersIdx);
        qDebug() << __FUNCTION__ << "DATA MODEL AND B VECTOR"
                 << "row =" << row
                 << "name =" << bIdx.data(UR_Name).toString()
                 << "order =" << bIdx.data(UR_SortOrder).toInt()
                 << "\t b[row].name = " << b[row].name
                    ;
    }
    qDebug() << __FUNCTION__ << "BORDER VECTOR";
    for (int i = 0; i < b.length(); ++i) {
        qDebug().noquote()
            << i << " "
            << "name =" << b[i].name
            << "top =" << b[i].top
            << "left =" << b[i].left
            << "right =" << b[i].right
            << "bottom =" << b[i].bottom
            << "tile =" << b[i].tile
            << "color =" << b[i].color
            << "opacity =" << b[i].opacity
            << "style =" << b[i].style;
    }
//    */
}

void EmbelProperties::addTexts()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // Texts header (Root)
    i.name = "Texts";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "Texts";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *textNewBtn = new BarBtn();
    textNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    textNewBtn->setToolTip("Create a new text item");
    btns.append(textNewBtn);
    connect(textNewBtn, &BarBtn::clicked, this, &EmbelProperties::newText);
    addItem(i);
    textsIdx = capIdx;

    QString path = templatePath + "/Texts";
    setting->beginGroup(path);
    int count = setting->childGroups().size();
    setting->endGroup();
    for (int i = 0; i < count; ++i) newText();
}

void EmbelProperties::addGraphics()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // Graphics header (Root)
    i.name = "Graphics";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "Graphics";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *graphicNewBtn = new BarBtn();
    graphicNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    graphicNewBtn->setToolTip("Create a new graphic item");
    btns.append(graphicNewBtn);
    connect(graphicNewBtn, &BarBtn::clicked, this, &EmbelProperties::newGraphic);
    addItem(i);
    graphicsIdx = capIdx;

    QString path = templatePath + "/Graphics";
    setting->beginGroup(path);
    int count = setting->childGroups().size();
    setting->endGroup();
    for (int i = 0; i < count; ++i) newGraphic();
}

void EmbelProperties::addStyles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    //    qDebug() << __FUNCTION__;
    // Styles header (Root)
    i.name = "Styles";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "Styles";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *styleNewBtn = new BarBtn();
    styleNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    styleNewBtn->setToolTip("Create a new style");
    connect(styleNewBtn, &BarBtn::clicked, this, &EmbelProperties::newStyle);
    btns.append(styleNewBtn);
    addItem(i);
    stylesIdx = capIdx;

    QString path = "Embel/Templates/" + templateName + "/Styles/";
    setting->beginGroup(path);
    styleList = setting->childGroups();
    int count = setting->childGroups().size();
    setting->endGroup();
//    qDebug() << __FUNCTION__ << stylesList;
    styleMap.clear();
    for (int i = 0; i < count; ++i) addStyle(styleList.at(i), i);
}

void EmbelProperties::addStyle(QString name, int n)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, name);
    #endif
    }
    // add the style header
    styleName = name;
    styleId = n;
//    qDebug() << __FUNCTION__ << styleName << name << n;
    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + styleName;
    i.isHeader = true;
    i.isDecoration = true;
    i.itemIndex = n;
    i.name = styleName;
    i.parIdx = stylesIdx;
    i.parentName = "Styles";
    i.path = "Embel/Templates/" + templateName + "/Styles/" + styleName;
    i.captionText = styleName;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    styleDeleteBtn = new BarBtn();
    styleDeleteBtn->setObjectName(styleName);
    styleDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    styleDeleteBtn->setToolTip("Delete this style");
    btns.append(styleDeleteBtn);
    connect(styleDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    BarBtn *effectNewBtn = new BarBtn();
    effectNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    effectNewBtn->setToolTip("Add an effect to this style");
    effectNewBtn->setObjectName(styleName);
    btns.append(effectNewBtn);
    connect(effectNewBtn, &BarBtn::clicked, this, &EmbelProperties::effectContextMenu);
//    styleEditor.append(static_cast<BarBtnEditor*>(addItem(i)));
    addItem(i);
    QModelIndex styleIdx = capIdx;
//    styleDeleteBtn->index = capIdx;
//    effectNewBtn->index = capIdx;
    styleDeleteBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();
    effectNewBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();

    // item reqd to sort effects after they have been read from settings
    QStandardItem *styleItem = new QStandardItem;
    styleItem = model->itemFromIndex(capIdx);

    // read the effects from settings
    setting->beginGroup(settingRootPath);
    QStringList items = setting->childGroups();
    setting->endGroup();
    effects.clear();
    for (int i = 0; i < items.length(); ++i) {
        QString effectName = items.at(i);
        if (items.at(i).contains("Blur")) addBlurEffect(styleIdx, effectName);
        if (items.at(i).contains("Sharpen")) addSharpenEffect(styleIdx, effectName);
        if (items.at(i).contains("Highlighter")) addHighlighterEffect(styleIdx, effectName);
        if (items.at(i).contains("Shadow")) addShadowEffect(styleIdx, effectName);
        if (items.at(i).contains("Brightness")) addBrightnessEffect(styleIdx, effectName);
        if (items.at(i).contains("Emboss")) addEmbossEffect(styleIdx, effectName);
        if (items.at(i).contains("Stroke")) addStrokeEffect(styleIdx, effectName);
        if (items.at(i).contains("Glow")) addGlowEffect(styleIdx, effectName);
    }

    // sort the effects
    styleItem->sortChildren(Qt::AscendingOrder);

    // add to local map used in embel
//    styleMap[styleName] = effects;
    sortEffectList(styleName);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addBorderHeaderButtons()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    effectUpBtn = new BarBtn();
    effectUpBtn->setIcon(":/images/icon16/up.png", G::iconOpacity);
    effectUpBtn->setToolTip("Move border up");
    effectUpBtn->name = i.name;
    effectUpBtn->parName = i.parentName;
    effectUpBtn->type = "border";
    // btn vector used in BarBtnEditor to show button in tree
    btns.append(effectUpBtn);
    connect(effectUpBtn, &BarBtn::clicked, this, &EmbelProperties::moveBorderUp);
    effectDownBtn = new BarBtn();
    effectDownBtn->setIcon(":/images/icon16/down.png", G::iconOpacity);
    effectDownBtn->setToolTip("Move border down");
    effectDownBtn->name = i.name;
    effectDownBtn->parName = i.parentName;
    effectDownBtn->type = "border";
    // btn vector used in BarBtnEditor to show button in tree
    btns.append(effectDownBtn);
    connect(effectDownBtn, &BarBtn::clicked, this, &EmbelProperties::moveBorderDown);
    effectDeleteBtn = new BarBtn();
    effectDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    effectDeleteBtn->setToolTip("Delete this effect");
    effectDeleteBtn->name = i.name;
    effectDeleteBtn->parName = i.parentName;
    effectDeleteBtn->type = "border";
    // btn vector used in BarBtnEditor to show button in tree
    btns.append(effectDeleteBtn);
    connect(effectDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    // add to model, where capIdx is defined
    addItem(i);
    effectUpBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();
    effectDownBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();
    effectDeleteBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();
}

void EmbelProperties::addEffectBtns()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    effectUpBtn = new BarBtn();
    effectUpBtn->setIcon(":/images/icon16/up.png", G::iconOpacity);
    effectUpBtn->setToolTip("Move effect up");
    effectUpBtn->name = i.name;
    effectUpBtn->parName = i.parentName;
    effectUpBtn->type = "effect";
    // btn vector used in BarBtnEditor to show button in tree
    btns.append(effectUpBtn);
    connect(effectUpBtn, &BarBtn::clicked, this, &EmbelProperties::moveEffectUp);
    effectDownBtn = new BarBtn();
    effectDownBtn->setIcon(":/images/icon16/down.png", G::iconOpacity);
    effectDownBtn->setToolTip("Move effect down");
    effectDownBtn->name = i.name;
    effectDownBtn->parName = i.parentName;
    effectDownBtn->type = "effect";
    // btn vector used in BarBtnEditor to show button in tree
    btns.append(effectDownBtn);
    connect(effectDownBtn, &BarBtn::clicked, this, &EmbelProperties::moveEffectDown);
    effectDeleteBtn = new BarBtn();
    effectDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    effectDeleteBtn->setToolTip("Delete this effect");
    effectDeleteBtn->name = i.name;
    effectDeleteBtn->parName = i.parentName;
    effectDeleteBtn->type = "effect";
    // btn vector used in BarBtnEditor to show button in tree
    btns.append(effectDeleteBtn);
    connect(effectDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    // add to model, where capIdx is defined
    addItem(i);
    effectUpBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();
    effectDownBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();
    effectDeleteBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();
}

void EmbelProperties::addBlurEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::blur;
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::blur, "Blur");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;      // tool button
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
//    qDebug() << __FUNCTION__
//             << "key =" << key
//             << "value =" << setting->value(key).toInt();
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
    parIdx = capIdx;

    // blur radius
    i.name = "radius";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Radius";
    i.tooltip = "The amount of transition from full shadow to background.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "radius";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.div = 1;
    i.fixedWidth = 50;
    effect.blur.radius = i.value.toDouble();
    addItem(i);

    // blur blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = false;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.blur.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addSharpenEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::sharpen;
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::sharpen, "Sharpen");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;      // tool button
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
//    qDebug() << __FUNCTION__
//             << "key =" << key
//             << "value =" << setting->value(key).toInt();
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
    parIdx = capIdx;

    // sharpen radius
    i.name = "radius";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Radius";
    i.tooltip = "The amount of sharpening.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "radius";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 10000;
    i.div = 100;
    i.fixedWidth = 50;
    effect.sharpen.radius = i.value.toDouble();
    addItem(i);

    // sharpen blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = false;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.sharpen.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addHighlighterEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::highlighter;
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::highlighter, "Highlighter");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
//    addItem(i);
    parIdx = capIdx;

    // highlighter top margin
    i.name = "top";  // x and y offsets are equal
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Top margin";
    i.tooltip = "The amount of highlighted margin on top of the item.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "top";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = -20;
    i.max = 20;
    i.fixedWidth = 50;
    effect.highlighter.top = i.value.toInt();
    addItem(i);

    // highlighter left margin
    i.name = "left";  // x and y offsets are equal
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Left margin";
    i.tooltip = "The amount of highlighted margin on left of the item.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "left";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = -20;
    i.max = 20;
    i.fixedWidth = 50;
    effect.highlighter.left = i.value.toInt();
    addItem(i);

    // highlighter right margin
    i.name = "right";  // x and y offsets are equal
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Right margin";
    i.tooltip = "The amount of highlighted margin on right of the item.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "right";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = -20;
    i.max = 20;
    i.fixedWidth = 50;
    effect.highlighter.right = i.value.toInt();
    addItem(i);

    // highlighter bottom margin
    i.name = "bottom";  // x and y offsets are equal
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Bottom margin";
    i.tooltip = "The amount of highlighted margin on bottom of the item.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "bottom";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = -20;
    i.max = 20;
    i.fixedWidth = 50;
    effect.highlighter.bottom = i.value.toInt();
    addItem(i);

    // highlighter opacity
    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the highlighter.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
    i.defaultValue = 50;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
//    i.fixedWidth = 50;
    effect.highlighter.a = i.value.toInt() * 255 / 100;
    addItem(i);

    // highlighter color
    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Color";
    i.tooltip = "Highlighter color.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.defaultValue = "#808080";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with white
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    QColor color(i.value.toString());
    effect.highlighter.r = color.red();
    effect.highlighter.g = color.green();
    effect.highlighter.b = color.blue();
//    effect.highlighter.a = color.alpha();
    addItem(i);

    // highlighter blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.defaultValue = "Below";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.highlighter.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addStrokeEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::stroke;
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::stroke, "Stroke");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;      // tool button
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
//    qDebug() << __FUNCTION__
//             << "key =" << key
//             << "value =" << setting->value(key).toInt();
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
//        qDebug() << __FUNCTION__ << i.path << i.sortOrder;
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
//    addItem(i);
    parIdx = capIdx;

    // stroke width
    i.name = "width";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Width";
    i.tooltip = "The width of the stroke.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "width";
    i.defaultValue = 4;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
//    i.fixedWidth = 50;
    effect.stroke.width = i.value.toInt();
    addItem(i);

    // stroke color
    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Color";
    i.tooltip = "Select a color that will be used to stroke.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with red
        i.value = "#FF0000";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    QColor color(i.value.toString());
    effect.stroke.r = color.red();
    effect.stroke.g = color.green();
    effect.stroke.b = color.blue();
    effect.stroke.a = color.alpha();
    addItem(i);

    // stroke opacity
    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the stroke.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
    i.defaultValue = 100;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType =  DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    effect.stroke.opacity = i.value.toDouble() * 1.0 / 100;
    addItem(i);

    // stroke blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.defaultValue = "Above";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.stroke.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addGlowEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::glow;
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::glow, "Glow");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;      // tool button
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
//    qDebug() << __FUNCTION__
//             << "key =" << key
//             << "value =" << setting->value(key).toInt();
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
//        qDebug() << __FUNCTION__ << i.path << i.sortOrder;
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
//    addItem(i);
    parIdx = capIdx;

    // glow width
    i.name = "width";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Width";
    i.tooltip = "The width of the glow.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "width";
    i.defaultValue = 4;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 30;
//    i.fixedWidth = 50;
    effect.glow.width = i.value.toInt();
    addItem(i);

    // glow blur
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blur";
    i.tooltip = "The blur of the glow effect.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blur";
    i.defaultValue = 6;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType =  DT_Slider;
    i.min = 0;
    i.max = 20;
    i.type = "int";
    effect.glow.blurRadius = i.value.toDouble();
    addItem(i);

    // glow color
    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Color";
    i.tooltip = "Select a color that will be used for the glow.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with red
        i.value = "#FF0000";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    QColor color(i.value.toString());
    effect.glow.r = color.red();
    effect.glow.g = color.green();
    effect.glow.b = color.blue();
    effect.glow.a = color.alpha();
    addItem(i);

//    // glow opacity
//    i.name = "opacity";
//    i.parIdx = parIdx;
//    i.parentName = effectName;
//    i.captionText = "Opacity";
//    i.tooltip = "The opacity of the glow.";
//    i.isIndent = true;
//    i.hasValue = true;
//    i.captionIsEditable = false;
//    i.key = "opacity";
//    i.defaultValue = 100;
//    i.path = settingRootPath + i.key;
//    if (setting->contains(settingRootPath + i.key))
//        i.value = setting->value(settingRootPath + i.key);
//    else {
//        i.value = i.defaultValue;
//        setting->setValue(settingRootPath + i.key, i.value);
//    }
//    i.delegateType =  DT_Slider;
//    i.type = "int";
//    i.min = 0;
//    i.max = 100;
//    effect.glow.opacity = i.value.toInt();
//    addItem(i);

    // glow blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.defaultValue = "Above";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.glow.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addBrightnessEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::brightness;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::brightness, "Brightness");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;      // tool button
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
    parIdx = capIdx;

    // brightness evDelta
    i.name = "evDelta";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Brightness";
    i.tooltip = "A change in value of 1 is equal to changing the exposure value by one stop, "
                "either halving or doubling the perceived brightness.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "evDelta";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = -500;
    i.max = +500;
    i.div = 100;
    double range = i.max - i.min;
    i.step = static_cast<int>(range / 255);
    i.fixedWidth = 50;
    effect.brightness.evDelta = i.value.toDouble();
    addItem(i);

    // brightness blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = false;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.blur.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addEmbossEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::emboss;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::brightness, "Emboss");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;      // tool button
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
    parIdx = capIdx;

    // emboss size
    i.name = "size";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Margin";
    i.tooltip = "The width or margin, as a percentage of the long side, of the emboss effect.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "size";
    i.defaultValue = 3;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 5000;
    i.div = 100;
    double range = i.max - i.min;
    i.step = static_cast<int>(range / longSidePx);
    i.fixedWidth = 50;
    effect.emboss.size = i.value.toDouble();
    addItem(i);

    // emboss inflection
    i.name = "inflection";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Inflection point";
    i.tooltip = "Set the point of inflection between the start and finish.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "inflection";
    i.defaultValue = 0.5;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.div = 100;
    i.fixedWidth = 50;
    effect.emboss.inflection = i.value.toDouble();
    addItem(i);

    // emboss exposure
    i.name = "exposure";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Exposure";
    i.tooltip = "Adjust the overall brightness.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "exposure";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = -300;
    i.max = 300;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / 255);
    i.fixedWidth = 50;
    effect.emboss.exposure = i.value.toDouble();
    addItem(i);

    // emboss contrast
    i.name = "contrast";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Contrast";
    i.tooltip = "The amount to contrast.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "contrast";
    i.defaultValue = 1;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 200;
    i.div = 100;
    i.step = 5;
    i.fixedWidth = 50;
    effect.emboss.contrast = i.value.toDouble();
    addItem(i);

    // emboss start
    i.name = "start";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Start EV";
    i.tooltip = "Adjust the brightness (EV) for the start point.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "start";
    i.defaultValue = -1;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = -400;
    i.max = 400;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / 255);
    i.fixedWidth = 50;
    effect.emboss.start = i.value.toDouble();
    addItem(i);

    // emboss middle
    i.name = "mid";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Mid EV";
    i.tooltip = "Adjust the brightness (EV) for the inflection point.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "mid";
    i.defaultValue = 1;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = -400;
    i.max = 400;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / 255);
    i.fixedWidth = 50;
    effect.emboss.mid = i.value.toDouble();
    addItem(i);

    // emboss end
    i.name = "end";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "End EV";
    i.tooltip = "Adjust the brightness (EV) for the end point.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "end";
    i.defaultValue = -1.25;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = -400;
    i.max = 400;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / 255);
    i.fixedWidth = 50;
    effect.emboss.end = i.value.toDouble();
    addItem(i);

    // emboss umbra
    i.name = "umbra";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Umbra";
    i.tooltip = "Adjust the black for areas in shade.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "umbra";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = -300;
    i.max = 300;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / 255);
    i.fixedWidth = 50;
    effect.emboss.umbra = i.value.toDouble();
    addItem(i);

    // emboss umbra apply gradient
    i.name = "isUmbraGradient";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Umbra gradient";
    i.tooltip = "The brightness of the shadows.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "isUmbraGradient";
    i.defaultValue = true;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    effect.emboss.isUmbraGradient = i.value.toBool();
    addItem(i);

    // emboss blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.defaultValue = "Above";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.emboss.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addShadowEffect(QModelIndex parIdx, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::shadow;
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::shadow, "Shadow");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Templates/" + templateName + "/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;      // tool button
    i.captionIsEditable = false;
    i.key = "sortOrder";
    i.delegateType = DT_BarBtns;
    // get settings data
    QString key = i.path + "/sortOrder";
//    qDebug() << __FUNCTION__
//             << "key =" << key
//             << "value =" << setting->value(key).toInt();
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
//        qDebug() << __FUNCTION__ << i.path << i.sortOrder;
    }
    else {
        i.sortOrder = model->rowCount(parIdx);
        setting->setValue(key, i.sortOrder);
    }
    effect.effectOrder = i.sortOrder;
    addEffectBtns();
//    addItem(i);
    parIdx = capIdx;

    // shadow size
    i.name = "length";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Length";
    i.tooltip = "The length of the shadow.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "length";
    i.defaultValue = 4;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 30;
//    i.fixedWidth = 50;
    effect.shadow.length = i.value.toInt();
    addItem(i);

    // shadow blur
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blur";
    i.tooltip = "The blur of the drop shadow.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blur";
    i.defaultValue = 6;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType =  DT_Slider;
    i.min = 0;
    i.max = 20;
    i.type = "int";
    effect.shadow.blurRadius = i.value.toDouble();
    addItem(i);

    // shadow color
    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Color";
    i.tooltip = "Select a color that will be used to fill the drop shadow.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with gray
        i.value = "#101010";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    QColor color(i.value.toString());
    effect.shadow.r = color.red();
    effect.shadow.g = color.green();
    effect.shadow.b = color.blue();
    effect.shadow.a = color.alpha();
    addItem(i);

    // shadow opacity
    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the stroke.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
    i.defaultValue = 100;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType =  DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    effect.shadow.opacity = i.value.toDouble() * 1.0 / 100;
    addItem(i);

    // shadow blend mode
    i.name = "blendMode";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blend mode";
    i.tooltip = "The way this effect blends with other effects in the style.";
    i.isIndent = true;;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blendMode";
    i.defaultValue = "Below";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList  << winnow_effects::blendModes;
    effect.shadow.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::newBorderFromBtn()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    newBorder("");
}

void EmbelProperties::newBorder(QString name)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    int rowCount = model->rowCount(bordersIdx);
    QString borderName = "";
    QStack<QString> bNum;
    if (name == "") {
        /* Get a new border name (Border1, border2...).  Account for border deletions and order
           changes. Start by getting all numbers used so far.  */
        for (int i = 0; i < rowCount; ++i) {
            QString s = model->index(i, 0, bordersIdx).data().toString();
            bNum.push(s.replace("Border", ""));
        }
        // Find first available and assign borderName
        for (int i = 0; i <= rowCount; ++i) {
            if (!bNum.contains(QString::number(i+1))) {
                borderName = "Border" + QString::number(i+1);
                break;
            }
            if (i == rowCount)
                borderName = "Border" + QString::number(i+1);
        }
    }
    else borderName = name;

    // add the new border
    addBorder(rowCount, borderName);
    updateBorderLists();
    if (G::isInitializing || isTemplateChange) return;
    QModelIndex idx = model->index(rowCount, CapColumn, bordersIdx);
//    selectionModel()->clear();
//    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    expand(bordersIdx);
    expand(idx);
    e->build("", __FUNCTION__);
    updateBorderLists();
}

void EmbelProperties::newText()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    int row = model->rowCount(textsIdx);
    addText(row);
    if (G::isInitializing || isTemplateChange) return;
    QModelIndex idx = model->index(row, CapColumn, textsIdx);
    expand(textsIdx);
    expand(idx);
    selectionModel()->clear();
    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    e->build("", __FUNCTION__);
}

void EmbelProperties::newGraphic()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    int row = model->rowCount(graphicsIdx);
    addGraphic(row);
    if (G::isInitializing || isTemplateChange) return;
    QModelIndex idx = model->index(row, CapColumn, graphicsIdx);
    expand(graphicsIdx);
    expand(idx);
    selectionModel()->clear();
    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    e->build("", __FUNCTION__);
}

void EmbelProperties::deleteTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // a blank template name could delete all templates
    if (templateName == "") {
        qDebug() << __FUNCTION__ << "Error: Blank template name";
        return;
    }

    int ret = (QMessageBox::warning(this, "Delete Template", "Confirm delete template " + templateName + "                     ",
                             QMessageBox::Cancel | QMessageBox::Ok));
    if (ret == QMessageBox::Cancel) return;

    QString pathToDelete = "Embel/Templates/" + templateName;

    // remove from tree combo list
    templateList.removeOne(templateName);
    templateListEditor->removeItem(templateName);
    // remove from settings
    setting->remove(pathToDelete);

    emit syncEmbellishMenu();
    syncWinnets();
}

//void EmbelProperties::deleteTile()
//{
//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
//    }

//    QString tileName;
//    int ret = (QMessageBox::warning(this, "Delete Tile", "Confirm delete tile " + tileName + "                     ",
//                             QMessageBox::Cancel | QMessageBox::Ok));
//    if (ret == QMessageBox::Cancel) return;

//    QString pathToDelete = "Embel/Tiles/" + tileName;

//    // remove from tree combo list
//    tileList.removeOne(tileName);
////    borderTileObjectEditor->removeItem(tileName);
//    // remove from settings
//    setting->remove(pathToDelete);
//}

void EmbelProperties::deleteItem()
{
/*
The tree header rows can contain delete buttons. All such delete operations are processed
here. The item (row in model) has to be removed from the data model, local vectors, scene
items in Embel and from QSettings. Also, if it is referred to elsewhere, such as a style in a
stylelist for a text or graphic, or an anchorObject, then the lists need to be updated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    QModelIndex idx = getItemIndex(btn->itemIndex);

        /*
    qDebug() << __FUNCTION__
             << "btn->type =" << btn->type
             << "btn->parName =" << btn->parName
             << "btn->name =" << btn->name
             << "idx.data() =" << idx.data().toString()
             << "idx.row() =" << idx.row()
             << "idx.data(UR_ItemIndex) =" << idx.data(UR_ItemIndex).toString()
             << "idx.data(UR_Path) =" << idx.data(UR_SettingsPath).toString()
                ;
//                */
    if (!idx.isValid()) return;

    int row = idx.row();
    QModelIndex parIdx = idx.parent();
    QString parName = parIdx.data(UR_Name).toString();
    QString name = idx.data(UR_Name).toString();
    QString path = idx.data(UR_SettingsPath).toString();

    // make sure not templatePath as this could delete all templates
    if (path == "Embel/Templates" || path == "Embel/Templates/" || path == "Embel/Templates//") {
        qDebug() << __FUNCTION__ << "ERROR: path to delte =" << path
                 << "would delete all templates!";
        return;
    }

    /*
    qDebug() << __FUNCTION__
             << "row =" << row
             << "name =" << name
             << "parName =" << parName
             << "path =" << path;
//    */
    int ret = (QMessageBox::warning(this, "Delete", "Confirm delete " + name + "                     ",
                             QMessageBox::Cancel | QMessageBox::Ok));
    if (ret == QMessageBox::Cancel) return;

    // remove from local vectors and Embel graphicItems
    if (parName == "Borders") {
        e->removeBorder(row);
        if (row < b.size()) b.remove(row);
    }

    if (parName == "Texts") {
        e->removeText(row);
         if (row < t.size()) {
             t.remove(row);
             textTokenObjectEditor.remove(row);
         }
    }

    if (parName == "Graphics") {
        e->removeGraphic(row);
         if (row < g.size()) g.remove(row);
    }

    // remove from datamodel
    model->removeRow(idx.row(), idx.parent());
    updateBorderOrderAfterDeletion();
    updateEffectOrderAfterDeletion(idx.parent());

    // remove from setting
    setting->remove(path);

    /* rename subsequent category items ie text2, text3 ... in setting, model and vectors
       This is not required for styles and effects as they are not numbered automatically.
       Borders can be reordered so they are not renumbered.  */
    QStringList itemsToRenumber;
    itemsToRenumber << "Texts" << "Graphics";
    if (itemsToRenumber.contains(parName)) {
        QString itemBase; //= parName.left(parName.length() - 1);
        if (parName == "Texts") itemBase = "Text";
        if (parName == "Graphics") itemBase = "Graphic";
        for (int i = row; i < model->rowCount(parIdx); ++i) {
            QString parPath = templatePath + parName;
            QString oldName = model->index(i, CapColumn, parIdx).data().toString();
            QString newName = itemBase + QString::number(i + 1);
            // update setting
            renameSettingKey(parPath, oldName, newName);
            // update model
            model->setData(model->index(i, CapColumn, parIdx), newName);
            // update local struct
            if (parName == "Texts") t[i].name = newName;
            if (parName == "Graphics") g[i].name = newName;
        }
    }

    /* if border deleted update lists that have borders and any anchorObjects in texts and
       graphics.  This needs to happen after all data structures have been updated.  */
    if (parName == "Borders") updateBorderLists();

    // update references to style
    styleList.removeAt(styleList.indexOf(name));
    // styleListObjectEditor is a list of editors with style lists
    if (parName == "Styles") {
        for (int i = 0; i < styleListObjectEditor.length(); ++i) {
            styleListObjectEditor[i]->removeItem(name);
        }
    }

    // update styleMap if a style was deleted
    if (parName == "Styles") {
        styleMap.remove(name);
    }

    // update styleMap if an effect was deleted
    if (btn->type == "effect") {
        // find the effect
        int i;
        bool effectFound = false;
        for (i = 0; i < styleMap[parName].length(); ++i) {
            if (styleMap[parName][i].effectName == name) {
                effectFound = true;
                break;
            }
        }
        if ( effectFound) styleMap[parName].removeAt(i);
    }

    // select another item
    int rowCount = model->rowCount(idx.parent());
    if (rowCount > 0) {
        QModelIndex nextIdx = model->index(row, CapColumn, parIdx);
        if (!nextIdx.isValid()) nextIdx = model->index(rowCount - 1, CapColumn, parIdx);
        // req'd ?
        selectionModel()->select(nextIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

    // refresh the graphicsScene
    e->build("", __FUNCTION__);
}

QString EmbelProperties::uniqueEffectName(QString styleName, int effectType, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
//    using namespace winnow_effects;
    /*
    qDebug();
    qDebug() << __FUNCTION__
             << "styleName =" << styleName
             << "effectType =" << effectType
             << "effectName =" << effectName
                ;
//                */
    // effects in style
    int effectCount = 0;
    for (int i = 0; i < styleMap[styleName].length(); ++i) {
        if (effectType == styleMap[styleName].at(i).effectType) {
            effectCount++;
        }
        qDebug() << __FUNCTION__ << "Iterate styleMap[styleName]:"
                 << "i =" << i
                 << "effectType =" << effectType
                 << "styleMap[styleName].at(i).effectType =" << styleMap[styleName].at(i).effectType
                    ;
    }
    if (effectCount > 0) effectName += QString::number(effectCount);
    /*
    qDebug() << __FUNCTION__ << "New effectName =" << effectName;
    qDebug();
//    */
    return effectName;
}

int EmbelProperties::effectIndex(QString style, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, effectName);
    #endif
    }
    using namespace winnow_effects;
    // effects in style
//    qDebug() << __FUNCTION__ << style << effectName;
    for (int i = 0; i < styleMap[style].length(); ++i) {
//        qDebug() << __FUNCTION__
//                 << "i =" << i
//                 << "effectName =" << effectName
//                 << "styleMap[style].at(i).effectName =" << styleMap[style].at(i).effectName;
        if (effectName == styleMap[style].at(i).effectName) return i;
    }
    return -1;
}

void EmbelProperties::flash(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // check index range (triggered by mouse movement while tree painting)
    if (idx.row() < 0) return;

    // check if an image has been selected.  if not then nothing to flash

    QString type;
    if (idx.parent() == bordersIdx) {
        type = "border";
        if (idx.row() > model->rowCount(bordersIdx)) return;
    }
    if (idx.parent() == textsIdx) {
        type = "text";
        if (idx.row() > model->rowCount(textsIdx)) return;
    }
    if (idx.parent() == graphicsIdx) {
        type = "graphic";
        if (idx.row() > model->rowCount(graphicsIdx)) return;
    }
    e->flashObject(type, idx.row(), true);
}

void EmbelProperties::mouseMoveEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // nothing to flash
    if (!G::isEmbellish) return;

    // make sure there is model data
    if (!bordersIdx.isValid()) return;

    QModelIndex idx = indexAt(event->pos());

    // check if invalid index
    if (!idx.isValid()) return;

    // check if flashable item in property tree
    if (idx.parent() == bordersIdx ||
        idx.parent().parent() == bordersIdx ||
        idx.parent() == textsIdx ||
        idx.parent().parent() == textsIdx ||
        idx.parent() == graphicsIdx ||
        idx.parent().parent() == graphicsIdx
       )
    {
        /*
        static int count = 0;
        qDebug() << __FUNCTION__ << count
                 << idx.data().toString()
                 << idx.parent().data().toString()
                 << idx.parent().parent().data().toString()
                    ;
//                    */
        if (idx.parent().parent() == bordersIdx) idx = idx.parent();
        if (idx.parent().parent() == textsIdx) idx = idx.parent();
        if (idx.parent().parent() == graphicsIdx) idx = idx.parent();
        flash(idx);
    }
}

//void EmbelProperties::mouseDoubleClickEvent(QMouseEvent *event)
//{
//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
//    }
//    QModelIndex idx = indexAt(event->pos());
//    idx = model->index(idx.row(), ValColumn, idx.parent());
//    QVariant value = idx.data(UR_DefaultValue);
//    if (idx.data(UR_DelegateType).toInt() == DT_Slider)
//        value = idx.data(UR_DefaultValue).toDouble() * idx.data(UR_Div).toInt();
//    setItemValue(idx, idx.data(UR_DelegateType).toInt(), value);
//}

void EmbelProperties::mousePressEvent(QMouseEvent *event)
{
/*
    Set the current index and expand/collapse when click anywhere on a row that has children.
    Do not pass on to QTreeView as this will enable QTreeView expanding and collapsing when
    the decoration is clicked.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }

    QModelIndex idx = indexAt(event->pos());
    currentIdx = model->index(idx.row(), CapColumn, idx.parent());
    QModelIndex currentValIdx = model->index(idx.row(), ValColumn, idx.parent());

    if (event->button() == Qt::LeftButton) {
        if (currentIdx.parent() == bordersIdx ||
            currentIdx.parent() == textsIdx ||
            currentIdx.parent() == graphicsIdx)
        {
            flash(currentIdx);
        }
    }

    if (event->button() == Qt::RightButton) {
        renameAction->setVisible(false);
        copyTemplateAction->setVisible(false);
        newTemplateAction->setVisible(false);
        readTemplateFromFileAction->setVisible(false);
        saveTemplateToFileAction->setVisible(false);
        copyStyleAction->setVisible(false);
        tokenEditorAction->setVisible(false);
        manageTilesAction->setVisible(false);
        moveUpAction->setVisible(false);
        moveDownAction->setVisible(false);

//        QModelIndex idx = indexAt(event->pos());
//        currentIdx = model->index(idx.row(), 0, idx.parent());
        /*
        qDebug() << __FUNCTION__
                 << currentIdx.data().toString()
                 << currentIdx.parent().data().toString()
                 << templateIdx.data().toString()
                    ;
//                    */

        if (currentIdx.parent() == templateIdx || currentIdx == templateIdx) {
            renameAction->setVisible(true);
            copyTemplateAction->setVisible(true);
            newTemplateAction->setVisible(true);
            readTemplateFromFileAction->setVisible(true);
            saveTemplateToFileAction->setVisible(true);
        }

        if (currentIdx.parent() == stylesIdx) {
            renameAction->setVisible(true);
            copyStyleAction->setVisible(true);
        }

        if (currentIdx.parent().parent() == stylesIdx &&
            model->rowCount(currentIdx.parent()) > 1)
        {
            moveUpAction->setVisible(true);
            moveDownAction->setVisible(true);
        }

        if (currentIdx.parent() == bordersIdx) {
            moveUpAction->setVisible(true);
            moveDownAction->setVisible(true);
        }

        if (currentIdx.parent().parent() == bordersIdx) {
            if (currentIdx.data().toString() == "Tile" && b[idx.parent().row()].tile != "Do not tile")
                manageTilesAction->setVisible(true);
        }

        if (currentIdx.parent() == textsIdx || currentIdx.parent().parent() == textsIdx) {
            tokenEditorAction->setVisible(true);
        }
    }

    PropertyEditor::mousePressEvent(event);
}

void EmbelProperties::treeChange(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
//    qDebug() << __FUNCTION__ << idx << idx.isValid();
    if (!idx.isValid()) return;
//    return;
    if (idx.column() != 0) idx = model->index(idx.row(), CapColumn, idx.parent());
    bool hasChildren = model->hasChildren(idx);
    bool hasGrandChildren = model->hasChildren(model->index(0,0,idx));
    bool templateHdr = (idx == model->index(0,0));
    QString selName = idx.data(UR_Name).toString();
    QString parName = idx.parent().data(UR_Name).toString();
    bool isStyle = styleList.contains(selName) || effectList.contains(selName);
    isStyle = true;
//    /*
    qDebug() << __FUNCTION__
             << "hasChildren =" << hasChildren
             << "hasGrandChildren =" << hasGrandChildren
             << "templateHdr =" << templateHdr
             << "selName =" << selName
             << "isStyle =" << isStyle
                ;
//    */
    if ((hasChildren && !hasGrandChildren && !templateHdr) || isStyle) {
        bool wasExpanded = isExpanded(idx);
//        qDebug() << __FUNCTION__ << idx.data(UR_Name).toString();
        // clear selection and delete buttons (re-instated after selection change)
        selectionModel()->clear();
        if (okToSelect(idx, selName)) {
            selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
//        if (!isStyle) collapseAll();
//        else {
            collapse(model->index(_image,0));
            collapse(model->index(_borders,0));
            collapse(model->index(_texts,0));
            collapse(model->index(_graphics,0));
//        }
        expand(model->index(_templates,0));
        expand(model->index(_borders,0));
        expand(model->index(_texts,0));
        expand(model->index(_graphics,0));
//        expand(model->index(_styles,0));
        wasExpanded ? collapse(idx) : expand(idx);
    }
}

//void EmbelProperties::selectionChanged(const QItemSelection &selection,
//                                      const QItemSelection &prevSelection)
//{
///*
//Check if the new selection is legal (a member of borders, texts, rectangles, graphics or
//file or image).  If not, if there was a previous selection, revert to that.  Hide all delete
//buttons except in the selected category.
//*/
//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
//    }
//    if (selection.isEmpty()) return;
//    QModelIndex idx = selection.indexes().first();
//    QModelIndex parIdx = idx.parent();
//    QString selName = idx.data(UR_Name).toString();
//    if (!okToSelect(idx, selName)) {
//        selectionModel()->clear();
//        if (!okToSelect(parIdx, selName)) return;
//        selectionModel()->select(parIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
//        idx = parIdx;
//    }
//}

bool EmbelProperties::okToSelect(QModelIndex idx, QString selName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    qDebug() << __FUNCTION__ << "selName =" << selName;
    QModelIndex parIdx = idx.parent();
    if (!parIdx.isValid()) return false;
    if (parIdx == model->index(_borders,0)) return true;
    if (parIdx == model->index(_texts,0)) return true;
    if (parIdx == model->index(_graphics,0)) return true;
    if (parIdx == model->index(_styles,0)) return true;
    if (styleList.contains(selName)) return true;
    if (effectList.contains(selName)) return true;
//    qDebug() << __FUNCTION__ << "false for" << selName << effectList;
    return false;
}

QString EmbelProperties::metaString(QString key)
{
/*
    Local information (datamodel, currentImage etc) is available.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (mw3->infoString->infoTemplates.contains(key)) {
        QString tokenString = mw3->infoString->infoTemplates[key];
        QString path = mw3->imageView->currentImagePath;
        QModelIndex curIdx = mw3->thumbView->currentIndex();
        return mw3->infoString->parseTokenString(tokenString, path, curIdx);
    }
    else return "";
}

QString EmbelProperties::metaString(QString key, QString fPath)
{
/*
    Look up information using Metadata if the image will not be active
    in Winnow (ie triggered by EmbelExport).
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    QString tokenString = mw3->infoString->infoTemplates[key];
    return mw3->infoString->parseTokenString(tokenString, fPath);
}

QString EmbelProperties::diagnostics()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "Embellish Diagnostics");
    rpt << "\n";
    /*
    qDebug().noquote() << "Diagnostics for index"
                       << idx.data(UR_Name).toString()
                       << "row =" << idx.row()
                       << "col =" << idx.column();
    qDebug().noquote()
        << "GParent" << idx.parent().parent().data(UR_Name).toString()
        << "Parent" << idx.parent().data(UR_Name).toString()
        << "Index" << idx.parent().data(UR_ItemIndex).toInt()
        << "Delegate" << idx.data(UR_DelegateType).toString()
        << "Source" << idx.data(UR_Source).toString();
        */
    rpt << "\ncurrent:  template = " << templateName;
    diagnosticStyles(rpt);
    diagnosticVectors(rpt);
    e->diagnostics(rpt);
    diagnosticModel(rpt);
    rpt << "\n\n" ;
    return reportString;
}

void EmbelProperties::expandAllRows()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    expandAll();
}

void EmbelProperties::collapseAllRows()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    collapseAll();
    expand(model->index(_templates,0));
}

void EmbelProperties::solo()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setSolo(soloAction->isChecked());
    setting->setValue("Embel/isSolo", soloAction->isChecked());
}

void EmbelProperties::expandRecursively()
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
    }
    setExpandRecursively(expandRecursivelyAction->isChecked());
    setting->setValue("Embel/expandRecursively", expandRecursivelyAction->isChecked());
}

void EmbelProperties::test1(QIcon icon)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    syncWinnets();
//    e->test();
//    diagnosticVectors();
//    diagnosticStyles();
}

void EmbelProperties::test2()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

}

void EmbelProperties::coordHelp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    HtmlWindow *w = new HtmlWindow("Winnow - Embel Container and Coordinate System",
                                   ":/Docs/embelcoordsystem.html",
                                   QSize(939,736));
}

void EmbelProperties::addBorder(int count, QString borderName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, borderName);
    #endif
    }
    // Name (used as node in settings and treeview)
//    QString borderName = "Border" + QString::number(count + 1);
    border.name = borderName;
//    qDebug() << __FUNCTION__ << borderName << count;
    QString settingRootPath = templatePath + "Borders/" + borderName + "/";

    // subheader for this border
    i.isHeader = true;
    i.isDecoration = true;
    i.name = borderName;
    i.itemIndex = count;
    i.parIdx = bordersIdx;
    i.parentName = "Borders";
    i.path = templatePath + "Borders/" + borderName;
    i.captionText = borderName;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    // get sort order for this border
    QString key = i.path + "/sortOrder";
    if (setting->contains(key)) {
        i.sortOrder = setting->value(key).toInt();
    }
    else {
        i.sortOrder = model->rowCount(bordersIdx);
        setting->setValue(key, i.sortOrder);
    }
    // get itemIndex for this border
//    key = i.path + "/itemIndex";
//    if (setting->contains(key)) {
//        i.itemIndex = setting->value(key).toInt();
//    }
//    else {
//        i.itemIndex = uniqueItemIndex(bordersIdx);
//        setting->setValue(key, i.itemIndex);
//    }
    // add buttons and addItem
    addBorderHeaderButtons();
    QModelIndex parIdx = capIdx;

    int defaultMargin = 5;

    i.name = "topMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Top %";
    i.tooltip = "This is the margin for the top part of the border (% of the long side).";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "topMargin";
    i.defaultValue = defaultMargin;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value.toDouble());
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
    double range = i.max - i.min;
    i.step = static_cast<int>(range * 5 / longSidePx);
    i.fixedWidth = 50;
    border.top = i.value.toDouble();
    addItem(i);

    i.name = "leftMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Left %";
    i.tooltip = "This is the margin for the left part of the border (% of the long side).";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "leftMargin";
    i.defaultValue = defaultMargin;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range * 5 / longSidePx);    i.fixedWidth = 50;
    border.left = i.value.toDouble();
    addItem(i);

    i.name = "rightMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Right %";
    i.tooltip = "This is the margin for the right part of the border (% of the long side).";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "rightMargin";
    i.defaultValue = defaultMargin;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range * 5 / longSidePx);    i.fixedWidth = 50;
    i.fixedWidth = 50;
    border.right = i.value.toDouble();
    addItem(i);

    i.name = "bottomMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Bottom %";
    i.tooltip = "This is the margin for the bottom part of the border (% of the long side).";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "bottomMargin";
    i.defaultValue = defaultMargin;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range * 5 / longSidePx);    i.fixedWidth = 50;
    i.fixedWidth = 50;
    border.bottom = i.value.toDouble();
    addItem(i);

    i.name = "tile";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Tile";
    i.tooltip = "Select a tile that will be used to fill the border area.\n"
                "Use the menu option 'Manage Tiles' to create, delete and\n"
                "rename tiles."
                ;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "tile";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
        border.tile = setting->value("Embel/Tiles/" + i.value.toString()).toByteArray();
    }
    else {
        i.value = "Do not tile";
        setting->setValue(settingRootPath + i.key, i.value);
        border.tile.clear();
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << tileList;
    borderTileObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));


    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Border color";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with a random color
        quint32 x = QRandomGenerator::global()->generateDouble() * 16777215;
        i.value = "#" + QString::number(x, 16);
//        qDebug() << __FUNCTION__ << i.value;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    border.color = i.value.toString();
    addItem(i);

    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the border.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 100;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType =  DT_Slider;
    i.min = 0;
    i.max = 100;
    i.type = "int";
    border.opacity = i.value.toInt();
    addItem(i);

    i.name = "style";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the border area.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "style";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "No style" << styleList;
    border.style = i.value.toString();
    styleListObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    // add the border info to the vector of borders
    b << border;

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addText(int count)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "Text " + QString::number(count));
    #endif
    }
    // Name (used as node in settings and treeview)
    QString textName = "Text" + QString::number(count + 1);
    QString settingRootPath = templatePath + "Texts/" + textName + "/";

    // subheader for this border
    i.isHeader = true;
    i.isDecoration = true;
//    i.itemIndex = uniqueItemIndex(textsIdx);
    i.name = textName;
    i.parentName = "Texts";
    i.path = templatePath + "Texts/" + textName;
    i.captionText = textName;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    textDeleteBtn = new BarBtn();
    textDeleteBtn->setObjectName(textName);
    textDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    textDeleteBtn->setToolTip("Delete this text item");

    // rgh req'd ?
    textDeleteBtn->name = i.name;
    textDeleteBtn->parName = i.parentName;

    textDeleteBtn->type = "text";
    btns.append(textDeleteBtn);
    connect(textDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    addItem(i);
    QModelIndex parIdx = capIdx;
    // itemIndex is assigned in addItem()
    textDeleteBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();

    // get index to use as parent when update tree depending on source
    text.name = textName;

    i.name = "anchorObject";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Put text in";
    i.tooltip = "Select a border or image to contain the text.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorObject";
    i.defaultValue = "Image";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
        if (i.value != "Image" && b.count() == 0) {
            i.value = "Image";
            setting->setValue(settingRootPath + i.key, i.value);
        }
    }
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << anchorObjectList;
    text.anchorObject = i.value.toString();
    textAnchorObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    i.name = "anchorContainer";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Border area";
    i.tooltip = "Select a border area to contain the text.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorContainer";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Bottom";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << anchorContainerList;
    text.anchorContainer = i.value.toString();
    addItem(i);
    // hide if anchorObject == "Image" (Image does not have border areas)
    if (text.anchorObject == "Image") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
        setRowHidden(capIdx.row(), capIdx.parent(), true);
    }

    i.name = "x";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "x coordinate";
    i.tooltip = "The x coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "x";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 10000;
    i.div = 100;
    double range = i.max - i.min;
    i.step = static_cast<int>(range / longSidePx);
    i.fixedWidth = 50;
    text.x = i.value.toDouble();
    addItem(i);

    i.name = "y";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "y coordinate";
    i.tooltip = "The y coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "y";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 10 * count;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 10000;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / longSidePx);    i.fixedWidth = 50;
    text.y = i.value.toDouble();
    addItem(i);

    i.name = "anchorPoint";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Justify";
    i.tooltip = "Select a text box anchor point.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorPoint";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Top Left";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << anchorPoints;
    text.anchorPoint = i.value.toString();
    addItem(i);

    i.name = "align";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Align";
    i.tooltip = "Align text anchor point to the nearest image edge.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "align";
    i.path = settingRootPath + i.key;
    i.defaultValue = false;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    text.align = i.value.toBool();
    addItem(i);

    i.name = "rotation";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Rotation";
    i.tooltip = "The rotation (degrees) of the text (+- 0-360)";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "rotation";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = -360;
    i.max = 360;
    i.fixedWidth = 50;
    text.rotation = i.value.toInt();
    addItem(i);

    i.name = "source";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Source";
    i.tooltip = "Select what is to be used: the text or text generated by a metadata template.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "source";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Text";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Text" << "Metadata template";
    text.source = i.value.toString();
    addItem(i);    

    i.name = "text";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Text";
    i.tooltip = "Enter text to be displayed.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "text";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = "Text #" + QString::number(count + 1);
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_LineEdit;
    i.type = "QString";
    border.color = i.value.toString();
    text.text = i.value.toString();
    addItem(i);

    // update tree based on source
    if (text.source == "Metadata template") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // capIdx defined by addItem
        setRowHidden(capIdx.row(), capIdx.parent(), true);
    }

    i.name = "metadataTemplate";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Template";
    i.tooltip = "Select a metadata template.  A template can include multiple metadata \n"
                "fields and user input text.  Right click in any text field to access\n"
                "the token editor to create or edit a metadata template.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "metadataTemplate";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Default info";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << metadataTemplatesList;
    QString key = i.value.toString();
    text.metadataTemplate = key;
    textTokenObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    // update tree based on source
    if (text.source == "Text") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // capIdx defined by addItem
        setRowHidden(capIdx.row(), capIdx.parent(), true);
    }

    i.name = "size";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Size";
    i.tooltip = "Enter a percentage of the long side pixels to set a font size in pixels.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "size";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 2.0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
    i.fixedWidth = 50;
    text.size = i.value.toDouble();
    addItem(i);

    i.name = "font";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Font";
    i.tooltip = "Select a font.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "font";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Choose font";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    QFontDatabase fonts;
    i.dropList << fonts.families();
    text.font = i.value.toString();
    addItem(i);

    i.name = "bold";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Bold";
    i.tooltip = "Use bold font.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "bold";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = false;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    text.isBold = i.value.toBool();
    addItem(i);

    i.name = "italic";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Italic";
    i.tooltip = "Use italic font.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "italic";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = false;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    text.isItalic = i.value.toBool();
    addItem(i);

    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Color";
    i.tooltip = "Select a color that will be used to fill the border area.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with white
        i.value = "#ffffff";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    text.color = i.value.toString();
    addItem(i);

    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the text.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
    i.defaultValue = 100;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType =  DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    text.opacity = i.value.toInt();
    addItem(i);

    i.name = "style";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the text.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "style";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
        if (!styleList.contains(i.value.toString())) {
            qDebug() << __FUNCTION__ << "Style " << i.value << "no longer exists";
            i.value = "No style";
        }
    }
    else {
        i.value = "No style";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "No style" << styleList;
    text.style = i.value.toString();
    styleListObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    // add the text info to the vector of texts t
    t << text;

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}

void EmbelProperties::addGraphic(int count)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "Graphic " + QString::number(count));
    #endif
    }
    // Name (used as node in settings and treeview)
    QString graphicName = "Graphic" + QString::number(count + 1);
    QString settingRootPath = templatePath + "Graphics/" + graphicName + "/";

    // subheader for this border
    i.isHeader = true;
    i.isDecoration = true;
    i.itemIndex = count;
    i.name = graphicName;
    i.parentName = "Graphics";
    i.path = templatePath + "Graphics/" + graphicName;
    i.captionText = graphicName;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    graphicDeleteBtn = new BarBtn();
    graphicDeleteBtn->setObjectName(graphicName);
    graphicDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    graphicDeleteBtn->setToolTip("Delete this text item");

    // rgh req'd ?
    graphicDeleteBtn->name = i.name;
    graphicDeleteBtn->parName = i.parentName;

    graphicDeleteBtn->type = "graphic";
    btns.append(graphicDeleteBtn);
    connect(graphicDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    addItem(i);
    QModelIndex parIdx = capIdx;
    graphicDeleteBtn->itemIndex = capIdx.data(UR_ItemIndex).toInt();

    // get index to use as parent when update tree depending on source
    graphic.name = graphicName;

    i.name = "graphic";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Graphic";
    i.tooltip = "Select the graphic to use.\n"
                "Use the menu option 'Manage Graphics' to create, delete and\n"
                "rename graphics.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "graphic";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
//        qDebug() << __FUNCTION__ << i.value;
        graphic.graphic = setting->value("Embel/Graphics/" + i.value.toString()).toByteArray();
    }
//    else {
//        i.value = "";
//        setting->setValue(settingRootPath + i.key, i.value);
//    }
    i.delegateType = DT_Combo;
    i.type = "string";
    graphic.name = i.value.toString();
    i.dropList << graphicList;
    graphicsObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    i.name = "anchorObject";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Place graphic in";
    i.tooltip = "Select a border or image to place the graphic.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorObject";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Image";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << anchorObjectList;
    graphic.anchorObject = i.value.toString();
    graphicAnchorObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    i.name = "anchorContainer";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Border area";
    i.tooltip = "Select a border area to contain the text.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorContainer";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Bottom";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << anchorContainerList;
    graphic.anchorContainer = i.value.toString();
    addItem(i);
    // hide if not selected by anchorObject
    if (graphic.anchorObject == "Image") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
        setRowHidden(capIdx.row(), capIdx.parent(), true);
    }

    i.name = "x";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "x coordinate";
    i.tooltip = "The x coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "x";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 10000;
    i.div = 100;
    double range = i.max - i.min;
    i.step = static_cast<int>(range / longSidePx);
    i.fixedWidth = 50;
    graphic.x = i.value.toDouble();
    addItem(i);

    i.name = "y";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "y coordinate";
    i.tooltip = "The y coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "y";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 10 * count;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 10000;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / longSidePx);
    i.fixedWidth = 50;
    graphic.y = i.value.toDouble();
    addItem(i);

    i.name = "anchorPoint";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Justify";
    i.tooltip = "Select a graphic anchor point.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorPoint";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Top Left";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << anchorPoints;
    graphic.anchorPoint = i.value.toString();
    addItem(i);

    i.name = "align";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Align";
    i.tooltip = "Align text anchor point to the nearest image edge.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "align";
    i.defaultValue = false;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    graphic.align = i.value.toBool();
    addItem(i);

    i.name = "size";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Size";
    i.tooltip = "The size of the graphic defined by the % of the long side of the frame.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "size";
    i.defaultValue = 10.0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 10000;
    i.div = 100;
    range = i.max - i.min;
    i.step = static_cast<int>(range / longSidePx);
    i.fixedWidth = 50;
    graphic.size = i.value.toDouble();
    addItem(i);

    i.name = "rotation";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Rotation";
    i.tooltip = "The rotation (degrees) of the graphic (+- 0-360)";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "rotation";
    i.defaultValue = 0;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = -360;
    i.max = 360;
    i.fixedWidth = 50;
    graphic.rotation = i.value.toInt();
    addItem(i);

    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the graphic object.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
    i.defaultValue = 100;
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = i.defaultValue;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType =  DT_Slider;
    i.min = 0;
    i.max = 100;
    i.type = "int";
    graphic.opacity = i.value.toInt();
    addItem(i);

    i.name = "style";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the graphic object.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "style";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "No style";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "No style" << styleList;
    graphic.style = i.value.toString();
    styleListObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    // add the graphic info to the vector of graphics g
    g << graphic;

    // scoll to last item in new object (PropertyEditor::addItem updates capIdx)
    scrollTo(capIdx);
}
