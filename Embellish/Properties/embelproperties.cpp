#include "embelproperties.h"
//#include "Embellish/embel.h"
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
    setIndentation(12);
    setAlternatingRowColors(false);
    resizeColumns("======String to fit in captions column======",
                  "=String to fit in values column=");

    // get the list of templates and which one was last active
    readTemplateList();
    // add the template header, which then selects to last active template
    addTemplateHeader();
    // add the File, Image, Borders, Texts, Rectangles and Graphics items for the template
    if (templateId != 0) addTemplateItems();
    expandAll();

    // test connection to Embel
    f.suffix = "testsuffix";
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
    for (int i = 1; i < templateList.length(); ++i) {
        QString t = templateList.at(i);
        bool isCurrent = t == templateName;
        QString path = "Embel/Templates/" + t + "/isCurrent";
        setting->setValue(path, isCurrent);
    }
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
//    if (!setting->contains("Embel/Templates")) return;

    setting->beginGroup("Embel/Templates");
    // 1st time, no templates created yet
    if (setting->childGroups().count() == 0) {
        setting->setValue("Do not Embellish/isCurrent", true);
        templateList << "Do not Embellish";
        templateId = 0;
    }
    else {
        for (int i = 0; i < setting->childGroups().count(); ++i) {
            templateList << setting->childGroups().at(i);
        }
        templateName = "";
        for (int i = 1; i < templateList.size(); ++i) {
            QString t = templateList.at(i);
            QString path = /*"Embel/Templates/" + */t + "/isCurrent";
            if (setting->value(path).toBool()) {
                templateName = t;
                templateId = i;
            }
        }
        templatePath = "Embel/Templates/" + templateName + "/";
    }
    setting->endGroup();
}

void EmbelProperties::newTemplate()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    templateName = Utilities::inputText("New Template", "Enter new template name");
    templateList << templateName;
    QString path = "Embel/Templates/" + templateName + "/isCurrent";
    setting->setValue(path, true);
    // if new template called from main menu the dock might not be visible
    if(!isVisible()) setVisible(true);
    G::isEmbel = true;
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
    qDebug() << __FUNCTION__ << v << source << parent << grandparent;

    if (source == "templateList") {
        qDebug() << __FUNCTION__;
        templateName = v.toString();
        // save which template is current
        setCurrentTemplate();
        // clear model except for template name
        model->removeRows(1, model->rowCount(QModelIndex()) - 1);
        // add items for this template
        addTemplateItems();
    }
    if (templateId == 0) return;

    if (parent == "FileHeader") fileItemChange(v, source);
    if (parent == "ImageHeader") imageItemChange(v, source);
    if (grandparent == "BordersHeader") borderItemChange(idx);
    if (grandparent == "TextsHeader") textItemChange(v, source, parent);
    if (grandparent == "RectanglesHeader") rectangleItemChange(v, source, parent);
    if (grandparent == "GraphicsHeader") graphicItemChange(v, source, parent);

    e->build();
}

