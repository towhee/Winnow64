#include "embelproperties.h"
#include "Main/mainwindow.h"

/*
This class holds the template information used by Embel to create borders, add text, rectangles
and graphics to any image in the loupe view.  Template0 is an empty template, which means no
changes are made to the loupe image. The information is stored in QSettings, each template,
when chosen, is represented in a StandardItemModel model and the model, in turn, is translated
into structs and QVestors of struct such as QVector<Border>b.  The structs make it easier to
use the information in Embel, where the graphics are build in the scene, which is contained in
ImageView.

Thus the embellish information is maintained in three information structures:

- QSettings
- DataModel
- Local structs

QSettings structure:

Embel
    Styles
    Templates
        Do not Embellish (template0)
        Template1                   isCurrent
            File
            Image
            Borders
                Border1
                ...
            Texts
                Text1
                ...
            Rectangles
                Rectangle1
                ...
            Graphics
                Graphic1
                ...
        Template2
        ...
    Tiles

Each of the above in turn includes a series of fields the contain the information required by
Embel.  The information is read from QSettings by addTemplateItems, addBorders ...  As each
item is read, it is sent to PropertyEditor::addItem, which builds a StandardItemModel to
mirror the QSettings. A custom widget is created in the treeview the is used to display the
information to the user for editing.

When items change in the datamodel this->itemChanged is signalled.  The change is saved in
QSettings and the local structs.  Embel::build is called to update the QGraphicsScene in
ImageView.

Associated classes

    MW::embelDockWidget (container for EmbelProperties - this treeview)
        Embel
            ImageView (contains the QGraphicsScene)
            EmbelProperties - PropertyEditor - PropertyDelegate - PropertyWidgets

*/

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

//    setSolo(true);
    collapseAll();
    expand(model->index(0,0,QModelIndex()));
    setIndentation(12);
    setAlternatingRowColors(false);
    propertyDelegate->isAlternatingRows = false;
    resizeColumns("====captions column====",
                  "====values column====");
    setStyleSheet(G::css);
    setStyleSheet
    (
        "QTreeView {"
            "selection-background-color: transparent;"
        "}"
    );

    initialize();
    // get the list of templates and which one was last active
    readTemplateList();
    // add the template header, which then selects the last active template
    addTemplateHeader();
    // add styles, which are independent of templates
    if (templateId != 0) addStyles();
    // add the File, Image, Borders, Texts, Rectangles and Graphics items for the template
    if (templateId != 0) addTemplateItems();
    // default state
    collapseAll();
    expand(model->index(_templates,0));
    expand(model->index(_borders,0));
    expand(model->index(_texts,0));
    expand(model->index(_rectangles,0));
    expand(model->index(_graphics,0));

//    updateHiddenRows(QModelIndex());
}

