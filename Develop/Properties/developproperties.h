#ifndef DEVELOPPROPERTIES_H
#define DEVELOPPROPERTIES_H

#include <QtWidgets>
#include "PropertyEditor/propertyeditor.h"
#include "Develop/editparams.h"

class MW;

/*
    Develop dock property tree (Lightroom-style parametric edits). It mirrors the
    EmbelProperties pattern: a PropertyEditor subclass that builds a tree of section
    headers, sliders, checkboxes and a combo. All values persist to QSettings under
    the "Develop/" branch.

    Layers (header with minus/plus, the same idiom as Embellish templates) let several
    independent adjustment sets be stored. Switching the "Select layer" combo rebuilds
    the Basic / Effects sections from that layer's saved values. Each layer maps onto a
    single EditParams -- the one source of truth read by the Develop processor (and, for
    RAW, the white-balance / denoise steps inside RawFormat).

    Binding the UI to the decode pipeline (re-decode on change) is deferred; for now a
    change persists to settings and emits paramsChanged() so the hook is ready.
*/
class DevelopProperties : public PropertyEditor
{
    Q_OBJECT
public:
    DevelopProperties(QWidget *parent, QSettings *setting);

    QStringList layerList;
    QString layerName;
    int layerId = 0;

    /* Current layer's values assembled into an EditParams. */
    EditParams editParams();
    QString diagnostics();

public slots:
    void itemChange(QModelIndex idx) override;

signals:
    void paramsChanged();           // a develop value changed (decode hook; deferred)
    void centralMsg(QString msg);

private:
    void initialize();
    void readLayerList();
    void setCurrentLayer(QString name);

    void addLayersHeader();
    void addLayerItems();           // Basic + Effects for the current layer
    void addBasic();
    void addEffects();

    void newLayer();
    void deleteLayer();

    /* Item builders. div converts the integer slider amount to a double (eg /100), and
       defaults to identity (0) so an absent value is a no-op edit. */
    void addHeader(const QString &name, const QString &caption, const QString &tooltip);
    void addSlider(const QString &key, const QString &caption, const QString &tooltip,
                   QModelIndex parIdx, const QString &parentName,
                   int min, int max, int div, double defaultValue = 0);
    void addCheckbox(const QString &key, const QString &caption, const QString &tooltip,
                     QModelIndex parIdx, const QString &parentName, bool defaultValue = false);

    QString layerRootPath() const;  // "Develop/Layers/<layerName>/"
    double layerValue(const QString &key, double defaultValue = 0) const;
    QString uniqueLayerName(const QString &name) const;

    MW *mw;
    QSettings *setting;
    ItemInfo i;

    QModelIndex root;
    QModelIndex layersIdx;
    ComboBoxEditor *layerListEditor = nullptr;

    /* Order of root rows in the tree. */
    enum roots { _layers, _basic, _effects };
};

#endif // DEVELOPPROPERTIES_H
