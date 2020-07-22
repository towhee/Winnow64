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
                Border0
                ...
            Texts
                Text0
                ...
            Rectangles
                Rectangle0
                ...
            Graphics
                Graphic0
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

    setSolo(true);
    collapseAll();
    expand(model->index(0,0,QModelIndex()));
    setIndentation(12);
    setAlternatingRowColors(false);
    resizeColumns("====captions column====",
                  "====values column====");

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

void EmbelProperties::diagnostic(QModelIndex parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
}

void EmbelProperties::renameCurrentTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    e->test();
//    mw3->embel->test();
    return;

    QModelIndex idx = sourceIdx["templateList"];
    model->setData(idx, templateName);
    propertyDelegate->setEditorData(templateListEditor, idx);
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
    templateId = 0;
    templateName = "Do not Embellish";
    setting->beginGroup("Embel/Templates");
    // 1st time, no templates created yet
    if (setting->childGroups().count() == 0) {
        setting->setValue("Do not Embellish/isCurrent", true);
//        templateId = 0;
//        templateName = "Do not Embellish";
        templateList << templateName;
    }
    else {
        for (int i = 0; i < setting->childGroups().count(); ++i) {
            templateList << setting->childGroups().at(i);
        }
//        templateName = "";
        for (int i = 1; i < templateList.size(); ++i) {
            QString t = templateList.at(i);
            QString path = /*"Embel/Templates/" + */t + "/isCurrent";
            if (setting->value(path).toBool()) {
                templateName = t;
                templateId = i;
            }
        }
        qDebug() << __FUNCTION__ << templateName << templateId;
//        templatePath = "Embel/Templates/" + templateName + "/";
    }
    setting->endGroup();
    templatePath = "Embel/Templates/" + templateName + "/";
    if (templateId == 0) G::isEmbellish = false;
    else G::isEmbellish = true;
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
        if (grandparent == "BordersHeader") borderItemChange(idx);
        if (grandparent == "TextsHeader") textItemChange(v, source, parent);
        if (grandparent == "RectanglesHeader") rectangleItemChange(v, source, parent);
        if (grandparent == "GraphicsHeader") graphicItemChange(v, source, parent);
    }

    e->build();
}

void EmbelProperties::templateChange(QVariant v)
{
    qDebug() << __FUNCTION__;
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
    // clear borders
    b.clear();
    // add items for this template
    if (templateId > 0) addTemplateItems();
    collapseAll();
    collapseAll();
    expand(model->index(_templates,0));
    expand(model->index(_borders,0));
    expand(model->index(_texts,0));
    expand(model->index(_rectangles,0));
    expand(model->index(_graphics,0));
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
        b[index].tile = v.toString();
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
        setting->setValue(path, v.toInt());
        b[index].outlineColor = v.toString();
    }

    if (source == "style") {
        setting->setValue(path, v.toString());
        b[index].style = v.toString();
    }
}

