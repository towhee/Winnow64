#include "embelproperties.h"

EmbelProperties::EmbelProperties(QWidget *parent): PropertyEditor(parent)
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

void EmbelProperties::addItems()
{
    ItemInfo i;
    /* template of ItemInfo
    i.name = "";
    i.parentName = "";
    i.captionText = "";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "";
    i.delegateType = DT_None;
    i.min = 0;
    i.max = 0;
    i.fixedWidth = 50;
    i.color = "#AABBCC";
    i.index = QModelIndex();
    i.dropList = {"1", "2"};
    */

    // TEMPLATES ============================================================================//

    // Templates header (Root)
    i.name = "TemplatesHeader";
    i.parentName = "???";
    i.captionText = "Templates";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Templates
    i.name = "templateList";
    i.parentName = "TemplatesHeader";
    i.captionText = "Select template";
    i.tooltip = "Templates contain all the properties to embellish your images.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0;
    i.valueName = "templateList";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "Publish 2048px Gray" << "Gloria title" << "Slideshow title";
    addItem(i);

    // FILES ================================================================================//

    // File header (Root)
    i.name = "FileHeader";
    i.parentName = "???";
    i.captionText = "File";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Fit strategy
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

    // Horizontal fit
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

    // Vertical fit
    i.name = "verticalFit";
    i.parentName = "FileHeader";
    i.captionText = "Maximum horizontal pixels";
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

    // File type
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

    // Save method
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

    // Folder name
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

    // Suffix name
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

    // Overwrite existing files
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

    // Image header (Root)
    i.name = "ImageHeader";
    i.parentName = "???";
    i.captionText = "Image";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Borders header (Root)
    i.name = "BordersHeader";
    i.parentName = "???";
    i.captionText = "Borders";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Texts header (Root)
    i.name = "TextHeader";
    i.parentName = "???";
    i.captionText = "Text Items";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Rectangles header (Root)
    i.name = "RectanglesHeader";
    i.parentName = "???";
    i.captionText = "Rectangles";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Graphics header (Root)
    i.name = "GraphicsHeader";
    i.parentName = "???";
    i.captionText = "Graphics";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);


}