void EmbelProperties::initialize()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    anchorPoints << "Top Left" << "Top Center" << "Top Right"
                 << "Middle Left" << "Middle Center" << "Middle Right"
                 << "Bottom Left" << "Bottom Center" << "Bottom Right";
    anchorContainerList << "Top" << "Left" << "Right" << "Bottom";
    borderCorners << "TopLeft" << "TopRight" << "BottomLeft" << "BottomRight";
    readTileList();
    QMapIterator<QString, QString> i(mw3->infoString->infoTemplates);
    while (i.hasNext()) {
        i.next();
        metadataTemplatesList << i.key();
    }
    // EFFECTS
    effectList << "Shadow" << "Bevel" << "Emboss" << "Blur" << "Sharpen"
               << "Colorize" << "Edges" << "Highlight";

    shadowEffectAction = new QAction(tr("Shadow"), this);
    shadowEffectAction->setObjectName("shadowEffectAction");
    connect(shadowEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    bevelEffectAction = new QAction(tr("Bevel"), this);
    bevelEffectAction->setObjectName("bevelEffectAction");
    connect(bevelEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    embossEffectAction = new QAction(tr("Emboss"), this);
    embossEffectAction->setObjectName("embossEffectAction");
    connect(embossEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    blurEffectAction = new QAction(tr("Blur"), this);
    blurEffectAction->setObjectName("blurEffectAction");
    connect(blurEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    sharpenEffectAction = new QAction(tr("Sharpen"), this);
    sharpenEffectAction->setObjectName("sharpenEffectAction");
    connect(sharpenEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    colorizeEffectAction = new QAction(tr("Colorize"), this);
    colorizeEffectAction->setObjectName("colorizeEffectAction");
    connect(colorizeEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    edgeEffectAction = new QAction(tr("Edge"), this);
    edgeEffectAction->setObjectName("edgeEffectAction");
    connect(edgeEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    highlightEffectAction = new QAction(tr("Highlight"), this);
    highlightEffectAction->setObjectName("highlightEffectAction");
    connect(highlightEffectAction, &QAction::triggered, this, &EmbelProperties::effectActionClicked);

    effectMenu = new QMenu(this);
    effectMenu->addAction(shadowEffectAction);
    effectMenu->addAction(bevelEffectAction);
    effectMenu->addAction(embossEffectAction);
    effectMenu->addAction(blurEffectAction);
    effectMenu->addAction(sharpenEffectAction);
    effectMenu->addAction(colorizeEffectAction);
    effectMenu->addAction(edgeEffectAction);
    effectMenu->addAction(highlightEffectAction);

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
    soloAction->setShortcut(QKeySequence("Ctrl+*"));
    soloAction->setCheckable(true);
    addAction(soloAction);
    soloAction->setChecked(isSolo);
    connect(soloAction, &QAction::triggered, this, &EmbelProperties::solo);
    solo();

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
    effectParentIdx = btn->index;
    effectMenu->exec(QCursor::pos());
}

void EmbelProperties::effectActionClicked()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString effect = (static_cast<QAction*>(sender()))->text();
    if (effect == "Blur") addBlurEffect(effectParentIdx);
    if (effect == "Highlight") addHighlightEffect(effectParentIdx);
    if (effect == "Shadow") addShadowEffect(effectParentIdx);
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
    }
    borderList.clear();
    anchorObjectList.clear();
    alignToCornerList.clear();
    alignToCornerList << "Do not align";
//    getIndex("Borders");
//    QModelIndex bordersIdx = foundIdx;
    for (int i = 0; i < model->rowCount(bordersIdx); ++i) {
        QString borderName = model->index(i, CapColumn, bordersIdx).data(UR_Name).toString();
        borderList << borderName;
        anchorObjectList << borderName;
        for (int j = 0; j < 4; ++j) {
            QString s = borderName + " " + borderCorners.at(j);
            alignToCornerList << s;
        }
    }
    anchorObjectList << "Image";
    // update textAnchorObjectEditor etc rgh
    for (int i = 0; i < t.size(); ++i) {
        textAlignToCornerObjectEditor[i]->refresh(alignToCornerList);
//        textAnchorObjectEditor[i]->refresh(anchorObjectList);
    }
}

void EmbelProperties::diagnosticStyles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    using namespace winnow_effects;
    qDebug() << "\nStyles\n";
    QMapIterator<QString, QList<Effect>> s(styleMap);
    while (s.hasNext()) {
        s.next();
        int n = s.value().length();
        qDebug() << "Style:" << s.key() << "has" << n << "effects.";
        // effects in style
        for (int i = 0; i < n; ++i) {
            const Effect &ef = s.value().at(i);
            qDebug() << "  effectName   =" << ef.effectName;
            qDebug() << "    Effect index =" << i;
            qDebug() << "    effectType   =" << ef.effectType;
            qDebug() << "    effectOrder  =" << ef.effectOrder;

            switch (ef.effectType) {
            case winnow_effects::blur:
                qDebug() << "      Blur: radius       =" << ef.blur.radius;
                qDebug() << "      Blur: quality      =" << ef.blur.quality;
                qDebug() << "      Blur: transposed   =" << ef.blur.transposed;
                break;
            case winnow_effects::highlight:
                qDebug() << "      Highlight: margin  =" << ef.highlight.margin;
                qDebug() << "      Highlight: r       =" << ef.highlight.r;
                qDebug() << "      Highlight: g       =" << ef.highlight.g;
                qDebug() << "      Highlight: b       =" << ef.highlight.b;
                qDebug() << "      Highlight: a       =" << ef.highlight.a;
                break;
            case winnow_effects::shadow:
                qDebug() << "      Shadow: size       =" << ef.shadow.size;
                qDebug() << "      Shadow: blurRadius =" << ef.shadow.blurRadius;
                qDebug() << "      Shadow: r          =" << ef.shadow.r;
                qDebug() << "      Shadow: g          =" << ef.shadow.g;
                qDebug() << "      Shadow: b          =" << ef.shadow.b;
                qDebug() << "      Shadow: a          =" << ef.shadow.a;
            }
        }
    }
}

void EmbelProperties::diagnosticVectors()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // vectors
    qDebug() << __FUNCTION__ << "BORDER VECTOR";
    for (int i = 0; i < b.length(); ++i) {
        qDebug().noquote()
            << i << " "
            << "name =" << b[i].name
            << "caption =" << b[i].caption
            << "parent =" << b[i].parent
            << "top =" << b[i].top
            << "left =" << b[i].left
            << "right =" << b[i].right
            << "bottom =" << b[i].bottom
            << "tile =" << b[i].tile
            << "color =" << b[i].color
            << "opacity =" << b[i].opacity
            << "style =" << b[i].style
            << "outlineWidth =" << b[i].outlineWidth
            << "outlineColor =" << b[i].outlineColor;
    }
    qDebug() << __FUNCTION__ << "TEXT VECTOR";
    for (int i = 0; i < t.length(); ++i) {
        qDebug().noquote()
            << i << " "
            << "name =" << t[i].name
            << "caption =" << t[i].caption
            << "parent =" << t[i].parent
            << "anchorObject =" << t[i].anchorObject
            << "anchorContainer =" << t[i].anchorContainer
            << "x =" << t[i].x
            << "y =" << t[i].y
            << "alignToCorner =" << t[i].alignToCorner
            << "alignTo_BorderId =" << t[i].alignTo_BorderId
            << "alignTo_CornerId =" << t[i].alignTo_CornerId
            << "anchorPoint =" << t[i].anchorPoint
            << "source =" << t[i].source
            << "text =" << t[i].text
            << "metadataTemplate =" << t[i].metadataTemplate
            << "size =" << t[i].size
            << "font =" << t[i].font
            << "isBold =" << t[i].isBold
            << "isItalic =" << t[i].isItalic
            << "color =" << t[i].color
            << "opacity =" << t[i].opacity
            << "style =" << t[i].style;
    }
}

void EmbelProperties::diagnostic(QModelIndex parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // model
    for(int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx0 = model->index(r, CapColumn, parent);
        QModelIndex idx1 = model->index(r, ValColumn, parent);
        QString p = parent.data(UR_Name).toString();
        QString n = idx0.data(UR_Name).toString();
        QVariant v = idx1.data(Qt::EditRole);
        QString s = idx0.data(UR_Source).toString();
        qDebug() << __FUNCTION__ << p << n << v;
        // iterate children
        if (model->hasChildren(idx0)) {
            diagnostic(idx0);
        }
    }
}

void EmbelProperties::renameSettingKey(QString path, QString from, QString to)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path1 = path + "/" + from;
    QString path2 = path + "/" + to;
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
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    QModelIndex idx = btn->index;
    QString oldName = idx.data().toString();
    QString newName = Utilities::inputText("Rename Style", "Rename style " + oldName, styleList);
    if (newName == "") return;

    // update setting
    renameSettingKey("Embel/Styles", oldName, newName);

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
    }
    QString name = Utilities::inputText("Rename Template",
                                        "Rename template " + templateName,
                                        templateList);
    if (name == "") return;
    renameSettingKey("Embel/Templates", templateName, name);
    readTemplateList();
    templateListEditor->refresh(templateList);
    templateListEditor->setValue(name);
}

void EmbelProperties::setCurrentTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < templateList.length(); ++i) {
        QString t = templateList.at(i);
        bool isCurrent = {t == templateName};
        QString path = "Embel/Templates/" + t + "/isCurrent";
        setting->setValue(path, isCurrent);
        if (isCurrent) templateId = i;
        templatePath = "Embel/Templates/" + templateName + "/";
    }
    qDebug() << __FUNCTION__ << templateId << templateName << templatePath;
}

