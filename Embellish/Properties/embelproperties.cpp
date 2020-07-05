#include "embelproperties.h"

EmbelProperties::EmbelProperties(QWidget *parent, QSettings* setting): PropertyEditor(parent)
{
    resizeColumns("======String to fit in captions column======",
                  "=String to fit in values column=");
    addItems();
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
    QModelIndex index = idx.data(UR_QModelIndex).toModelIndex();
}

void EmbelProperties::addBorders()
{
    getIndex("Borders");
    int borderCount = model->rowCount(foundIdx) + 1;

}

void EmbelProperties::newBorder()
{
    getIndex("BordersHeader");
    int row = model->rowCount(foundIdx);
    qDebug() << __FUNCTION__ << row << foundIdx << foundIdx.data();
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
    QModelIndex idx = model->index(row, 0, foundIdx);
    setExpanded(foundIdx, true);
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

void EmbelProperties::addItems()
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
    BarBtn *templateDeleteBtn = new BarBtn();
    templateDeleteBtn->setIcon(QIcon(":/images/icon16/delete.png"));
    templateDeleteBtn->setToolTip("Delete the selected template");
    btns.append(templateDeleteBtn);
    BarBtn *templateNewBtn = new BarBtn();
    templateNewBtn->setIcon(QIcon(":/images/icon16/new.png"));
    templateNewBtn->setToolTip("Create a new template");
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
    i.dropList << "Publish 2048px Gray" << "Gloria title" << "Slideshow title";
    addItem(i);

    // end TEMPLATES
    }

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

    // FILES Fit strategy
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
    addItem(i);

    // FILES Horizontal fit
    i.name = "horizontalFit";
    i.parentName = "FileHeader";
    i.captionText = "Maximum horizontal pixels";
    i.tooltip = "The number of pixels in the horizontal axis including the borders";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "horizontalFit";
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
    i.value = 0;
    i.valueName = "verticalFit";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 10000;
    i.fixedWidth = 50;
    addItem(i);

    // FILES File type
    i.name = "fileList";
    i.parentName = "FileHeader";
    i.captionText = "Select file type";
    i.tooltip = "Select file type.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "fileList";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Tiff" << "Jpg" << "Png";
    addItem(i);

    // FILES Save method
    i.name = "saveLocationOptionsList";
    i.parentName = "FileHeader";
    i.captionText = "Select save method";
    i.tooltip = "Select where to save the file.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "saveLocationOptionsList";
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
    i.value = "";
    i.valueName = "folderPath";
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
    i.value = "";
    i.valueName = "suffix";
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
    i.value = false;
    i.valueName = "overwriteFiles";
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
    {

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

    /*
    // BORDER border0 header
    i.name = "borderHeader0";
    i.parentName = "BordersHeader";
    i.captionText = "Border 1";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // BORDER Border0 top size
    i.name = "border0_TopSize";
    i.parentName = "borderHeader0";
    i.captionText = "Top amount";
    i.tooltip = "This is the width of the top part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "border0_TopSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Border0 Left size
    i.name = "border0_LeftSize";
    i.parentName = "borderHeader0";
    i.captionText = "Left amount";
    i.tooltip = "This is the width of the left part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "border0_LeftSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Border0 right size
    i.name = "border0_RightSize";
    i.parentName = "borderHeader0";
    i.captionText = "Right amount";
    i.tooltip = "This is the width of the right part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "border0_RightSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Border0 bottom size
    i.name = "border0_BottomSize";
    i.parentName = "borderHeader0";
    i.captionText = "Bottom amount";
    i.tooltip = "This is the width of the bottom part of the border (% of the long side).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "border0_BottomSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // BORDER Tile
    i.name = "borderTile";
    i.parentName = "borderHeader0";
    i.captionText = "Tile";
    i.tooltip = "Select a tile that will be used to full the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "borderTile";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Use color" << "Tile 1" << "Tile 2";
    addItem(i);

    // BORDER Color
    i.name = "borderColor";
    i.parentName = "borderHeader0";
    i.captionText = "Colour (#RRGGBB)";
    i.tooltip = "Select a color that will be used to full the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "borderColor";
    i.delegateType =
    i.delegateType = DT_Color;
    i.type = "QString";
    addItem(i);

    // BORDER Opacity
    i.name = "borderOpacityr";
    i.parentName = "borderHeader0";
    i.captionText = "Opacity";
    i.tooltip = "The opacity of the border.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "borderOpacityr";
    i.delegateType =  DT_Slider;
    i.min = 0;
    i.max = 100;
    i.type = "int";
    addItem(i);

    // BORDER Style
    i.name = "borderStyle";
    i.parentName = "borderHeader0";
    i.captionText = "Style";
    i.tooltip = "Select a style that will be applied to the border area.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "borderStyle";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "None" << "Style 1";
    addItem(i);

    // end BORDER
    */
    }

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