void EmbelProperties::textItemChange(QVariant v, QString source, QString parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
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
    i.valueName = "templateList";
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
    i.decorateGradient = true;
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "horizontalFit";
    if (setting->contains(templatePath + "File/" + i.valueName))
        i.value = setting->value(templatePath + "File/" + i.valueName);
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "verticalFit";
    if (setting->contains(templatePath + "File/" + i.valueName))
        i.value = setting->value(templatePath + "File/" + i.valueName);
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "fileType";
    i.value = setting->value(templatePath + "File/" + i.valueName);
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "saveMethod";
    i.value = setting->value(templatePath + "File/" + i.valueName);
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "folderPath";
    i.value = setting->value(templatePath + "File/" + i.valueName);
    i.delegateType = DT_LineEdit;
    i.type = "string";
    f.folderPath = i.value.toString();
    addItem(i);

    // FILES Suffix name
    i.name = "suffix";
    i.parentName = "FileHeader";
    i.captionText = "Suffix";
    i.tooltip = "Suffix to add to file names.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "suffix";
    i.value = setting->value(templatePath + "File/" + i.valueName);
    i.delegateType = DT_LineEdit;
    i.type = "string";
    f.suffix = i.value.toString();
    addItem(i);

    // FILES Overwrite existing files
    i.name = "overwriteFiles";
    i.parentName = "FileHeader";
    i.captionText = "Overwrite";
    i.tooltip = "Overwrite existing files.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "overwriteFiles";
    i.value = setting->value(templatePath + "File/" + i.valueName);
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
    i.decorateGradient = true;
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "outlineWidth";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "outlineColor";
    if (setting->contains(settingRootPath + i.valueName)) {
        i.value = setting->value(settingRootPath + i.valueName);
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "imageStyle";
    i.valueName = "outlineColor";
    if (setting->contains(settingRootPath + i.valueName)) {
        i.value = setting->value(settingRootPath + i.valueName);
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
    i.name = "BordersHeader";
    i.parentName = "???";
    i.decorateGradient = true;
    i.isDecoration = false;
    i.captionText = "Borders";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    borderDeleteBtn = new BarBtn();
    borderDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    borderDeleteBtn->setToolTip("Delete the open border");
    btns.append(borderDeleteBtn);
    BarBtn *borderNewBtn = new BarBtn();
    borderNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    borderNewBtn->setToolTip("Create a new border");
    btns.append(borderNewBtn);
    connect(borderNewBtn, &BarBtn::clicked, this, &EmbelProperties::newBorder);
    addItem(i);

    QString path = templatePath + "/Borders";
    setting->beginGroup(path);
    int borderCount = setting->childGroups().size();
    qDebug() << __FUNCTION__ << path << "borderCount =" << borderCount;
    setting->endGroup();
    for (int i = 0; i < borderCount; ++i) newBorder();
}

void EmbelProperties::addTexts()
{
    // Texts header (Root)
    i.name = "TextsHeader";
    i.parentName = "???";
    i.decorateGradient = true;
    i.isDecoration = false;
    i.captionText = "Text Items";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    textDeleteBtn = new BarBtn();
    textDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    textDeleteBtn->setToolTip("Delete the open text item");
    btns.append(textDeleteBtn);
    BarBtn *textNewBtn = new BarBtn();
    textNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    textNewBtn->setToolTip("Create a new border");
    btns.append(textNewBtn);
    addItem(i);
}

void EmbelProperties::addRectangles()
{
    // Rectangles header (Root)
    i.name = "RectanglesHeader";
    i.parentName = "???";
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
    BarBtn *rectangleNewBtn = new BarBtn();
    rectangleNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    rectangleNewBtn->setToolTip("Create a new border");
    btns.append(rectangleNewBtn);
    addItem(i);
}

void EmbelProperties::addGraphics()
{
    // Graphics header (Root)
    i.name = "GraphicsHeader";
    i.parentName = "???";
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
    BarBtn *graphicNewBtn = new BarBtn();
    graphicNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    graphicNewBtn->setToolTip("Create a new graphic");
    btns.append(graphicNewBtn);
    addItem(i);
}

void EmbelProperties::newBorder()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    getIndex("BordersHeader");
    int row = model->rowCount(foundIdx);
    qDebug() << __FUNCTION__ << row << foundIdx << foundIdx.data();
    addBorder(row);
    // expand only the new border
    collapseAll();
    // get the parent of the new border
    getIndex("BordersHeader");
    // get the new border
    QModelIndex idx = model->index(row, 0, foundIdx);
    setExpanded(foundIdx, true);
    setExpanded(idx, true);
}

void EmbelProperties::addBorder(int count)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Border name (used as node in settings and treeview)
    QString borderName = "Border" + QString::number(count + 1);
    QString settingRootPath = templatePath + "Borders/" + borderName + "/";

    // subheader for this border
    qDebug() << __FUNCTION__ << "count =" << count;
    i.itemIndex = count;
    i.name = borderName;
    i.parentName = "BordersHeader";
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "topMargin";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = 0;
    qDebug() << __FUNCTION__ << i.value;
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "leftMargin";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = 0;
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "rightMargin";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = 0;
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "bottomMargin";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = 0;
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
    i.tooltip = "Select a tile that will be used to full the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "tile";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = "";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Use color" << "Tile 1" << "Tile 2";
    border.tile = i.value.toString();
    addItem(i);

    i.name = "color";
    i.parentName = borderName;
    i.captionText = "Border color";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "color";
    if (setting->contains(settingRootPath + i.valueName)) {
        i.value = setting->value(settingRootPath + i.valueName);
    }
    else i.value = "";
    i.delegateType = DT_Color;
    i.type = "QString";
    border.color = i.value.toString();
    addItem(i);

    i.name = "opacity";
    i.parentName = borderName;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the border.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "opacity";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = 100;
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "outlineWidth";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = 0;
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "outlineColor";
    if (setting->contains(settingRootPath + i.valueName)) {
        i.value = setting->value(settingRootPath + i.valueName);
    }
    else i.value = "";
    i.delegateType = DT_Color;
    i.type = "QString";
    border.outlineColor = i.value.toString();
    addItem(i);

    i.name = "style";
    i.parentName = borderName;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "style";
    if (setting->contains(settingRootPath + i.valueName))
        i.value = setting->value(settingRootPath + i.valueName);
    else i.value = "";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "None" << "Style 1";
    border.style = i.value.toString();
    addItem(i);

    // add the border info to the vector of borders
    b << border;
}

void EmbelProperties::mouseDoubleClickEvent(QMouseEvent */*event*/)
{
    qDebug() << __FUNCTION__;
    // ignore
}

void EmbelProperties::confirmDelete(QModelIndex idx)
{

    QMessageBox msg;
    msg.setStyleSheet(G::css);
    if (selectionModel()->selection().isEmpty()) {
        QMessageBox::information(this, "Delete", "No item selected for deletion");
        return;
    }
    QModelIndex delIdx = selectionModel()->selection().indexes().first();
    delIdx = model->index(delIdx.row(), 0, delIdx.parent());
}

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
        qDebug() << __FUNCTION__ << wasExpanded;
        selectionModel()->clear();
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
file or image).  If not, if there was a previous selection, revert to that.  Turn off all
deletes except in the selected category.
*/
{
//    qDebug() << __FUNCTION__ << selection.isEmpty() <<  selection.indexes();
    if (selection.isEmpty()) return;
    if (prevSelection.isEmpty()) return;
    QModelIndex idx = selection.indexes().first();
    QModelIndex prevIdx = prevSelection.indexes().first();
    if (!okToSelect(idx) && okToSelect(prevIdx)) {
        selectionModel()->select(prevSelection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        idx = selection.indexes().first();
    }
    else {
        selectionModel()->clear();
    }
    // make sure using index for column 0
    idx = model->index(idx.row(), 0, idx.parent());
    // deal with delete buttons
//    QStringList delBtns;
//    delBtns << "Borders" << "Texts" << "Rectangles" << "Graphics";
    QString selectedCategory = idx.parent().data(UR_Name).toString();
    /*if (delBtns.contains(selectedCategory))*/ showRelevantDeleteBtn(selectedCategory);
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
    borderDeleteBtn->setVisible(false);
    return;
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