void EmbelProperties::readTemplateList()
{
/*
Read the template list using QSettings
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
        for (int i = 0; i < setting->childGroups().count(); ++i) {
            if (setting->childGroups().at(i) != "Do not Embellish")
                templateList << setting->childGroups().at(i);
        }
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
}

void EmbelProperties::readTileList()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    tileList.clear();
    tileList << "Do not tile";
    setting->beginGroup("Embel/Tiles");
    tileList << setting->allKeys();
    setting->endGroup();
}

void EmbelProperties::newEmbelTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    templateName = Utilities::inputText("New Template",
                                        "Enter new template name",
                                        templateList);
    templateId = templateList.count();
//    // set all templates except templateName isCurrent = false
//    setCurrentTemplate();
    templateList << templateName;
    templateListEditor->addItem(templateName);
    templateListEditor->setValue(templateName);
    // add the File, Image, Borders, Texts, Rectangles and Graphics items for the template
//    addTemplateItems();
}

void EmbelProperties::newStyle()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // rgh prevent duplicate style names
    QString name = Utilities::inputText("New Style",
                                        "Enter new style name (not created until you add an effect)",
                                        styleList);
    styleName = name;
    styleList << name;
    int n = styleList.length() - 1;
    addStyle(name, n);
    for (int i = 0; i < styleListObjectEditor.length(); ++i) {
        styleListObjectEditor[i]->addItem(name);
    }
    // rgh add for rectangles and graphics
}

void EmbelProperties::invokeFromAction(QAction *embelAction)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (templateName == embelAction->text()) return;
    for (int i = 0; i < templateList.count(); ++i) {
        if (templateList.at(i)  == embelAction->text()) {
            templateListEditor->setValue(templateList.at(i));
        }
    }
}

void EmbelProperties::updateEffectOrder(QStandardItem *styleItem, int row, int swapRow)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // swap the current row with the one above
    QStandardItem *item = styleItem->child(row);
    QStandardItem *swapItem = styleItem->child(swapRow);
    int swapOrder = swapItem->data(UR_SortOrder).toInt();
    int thisOrder = item->data(UR_SortOrder).toInt();
    swapItem->setData(thisOrder, UR_SortOrder);
    item->setData(swapOrder, UR_SortOrder);
    styleItem->sortChildren(Qt::AscendingOrder);

    // update order in settings
    QString style = styleItem->data(Qt::DisplayRole).toString();
    QString stylePath = "Embel/Styles/" + style + "/";
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
    e->build();

    return;
}

void EmbelProperties::moveEffectUp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    QModelIndex idx = btn->index;
    if (!idx.isValid()) return;

    // get current row for this index as it may have been sorted
    QString effectName = btn->name;
    QStandardItem *styleItem = new QStandardItem;
    styleItem = model->itemFromIndex(idx.parent());
    int row;
    for (row = 0; row < styleItem->rowCount(); ++row) {
        QString sortedItemName = styleItem->child(row)->data(Qt::DisplayRole).toString();
        if (sortedItemName == effectName) break;
    }
    if (row == 0) return;

    // swap the current row with the one above
    int swapRow = row - 1;
    updateEffectOrder(styleItem, row, swapRow);

    return;
}

void EmbelProperties::moveEffectDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    QModelIndex idx = btn->index;
    if (!idx.isValid()) return;

    // get current row for this index as it may have been sorted
    QString effectName = btn->name;
    QStandardItem *styleItem = new QStandardItem;
    styleItem = model->itemFromIndex(idx.parent());
    int row;
    for (row = 0; row < styleItem->rowCount(); ++row) {
        QString sortedItemName = styleItem->child(row)->data(Qt::DisplayRole).toString();
        /*
        qDebug() << __FUNCTION__
                 << "row =" << row
                 << "sortedItemName =" << sortedItemName
                 << "effectName =" << effectName;
//                 */
        if (sortedItemName == effectName) break;
    }
    if (row == styleItem->rowCount() - 1) return;

    // swap the current row with the one above
    int swapRow = row + 1;
    updateEffectOrder(styleItem, row, swapRow);

    return;
}

void EmbelProperties::sortEffectList(QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!styleMap.contains(style)) {
        qDebug() << __FUNCTION__ << style << "not in list";
        return;
    }
    std::sort(styleMap[style].begin(), styleMap[style].end(),
              [](winnow_effects::Effect const &l, winnow_effects::Effect const &r) {
              return l.effectOrder < r.effectOrder; });
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
    }

//    if (!propertyDelegate->submitted) return;
//    propertyDelegate->submitted = false;
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString parent = idx.parent().data(UR_Name).toString();
    QString grandparent = idx.parent().parent().data(UR_Name).toString();

    templatePath = "Embel/Templates/" + templateName + "/";
//    QModelIndex index = idx.data(UR_QModelIndex).toModelIndex();
    /*
    qDebug() << __FUNCTION__
             << " =" << templateId
             << "idx.isValid() =" << idx.isValid()
             << "v =" << v
             << "source =" << source
             << "parent =" << parent
             << "grandparent =" << grandparent;
             */

    if (source == "templateList") templateChange(v);
    if (templateId != 0) {
        if (parent == "FileHeader") fileItemChange(v, source);
        if (parent == "ImageHeader") imageItemChange(v, source);
        if (grandparent == "Borders") borderItemChange(idx);
        if (grandparent == "Texts") textItemChange(idx);
        if (grandparent == "Rectangles") rectangleItemChange(v, source, parent);
        if (grandparent == "Graphics") graphicItemChange(v, source, parent);
        if (source == "globalLightDirection") globalLightItemChange(v, source, parent);
        if (parent.left(6) == "Shadow") shadowItemChange(v, source, parent, grandparent);
        if (parent.left(4) == "Blur") blurItemChange(v, source, parent, grandparent);
        if (parent.left(9) == "Highlight") highlightItemChange(v, source, parent, grandparent);
    }

    e->build();
}

void EmbelProperties::templateChange(QVariant v)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__;
    isTemplateChange = true;
    templateName = v.toString();
    // save which template is current
    setCurrentTemplate();
    // update global for ImageView
    if (templateId == 0) G::isEmbellish = false;
    else G::isEmbellish = true;
    // update View > Embellish menu
    emit templateChanged(templateId);
    // clear model except for template name header
    model->removeRows(1, model->rowCount(QModelIndex()) - 1);
    // clear borders, texts, rectangles and graphics
    b.clear();
    t.clear();
    // add styles
    if (templateId > 0) addStyles();
    // add items for this template
    if (templateId > 0) addTemplateItems();
    collapseAll();
    collapseAll();
    expand(model->index(_templates,0));
    expand(model->index(_borders,0));
    expand(model->index(_texts,0));
    expand(model->index(_rectangles,0));
    expand(model->index(_graphics,0));
    isTemplateChange = false;

    mw3->imageView->loadImage(mw3->dm->currentFilePath);
}

void EmbelProperties::fileItemChange(QVariant v, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = templatePath + "File/" + source;
    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "horizontalFit") {
        setting->setValue(path, v.toInt());
        f.horizontalFitPx = v.toInt();
    }

    if (source == "verticalFit") {
        setting->setValue(path, v.toInt());
        f.verticalFitPx = v.toInt();
    }

    if (source == "fileType") {
        setting->setValue(path, v.toString());
        f.fileType = v.toString();
    }

    if (source == "saveMethod") {
        setting->setValue(path, v.toString());
        f.saveMethod = v.toString();
    }

    if (source == "folderPath") {
        setting->setValue(path, v.toString());
        f.folderPath = v.toString();
    }

    if (source == "suffix") {
        setting->setValue(path, v.toString());
        f.suffix = v.toString();
    }

    if (source == "overwriteFiles") {
        setting->setValue(path, v.toBool());
        f.overwriteFiles = v.toBool();
    }
}

void EmbelProperties::imageItemChange(QVariant v, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = templatePath + "Image/" + source;
    qDebug() << __FUNCTION__ << path;

    if (source == "outlineWidth") {
        setting->setValue(path, v.toDouble());
        image.outlineWidth = v.toDouble();
    }

    if (source == "outlineColor") {
        setting->setValue(path, v.toString());
        image.outlineColor = v.toString();
    }

    if (source == "style") {
        setting->setValue(path, v.toString());
        image.style = v.toString();
    }

}

