#include "embelproperties.h"
#include "Main/mainwindow.h"

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
Embel. The information is read from QSettings by addTemplateItems, addBorders ... As each item
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
    isExpandRecursively = false;
    collapseAll();
    expand(model->index(0,0,QModelIndex()));
    setIndentation(10);
    setAlternatingRowColors(false);
    setMouseTracking(false);
    propertyDelegate->isAlternatingRows = false;
    resizeColumns("====captions column====",
                  "====values column====");
    qDebug() << __FUNCTION__ << columnWidth(0) << columnWidth(1);
    setStyleSheet(G::css);

    // Assign blank png for treeview decorations so do not need to hide in delegate paint override
    setStyleSheet
    (
        "QTreeView {"
            "selection-background-color: transparent;"
        "}"
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings {"
            "border-image: none;"
            "image: url(:/images/branch-blank.png);"
        "}"

        "QTreeView::branch:open:has-children:!has-siblings,"
        "QTreeView::branch:open:has-children:has-siblings  {"
            "border-image: none;"
            "image: url(:/images/branch-blank.png);"
        "}"
    );

    QModelIndex root = model->invisibleRootItem()->index();

    initialize();
    // get the list of templates and which one was last active
    readTemplateList();
    // add the template header, which then selects the last active template
    addTemplateHeader();
    // add styles, which are independent of templates, and hide if templateId == 0
    addStyles();
    if (templateId > 0) setRowHidden(1, root, false);
    else setRowHidden(1, root, true);
    // add the File, Image, Borders, Texts, Rectangles and Graphics items for the template
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
    anchorPoints << "Top Left" << "Top Center" << "Top Right"
                 << "Middle Left" << "Middle Center" << "Middle Right"
                 << "Bottom Left" << "Bottom Center" << "Bottom Right";
    anchorObjectList << "Image";
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
               << "Brighten" << "Edges" << "Highlight" << "Stroke";

    shadowEffectAction = new QAction(tr("Shadow"), this);
    shadowEffectAction->setObjectName("shadowEffectAction");
    connect(shadowEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    bevelEffectAction = new QAction(tr("Bevel"), this);
    bevelEffectAction->setObjectName("bevelEffectAction");
    connect(bevelEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    embossEffectAction = new QAction(tr("Emboss"), this);
    embossEffectAction->setObjectName("embossEffectAction");
    connect(embossEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    blurEffectAction = new QAction(tr("Blur"), this);
    blurEffectAction->setObjectName("blurEffectAction");
    connect(blurEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    sharpenEffectAction = new QAction(tr("Sharpen"), this);
    sharpenEffectAction->setObjectName("sharpenEffectAction");
    connect(sharpenEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    brightenEffectAction = new QAction(tr("Brighten"), this);
    brightenEffectAction->setObjectName("brightenEffectAction");
    connect(brightenEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    edgeEffectAction = new QAction(tr("Edge"), this);
    edgeEffectAction->setObjectName("edgeEffectAction");
    connect(edgeEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    highlightEffectAction = new QAction(tr("Highlight"), this);
    highlightEffectAction->setObjectName("highlightEffectAction");
    connect(highlightEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    strokeEffectAction = new QAction(tr("Stroke"), this);
    strokeEffectAction->setObjectName("strokeEffectAction");
    connect(strokeEffectAction, &QAction::triggered, this, &EmbelProperties::newEffectActionClicked);

    effectMenu = new QMenu(this);
    effectMenu->addAction(shadowEffectAction);
    effectMenu->addAction(bevelEffectAction);
    effectMenu->addAction(embossEffectAction);
    effectMenu->addAction(blurEffectAction);
    effectMenu->addAction(sharpenEffectAction);
    effectMenu->addAction(brightenEffectAction);
    effectMenu->addAction(edgeEffectAction);
    effectMenu->addAction(highlightEffectAction);
    effectMenu->addAction(strokeEffectAction);

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

    /*
    soloAction = new QAction(tr("Solo"), this);
    soloAction->setShortcutVisibleInContextMenu(true);
    soloAction->setShortcut(QKeySequence("Ctrl+*"));
    soloAction->setCheckable(true);
    soloAction->setVisible(false);
    addAction(soloAction);
    soloAction->setChecked(true);
    connect(soloAction, &QAction::triggered, this, &EmbelProperties::solo);
//    solo();
//        */

    separatorAction0 = new QAction(this);
    separatorAction0->setSeparator(true);
    addAction(separatorAction0);

    renameAction = new QAction(tr("Rename"), this);
    addAction(renameAction);
    connect(renameAction, &QAction::triggered, this, &EmbelProperties::rename);

    copyAction = new QAction(tr("Copy"), this);
    addAction(copyAction);
    connect(copyAction, &QAction::triggered, this, &EmbelProperties::copyTemplate);

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
    effectParentIdx = btn->index;
    effectMenu->exec(QCursor::pos());
}

void EmbelProperties::newEffectActionClicked()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString effect = (static_cast<QAction*>(sender()))->text();
    qDebug() << __FUNCTION__ << "effect =" << effect;
    if (effect == "Blur") addBlurEffect(effectParentIdx);
    if (effect == "Sharpen") addSharpenEffect(effectParentIdx);
    if (effect == "Highlight") addHighlightEffect(effectParentIdx);
    if (effect == "Shadow") addShadowEffect(effectParentIdx);
    if (effect == "Brighten") addBrightenEffect(effectParentIdx);
    if (effect == "Emboss") addEmbossEffect(effectParentIdx);
}

void EmbelProperties::updateBorderLists(int row)
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
    int borderCount = model->rowCount(bordersIdx);
    for (int i = 0; i < borderCount; ++i) {
//        QString borderName = model->index(i, CapColumn, bordersIdx).data(UR_Name).toString();
        QString borderName = "Border" + QString::number(i + 1);
        borderList << borderName;
        anchorObjectList << borderName;
    }
    anchorObjectList << "Image";

    // update texts and graphics that are anchored on a border
    updateAnchorObjects(row);

}

void EmbelProperties::updateAnchorObjects(int deletedRow)
{
/*
When borders are created or deleted the border and anchor lists are updated.  This function
iterates through all texts and graphics to refresh the anchorObjectList and reassign the
anchor object if it was a border, since the border list will have changed.

Note that refreshing the anchorObjectList or each text and graphic removes the previous
selected value, so even if the anchor object remains the same it still must be reassigned.

Also note that when the setValue function of the textAnchorObjectEditor and
graphicAnchorObjectEditor trigger itemChange which in turn updates settings and local vectors.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int borderCount = model->rowCount(bordersIdx);

    // update texts anchor lists
    for (int i = 0; i < t.size(); ++i) {
        QString oldAnchoName = textAnchorObjectEditor.at(i)->value();
        // rows are zero based (o, 1, ...), border names ie Border2 are 1 based (1, 2, ...)
        int oldAnchorRow = oldAnchoName.right(oldAnchoName.length() - 6).toInt() - 1;
        textAnchorObjectEditor.at(i)->refresh(anchorObjectList);
        // refreshing anchorObjectList removes old value for the text - reassign anchor object
        /*
        qDebug() << __FUNCTION__
                 << "TEXT ITEM #" << i
                 << "oldAnchoName =" << oldAnchoName
                 << "oldAnchorRow =" << oldAnchorRow
                 << "border row deleted =" << deletedRow
                 << "anchorObjectList =" << anchorObjectList
                    ;
//                  */
        // if all borders deleted then anchor to the Image
        if (borderCount == 0 || oldAnchoName == "Image")
            textAnchorObjectEditor.at(i)->setValue("Image");
        // oldAnchorRow okay if deleted row is greater than anchor row (oldAnchorRow)
        else if (deletedRow > oldAnchorRow)
            textAnchorObjectEditor.at(i)->setValue(oldAnchoName);
        // deleted row <= oldAnchorRow then newRow = oldAnchorRow - 1
        else if (deletedRow <= oldAnchorRow) {
            QString newStr = "Border" + QString::number(oldAnchorRow);
            textAnchorObjectEditor.at(i)->setValue(newStr);
        }
    }

    // update graphics anchor lists
    for (int i = 0; i < t.size(); ++i) {
        QString oldAnchoName = graphicAnchorObjectEditor.at(i)->value();
        // rows are zero based (o, 1, ...), border names ie Border2 are 1 based (1, 2, ...)
        int oldAnchorRow = oldAnchoName.right(oldAnchoName.length() - 6).toInt() - 1;
        graphicAnchorObjectEditor.at(i)->refresh(anchorObjectList);
        // refreshing anchorObjectList removes old value for the graphic - reassign anchor object
        /*
        qDebug() << __FUNCTION__
                 << "GRAPHIC ITEM #" << i
                 << "oldAnchoName =" << oldAnchoName
                 << "oldAnchorRow =" << oldAnchorRow
                 << "border row deleted =" << deletedRow
                 << "anchorObjectList =" << anchorObjectList
                    ;
//                  */
        // if all borders deleted then anchor to the Image
        if (borderCount == 0 || oldAnchoName == "Image")
            graphicAnchorObjectEditor.at(i)->setValue("Image");
        // oldAnchorRow okay if deleted row is greater than anchor row (oldAnchorRow)
        else if (deletedRow > oldAnchorRow)
            graphicAnchorObjectEditor.at(i)->setValue(oldAnchoName);
        // deleted row <= oldAnchorRow then newRow = oldAnchorRow - 1
        else if (deletedRow <= oldAnchorRow) {
            QString newStr = "Border" + QString::number(oldAnchorRow);
            graphicAnchorObjectEditor.at(i)->setValue(newStr);
        }
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
                break;
            case winnow_effects::brighten:
                qDebug() << "      Brighten: evDelta  =" << ef.brighten.evDelta;
                break;
            case winnow_effects::emboss:
                qDebug() << "      Emboss: size       =" << ef.emboss.size;
                qDebug() << "      Emboss: soften     =" << ef.emboss.soften;
                qDebug() << "      Emboss: contou r   =" << ef.emboss.contour;
                qDebug() << "      Emboss: highlight  =" << ef.emboss.highlight;
                qDebug() << "      Emboss: shadow     =" << ef.emboss.shadow;
                qDebug() << "      Emboss: opacity    =" << ef.emboss.opacity;
                break;
            case winnow_effects::sharpen:
                qDebug() << "      Sharpen: radius    =" << ef.sharpen.radius;
                break;
            case winnow_effects::highlight:
                qDebug() << "      Highlight: top     =" << ef.highlight.top;
                qDebug() << "      Highlight: left    =" << ef.highlight.left;
                qDebug() << "      Highlight: right   =" << ef.highlight.right;
                qDebug() << "      Highlight: bottom  =" << ef.highlight.bottom;
                qDebug() << "      Highlight: r       =" << ef.highlight.r;
                qDebug() << "      Highlight: g       =" << ef.highlight.g;
                qDebug() << "      Highlight: b       =" << ef.highlight.b;
                qDebug() << "      Highlight: a       =" << ef.highlight.a;
                break;
            case winnow_effects::shadow:
                qDebug() << "      Shadow: size       =" << ef.shadow.length;
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
//            << "order =" << b[i].order
            << "name =" << b[i].name
//            << "caption =" << b[i].caption
//            << "parent =" << b[i].parent
            << "top =" << b[i].top
            << "left =" << b[i].left
            << "right =" << b[i].right
            << "bottom =" << b[i].bottom
            << "tile =" << b[i].tile
            << "color =" << b[i].color
            << "opacity =" << b[i].opacity
            << "style =" << b[i].style;
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
            << "anchorPoint =" << t[i].anchorPoint
            << "x =" << t[i].x
            << "y =" << t[i].y
            << "text =" << t[i].text
            << "rotation =" << t[i].rotation
            << "metadataTemplate =" << t[i].metadataTemplate
            << "size =" << t[i].size
            << "font =" << t[i].font
            << "isBold =" << t[i].isBold
            << "isItalic =" << t[i].isItalic
            << "color =" << t[i].color
            << "opacity =" << t[i].opacity
            << "style =" << t[i].style
               ;
    }
    qDebug() << __FUNCTION__ << "GRAPHIC VECTOR";
    for (int i = 0; i < g.length(); ++i) {
        qDebug().noquote()
            << i << " "
            << "name =" << g[i].name
            << "caption =" << g[i].caption
            << "parent =" << g[i].parent
            << "filePath =" << g[i].filePath
            << "anchorObject =" << g[i].anchorObject
            << "anchorContainer =" << g[i].anchorContainer
            << "anchorPoint =" << g[i].anchorPoint
            << "x =" << g[i].x
            << "y =" << g[i].y
            << "rotation =" << g[i].rotation
            << "size =" << g[i].size
            << "opacity =" << g[i].opacity
            << "style =" << g[i].style
               ;
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
    for (int r = 0; r < model->rowCount(parent); ++r) {
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

void EmbelProperties::rename() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    }
//    BarBtn *btn = qobject_cast<BarBtn*>(sender());
//    QModelIndex idx = btn->index;
    QModelIndex idx = currentIdx;
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
    qDebug() << __FUNCTION__;
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
/*
Sets the current template to templateName, which must be asigned before calling this function.
The templatePath and isCurrent are defined. The template is selected.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < templateList.length(); ++i) {
        QString t = templateList.at(i);
        bool isCurrent = (t == templateName);
        QString path = "Embel/Templates/" + t + "/isCurrent";
        setting->setValue(path, isCurrent);
        if (isCurrent) templateId = i;
        templatePath = "Embel/Templates/" + templateName + "/";
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
Read the template list using QSettings.  The sort order of the templateList has "Do not
Embellish" first, followed by the template items in alphabetical order.  The template items
are read from settings to a temp list, sorted, and then appended to templateList.

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
}

QString EmbelProperties::uniqueTemplateName(QString name)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setting->beginGroup("Embel/Templates");
    QStringList keys = setting->childGroups();
    setting->endGroup();
    bool nameExists = true;
    QString newName = name;
    int count = 0;
    while (nameExists) {
        if (keys.contains(newName)) newName = name + QString::number(++count);
        else nameExists = false;
    }
    return newName;
}

void EmbelProperties::copyTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "Embel/Templates/" + templateName + "/";
    QString copyName = uniqueTemplateName(templateName + " copy");
    qDebug() << __FUNCTION__ << copyName;
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

void EmbelProperties::doNotEmbellish()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    e->build();
}

void EmbelProperties::moveBorderUp()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    QModelIndex idx = btn->index;
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
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    QModelIndex idx = btn->index;
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
settings, model (tree) and styleMap must be updated. When the model is saved to settings the
various items order is not maintained, so the last sort order must be re-established after the
settings have been read, based on the setting sortOrder. This is done in sortEffectList, which
is called by addStyle, using the model role UR_SortOrder. The sorting is accomplished using
QStandardItem::sortChildren. styleItem is the parent and the effects are the children being
sorted.
*/
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

    // get current row for this index as it may have been sorted already
    QString effectName = btn->name;
    qDebug() << __FUNCTION__ << effectName;
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
    swapEffects(styleItem, row, swapRow);

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
    swapEffects(styleItem, row, swapRow);

    return;
}

void EmbelProperties::syncBorderVector()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    QModelIndex grandparentIdx = idx.parent().parent();

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
        if (parent.left(9) == "Highlight") itemChangeHighlightEffect(v, source, parent, grandparent);
        if (parent.left(8) == "Brighten") itemChangeBrightenEffect(v, source, parent, grandparent);
        if (parent.left(6) == "Emboss") itemChangeEmbossEffect(v, source, parent, grandparent);
    }

//    e->build();
}

void EmbelProperties::itemChangeTemplate(QVariant v)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    isTemplateChange = true;
    templateName = v.toString();

    // save which template is current and set templateId and templateName
    setCurrentTemplate();

    // update global for ImageView              rgh req'd??
    if (templateId == 0) G::isEmbellish = false;
    else G::isEmbellish = true;

    // update View > Embellish menu
    emit templateChanged(templateId);

    // clear model except for template name header and styles (row 0 and row 1)
    model->removeRows(2, model->rowCount(QModelIndex()) - 2, QModelIndex());

    // clear borders, texts, rectangles and graphics
    b.clear();
    t.clear();
    g.clear();

    // show/hide Styles
    if (templateId > 0) setRowHidden(1, root, false);
    else setRowHidden(1, root, true);

    // clear all graphics items
    if (templateId == 0) e->clear();

    // add items for this template
    if (templateId > 0) addTemplateItems();
    collapseAll();
    expand(model->index(_templates,0));
    isTemplateChange = false;

    mw3->imageView->loadImage(mw3->dm->currentFilePath);
}

void EmbelProperties::itemChangeExport(QModelIndex idx, QVariant v, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = templatePath + "Export/" + source;

    if (source == "fileType") {
        setting->setValue(path, v.toString());
        exportFileType = v.toString();
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
    }
    QString path = templatePath + "General/" + source;
//    qDebug() << __FUNCTION__ << path;

    if (source == "horizontalFit") {
        setting->setValue(path, v.toInt());
        horizontalFitPx = v.toInt();
    }

    if (source == "verticalFit") {
        setting->setValue(path, v.toInt());
        verticalFitPx = v.toInt();
    }

    if (source == "globalLightDirection") {
        setting->setValue(path, v.toString());
        globalLightDirection = v.toInt();
    }

    if (source == "imageStyle") {
        setting->setValue(path, v.toString());
        image.style = v.toString();
    }
    e->build();
}

void EmbelProperties::itemChangeBorder(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
        qDebug() << __FUNCTION__ << "item change topMargin" << "x =" << x;
        setting->setValue(path, x);
        b[index].top = x;
        // build embel as change in border dimension changes all coordinates
        e->build();
        return;
    }

    if (source == "leftMargin") {
        double x = v.toDouble();
        setting->setValue(path, x);
        b[index].left = x;
        // build embel as change in border dimension changes all coordinates
        e->build();
        return;
    }

    if (source == "rightMargin") {
        double x = v.toDouble();
        setting->setValue(path, x);
        b[index].right = x;
        // build embel as change in border dimension changes all coordinates
        e->build();
        return;
    }

    if (source == "bottomMargin") {
        double x = v.toDouble();
        setting->setValue(path, x);
        b[index].bottom = x;
        // build embel as change in border dimension changes all coordinates
        e->build();
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
    }
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString parent = idx.parent().data(UR_Name).toString();
    int index = idx.parent().data(UR_ItemIndex).toInt();
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
    }

}

void EmbelProperties::itemChangeGraphic(QModelIndex idx, QVariant v, QString source, QString parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int index = idx.parent().data(UR_ItemIndex).toInt();
    QString path = templatePath + "Graphics/" + parent + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "filePath") {
        setting->setValue(path, v.toString());
        g[index].filePath = v.toString();
        e->build();
        return;
    }

    if (source == "anchorObject") {
        setting->setValue(path, v.toString());
        g[index].anchorObject = v.toString();
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

    if (source == "size") {
        setting->setValue(path, v.toDouble());
        g[index].size = v.toDouble();
        qDebug() << __FUNCTION__ << "g[index].size =" << g[index].size;
    }

    if (source == "rotation") {
        setting->setValue(path, v.toDouble());
        g[index].rotation = v.toDouble();
    }

    if (source == "anchorPoint") {
        setting->setValue(path, v.toString());
        g[index].anchorPoint = v.toString();
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
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
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

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].shadow.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeBlurEffect(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
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
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
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

void EmbelProperties::itemChangeHighlightEffect(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "top") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.top = v.toInt();
    }

    if (source == "left") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.left = v.toInt();
    }

    if (source == "right") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.right = v.toInt();
    }

    if (source == "bottom") {
        setting->setValue(path, v.toInt());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.bottom = v.toInt();
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

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].highlight.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeBrightenEffect(QVariant v, QString source, QString effectName, QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "Embel/Styles/" + style + "/" + effectName + "/" + source;
//    qDebug() << __FUNCTION__ << path << source << v.toInt();

    if (source == "evDelta") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].brighten.evDelta = v.toDouble();
    }

    if (source == "blendMode") {
        setting->setValue(path, v.toString());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].brighten.blendMode = winnow_effects::blendModeMap[v.toString()];
    }

    e->updateStyle(style);
}

