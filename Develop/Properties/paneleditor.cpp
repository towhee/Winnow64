#include "Develop/Properties/paneleditor.h"
#include "PropertyEditor/propertywidgets.h"     // SliderEditor
#include "Main/global.h"

#include <QScrollBar>
#include <QMouseEvent>
#include <QVariantAnimation>

PanelEditor::PanelEditor(QWidget *parent) : PropertyEditor(parent)
{
    if (G::isLogger) G::log("PanelEditor::PanelEditor");

    /* Same chrome as the Develop tree so the rows render identically. */
    setIndentation(10);
    setAlternatingRowColors(false);
    setMouseTracking(false);
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::NoFrame);
    /* setFrameShape alone leaves a 1px gray frame once the app QSS styles QTreeView; a
       widget-level rule removes it so the editor blends into the panel. */
    setStyleSheet("QTreeView { border: none; }");
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAutoFillBackground(false);

    /* Column split identical to DevelopProperties (owners align via setCaptionWidth). */
    stringToFitCaptions = "=captions column=";
    stringToFitValues   = "======values column======";
    resizeColumns();
}

void PanelEditor::setCaptionWidth(int w)
{
    if (w > 0) setColumnWidth(CapColumn, w);
}

void PanelEditor::clearRows()
{
    if (model->rowCount() > 0) {
        close(QModelIndex());
        model->removeRows(0, model->rowCount());
    }
    sourceIdx.clear();
}

void PanelEditor::addSlider(const QString &key, const QString &caption,
                            const QString &tooltip, int min, int max)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = QModelIndex();
    i.parentName = "";
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = 0;
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Slider;
    i.type = "int";                 // div 0 = integer slider (0..max)
    i.min = min;
    i.max = max;
    i.div = 0;
    i.logScale = false;
    i.step = 1;
    i.color = G::darkgray;
    i.color1 = G::lightgray;
    i.fixedWidth = 50;
    addItem(i);
}

void PanelEditor::addCheckbox(const QString &key, const QString &caption, const QString &tooltip)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = QModelIndex();
    i.parentName = "";
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = false;
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
}

void PanelEditor::setSliderReal(const QString &key, double real)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;   // editor not realized
    const int div = vIdx.data(UR_Div).toInt();
    const int factor = (div == 0) ? 1 : div;
    /* Programmatic seed -> must not echo back as settingChanged. */
    const bool wasPop = isPopulating;
    isPopulating = true;
    setItemValue(vIdx, static_cast<int>(qRound(real * factor)));
    isPopulating = wasPop;
}

void PanelEditor::setCheckboxValue(const QString &key, bool on)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;
    const bool wasPop = isPopulating;
    isPopulating = true;
    setItemValue(vIdx, on);
    isPopulating = wasPop;
}

void PanelEditor::setRowEnabled(const QString &key, bool on)
{
    setItemEnabled(key, on);
}

int PanelEditor::sliderInt(const QString &key) const
{
    const QModelIndex vIdx = sourceIdx.value(key);
    return vIdx.isValid() ? vIdx.data(Qt::EditRole).toInt() : 0;
}

void PanelEditor::fitHeight()
{
    int h = 6;
    for (int r = 0; r < model->rowCount(); ++r)
        h += sizeHintForRow(r);
    setFixedHeight(h);
}

void PanelEditor::itemChange(QModelIndex index)
{
    if (isPopulating) return;
    emit settingChanged(index.data(UR_Source).toString(), index.data(Qt::EditRole));
}

void PanelEditor::mousePressEvent(QMouseEvent *event)
{
    /* Click a slider row's caption -> focus its slider (arrow keys nudge) + flash the
       caption, like DevelopProperties::mousePressEvent (the base does not select on
       click). */
    /* Not a branch expand/collapse unless the base says so below: stale state would make
       the following double click (if any) toggle an unrelated branch. */
    pressBranch = QModelIndex();
    if (event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid() && idx.column() == CapColumn) {
            const QModelIndex valIdx = model->index(idx.row(), ValColumn, idx.parent());
            if (valIdx.data(UR_DelegateType).toInt() == DT_Slider) {
                if (auto *se = static_cast<SliderEditor*>(
                        valIdx.data(UR_Editor).value<void*>()))
                    se->focusSlider();
                flashCaption(idx);
                return;
            }
        }
    }
    PropertyEditor::mousePressEvent(event);
}

void PanelEditor::flashCaption(const QModelIndex &capIdx)
{
    if (!capIdx.isValid()) return;
    if (flashAnim) { flashAnim->stop(); flashAnim->deleteLater(); }
    if (flashIdx.isValid() && flashIdx != capIdx)
        model->setData(flashIdx, QVariant(), UR_FlashLevel);
    flashIdx = capIdx;
    auto *anim = new QVariantAnimation(this);
    flashAnim = anim;
    anim->setDuration(450);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        if (flashIdx.isValid()) model->setData(flashIdx, val.toReal(), UR_FlashLevel);
    });
    connect(anim, &QVariantAnimation::finished, this, [this, anim]{
        if (flashIdx.isValid()) model->setData(flashIdx, QVariant(), UR_FlashLevel);
        flashIdx = QModelIndex();
        anim->deleteLater();
    });
    anim->start();
}