void EmbelProperties::borderItemChange(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString parent = idx.parent().data(UR_Name).toString();
    int index = idx.parent().data(UR_ItemIndex).toInt();
    QString path = templatePath + "Borders/" + parent + "/" + source;
//    diagnostics(idx);

    if (source == "topMargin") {
        setting->setValue(path, v);
        b[index].top = v.toDouble();
    }

    if (source == "leftMargin") {
        setting->setValue(path, v.toDouble());
        b[index].left = v.toDouble();
    }

    if (source == "rightMargin") {
        setting->setValue(path, v.toDouble());
        b[index].right = v.toDouble();
    }

    if (source == "bottomMargin") {
        setting->setValue(path, v.toDouble());
        b[index].bottom = v.toDouble();
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
        b[index].opacity = v.toInt();
    }

    if (source == "outlineWidth") {
        setting->setValue(path, v.toInt());
        b[index].outlineWidth = v.toInt();
    }

    if (source == "outlineColor") {
        setting->setValue(path, v.toString());
        b[index].outlineColor = v.toString();
    }

    if (source == "style") {
        setting->setValue(path, v.toString());
        b[index].style = v.toString();
    }
}

void EmbelProperties::textItemChange(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString parent = idx.parent().data(UR_Name).toString();
    int index = idx.parent().data(UR_ItemIndex).toInt();
    QString path = templatePath + "Texts/" + parent + "/" + source;

    qDebug() << __FUNCTION__
             << "source =" << source
             << "parent =" << parent;

    if (source == "anchorObject") {
        setting->setValue(path, v.toString());
        t[index].anchorObject = v.toString();
        int row = idx.row();
        // also hidden on creation using updateHiddenRows(QModelIndex parent)
        if (v.toString() == "Image")
            setRowHidden(row + 1, idx.parent(), true);
        else
            setRowHidden(row + 1, idx.parent(), false);
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

    if (source == "alignToCorner") {
        setting->setValue(path, v.toString());
        t[index].alignToCorner = v.toString();
        parseAlignToCorner(v.toString(), t[index].alignTo_BorderId, t[index].alignTo_CornerId);
    }

    if (source == "anchorPoint") {
        setting->setValue(path, v.toString());
        t[index].anchorPoint = v.toString();
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
        QString key = v.toString();
        setting->setValue(path, key);
        t[index].metadataTemplate = key;
//        t[index].text = metaString(key);
    }

    if (source == "font") {
        setting->setValue(path, v.toString());
        t[index].font = v.toString();
    }

    if (source == "size") {
        setting->setValue(path, v.toDouble());
        t[index].size = v.toDouble();
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
}

void EmbelProperties::rectangleItemChange(QVariant v, QString source, QString parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

}

void EmbelProperties::graphicItemChange(QVariant v, QString source, QString parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
}

void EmbelProperties::globalLightItemChange(QVariant v, QString source, QString parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = templatePath + source;
    qDebug() << __FUNCTION__ << path << source << v.toInt();
    setting->setValue(path, v.toString());
    globalLightDirection = v.toInt();
}

void EmbelProperties::shadowItemChange(QVariant v, QString source, QString effectName, QString style)
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "size") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].shadow.size = v.toDouble();
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
}

void EmbelProperties::blurItemChange(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "radius") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].blur.radius = v.toDouble();
    }

    if (source == "quality") {
        setting->setValue(path, v.toBool());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].blur.quality = v.toBool();
    }
}

void EmbelProperties::highlightItemChange(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "margin") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.margin = v.toInt();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.a = v.toInt() * 255 / 100;
    }

    if (source == "color") {
        setting->setValue(path, v.toString());
        QColor color(v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.r = color.red();
        styleMap[style][effect].highlight.g = color.green();
        styleMap[style][effect].highlight.b = color.blue();
        styleMap[style][effect].highlight.a = color.alpha();
    }
}

void EmbelProperties::addTemplateHeader()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Templates header (Root)
    i.name = "TemplatesHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = false;
    i.captionText = "Templates";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;

    BarBtn *templateRenameBtn = new BarBtn();
    templateRenameBtn->setIcon(":/images/icon16/delta.png", G::iconOpacity);
    templateRenameBtn->setToolTip("Rename the selected template");
    btns.append(templateRenameBtn);
    connect(templateRenameBtn, &BarBtn::clicked, this, &EmbelProperties::renameCurrentTemplate);
    BarBtn *templateDeleteBtn = new BarBtn();
    templateDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    templateDeleteBtn->setToolTip("Delete the selected template");
    btns.append(templateDeleteBtn);
    connect(templateDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::test2);
    BarBtn *templateNewBtn = new BarBtn();
    templateNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    templateNewBtn->setToolTip("Create a new template");
    connect(templateNewBtn, &BarBtn::clicked, this, &EmbelProperties::newEmbelTemplate);
    btns.append(templateNewBtn);
    addItem(i);
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
    }
    addFile();
    addImage();
    addBorders();
    addTexts();
    addRectangles();
    addGraphics();
}

void EmbelProperties::addFile()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // FILE File header (Root)
    i.name = "FileHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "File";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);
    QModelIndex parIdx = capIdx;