void EmbelProperties::itemChangeEmbossEffect(QVariant v, QString source, QString effectName, QString style)
{
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
        styleMap[style][effect].emboss.size = v.toDouble();
    }

    if (source == "soften") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.soften = v.toDouble();
    }

    // add contour

    if (source == "highlight") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.highlight = v.toDouble();
    }

    if (source == "shadow") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.shadow = v.toDouble();
    }

    if (source == "opacity") {
        setting->setValue(path, v.toDouble());
        int effect = effectIndex(style, effectName);
        if (effect == -1) return;
        styleMap[style][effect].emboss.opacity = v.toDouble();
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

    BarBtn *templateDeleteBtn = new BarBtn();
    templateDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    templateDeleteBtn->setToolTip("Delete the selected template");
    btns.append(templateDeleteBtn);
    connect(templateDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteTemplate);
    BarBtn *templateNewBtn = new BarBtn();
    templateNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    templateNewBtn->setToolTip("Create a new template");
    connect(templateNewBtn, &BarBtn::clicked, this, &EmbelProperties::newEmbelTemplate);
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
    }
    addGeneral();
    addExport();
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
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "fileType";
    i.value = setting->value(settingRootPath + i.key);
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "tif" << "jpg" << "png";
    exportFileType = i.value.toString();
    addItem(i);

    // EXPORT Save method
    i.name = "saveMethod";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Save method";
    i.tooltip = "Select where to save the export file.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "saveMethod";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else i.value = "Subfolder";
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
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "folderPath";
    i.value = setting->value(settingRootPath + i.key);
    i.delegateType = DT_SelectFolder;
    i.type = "string";
    exportFolderPath = i.value.toString();
    addItem(i);
    // hide if not selected by saveMethod
    if (saveMethod == "Subfolder") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
    }

    // EXPORT Subfolder name
    i.name = "subfolder";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Subfolder name";
    i.tooltip = "Enter the name of the subfolder to contain the exported files.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "subfolder";
    i.value = setting->value(settingRootPath + i.key);
    i.delegateType = DT_LineEdit;
    i.type = "string";
    exportSubfolder = i.value.toString();
    addItem(i);
    // hide if not selected by saveMethod
    if (saveMethod == "Another folder somewhere else") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
    }

    // EXPORT Overwrite existing files
    i.name = "overwriteFiles";
    i.parIdx = parIdx;
    i.parentName = "ExportHeader";
    i.captionText = "Overwrite";
    i.tooltip = "Overwrite existing files.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "overwriteFiles";
    i.value = setting->value(settingRootPath + i.key);
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
    }
    // IMAGE Image header (Root)
    i.name = "GeneralHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = "General";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
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
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "horizontalFit";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else i.value = 500;
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
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "verticalFit";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else i.value = 500;
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 10000;
    i.fixedWidth = 50;
    verticalFitPx = i.value.toInt();
    addItem(i);

    // Global light direction
    i.name = "globalLightDirection";
    i.parIdx = parIdx;
    i.parentName = "General";
    i.captionText = "Light direction";
    i.tooltip = "Light source direction (0 - 360 degrees)";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "globalLightDirection";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else i.value = 315;
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 360;
    i.fixedWidth = 50;
    globalLightDirection = i.value.toInt();
    addItem(i);

    // IMAGE style
    i.name = "imageStyle";
    i.parIdx = parIdx;
    i.parentName = "GeneralHeader";
    i.captionText = "Image style";
    i.tooltip = "Select style to apply to the image ie a shadow.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "imageStyle";
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else i.value = "No style";
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
    connect(borderNewBtn, &BarBtn::clicked, this, &EmbelProperties::newBorder);
    addItem(i);
    bordersIdx = capIdx;

    // item reqd to sort borders after they have been read from settings
    QStandardItem *borders = new QStandardItem;
    borders = model->itemFromIndex(bordersIdx);

    QString path = templatePath + "/Borders";
    setting->beginGroup(path);
    int count = setting->childGroups().size();
    setting->endGroup();
    for (int i = 0; i < count; ++i) newBorder();

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
//    styleRenameBtn->index = capIdx;
    effectNewBtn->index = capIdx;

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
        if (items.at(i).contains("Highlight")) addHighlightEffect(styleIdx, effectName);
        if (items.at(i).contains("Shadow")) addShadowEffect(styleIdx, effectName);
        if (items.at(i).contains("Brighten")) addBrightenEffect(styleIdx, effectName);
        if (items.at(i).contains("Emboss")) addEmbossEffect(styleIdx, effectName);
    }

    // sort the effects
    styleItem->sortChildren(Qt::AscendingOrder);

    // add to local map used in embel
