#include "embelproperties.h"

EmbelProperties::EmbelProperties(QWidget *parent, QSettings* setting): PropertyEditor(parent)
{
    this->setting = setting;
    resizeColumns("======String to fit in captions column======",
                  "=String to fit in values column=");
    // get the list of templates and which one was last active
    readTemplateList();
    // add the template header, which then selects to last active template
    addTemplateHeader();
    // add the File, Image, Borders, Textx, Rectangles and Graphics items for the template
    addTemplateItems();
    expandAll();
}

void EmbelProperties::diagnostic(QModelIndex parent)
{
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
//    qDebug() << __FUNCTION__ << sourceIdx;
    QModelIndex idx = sourceIdx["templateList"];
    model->setData(idx, templateName);
    propertyDelegate->setEditorData(templateListEditor, idx);
//    model->setData(sourceIdx[templateName], templateName);
}

void EmbelProperties::setCurrentTemplate()
{
    for (int i = 0; i < templateList.length(); ++i) {
        QString t = templateList.at(i);
        bool isCurrent = t == templateName;
        QString path = "Embel/Templates/" + t + "/isCurrent";
        setting->setValue(path, isCurrent);
    }
}

void EmbelProperties::readTemplateList()
{
    setting->beginGroup("Embel/Templates");
    templateList = setting->childGroups();
    setting->endGroup();
    qDebug() << __FUNCTION__ << templateList;
    templateName = "";
    for (int i = 0; i < templateList.size(); ++i) {
        QString t = templateList.at(i);
        QString path = "Embel/Templates/" + t + "/isCurrent";
        bool isCurrent = setting->value(path).toBool();
        qDebug() << __FUNCTION__ << t << isCurrent;
        if (setting->value(path).toBool()) {
            templateName = t;
            qDebug() << __FUNCTION__ << "The template is" << t;
        }
    }
    if (templateName == "") templateName = templateList.at(0);
    templatePath = "Embel/Templates/" + templateName + "/";
}

void EmbelProperties::newTemplate()
{
    templateName = Utilities::inputText("New Template", "Enter new template name");
    templateList << templateName;
    QString path = "Embel/Templates/" + templateName + "/isCurrent";
    setting->setValue(path, true);
}

void EmbelProperties::addBorders()
{
    getIndex("Borders");
    int borderCount = model->rowCount(foundCatIdx) + 1;
}

void EmbelProperties::newBorder()
{
    getIndex("BordersHeader");
    int row = model->rowCount(foundCatIdx);
    qDebug() << __FUNCTION__ << row << foundCatIdx << foundCatIdx.data();
    border.name = "Border" + QString::number(row + 1);
    border.caption = "Border " + QString::number(row + 1);
    border.top = 0;
    border.left = 0;
    border.right = 0;
    border.bottom = 0;
    border.tile = "";
    border.color = "";
    border.opacity = 100;
    border.style = "";
    addBorder();
    // expand only the new border
    collapseAll();
    // get the parent of the new border
    getIndex("BordersHeader");
    // get the new border
    QModelIndex idx = model->index(row, 0, foundCatIdx);
    setExpanded(foundCatIdx, true);
    setExpanded(idx, true);
}