/*    // FILES Fit strategy
    i.name = "fitList";
    i.parentName = "FileHeader";
    i.captionText = "Select fit strategy";
    i.tooltip = "Select strategy to fit to dimensions.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "fitList";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Long dimension" << "Horizontal" << "Vertical";
    addItem(i);*/

    // FILES Horizontal fit
    i.name = "horizontalFit";
    i.parIdx = parIdx;
    i.parentName = "FileHeader";
    i.captionText = "Fit horizontal";
    i.tooltip = "The number of pixels in the horizontal axis including the borders";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "horizontalFit";
    if (setting->contains(templatePath + "File/" + i.key))
        i.value = setting->value(templatePath + "File/" + i.key);
    else i.value = 500;
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 10000;
    i.fixedWidth = 50;
    f.horizontalFitPx = i.value.toInt();
    addItem(i);

    // FILES Vertical fit
    i.name = "verticalFit";
    i.parIdx = parIdx;
    i.parentName = "FileHeader";
    i.captionText = "Fit vertical";
    i.tooltip = "The number of pixels in the vertical axis including the borders";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "verticalFit";
    if (setting->contains(templatePath + "File/" + i.key))
        i.value = setting->value(templatePath + "File/" + i.key);
    else i.value = 500;
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 10000;
    i.fixedWidth = 50;
    f.verticalFitPx = i.value.toInt();
    addItem(i);

    // FILES File type
    i.name = "fileType";
    i.parIdx = parIdx;
    i.parentName = "FileHeader";
    i.captionText = "File type";
    i.tooltip = "Select file type.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "fileType";
    i.value = setting->value(templatePath + "File/" + i.key);
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Tiff" << "Jpg" << "Png";
    f.fileType = i.value.toString();
    addItem(i);

    // FILES Save method
    i.name = "saveMethod";
    i.parIdx = parIdx;
    i.parentName = "FileHeader";
    i.captionText = "Save method";
    i.tooltip = "Select where to save the file.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "saveMethod";
    i.value = setting->value(templatePath + "File/" + i.key);
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Same folder" << "Subfolder" << "Template";
    f.saveMethod = i.value.toString();
    addItem(i);

    // FILES Folder name
    i.name = "folderPath";
    i.parIdx = parIdx;
    i.parentName = "FileHeader";
    i.captionText = "Folder path";
    i.tooltip = "Enter the full folder path, the folder name of a subfolder or the"""
                "template to use.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "folderPath";
    i.value = setting->value(templatePath + "File/" + i.key);
    i.delegateType = DT_LineEdit;
    i.type = "string";
    f.folderPath = i.value.toString();
    addItem(i);

    // FILES Suffix name
    i.name = "suffix";
    i.parIdx = parIdx;
    i.parentName = "FileHeader";
    i.captionText = "Suffix";
    i.tooltip = "Suffix to add to file names.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "suffix";
    i.value = setting->value(templatePath + "File/" + i.key);
    i.delegateType = DT_LineEdit;
    i.type = "string";
    f.suffix = i.value.toString();
    addItem(i);

    // FILES Overwrite existing files
    i.name = "overwriteFiles";
    i.parIdx = parIdx;
    i.parentName = "FileHeader";
    i.captionText = "Overwrite";
    i.tooltip = "Overwrite existing files.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "overwriteFiles";
    i.value = setting->value(templatePath + "File/" + i.key);
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    f.overwriteFiles = i.value.toBool();
    addItem(i);
}

void EmbelProperties::addImage()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // IMAGE Image header (Root)
    i.name = "ImageHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "Image";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);
    QModelIndex parIdx = capIdx;

    QString settingRootPath = templatePath + "Image/";

    i.name = "outlineWidth";
    i.parIdx = parIdx;
    i.parentName = "ImageHeader";
    i.captionText = "Outine width";
    i.tooltip = "This is the outline for the image (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "outlineWidth";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else i.value = 0.0;
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.outlineWidth = i.value.toDouble();
    addItem(i);

    i.name = "outlineColor";
    i.parIdx = parIdx;
    i.parentName = "ImageHeader";
    i.captionText = "Outline color";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "outlineColor";
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else i.value = "#3f5f53";
    i.delegateType = DT_Color;
    i.type = "QString";
    border.outlineColor = i.value.toString();
    addItem(i);

    // IMAGE style
    i.name = "imageStyle";
    i.parIdx = parIdx;
    i.parentName = "ImageHeader";
    i.captionText = "Style";
    i.tooltip = "Select style to apply to the image ie a shadow.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "imageStyle";
    i.key = "outlineColor";
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else i.value = "";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "None" << "Emboss and dropshadow" << "Dropshadow 2";
    addItem(i);
}

void EmbelProperties::addBorders()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // BORDER header (Root)
    i.name = "Borders";
    i.parentName = "???";
    i.isHeader = true;
    i.isDecoration = false;
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
    connect(borderNewBtn, &BarBtn::clicked, this, &EmbelProperties::newBorder);
    addItem(i);
    bordersIdx = capIdx;

    QString path = templatePath + "/Borders";
//    qDebug() << __FUNCTION__ << path;
    setting->beginGroup(path);
    int count = setting->childGroups().size();
    setting->endGroup();
    for (int i = 0; i < count; ++i) newBorder();
}

void EmbelProperties::addTexts()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Texts header (Root)
    i.name = "Texts";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = false;
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

void EmbelProperties::addRectangles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__;
    // Rectangles header (Root)
    i.name = "Rectangles";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = false;
    i.captionText = "Rectangles";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
//    rectangleDeleteBtn = new BarBtn();
//    rectangleDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
//    rectangleDeleteBtn->setToolTip("Delete the open rectangle");
//    btns.append(rectangleDeleteBtn);
    BarBtn *rectangleNewBtn = new BarBtn();
    rectangleNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    rectangleNewBtn->setToolTip("Create a new rectangle");
    btns.append(rectangleNewBtn);
    addItem(i);
    rectanglesIdx = capIdx;
//    rectangleDeleteBtn->index = capIdx;

//    QString path = templatePath + "/Rectangles";
//    setting->beginGroup(path);
//    int count = setting->childGroups().size();
//    qDebug() << __FUNCTION__ << path << "count =" << count;
//    setting->endGroup();
//    for (int i = 0; i < count; ++i) newRectangle();
}

void EmbelProperties::addGraphics()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__;
    // Graphics header (Root)
    i.name = "Graphics";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = false;
    i.captionText = "Graphics";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
//    graphicDeleteBtn = new BarBtn();
//    graphicDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
//    graphicDeleteBtn->setToolTip("Delete the open graphic");
//    btns.append(graphicDeleteBtn);
    BarBtn *graphicNewBtn = new BarBtn();
    graphicNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    graphicNewBtn->setToolTip("Create a new graphic");
    btns.append(graphicNewBtn);
    addItem(i);
    graphicsIdx = capIdx;
//    graphicDeleteBtn->index = capIdx;

//    QString path = templatePath + "/Graphics";
//    setting->beginGroup(path);
//    int count = setting->childGroups().size();
//    qDebug() << __FUNCTION__ << path << "count =" << count;
//    setting->endGroup();
//    for (int i = 0; i < count; ++i) newGraphic();
}

void EmbelProperties::addStyles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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

    // STYLE global light direction
    i.name = "globalLightDirection";
    i.parIdx = stylesIdx;
    i.parentName = "Styles";
    i.captionText = "Light direction";
    i.tooltip = "Light source direction (0 - 360 degrees)";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "globalLightDirection";
    if (setting->contains(templatePath + i.key))
        i.value = setting->value(templatePath + i.key);
    else i.value = 225;
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 360;
    i.fixedWidth = 50;
    globalLightDirection = i.value.toInt();
    addItem(i);

    QString path = "Embel/Styles";
    setting->beginGroup(path);
    styleList = setting->childGroups();
    int count = setting->childGroups().size();
    setting->endGroup();
//    qDebug() << __FUNCTION__ << stylesList;
    for (int i = 0; i < count; ++i) addStyle(styleList.at(i), i);
}

void EmbelProperties::addStyle(QString name, int n)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // add the style header
    styleName = name;
    styleId = n;