//    styleMap[styleName] = effects;
    sortEffectList(styleName);
}

void EmbelProperties::addBorderHeaderButtons()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    effectUpBtn->index = capIdx;
    effectDownBtn->index = capIdx;
    effectDeleteBtn->index = capIdx;
}

void EmbelProperties::addEffectBtns()
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

void EmbelProperties::addBlurEffect(QModelIndex parIdx, QString effectName)
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
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::blur, "Blur");
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
    i.type = "double";
    i.min = 0;
    i.max = 10000;
    i.div = 100;
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
}

void EmbelProperties::addSharpenEffect(QModelIndex parIdx, QString effectName)
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
    effect.effectType = winnow_effects::sharpen;
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::sharpen, "Sharpen");
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
}

void EmbelProperties::addHighlightEffect(QModelIndex parIdx, QString effectName)
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
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::highlight, "Highlight");
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
    addEffectBtns();
//    addItem(i);
    parIdx = capIdx;

    // highlight top margin
    i.name = "top";  // x and y offsets are equal
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Top margin";
    i.tooltip = "The amount of highlighted margin on top of the item.";
    i.isIndent = true;;
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
    effect.highlight.top = i.value.toInt();
    addItem(i);

    // highlight left margin
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
    effect.highlight.left = i.value.toInt();
    addItem(i);

    // highlight right margin
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
    effect.highlight.right = i.value.toInt();
    addItem(i);

    // highlight bottom margin
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
    effect.highlight.bottom = i.value.toInt();
    addItem(i);

    // highlight opacity
    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the highlight.";
    i.isIndent = true;;
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
    i.isIndent = true;;
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

    // highlight blend mode
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
    effect.highlight.blendMode = winnow_effects::blendModeMap[i.value.toString()];
    addItem(i);