void EmbelProperties::addBorder()
{
    // BORDER border0 header
    i.name = border.name;
    i.parentName = "BordersHeader";
    i.captionText = border.caption;
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // BORDER Border0 top size
    i.name = "topSize";
    i.parentName = border.name;
    i.captionText = "Top amount";
    i.tooltip = "This is the width of the top part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.top;
    i.valueName = "topSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Border0 Left size
    i.name = "leftSize";
    i.parentName = border.name;
    i.captionText = "Left amount";
    i.tooltip = "This is the width of the left part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.left;
    i.valueName = "leftSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Border0 right size
    i.name = "rightSize";
    i.parentName = border.name;
    i.captionText = "Right amount";
    i.tooltip = "This is the width of the right part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.right;
    i.valueName = "rightSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Border0 bottom size
    i.name = "bottomSize";
    i.parentName = border.name;
    i.captionText = "Bottom amount";
    i.tooltip = "This is the width of the bottom part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.bottom;
    i.valueName = "bottomSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Tile
    i.name = "borderTile";
    i.parentName = border.name;
    i.captionText = "Tile";
    i.tooltip = "Select a tile that will be used to full the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.tile;
    i.valueName = "borderTile";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Use color" << "Tile 1" << "Tile 2";
    addItem(i);

    // BORDER Color
    i.name = "borderColor";
    i.parentName = border.name;
    i.captionText = "Colour (#RRGGBB)";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.color;
    i.valueName = "borderColor";
    i.delegateType =
    i.delegateType = DT_Color;
    i.type = "QString";
    addItem(i);

    // BORDER Opacity
    i.name = "borderOpacityr";
    i.parentName = border.name;
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the border.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.opacity;
    i.valueName = "borderOpacityr";
    i.delegateType =  DT_Slider;
    i.min = 0;
    i.max = 100;
    i.type = "int";
    addItem(i);

    // BORDER Style
    i.name = "borderStyle";
    i.parentName = border.name;
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = border.style;
    i.valueName = "borderStyle";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "None" << "Style 1";
    addItem(i);
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

    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    templatePath = "Embel/Templates/" + templateName + "/";
//    QModelIndex index = idx.data(UR_QModelIndex).toModelIndex();
//    qDebug() << __FUNCTION__ << v << source << index;

    if (source == "templateList") {
        qDebug() << __FUNCTION__;
        templateName = v.toString();
        templatePath = "Embel/Templates/" + templateName + "/";
        // save which template is current
        setCurrentTemplate();
        // clear model except for template name
        model->removeRows(1, model->rowCount(QModelIndex()) - 1);
        // add items for this template
        addTemplateItems();
        return;
    }

    if (source == "horizontalFit") {
        setting->setValue(templatePath + "File/" + source, v.toInt());
    }

    if (source == "verticalFit") {
        setting->setValue(templatePath + "File/" + source, v.toInt());
    }

    if (source == "fileType") {
        setting->setValue(templatePath + "File/" + source, v.toString());
    }

    if (source == "saveMethod") {
        setting->setValue(templatePath + "File/" + source, v.toString());
    }

    if (source == "folderPath") {
        setting->setValue(templatePath + "File/" + source, v.toString());
    }

    if (source == "suffix") {
        setting->setValue(templatePath + "File/" + source, v.toString());
    }

    if (source == "overwriteFiles") {
        setting->setValue(templatePath + "File/" + source, v.toBool());
    }

}

void EmbelProperties::addTemplateHeader()
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
    connect(templateRenameBtn, &BarBtn::clicked, this, &EmbelProperties::renameCurrentTemplate);
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
    i.value = templateName;
    i.valueName = "templateList";
    i.delegateType = DT_Combo;      // no parent, delegateType > 0 -> No header, has value
    i.type = "QString";
    i.dropList = templateList;
    templateListEditor = addItem(i);

    // select the active template
    QModelIndex idx = sourceIdx["templateList"];
    model->setData(idx, templateName);
    propertyDelegate->setEditorData(templateListEditor, idx);

    if (templateList.size() == 0) {
        QString msg = "You must create a template as a first step before adding\n"
                      "borders, text, rectangles or graphics.";
        G::popUp->showPopup(msg, 2000);
    }
}

void EmbelProperties::addTemplateItems()
{
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

    // IMAGE style
    i.name = "imageStyle";
    i.parentName = "ImageHeader";
    i.captionText = "Select style";
    i.tooltip = "Select style to apply to the image ie a shadow.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "imageStyle";
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

    // TEXT =================================================================================//

    // Texts header (Root)
    i.name = "TextHeader";
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
