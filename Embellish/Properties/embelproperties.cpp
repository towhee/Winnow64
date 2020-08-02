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
    is = new InfoString(this, mw3->dm);
    this->setting = setting;

    setSolo(true);
    collapseAll();
    expand(model->index(0,0,QModelIndex()));
    setIndentation(12);
    setAlternatingRowColors(false);
    resizeColumns("====captions column====",
                  "====values column====");
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
    // add the File, Image, Borders, Texts, Rectangles and Graphics items for the template
    if (templateId != 0) addTemplateItems();
    // default state
    collapseAll();
    expand(model->index(_templates,0));
    expand(model->index(_borders,0));
    expand(model->index(_texts,0));
    expand(model->index(_rectangles,0));
    expand(model->index(_graphics,0));
}

void EmbelProperties::initialize()
{
    anchorPoints << "Top Left" << "Top Center" << "Top Right"
                 << "Middle Left" << "Middle Center" << "Middle Right"
                 << "Bottom Left" << "Bottom Center" << "Bottom Right";
    anchorContainerList << "Top" << "Left" << "Right" << "Bottom";
    readTileList();
}

void EmbelProperties::updateBorderList()
{
    borderList.clear();
    anchorObjectList.clear();
    getIndex("Borders");
    QModelIndex bordersIdx = foundIdx;
    for (int i = 0; i < model->rowCount(bordersIdx); ++i) {
        borderList << model->index(i, 0, bordersIdx).data(UR_Name).toString();
        anchorObjectList << model->index(i, 0, bordersIdx).data(UR_Name).toString();
    }
    anchorObjectList << "Image";
    // update textAnchorObjectEditor etc rgh
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
        QModelIndex idx0 = model->index(r, 0, parent);
        QModelIndex idx1 = model->index(r, 1, parent);
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
    // vectors
    qDebug() << __FUNCTION__ << "BORDER VECTOR";
    for (int i = 0; i < b.length(); ++i) {
        qDebug().noquote()
            << i << " "
            << "name = " << b[i].name
            << "caption = " << b[i].caption
            << "parent = " << b[i].parent
            << "top = " << b[i].top
            << "left = " << b[i].left
            << "right = " << b[i].right
            << "bottom = " << b[i].bottom
            << "tile = " << b[i].tile
            << "color = " << b[i].color
            << "opacity = " << b[i].opacity
            << "style = " << b[i].style
            << "outlineWidth = " << b[i].outlineWidth
            << "outlineColor = " << b[i].outlineColor;
    }
    qDebug() << __FUNCTION__ << "TEXT VECTOR";
    for (int i = 0; i < t.length(); ++i) {
        qDebug().noquote()
            << i << " "
            << "name = " << t[i].name
            << "caption = " << t[i].caption
            << "parent = " << t[i].parent
            << "anchorObject = " << t[i].anchorObject
            << "anchorContainer = " << t[i].anchorContainer
            << "x = " << t[i].x
            << "y = " << t[i].y
            << "anchorPoint = " << t[i].anchorPoint
            << "source = " << t[i].source
            << "text = " << t[i].text
            << "metadataField = " << t[i].metadataField
            << "metadataTemplate = " << t[i].metadataTemplate
            << "size = " << t[i].size
            << "font = " << t[i].font
            << "isBold = " << t[i].isBold
            << "isItalic = " << t[i].isItalic
            << "color = " << t[i].color
            << "opacity = " << t[i].opacity
            << "style = " << t[i].style;
    }
}

void EmbelProperties::rename(QString path, QString from, QString to)
{
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

void EmbelProperties::renameCurrentTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString name = Utilities::inputText("Rename Template", "Rename template " + templateName);
    if (name == "") return;
    rename("Embel/Templates", templateName, name);
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
        qDebug() << __FUNCTION__ << templateName << templateId;
    }
    setting->endGroup();
    templatePath = "Embel/Templates/" + templateName + "/";
    if (templateId == 0) G::isEmbellish = false;
    else G::isEmbellish = true;
}

void EmbelProperties::readTileList()
{
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
    templateName = Utilities::inputText("New Template", "Enter new template name");
    templateId = templateList.count();
//    // set all templates except templateName isCurrent = false
//    setCurrentTemplate();
    templateList << templateName;
    templateListEditor->addItem(templateName);
    templateListEditor->setValue(templateName);
    // add the File, Image, Borders, Texts, Rectangles and Graphics items for the template
//    addTemplateItems();
}