//    effects.append(effect);
    styleMap[styleName].append(effect);
}

void EmbelProperties::addBrightenEffect(QModelIndex parIdx, QString effectName)
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
    effect.effectType = winnow_effects::brighten;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::brighten, "Brighten");
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

    // brighten evDelta
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
    i.fixedWidth = 50;
    effect.brighten.evDelta = i.value.toDouble();
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
}

void EmbelProperties::addEmbossEffect(QModelIndex parIdx, QString effectName)
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
    effect.effectType = winnow_effects::emboss;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::brighten, "Emboss");
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
    i.captionText = "Size";
    i.tooltip = "The width, as a percentage of the long side, of the emboss effect.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "size";
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
    i.min = 0;
    i.max = 1000;
    i.div = 100;
    i.fixedWidth = 50;
    effect.emboss.size = i.value.toDouble();
    qDebug() << __FUNCTION__
             << "effect.emboss.size =" << effect.emboss.size;
    addItem(i);

    // emboss soften
    i.name = "soften";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Soften";
    i.tooltip = "The amount to soften, from chisel to smooth.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "soften";
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
    i.min = 0;
    i.max = 100;
    i.div = 100;
    i.fixedWidth = 50;
    effect.emboss.soften = i.value.toDouble();
    addItem(i);

    // embos contour (to be added)

       // emboss highlight
    i.name = "highlight";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Highlight";
    i.tooltip = "The brightness of the highlights.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "highlight";
    i.defaultValue = 1.0;
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
    i.fixedWidth = 50;
    effect.emboss.highlight = i.value.toDouble();
    addItem(i);

    // emboss shadow
    i.name = "shadow";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Shadow";
    i.tooltip = "The brightness of the shadows.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "shadow";
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
    i.fixedWidth = 50;
    effect.emboss.shadow = i.value.toDouble();
    addItem(i);

    // emboss opacity
    i.name = "opacity";
    i.parIdx = parIdx;
    i.parentName = effectName;
    i.captionText = "Opacity";
    i.tooltip = "The brightness of the shadows.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
    i.defaultValue = 100;
    i.path = settingRootPath + i.key;
    if (setting->contains(i.path))
        i.value = setting->value(i.path);
    else {
        i.value = i.defaultValue;
        setting->setValue(i.path, i.value);
    }
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
//    i.div = 100;
    i.fixedWidth = 50;
    effect.emboss.opacity = i.value.toInt();
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
}

