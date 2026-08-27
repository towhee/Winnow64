#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QtWidgets>
#include "propertyeditor.h"
#include "propertywidgets.h"

class Preferences : public PropertyEditor
{
    Q_OBJECT
public:
    Preferences(QWidget *parent);

public slots:
    void itemChange(QModelIndex idx) override;
    void rory();

private:
    ItemInfo i;
    void addItems();
    void addGeneral();
    void addModify();
    void addUserInterface();
    void addProductivity();
    void addDevPreviews();

    /* devPreview settings are shown as words but stored as numbers. These pairs are the
       only place the mapping lives, so a label and its value cannot drift apart. */
    static QString devPreviewSizeLabel(int maxEdge);
    static int devPreviewSizeValue(const QString &label);
    static QString devPreviewQualityLabel(int quality);
    static int devPreviewQualityValue(const QString &label);
    static QString devPreviewCacheLabel(qint64 bytes);
    static qint64 devPreviewCacheValue(const QString &label);
    void addSlideShow();
    void addFullScreen();
    void addMetadataPanel();
    void addTableView();
    void addUtilities();

    /* items we will want to modify dynamically in itemChange (returned from addItem)
       ie static_cast<LabelEditor*>(availMBMsgWidget)->setValue(availMBMsg);    */
    QWidget *availMBMsgWidget;
};

#endif // PREFERENCES_H