void EmbelProperties::invokeFromAction(QAction *embelAction)
{
    if (templateName == embelAction->text()) return;
    for (int i = 0; i < templateList.count(); ++i) {
        if (templateList.at(i)  == embelAction->text()) {
            templateListEditor->setValue(templateList.at(i));
        }
    }
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
    qDebug() << __FUNCTION__ << "idx.isValid() =" << idx.isValid()
             << v << source << templateId << parent << grandparent;

    if (source == "templateList") templateChange(v);
    if (templateId != 0) {
        if (parent == "FileHeader") fileItemChange(v, source);
        if (parent == "ImageHeader") imageItemChange(v, source);
        if (grandparent == "Borders") borderItemChange(idx);
        if (grandparent == "Texts") textItemChange(idx);
        if (grandparent == "Rectangles") rectangleItemChange(v, source, parent);
        if (grandparent == "Graphics") graphicItemChange(v, source, parent);
    }

    e->build();
}

void EmbelProperties::templateChange(QVariant v)
{
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
        f.horizontalFit = v.toInt();
    }

    if (source == "verticalFit") {
        setting->setValue(path, v.toInt());
        f.verticalFit = v.toInt();
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
    diagnostics(idx);

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
    qDebug() << __FUNCTION__ << path;

    if (source == "anchorObject") {
        setting->setValue(path, v.toString());
        t[index].anchorObject = v.toString();
    }

    if (source == "anchorContainer") {
        setting->setValue(path, v.toString());
        t[index].anchorContainer = v.toString();
    }

    if (source == "x") {
        setting->setValue(path, v.toInt());
        t[index].x = v.toInt();
    }

    if (source == "y") {
        setting->setValue(path, v.toInt());
        t[index].y = v.toInt();
    }

    if (source == "anchorPoint") {
        setting->setValue(path, v.toString());
        t[index].anchorPoint = v.toString();
    }

    if (source == "source") {
        QString s = v.toString();
        setting->setValue(path, s);
        t[index].source = s;
        if (G::isInitializing) return;
        int row = idx.row();
        qDebug() << __FUNCTION__ << s << row;
        // only works if branch is expanded
        if (s == "Text") {
            setRowHidden(row + 1, idx.parent(), false);
            setRowHidden(row + 2, idx.parent(), true);
            setRowHidden(row + 3, idx.parent(), true);
        }
        if (s == "Metadata field") {
            setRowHidden(row + 1, idx.parent(), true);
            setRowHidden(row + 2, idx.parent(), false);
            setRowHidden(row + 3, idx.parent(), true);
        }
        if (s == "Metadata template") {
            setRowHidden(row + 1, idx.parent(), true);
            setRowHidden(row + 2, idx.parent(), true);
            setRowHidden(row + 3, idx.parent(), false);
        }
    }

    if (source == "text") {
        setting->setValue(path, v.toString());
        t[index].text = v.toString();
    }

    if (source == "metadataField") {
        setting->setValue(path, v.toString());
        t[index].metadataField = v.toString();
    }

    if (source == "metadataTemplate") {
        setting->setValue(path, v.toString());
        t[index].metadataTemplate = v.toString();
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
    templateRenameBtn->setIcon(QIcon(":/images/icon16/delta.png"));
    templateRenameBtn->setToolTip("Rename the selected template");
    btns.append(templateRenameBtn);
    connect(templateRenameBtn, &BarBtn::clicked, this, &EmbelProperties::test1);
    BarBtn *templateDeleteBtn = new BarBtn();
    templateDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    templateDeleteBtn->setToolTip("Delete the selected template");
    btns.append(templateDeleteBtn);
    connect(templateDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::test2);
    BarBtn *templateNewBtn = new BarBtn();
    templateNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    templateNewBtn->setToolTip("Create a new template");
    connect(templateNewBtn, &BarBtn::clicked, this, &EmbelProperties::newEmbelTemplate);
    btns.append(templateNewBtn);
    addItem(i);

    // Templates
    i.name = "templateList";
    i.parentName = "TemplatesHeader";
    i.decorateGradient = false;
    i.captionText = "Select template";
    i.tooltip = "Templates contain all the properties to embellish your images.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = templateName;
    i.key = "templateList";
    i.delegateType = DT_Combo;      // no parent, delegateType > 0 -> No header, has value
    i.type = "QString";
    i.color = "#1b8a83";
    i.dropList = templateList;
    templateListEditor = static_cast<ComboBoxEditor*>(addItem(i));

    qDebug() << __FUNCTION__ << "templateList.size() =" << templateList.size();
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
    f.horizontalFit = i.value.toInt();
    addItem(i);

    // FILES Vertical fit
    i.name = "verticalFit";
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
    f.verticalFit = i.value.toInt();
    addItem(i);

    // FILES File type
    i.name = "fileType";
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

    QString settingRootPath = templatePath + "Image/";

    i.name = "outlineWidth";
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
    // BORDER header (Root)
    i.name = "Borders";
    i.parentName = "???";
    i.isHeader = true;
    i.isDecoration = false;
    i.decorateGradient = true;
    i.isDecoration = false;
    i.captionText = "Borders";
    i.tooltip = "";
    //i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    borderDeleteBtn = new BarBtn();
    borderDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    borderDeleteBtn->setToolTip("Delete the open border");
    btns.append(borderDeleteBtn);
    connect(borderDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    borderDeleteBtn->setVisible(false);
    BarBtn *borderNewBtn = new BarBtn();
    borderNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    borderNewBtn->setToolTip("Create a new border");
    btns.append(borderNewBtn);
    connect(borderNewBtn, &BarBtn::clicked, this, &EmbelProperties::newBorder);
    addItem(i);

    QString path = templatePath + "/Borders";
    setting->beginGroup(path);
    int count = setting->childGroups().size();
    qDebug() << __FUNCTION__ << path << "count =" << count;
    setting->endGroup();
    for (int i = 0; i < count; ++i) newBorder();
}

void EmbelProperties::addTexts()
{
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
    textDeleteBtn = new BarBtn();
    textDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    textDeleteBtn->setToolTip("Delete the open text item");
    btns.append(textDeleteBtn);
    connect(textDeleteBtn, &BarBtn::clicked, this, &EmbelProperties::deleteItem);
    textDeleteBtn->setVisible(false);
    BarBtn *textNewBtn = new BarBtn();
    textNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    textNewBtn->setToolTip("Create a new border");
    btns.append(textNewBtn);
    connect(textNewBtn, &BarBtn::clicked, this, &EmbelProperties::newText);
    addItem(i);

    QString path = templatePath + "/Texts";
    setting->beginGroup(path);
    int count = setting->childGroups().size();
    qDebug() << __FUNCTION__ << path << "count =" << count;
    setting->endGroup();
    for (int i = 0; i < count; ++i) newText();
}

void EmbelProperties::addRectangles()
{
    qDebug() << __FUNCTION__;
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
    rectangleDeleteBtn = new BarBtn();
    rectangleDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    rectangleDeleteBtn->setToolTip("Delete the open rectangle");
    btns.append(rectangleDeleteBtn);
    rectangleDeleteBtn->setVisible(false);
    BarBtn *rectangleNewBtn = new BarBtn();
    rectangleNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    rectangleNewBtn->setToolTip("Create a new border");
    btns.append(rectangleNewBtn);
    addItem(i);

//    QString path = templatePath + "/Rectangles";
//    setting->beginGroup(path);
//    int count = setting->childGroups().size();
//    qDebug() << __FUNCTION__ << path << "count =" << count;
//    setting->endGroup();
//    for (int i = 0; i < count; ++i) newRectangle();
}

void EmbelProperties::addGraphics()
{
    qDebug() << __FUNCTION__;
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
    graphicDeleteBtn = new BarBtn();
    graphicDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    graphicDeleteBtn->setToolTip("Delete the open graphic");
    btns.append(graphicDeleteBtn);
    graphicDeleteBtn->setVisible(false);
    BarBtn *graphicNewBtn = new BarBtn();
    graphicNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    graphicNewBtn->setToolTip("Create a new graphic");
    btns.append(graphicNewBtn);
    addItem(i);

//    QString path = templatePath + "/Graphics";
//    setting->beginGroup(path);
//    int count = setting->childGroups().size();
//    qDebug() << __FUNCTION__ << path << "count =" << count;
//    setting->endGroup();
//    for (int i = 0; i < count; ++i) newGraphic();
}

void EmbelProperties::newBorder()
{
    getIndex("Borders");
    QModelIndex bordersIdx = foundIdx;
    int row = model->rowCount(bordersIdx);
    addBorder(row);
    updateBorderList();
    if (G::isInitializing || isTemplateChange) return;
    QModelIndex idx = model->index(row, 0, bordersIdx);
    selectionModel()->clear();
    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    e->build();
}

void EmbelProperties::newText()
{
    getIndex("Texts");
    QModelIndex textsIdx = foundIdx;
    int row = model->rowCount(textsIdx);
    addText(row);
    if (G::isInitializing || isTemplateChange) return;
    QModelIndex idx = model->index(row, 0, textsIdx);
    selectionModel()->clear();
    selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    e->build();
}

void EmbelProperties::deleteItem()
{
    // if no item selected then return
    if (selectionModel()->selection().isEmpty()) return;
    // get selected item index, name, parent, path ...
    QModelIndex idx = selectionModel()->selection().indexes().first();
    idx = model->index(idx.row(), 0, idx.parent());
    int row = idx.row();
    QString name = idx.data(UR_Name).toString();
    QModelIndex parIdx = idx.parent();
    QString parName = parIdx.data(UR_Name).toString();
    QString itemBase = parName.left(parName.length() - 1);
    QString objPath = templatePath + parName;
    QString itemToDeletePath = objPath + "/" + name;
    qDebug() << __FUNCTION__
             << "row =" << row
             << "name =" << name
             << "parName =" << parName
             << "itemBase =" << itemBase
             << "objPath =" << objPath
             << "itemToDeletePath =" << itemToDeletePath;
    int ret = (QMessageBox::warning(this, "Delete", "Confirm delete " + name + "                     ",
                             QMessageBox::Cancel | QMessageBox::Ok));
    if (ret == QMessageBox::Cancel) return;

    // remove from local vectors
    if (parName == "Borders") b.remove(row);
    if (parName == "Texts") t.remove(row);
    // remove from datamodel
    model->removeRow(idx.row(), idx.parent());
    // remove from setting
    setting->remove(itemToDeletePath);

    // rename subsequent category items ie text2, text3 ... in setting, model and vectors
    for (int i = row; i < model->rowCount(parIdx); ++i) {
        QString oldName = model->index(i,0,parIdx).data().toString();
        QString newName = itemBase + QString::number(i + 1);
        qDebug() << __FUNCTION__ << i << objPath << oldName << newName;
        // update setting
        rename(objPath, oldName, newName);
        // update model
        model->setData(model->index(i,0,parIdx), newName);
        // update local struct
        if (parName == "Borders") b[i].name = newName;
        if (parName == "Texts") t[i].name = newName;
    }

    // select another item
    int rowCount = model->rowCount(idx.parent());
    if (rowCount > 0) {
        QModelIndex nextIdx = model->index(row, 0, parIdx);
        if (!nextIdx.isValid()) nextIdx = model->index(rowCount - 1, 0, parIdx);
        selectionModel()->select(nextIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

    // if no items left in category then hide delete button
    else {
        if (parName == "Borders") borderDeleteBtn->setVisible(false);
        if (parName == "Texts") textDeleteBtn->setVisible(false);
        if (parName == "Rectangles") rectangleDeleteBtn->setVisible(false);
        if (parName == "Graphics") graphicDeleteBtn->setVisible(false);
    }

    // refresh the graphicsScene
    e->build();
}

//void EmbelProperties::mouseDoubleClickEvent(QMouseEvent */*event*/)
//{
//    qDebug() << __FUNCTION__;
//    // ignore
//}

void EmbelProperties::mousePressEvent(QMouseEvent *event)
/*
Set the current index and expand/collapse when click anywhere on a row that has children.
Do not pass on to QTreeView as this will enable QTreeView expanding and collapsing when the
decoration is clicked.
*/
{
    // ignore right mouse clicks for context menu
    if (event->button() == Qt::RightButton) return;

    treeChange(indexAt(event->pos()));
}

void EmbelProperties::treeChange(QModelIndex idx)
{
    if (!idx.isValid()) return;
    if (idx.column() != 0) idx = model->index(idx.row(), 0, idx.parent());
    bool hasChildren = model->hasChildren(idx);
    bool hasGrandChildren = model->hasChildren(model->index(0,0,idx));
    bool templateHdr = (idx == model->index(0,0));
    if (hasChildren && !hasGrandChildren && !templateHdr) {
        bool wasExpanded = isExpanded(idx);
        qDebug() << __FUNCTION__ << idx.data(UR_Name).toString();
        // clear selection and delete buttons (re-instated after selection change)
        selectionModel()->clear();
        showRelevantDeleteBtn();
        if (okToSelect(idx)) {
            selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        collapseAll();
        expand(model->index(_templates,0));
        expand(model->index(_borders,0));
        expand(model->index(_texts,0));
        expand(model->index(_rectangles,0));
        expand(model->index(_graphics,0));
        wasExpanded ? collapse(idx) : expand(idx);
    }
}

void EmbelProperties::selectionChanged(const QItemSelection &selection,
                                      const QItemSelection &prevSelection)
/*
Check if the new selection is legal (a member of borders, texts, rectangles, graphics or
file or image).  If not, if there was a previous selection, revert to that.  Hide all delete
buttons except in the selected category.
*/
{
    if (selection.isEmpty()) return;
    QModelIndex idx = selection.indexes().first();
    QModelIndex parIdx = idx.parent();
    QString selName = idx.data(UR_Name).toString();
    if (!okToSelect(idx)) {
        selectionModel()->clear();
        if (!okToSelect(parIdx)) return;
        selectionModel()->select(parIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        idx = parIdx;
    }
    // make sure using index for column 0
    idx = model->index(idx.row(), 0, idx.parent());
    // deal with delete buttons
    QString selectedCategory = idx.parent().data(UR_Name).toString();
    showRelevantDeleteBtn(selectedCategory);
}

bool EmbelProperties::okToSelect(QModelIndex idx)
{
    QModelIndex parIdx = idx.parent();
    if (!parIdx.isValid()) return false;
    if (parIdx == model->index(_borders,0)) return true;
    if (parIdx == model->index(_texts,0)) return true;
    if (parIdx == model->index(_rectangles,0)) return true;
    if (parIdx == model->index(_graphics,0)) return true;
    return false;
}

void EmbelProperties::showRelevantDeleteBtn(QString btnToShow)
{
    borderDeleteBtn->setVisible(false);
    textDeleteBtn->setVisible(false);
    rectangleDeleteBtn->setVisible(false);
    graphicDeleteBtn->setVisible(false);
    if (btnToShow == "Borders") borderDeleteBtn->setVisible(true);
    if (btnToShow == "Texts") textDeleteBtn->setVisible(true);
    if (btnToShow == "Rectangles") rectangleDeleteBtn->setVisible(true);
    if (btnToShow == "Graphics") graphicDeleteBtn->setVisible(true);
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

void EmbelProperties::test1()
{
    e->test();
}

void EmbelProperties::test2()
{
    qDebug() << __FUNCTION__;
//    setting->remove("Embel/Templates");
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
    qDebug() << __FUNCTION__ << "count =" << count;
    i.isHeader = true;
    i.isDecoration = true;
    i.itemIndex = count;
    i.name = borderName;
    i.parentName = "Borders";
    i.captionText = borderName;
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_Checkbox;
    addItem(i);

    i.name = "topMargin";
    i.parentName = borderName;
    i.captionText = "Top %";
    i.tooltip = "This is the margin for the top part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "topMargin";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 1;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    qDebug() << __FUNCTION__ << settingRootPath << i.value;
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.top = i.value.toDouble();
    addItem(i);

    i.name = "leftMargin";
    i.parentName = borderName;
    i.captionText = "Left %";
    i.tooltip = "This is the margin for the left part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "leftMargin";
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
    i.parentName = borderName;
    i.captionText = "Right %";
    i.tooltip = "This is the margin for the right part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "rightMargin";
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
    i.parentName = borderName;
    i.captionText = "Bottom %";
    i.tooltip = "This is the margin for the bottom part of the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "bottomMargin";
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
    i.parentName = borderName;
    i.captionText = "Tile";
    i.tooltip = "Select a tile that will be used to fill the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "tile";
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
    i.parentName = borderName;
    i.captionText = "Border color";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
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
    i.parentName = borderName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the border.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
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
    i.parentName = borderName;
    i.captionText = "Outine width";
    i.tooltip = "This is the outline for the border (% of the long side).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "outlineWidth";
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
    i.parentName = borderName;
    i.captionText = "Outline color";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "outlineColor";
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
    i.parentName = borderName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "style";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "None" << "Style 1";
    border.style = i.value.toString();
    addItem(i);

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
    qDebug() << __FUNCTION__ << "count =" << count;
    i.isHeader = true;
    i.isDecoration = true;
    i.itemIndex = count;
    i.name = textName;
    i.parentName = "Texts";
    i.captionText = textName;
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_Checkbox;
    addItem(i);

    // get index to use as parent when update tree depending on source
    getIndex(i.name);
    QModelIndex parIdx = foundIdx;
    text.name = textName;

    i.name = "anchorObject";
    i.parentName = textName;
    i.captionText = "Put text in";
    i.tooltip = "Select a border or image to contain the text.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorObject";
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
    i.parentName = textName;
    i.captionText = "Border area";
    i.tooltip = "Select a border area to contain the text.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorContainer";
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

    i.name = "x";
    i.parentName = textName;
    i.captionText = "x coordinate";
    i.tooltip = "The x coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "x";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    qDebug() << __FUNCTION__ << settingRootPath << i.value;
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    text.x = i.value.toDouble();
    addItem(i);

    i.name = "y";
    i.parentName = textName;
    i.captionText = "y coordinate";
    i.tooltip = "The y coordinate for the container (0-100). Top left = 0,0.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "y";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 10 * count;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    qDebug() << __FUNCTION__ << settingRootPath << i.value;
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    text.y = i.value.toDouble();
    addItem(i);

    i.name = "anchorPoint";
    i.parentName = textName;
    i.captionText = "Anchor point";
    i.tooltip = "Select a text box anchor point.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "anchorPoint";
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

    i.name = "source";
    i.parentName = textName;
    i.captionText = "Source";
    i.tooltip = "Select a text box anchor point.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "source";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Text";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Text" << "Metadata field" << "Metadata template";
    text.source = i.value.toString();
    addItem(i);

    // remember index from PropertyEditor::addItem when update visible source rows below
    QModelIndex sourceIdx = capIdx;

    i.name = "text";
    i.parentName = textName;
    i.captionText = "Text";
    i.tooltip = "Enter text to be displayed.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "text";
    if (setting->contains(settingRootPath + i.key)) {
        i.value = setting->value(settingRootPath + i.key);
    }
    else {
        i.value = "Text #" + QString::number(count + 1);
        qDebug() << __FUNCTION__ << i.value;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_LineEdit;
    i.type = "QString";
    border.color = i.value.toString();
    text.text = i.value.toString();
    addItem(i);

    i.name = "metadataField";
    i.parentName = textName;
    i.captionText = "Meta field";
    i.tooltip = "Select a metadata field.  The metadata will be substituted in the text box.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "metadataField";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "Title";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << is->tokens;
    text.metadataField = i.value.toString();
    addItem(i);

    i.name = "metadataTemplate";
    i.parentName = textName;
    i.captionText = "Meta template";
    i.tooltip = "Select a metadata template.  A template can include multiple metadata \n"
                "fields and user input text.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "metadataField";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Text" << "Metadata field" << "Metadata template";
    text.metadataTemplate = i.value.toString();
    addItem(i);

    // update tree based on source
    qDebug() << __FUNCTION__ << "Update text source" << sourceIdx;
    textItemChange(sourceIdx);

    i.name = "size";
    i.parentName = textName;
    i.captionText = "Size";
    i.tooltip = "Enter a percentage of the long side pixels to set a font size in pixels.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "size";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = 2.0;
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    text.size = i.value.toDouble();
    addItem(i);

    i.name = "font";
    i.parentName = textName;
    i.captionText = "Font";
    i.tooltip = "Select a font.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "font";
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
    i.parentName = textName;
    i.captionText = "Bold";
    i.tooltip = "Use bold font.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "bold";
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
    i.parentName = textName;
    i.captionText = "Italic";
    i.tooltip = "Use italic font.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "italic";
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
    i.parentName = textName;
    i.captionText = "Color";
    i.tooltip = "Select a color that will be used to fill the border area.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "color";
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
    i.parentName = textName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the text.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "opacity";
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
    i.parentName = textName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the text.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "style";
    if (setting->contains(settingRootPath + i.key))
        i.value = setting->value(settingRootPath + i.key);
    else {
        i.value = "";
        setting->setValue(settingRootPath + i.key, i.value);
    }
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "None" << "Style 1";
    border.style = i.value.toString();
    addItem(i);

    // add the text info to the vector of texts t
    t << text;
}