void EmbelProperties::addShadowEffect(QModelIndex parIdx, QString effectName)
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
//    qDebug() << __FUNCTION__ << "effectName =" << effectName;
    if (effectName == "")
        effectName = uniqueEffectName(parentName, winnow_effects::shadow, "Shadow");
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
    i.path = settingRootPath + i.key;
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
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
    effect.shadow.blendMode = winnow_effects::blendModeMap[i.value.toString()];
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
    updateBorderLists(row);
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

void EmbelProperties::newGraphic()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    e->build();
}

void EmbelProperties::deleteTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
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
}

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
    qDebug() << __FUNCTION__ << idx.data().toString();
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

    // remove from local vectors and Embel graphicItems
    if (parName == "Borders") {
        e->removeBorder(row);
        b.remove(row);
    }

    if (parName == "Texts") {
        e->removeText(row);
        t.remove(row);
    }

    if (parName == "Graphics") {
        e->removeGraphic(row);
        g.remove(row);
    }

    // remove from datamodel
    model->removeRow(idx.row(), idx.parent());

    // remove from setting
    setting->remove(path);

    /* rename subsequent category items ie text2, text3 ... in setting, model and vectors
       This is not required for styles and effects as they are not numbered automatically */
    QStringList templateItems;
    templateItems << "Borders" << "Texts" << "Graphics";
    if (templateItems.contains(parName)) {
        QString itemBase = parName.left(parName.length() - 1);
        for (int i = row; i < model->rowCount(parIdx); ++i) {
            QString parPath = templatePath + parName;
            QString oldName = model->index(i, CapColumn, parIdx).data().toString();
            QString newName = itemBase + QString::number(i + 1);
            qDebug() << __FUNCTION__ << i << path << oldName << newName;
            // update setting
            renameSettingKey(parPath, oldName, newName);
            // update model
            model->setData(model->index(i, CapColumn, parIdx), newName);
            // update local struct
            if (parName == "Borders") b[i].name = newName;
            if (parName == "Texts") t[i].name = newName;
            if (parName == "Graphics") g[i].name = newName;
        }
    }

    /* if border deleted update lists that have borders and any anchorObjects in texts and
       graphics.  This needs to happen after all data structures have been updated.  */
    if (parName == "Borders") updateBorderLists(row);

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
    }
    // check index range (triggered by mouse movement while tree painting)
    if (idx.row() < 0) return;

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
        static int count = 0;
        qDebug() << __FUNCTION__ << count
                 << idx.data().toString()
                 << idx.parent().data().toString()
                 << idx.parent().parent().data().toString()
                    ;
        flash(idx);
    }
}