//    qDebug() << __FUNCTION__ << styleName << name << n;
    QString settingRootPath = "Embel/Styles/" + styleName;
    i.isHeader = true;
    i.isDecoration = true;
    i.itemIndex = n;
    i.name = styleName;
    i.parIdx = stylesIdx;
    i.parentName = "Styles";
    i.path = "Embel/Styles/" + styleName;
    i.captionText = styleName;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *styleRenameBtn = new BarBtn();
    styleRenameBtn->setObjectName(styleName);
    styleRenameBtn->setIcon(":/images/icon16/delta.png", G::iconOpacity);
    styleRenameBtn->setToolTip("Rename this style");
    btns.append(styleRenameBtn);
    connect(styleRenameBtn, &BarBtn::clicked, this, &EmbelProperties::renameCurrentStyle);
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
    styleDeleteBtn->index = capIdx;
    styleRenameBtn->index = capIdx;
    effectNewBtn->index = capIdx;

    // item reqd to sort effects after they have been read from settings
    QStandardItem *styleItem = new QStandardItem;
    styleItem = model->itemFromIndex(capIdx);

    // read the effects from settings
    setting->beginGroup(settingRootPath);
    QStringList groups = setting->childGroups();
    setting->endGroup();
    effects.clear();
    if (groups.contains("Blur")) addBlurEffect(styleIdx);
    if (groups.contains("Highlight")) addHighlightEffect(styleIdx);
    if (groups.contains("Shadow")) addShadowEffect(styleIdx);

    // sort the effects
    styleItem->sortChildren(Qt::AscendingOrder);

    // add to local map used in embel
//    styleMap[styleName] = effects;
    sortEffectList(styleName);
}

void EmbelProperties::addEffectHeaderButtons()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    effectUpBtn->index = capIdx;
    effectDownBtn->index = capIdx;
    effectDeleteBtn->index = capIdx;
}

void EmbelProperties::addBlurEffect(QModelIndex parIdx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::blur;
    QString effectName = uniqueEffectName(parentName, winnow_effects::blur, "Blur");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Styles/" + parentName + "/" + effectName;
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
    addEffectHeaderButtons();
    parIdx = capIdx;

    // blur radius
    i.name = "radius";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Radius";
    i.tooltip = "The amount of transition from full shadow to background.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "radius";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    effect.blur.radius = i.value.toDouble();
    addItem(i);

    // blur quality
    i.name = "quality";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Quality";
    i.tooltip = "Render best quality instead of best performance.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "quality";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = false;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    effect.blur.quality = i.value.toBool();
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);
}

void EmbelProperties::addHighlightEffect(QModelIndex parIdx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::highlight;
    QString effectName = uniqueEffectName(parentName, winnow_effects::highlight, "Highlight");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Styles/" + parentName + "/" + effectName;
    i.captionText = effectName;
    i.tooltip = "";
    i.hasValue = true;
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
    addEffectHeaderButtons();
//    addItem(i);
    parIdx = capIdx;

    // highlight size
    i.name = "margin";  // x and y offsets are equal
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Margin";
    i.tooltip = "The amount of highlighted margin surrounding the item.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "margin";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 50;
    i.fixedWidth = 50;
    effect.highlight.margin = i.value.toInt();
    addItem(i);

    // highlight opacity
    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the highlight.";
    i.isIndent = false;
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
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
//    i.fixedWidth = 50;
    effect.highlight.a = i.value.toInt() * 255 / 100;
    addItem(i);

    // highlight color
    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Color";
    i.tooltip = "Highlight color.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with white
        i.value = "#FFFFFF";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    QColor color(i.value.toString());
    effect.highlight.r = color.red();
    effect.highlight.g = color.green();
    effect.highlight.b = color.blue();
//    effect.highlight.a = color.alpha();
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);
}

void EmbelProperties::addShadowEffect(QModelIndex parIdx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // styleName = parent
    QString parentName = parIdx.data(UR_Name).toString();
    styleName = parentName;
    winnow_effects::Effect effect;
    effect.effectType = winnow_effects::shadow;
    QString effectName = uniqueEffectName(parentName, winnow_effects::shadow, "Shadow");
    effect.effectName = effectName;

    QString settingRootPath = "Embel/Styles/" + parentName + "/" + effectName + "/";

    // subheader for this effect
    i.isHeader = true;
    i.isDecoration = true;
    i.name = effectName;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.path = "Embel/Styles/" + parentName + "/" + effectName;
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
    addEffectHeaderButtons();
//    addItem(i);
    parIdx = capIdx;

    // shadow size
    i.name = "size";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Size";
    i.tooltip = "The length of the shadow.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "size";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 30;
//    i.fixedWidth = 50;
    effect.shadow.size = i.value.toInt();
    addItem(i);

    // shadow blur
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Blur";
    i.tooltip = "The blur of the drop shadow.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "blur";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 100;
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
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with gray
        i.value = "#404040";
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

//    effects.append(effect);
    styleMap[styleName].append(effect);
}

void EmbelProperties::newBorder()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row = model->rowCount(bordersIdx);
    addBorder(row);
    updateBorderLists();
    if (G::isInitializing || isTemplateChange) return;
    QModelIndex idx = model->index(row, CapColumn, bordersIdx);
    selectionModel()->clear();
    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    e->build();
}

void EmbelProperties::newText()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row = model->rowCount(textsIdx);
    addText(row);
    if (G::isInitializing || isTemplateChange) return;
    QModelIndex idx = model->index(row, CapColumn, textsIdx);
    selectionModel()->clear();
    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    e->build();
}