void EmbelProperties::fileItemChange(QVariant v, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = templatePath + "File/" + source;

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
    connect(templateNewBtn, &BarBtn::clicked, this, &EmbelProperties::newTemplate);
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
    i.dropList = templateList;
    templateListEditor = addItem(i);

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
    /* template of ItemInfo
    i.name = "";                    // all
    i.parentName = "";              // all
    i.hasValue = true;              // all
    i.tooltip = "";                 // all
    i.captionText = "";             // all
    i.captionIsEditable = false;    // all
    i.delegateType= DT_None;        // all
    i.value = 0;                    // except hdr
    i.valueName = "";               // except hdr
    i.type = "";                    // except hdr
    i.min = 0;                      // DT_Spinbox, DT_Slider
    i.max = 0;                      // DT_Spinbox, DT_Slider
    i.fixedWidth = 50;              // DT_Slider
    i.dropList = {}                 // DT_Combo
    i.index = QModelIndex();        // except hdr if connected to datamodel (ie InfoView fields to show)
    */

    // TEMPLATES ============================================================================//

    /*
       {

    // Templates header (Root)
    i.name = "TemplatesHeader";
    i.parentName = "???";
    i.decorateGradient = true;
    i.captionText = "Templates";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *templateRenameBtn = new BarBtn();
    templateRenameBtn->setIcon(QIcon(":/images/icon16/delta.png"));
    templateRenameBtn->setToolTip("Rename the selected template");
    btns.append(templateRenameBtn);
    connect(templateRenameBtn, &BarBtn::clicked, this, &EmbelProperties::clearTemplate);
    BarBtn *templateDeleteBtn = new BarBtn();
    templateDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    templateDeleteBtn->setToolTip("Delete the selected template");
    btns.append(templateDeleteBtn);
    BarBtn *templateNewBtn = new BarBtn();
    templateNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    templateNewBtn->setToolTip("Create a new template");
    connect(templateNewBtn, &BarBtn::clicked, this, &EmbelProperties::newTemplate);
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
    i.value = 0;
    i.valueName = "templateList";
    i.delegateType = DT_Combo;      // no parent, delegateType > 0 -> No header, has value
    i.type = "QString";
    qDebug() << __FUNCTION__ << templateList;
    i.dropList = templateList;
    templateListEditor = addItem(i);

    QModelIndex templatesIdx = model->index(0, 0, QModelIndex());
    templateCount = templatesIdx.data(UR_StringList).toStringList().count();
    if (templateCount == 0) {
        QString msg = "You must create a template as a first step before adding\n"
                      "borders, text, rectangles or graphics.";
        G::popUp->showPopup(msg, 2000);
    }

    // end TEMPLATES
    }*/

    // FILES ================================================================================//
    {

    // FILES File header (Root)
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
    i.captionText = "Maximum horizontal pixels";
    i.tooltip = "The number of pixels in the horizontal axis including the borders";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "horizontalFit";
    i.value = setting->value(templatePath + "File/" + i.valueName);
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
    i.captionText = "Maximum vertical pixels";
    i.tooltip = "The number of pixels in the vertical axis including the borders";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "verticalFit";
    i.value = setting->value(templatePath + "File/" + i.valueName);
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
    i.captionText = "Select file type";
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
    i.captionText = "Select save method";
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
    i.captionText = "The full folder path";
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
    i.captionText = "Suffix to add to file names";
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
    i.captionText = "Overwrite existing files";
    i.tooltip = "Overwrite existing files.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "overwriteFiles";
    i.value = setting->value(templatePath + "File/" + i.valueName);
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    f.overwriteFiles = i.value.toBool();
    addItem(i);

    // end FILES
    }

    // IMAGE ================================================================================//
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
    else i.value = 0;
    i.delegateType = DT_DoubleSpinbox;
    i.type = "double";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    border.outlineWidth = i.value.toDouble();
    addItem(i);

    i.name = "outlineColor";
    i.parentName = "ImageHeader";
    i.captionText = "Outline color (#RRGGBB)";
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

    // IMAGE style
    i.name = "imageStyle";
    i.parentName = "ImageHeader";
    i.captionText = "Select style";
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

    // end IMAGE
    }

    // BORDER ===============================================================================//

    // BORDER header (Root)
    i.name = "BordersHeader";
    i.parentName = "???";
    i.decorateGradient = true;
    i.captionText = "Borders";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *borderDeleteBtn = new BarBtn();
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

    // TEXT =================================================================================//

    // Texts header (Root)
    i.name = "TextsHeader";
    i.parentName = "???";
    i.decorateGradient = true;
    i.captionText = "Text Items";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *textDeleteBtn = new BarBtn();
    textDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    textDeleteBtn->setToolTip("Delete the open text item");
    btns.append(textDeleteBtn);
    BarBtn *textNewBtn = new BarBtn();
    textNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    textNewBtn->setToolTip("Create a new border");
    btns.append(textNewBtn);
    addItem(i);

    // RECTANGLE ============================================================================//

    // Rectangles header (Root)
    i.name = "RectanglesHeader";
    i.parentName = "???";
    i.decorateGradient = true;
    i.captionText = "Rectangles";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *rectangleDeleteBtn = new BarBtn();
    rectangleDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    rectangleDeleteBtn->setToolTip("Delete the open rectangle");
    btns.append(rectangleDeleteBtn);
    BarBtn *rectangleNewBtn = new BarBtn();
    rectangleNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    rectangleNewBtn->setToolTip("Create a new border");
    btns.append(rectangleNewBtn);
    addItem(i);

    // GRAPHIC ==============================================================================//

    // Graphics header (Root)
    i.name = "GraphicsHeader";
    i.parentName = "???";
    i.decorateGradient = true;
    i.captionText = "Graphics";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    BarBtn *graphicDeleteBtn = new BarBtn();
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
/*    border.name = "Border" + QString::number(row + 1);
    border.caption = "Border " + QString::number(row + 1);
    border.top = 0;
    border.left = 0;
    border.right = 0;
    border.bottom = 0;
    border.tile = "";
    border.color = "";
    border.opacity = 100;
    border.style = "";  */
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
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = UR_ItemIndex;
    addItem(i);

    i.name = "topMargin";
    i.parentName = borderName;
    i.captionText = "Top amount";
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
    i.captionText = "Left amount";
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
    i.captionText = "Right amount";
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
    i.captionText = "Bottom amount";
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
    i.captionText = "Border color (#RRGGBB)";
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
    i.captionText = "Outline color (#RRGGBB)";
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
    e->build();
}

void EmbelProperties::test2()
{
    qDebug() << __FUNCTION__;
    setting->remove("Embel/Templates");
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