void EmbelProperties::mouseDoubleClickEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx = indexAt(event->pos());
    idx = model->index(idx.row(), ValColumn, idx.parent());
    setItemValue(idx, idx.data(UR_DelegateType).toInt(), idx.data(UR_DefaultValue));
}

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

    if (event->button() == Qt::LeftButton) {
        QModelIndex idx = indexAt(event->pos());
        currentIdx = model->index(idx.row(), 0, idx.parent());
        if (currentIdx.parent() == bordersIdx ||
            currentIdx.parent() == textsIdx ||
            currentIdx.parent() == graphicsIdx)
        {
            flash(currentIdx);
        }
    }

    if (event->button() == Qt::RightButton) {
        renameAction->setVisible(false);
        copyAction->setVisible(false);
        moveUpAction->setVisible(false);
        moveDownAction->setVisible(false);

        QModelIndex idx = indexAt(event->pos());
        currentIdx = model->index(idx.row(), 0, idx.parent());
        /*
        qDebug() << __FUNCTION__
                 << currentIdx.data().toString()
                 << currentIdx.parent().data().toString()
                 << templateIdx.data().toString()
                    ;
//                    */

        if (currentIdx.parent() == templateIdx || currentIdx == templateIdx) {
            renameAction->setVisible(true);
            copyAction->setVisible(true);
        }

        if (currentIdx.parent() == stylesIdx) {
            renameAction->setVisible(true);
        }

        if (currentIdx.parent().parent() == stylesIdx &&
            model->rowCount(currentIdx.parent()) > 1)
        {
            moveUpAction->setVisible(true);
            moveDownAction->setVisible(true);
        }

        if (currentIdx.parent() == bordersIdx) {
            renameAction->setVisible(true);
            copyAction->setVisible(true);
            moveUpAction->setVisible(true);
            moveDownAction->setVisible(true);
        }
    }

    PropertyEditor::mousePressEvent(event);