void EmbelProperties::deleteItem()
{
/*
The tree header rows can contain delete buttons.  All such delete operations are
processed here.  The item (row in model) has to be removed from the data model,
from local vectors and from QSettings.  Also, if it is referred to elsewhere, such
as a style in a stylelist for a text, then the lists need to be updated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    QModelIndex idx;
    /* If an effect is being deleted its index cannot be determined by btn->index as it may
       have been sorted.  Instead the index is determined by searching for the name and
       parName */
    if (btn->type == "effect") {
        getIndexFromNameAndParent(btn->name, btn->parName);
        idx = foundIdx;
    }
    else {
        idx = btn->index;
    }
    if (!idx.isValid()) return;

    int row = idx.row();
    QString name = idx.data(UR_Name).toString();
    QModelIndex parIdx = idx.parent();
    QString parName = parIdx.data(UR_Name).toString();
    QString path = idx.data(UR_Path).toString();
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

    // remove from local vectors
    if (parName == "Borders") {
        b.remove(row);
        updateBorderLists();
    }
    if (parName == "Texts") t.remove(row);

    // remove from datamodel
    model->removeRow(idx.row(), idx.parent());

    // remove from setting
    setting->remove(path);

    /* rename subsequent category items ie text2, text3 ... in setting, model and vectors
       This is not required for styles and effects as they are not numbered automatically */
    QStringList templateItems;
    templateItems << "Borders" << "Texts" << "Rectangles" << "Graphics";
    if (templateItems.contains(parName)) {
        QString itemBase = parName.left(parName.length() - 1);
        for (int i = row; i < model->rowCount(parIdx); ++i) {
            QString parPath = templatePath + parName;
            QString oldName = model->index(i,CapColumn,parIdx).data().toString();
            QString newName = itemBase + QString::number(i + 1);
            qDebug() << __FUNCTION__ << i << path << oldName << newName;
            // update setting
            renameSettingKey(parPath, oldName, newName);
            // update model
            model->setData(model->index(i,CapColumn,parIdx), newName);
            // update local struct
            if (parName == "Borders") b[i].name = newName;
            if (parName == "Texts") t[i].name = newName;
        }
    }

    // update references to style
    if (parName == "Styles") {
        for (int i = 0; i < styleListObjectEditor.length(); ++i) {
            styleListObjectEditor[i]->removeItem(name);
        }
        // rgh add for rectangles, borders and graphics
    }

    // update styleMap if a style was deleted
    if (parName == "Styles") {
        styleMap.remove(name);
    }

    // update styleMap if an effect was deleted
    if (btn->type == "effect") {
        // find the effect
        int i;
        for (i = 0; i < styleMap[parName].length(); ++i) {
            if (styleMap[parName][i].effectName == name) break;
        }
        styleMap[parName].removeAt(i);
    }

    // select another item
    int rowCount = model->rowCount(idx.parent());
    if (rowCount > 0) {
        QModelIndex nextIdx = model->index(row, CapColumn, parIdx);
        if (!nextIdx.isValid()) nextIdx = model->index(rowCount - 1, CapColumn, parIdx);
        selectionModel()->select(nextIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

    // refresh the graphicsScene
    e->build();
}

QString EmbelProperties::uniqueEffectName(QString styleName, int effectType, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    using namespace winnow_effects;
    // effects in style
    int effectCount = 0;
    for (int i = 0; i < styleMap[styleName].length(); ++i) {
        if (effectType == styleMap[styleName].at(i).effectType) effectCount++;
    }
    if (effectCount > 0) effectName += QString::number(effectCount);
    return effectName;
}

int EmbelProperties::effectIndex(QString style, QString effectName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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

/*void EmbelProperties::mouseDoubleClickEvent(QMouseEvent *event)
{
    qDebug() << __FUNCTION__;
    // ignore
}*/

void EmbelProperties::mousePressEvent(QMouseEvent *event)
{
/*
Set the current index and expand/collapse when click anywhere on a row that has children.
Do not pass on to QTreeView as this will enable QTreeView expanding and collapsing when the
decoration is clicked.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // ignore right mouse clicks for context menu
    if (event->button() == Qt::RightButton) return;

    getIndexFromNameAndParent("Highlight", "zenblur");
    QModelIndex btnIdx = foundIdx;
    QModelIndex idx = indexAt(event->pos());
//    qDebug() << __FUNCTION__ << "idx   " << idx << idx.data().toString();
//    qDebug() << __FUNCTION__ << "btnIdx" << btnIdx << btnIdx.data().toString();

//    qDebug() << " =" << event->globalPos();
//    qDebug() << " =" << event->screenPos();
//    qDebug() << " =" << event->localPos();
//    qDebug() << " =" << event->pos();
//    qDebug() << " =" << this->window()->mapFromGlobal(event->globalPos());
//    qDebug() << " =" << this->geometry();
//    qDebug() << " =" << this->window()->pos();


    treeChange(indexAt(event->pos()));
}

void EmbelProperties::treeChange(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__ << idx << idx.isValid();
    if (!idx.isValid()) return;
    if (idx.column() != 0) idx = model->index(idx.row(), CapColumn, idx.parent());
    bool hasChildren = model->hasChildren(idx);
    bool hasGrandChildren = model->hasChildren(model->index(0,0,idx));
    bool templateHdr = (idx == model->index(0,0));
    QString selName = idx.data(UR_Name).toString();
    QString parName = idx.parent().data(UR_Name).toString();
    bool isStyle = styleList.contains(selName) || effectList.contains(selName);
    /*
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
            collapse(model->index(_rectangles,0));
            collapse(model->index(_graphics,0));
//        }
        expand(model->index(_templates,0));
        expand(model->index(_borders,0));
        expand(model->index(_texts,0));
        expand(model->index(_rectangles,0));
        expand(model->index(_graphics,0));
//        expand(model->index(_styles,0));
        wasExpanded ? collapse(idx) : expand(idx);
    }
}

void EmbelProperties::selectionChanged(const QItemSelection &selection,
                                      const QItemSelection &prevSelection)
{
/*
Check if the new selection is legal (a member of borders, texts, rectangles, graphics or
file or image).  If not, if there was a previous selection, revert to that.  Hide all delete
buttons except in the selected category.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (selection.isEmpty()) return;
    QModelIndex idx = selection.indexes().first();
    QModelIndex parIdx = idx.parent();
    QString selName = idx.data(UR_Name).toString();
    if (!okToSelect(idx, selName)) {
        selectionModel()->clear();
        if (!okToSelect(parIdx, selName)) return;
        selectionModel()->select(parIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        idx = parIdx;
    }
}

bool EmbelProperties::okToSelect(QModelIndex idx, QString selName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__ << "selName =" << selName;
    QModelIndex parIdx = idx.parent();
    if (!parIdx.isValid()) return false;
    if (parIdx == model->index(_borders,0)) return true;
    if (parIdx == model->index(_texts,0)) return true;
    if (parIdx == model->index(_rectangles,0)) return true;
    if (parIdx == model->index(_graphics,0)) return true;
    if (parIdx == model->index(_styles,0)) return true;
    if (styleList.contains(selName)) return true;
    if (effectList.contains(selName)) return true;
//    qDebug() << __FUNCTION__ << "false for" << selName << effectList;
    return false;
}

QString EmbelProperties::metaString(QString key)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString tokenString = mw3->infoString->infoTemplates[key];
    QString path = mw3->imageView->currentImagePath;
    QModelIndex curIdx = mw3->thumbView->currentIndex();
    return mw3->infoString->parseTokenString(tokenString, path, curIdx);
}

void EmbelProperties::diagnostics(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

}

void EmbelProperties::parseAlignToCorner(QString alignTo, int &iBorder, int &iCorner)
{
/*
The string alignTo example: "Border2 BottomLeft".  We need to know the number of the border
and the corner.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    iBorder = alignTo.mid(6, 1).toInt() - 1;
    QString s = alignTo.right(alignTo.length() - 8);
    if (s == "TopLeft") iCorner = 0;
    if (s == "TopRight") iCorner = 1;
    if (s == "BottomLeft") iCorner = 2;
    if (s == "BottomRight") iCorner = 3;
//    qDebug() << __FUNCTION__ << alignTo << s << iBorder << iCorner;
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
    isSolo = soloAction->isChecked();
}

void EmbelProperties::test1()
{
//    e->test();

}

void EmbelProperties::test2()
{
    diagnosticStyles();
}

void EmbelProperties::coordHelp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    QWidget *helpDoc = new QWidget;
//    Ui::embelCoord ui;
//    ui.setupUi(helpDoc);
//    helpDoc->show();

}

void EmbelProperties::addBorder(int count)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Name (used as node in settings and treeview)
    QString borderName = "Border" + QString::number(count + 1);
    border.name = borderName;
    QString settingRootPath = templatePath + "Borders/" + borderName + "/";

    // subheader for this border
    i.isHeader = true;
    i.isDecoration = true;
    i.itemIndex = count;
    i.name = borderName;
    i.parIdx = bordersIdx;
    i.parentName = "Borders";
    i.path = templatePath + "Borders/" + borderName;
    i.captionText = borderName;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    borderDeleteBtn = new BarBtn();
    borderDeleteBtn->setObjectName(borderName);
    borderDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    borderDeleteBtn->setToolTip("Delete this border");
    btns.append(borderDeleteBtn);
    connect(borderDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    addItem(i);
    QModelIndex parIdx = capIdx;
    borderDeleteBtn->index = capIdx;

    i.name = "topMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Top %";
    i.tooltip = "This is the margin for the top part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "topMargin";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.top = i.value.toDouble();
    addItem(i);

    i.name = "leftMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Left %";
    i.tooltip = "This is the margin for the left part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "leftMargin";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.left = i.value.toDouble();
    addItem(i);

    i.name = "rightMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Right %";
    i.tooltip = "This is the margin for the right part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "rightMargin";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.right = i.value.toDouble();
    addItem(i);

    i.name = "bottomMargin";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Bottom %";
    i.tooltip = "This is the margin for the bottom part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "bottomMargin";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.bottom = i.value.toDouble();
    addItem(i);

    i.name = "tile";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Tile";
    i.tooltip = "Select a tile that will be used to fill the border area.";
    i.isIndent = false;
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
    addItem(i);

    i.name = "color";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Border color";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        // start with a random color
        quint32 x = QRandomGenerator::global()->generate() * 16777215;
        i.value = "#" + QString::number(x, 16);
        qDebug() << __FUNCTION__ << i.value;
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
    i.isIndent = false;
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

    i.name = "outlineWidth";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Outine width";
    i.tooltip = "This is the outline for the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "outlineWidth";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0.02;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.outlineWidth = i.value.toDouble();
    addItem(i);

    i.name = "outlineColor";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Outline color";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "outlineColor";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = "#FFFFFF";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Color;
    i.type = "QString";
    border.outlineColor = i.value.toString();
    addItem(i);

    i.name = "style";
    i.parIdx = parIdx;
    i.parentName = borderName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the border area.";
    i.isIndent = false;
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
}

void EmbelProperties::addText(int count)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Name (used as node in settings and treeview)
    QString textName = "Text" + QString::number(count + 1);
    QString settingRootPath = templatePath + "Texts/" + textName + "/";

    // subheader for this border
    i.isHeader = true;
    i.isDecoration = true;
    i.itemIndex = count;
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
    btns.append(textDeleteBtn);
    connect(textDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    addItem(i);
    QModelIndex parIdx = capIdx;
    textDeleteBtn->index = capIdx;

    // get index to use as parent when update tree depending on source
    text.name = textName;

    i.name = "anchorObject";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Put text in";
    i.tooltip = "Select a border or image to contain the text.";
    i.isIndent = false;
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
    text.anchorObject = i.value.toString();
    textAnchorObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    i.name = "anchorContainer";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Border area";
    i.tooltip = "Select a border area to contain the text.";
    i.isIndent = false;
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

    // update tree based on anchorObject (Image does not have border areas)
    if (text.anchorObject == "Image") {
        model->setData(capIdx, true, UR_isHidden);
        model->setData(valIdx, true, UR_isHidden);
    }

    i.name = "anchorPoint";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Anchor point";
    i.tooltip = "Select a text box anchor point.";
    i.isIndent = false;
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
    textAnchorObjectEditor[count] = static_cast<ComboBoxEditor*>(addItem(i));

    i.name = "alignToCorner";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Align to corner";
    i.tooltip = "Select a border corner to align to the anchor point.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "alignToCorner";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Do not align";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << alignToCornerList;
    text.alignToCorner = i.value.toString();
    parseAlignToCorner(text.alignToCorner,text.alignTo_BorderId, text.alignTo_CornerId);
    textAlignToCornerObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));
//    textAlignToCornerObjectEditor[count] = static_cast<ComboBoxEditor*>(addItem(i));

    i.name = "x";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "x coordinate";
    i.tooltip = "The x coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = false;
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
    i.max = 100;
    i.fixedWidth = 50;
    text.x = i.value.toDouble();
    addItem(i);

    i.name = "y";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "y coordinate";
    i.tooltip = "The y coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = false;
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
    i.max = 100;
    i.fixedWidth = 50;
    text.y = i.value.toDouble();
    addItem(i);

    i.name = "rotation";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Rotation";
    i.tooltip = "The rotation (degrees) of the text (+- 0-360)";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "rotation";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = -360;
    i.max = 360;
    i.fixedWidth = 50;
    text.rotation = i.value.toDouble();
    addItem(i);

    i.name = "source";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Source";
    i.tooltip = "Select what is to be used: the text or text generated by a metadata template.";
    i.isIndent = false;
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
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "text";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = "Text #" + QString::number(count + 1);
//        qDebug() << __FUNCTION__ << i.value;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_LineEdit;
    i.type = "QString";
    border.color = i.value.toString();
    text.text = i.value.toString();
    addItem(i);

    // update tree based on source
    if (text.source == "Metadata template") {
        model->setData(capIdx, true, UR_isHidden);
        model->setData(valIdx, true, UR_isHidden);
    }

    i.name = "metadataTemplate";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Metadata template";
    i.tooltip = "Select a metadata template.  A template can include multiple metadata \n"
                "fields and user input text.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "metadataTemplate";
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << metadataTemplatesList;
    QString key = i.value.toString();
    text.metadataTemplate = key;
//    text.text = metaString(key);
    addItem(i);

    // update tree based on source
    if (text.source == "Text") model->setData(capIdx, true, UR_isHidden);
    if (text.source == "Text") model->setData(valIdx, true, UR_isHidden);

    i.name = "size";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Size";
    i.tooltip = "Enter a percentage of the long side pixels to set a font size in pixels.";
    i.isIndent = false;
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
    i.max = 100;
    i.fixedWidth = 50;
    text.size = i.value.toDouble();
    addItem(i);

    i.name = "font";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Font";
    i.tooltip = "Select a font.";
    i.isIndent = false;
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
    i.isIndent = false;
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
    i.isIndent = false;
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
    i.isIndent = false;
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
    i.isIndent = false;
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
    text.opacity = i.value.toInt();
    addItem(i);

    i.name = "style";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the text.";
    i.isIndent = false;
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
    text.style = i.value.toString();
    styleListObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    // add the text info to the vector of texts t
    t << text;
}