//    QTreeView::mousePressEvent(event);
//    treeChange(indexAt(event->pos()));
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
    }
    qDebug() << __FUNCTION__ << "selName =" << selName;
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

//void EmbelProperties::parseAlignToCorner(QString alignTo, int &iBorder, int &iCorner)
//{
///*
//The string alignTo example: "Border2 BottomLeft".  We need to know the number of the border
//and the corner.
//*/
//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
//    }
//    iBorder = alignTo.mid(6, 1).toInt() - 1;
//    QString s = alignTo.right(alignTo.length() - 8);
//    if (s == "TopLeft") iCorner = 0;
//    if (s == "TopRight") iCorner = 1;
//    if (s == "BottomLeft") iCorner = 2;
//    if (s == "BottomRight") iCorner = 3;
////    qDebug() << __FUNCTION__ << alignTo << s << iBorder << iCorner;
//}

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
}

void EmbelProperties::test1()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    e->test();

}

void EmbelProperties::test2()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    BarBtn *btn = qobject_cast<BarBtn*>(sender());
    int ret = (QMessageBox::warning(this, "Delete Template", "Confirm delete template " + templateName + "                     ",
                             QMessageBox::Cancel | QMessageBox::Ok));
    if (ret == QMessageBox::Cancel) return;

    QString pathToDelete = "Embel/Templates/" + templateName;
    qDebug() << __FUNCTION__ << pathToDelete;

    // remove from tree combo list
    templateListEditor->removeItem(templateName);
    // remove from settings
    setting->remove(pathToDelete);
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
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
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
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
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
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
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
    i.delegateType = DT_Slider;
    i.type = "double";
    i.min = 0;
    i.max = 2000;
    i.div = 100;
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
    // hide if anchorObject == "Image" (Image does not have border areas)
    if (text.anchorObject == "Image") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
    }

    i.name = "anchorPoint";
    i.parIdx = parIdx;
    i.parentName = textName;
    i.captionText = "Text box anchor";
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
    addItem(i);
//    textAnchorObjectEditor[count] = static_cast<ComboBoxEditor*>(addItem(i));

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
    i.max = 10000;
    i.div = 100;
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
    i.max = 10000;
    i.div = 100;
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

void EmbelProperties::addGraphic(int count)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    btns.append(graphicDeleteBtn);
    connect(graphicDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    addItem(i);
    QModelIndex parIdx = capIdx;
    graphicDeleteBtn->index = capIdx;

    // get index to use as parent when update tree depending on source
    graphic.name = graphicName;

    i.name = "filePath";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "File path";
    i.tooltip = "Enter the file path for the graphic.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "filePath";
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = "";
//        i.value = ":/images/icon16/winnow288.png";
        setting->setValue(settingRootPath + i.key, i.value);
    }
//    i.value = setting->value(settingRootPath + i.key);
    i.delegateType = DT_SelectFile;
    i.type = "string";
    graphic.filePath = i.value.toString();
    addItem(i);

    i.name = "anchorObject";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Place graphic in";
    i.tooltip = "Select a border or image to place the graphic.";
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
    graphic.anchorObject = i.value.toString();
    graphicAnchorObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    i.name = "anchorContainer";
    i.parIdx = parIdx;
    i.parentName = graphicName;
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
    graphic.anchorContainer = i.value.toString();
    addItem(i);
    // hide if not selected by anchorObject
    if (graphic.anchorObject == "Image") {
        model->setData(capIdx, true, UR_isHidden);      // capIdx defined by addItem
        model->setData(valIdx, true, UR_isHidden);      // valIdx defined by addItem
    }

    i.name = "anchorPoint";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Graphic box anchor";
    i.tooltip = "Select a graphic anchor point.";
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
    graphic.anchorPoint = i.value.toString();
    addItem(i);
//    graphicAnchorObjectEditor[count] = static_cast<ComboBoxEditor*>(addItem(i));

    i.name = "x";
    i.parIdx = parIdx;
    i.parentName = graphicName;
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
    i.max = 10000;
    i.div = 100;
    i.fixedWidth = 50;
    graphic.x = i.value.toDouble();
    addItem(i);

    i.name = "y";
    i.parIdx = parIdx;
    i.parentName = graphicName;
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
    i.max = 10000;
    i.div = 100;
    i.fixedWidth = 50;
    graphic.y = i.value.toDouble();
    addItem(i);

    i.name = "size";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Size";
    i.tooltip = "The size of the graphic defined by the % of the long side of the frame.";
    i.isIndent = false;
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
    i.max = 2500;
    i.div = 100;
    i.fixedWidth = 50;
    graphic.size = i.value.toDouble();
    addItem(i);

    i.name = "rotation";
    i.parIdx = parIdx;
    i.parentName = graphicName;
    i.captionText = "Rotation";
    i.tooltip = "The rotation (degrees) of the graphic (+- 0-360)";
    i.isIndent = false;
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
    i.isIndent = false;
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
    graphic.style = i.value.toString();
    styleListObjectEditor.append(static_cast<ComboBoxEditor*>(addItem(i)));

    // add the text info to the vector of texts t
    g << graphic;

}
