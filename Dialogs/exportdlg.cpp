#include "Dialogs/exportdlg.h"

#include "Export/imageexporter.h"
#include "Main/global.h"
#include "Dialogs/tokendlg.h"
#include "Utilities/tokenfilename.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

/*
    See exportdlg.h. The dialog is a plain settings form; the only two pieces of behaviour
    worth knowing about are:

    FORMAT-DRIVEN VISIBILITY. The format table (Export/exportsettings.h) says which
    controls a format supports, so updateEnabledStates() HIDES (caption and all) Depth for
    JPEG, Quality for PNG/TIFF and Compression for everything but TIFF -- a setting the
    format does not have is not a setting you could switch on. Nothing here hard-codes a
    format name. Enabling, as opposed to hiding, is reserved for controls whose OWN row is
    live but momentarily irrelevant (the folder field, the sizing spin boxes).

    PRESET DIRTY STATE. Editing any control clears the preset combo's selection back to
    "(none)": the settings on screen no longer ARE that preset. Update writes them back to
    the last-chosen preset name.
*/

namespace {
/* The fixed geometry of the form. The dialog is not resizable (see fitToContents), so
   these are the layout, not a starting point: the fields are sized to their content --
   a filename template needs room, a format name or "16-bit" does not -- rather than each
   stretching to whatever share of the window its grid column happens to get. */
constexpr int kDialogWidth   = 820;
constexpr int kTemplateWidth = 290;
constexpr int kFormatWidth   = 100;   // every combo in the Format group

/* Every button in this dialog is the same 100px wide. That happens to match the global
   stylesheet's QPushButton min-width, but it is set explicitly here so the row widths do
   not silently change if that global is ever revisited (see memory
   project_global_pushbutton_minwidth). noFocus keeps the secondary buttons out of the tab
   order so Export stays the default.

   fitText=true is for a caption that will not fit 100px ("Template editor"): the width is
   left to the caption instead, which the global min-width still floors at 100.

   IngestDlg's buttons are 32px high, so the Ingest-style formatting matches that. */
void sizeButton(QPushButton *b, bool noFocus = true, bool fitText = false)
{
    if (fitText) b->setMinimumWidth(b->sizeHint().width());
    else b->setFixedWidth(100);
    if (G::useExportDlgIngestStyle) b->setFixedHeight(32);
    if (noFocus) b->setFocusPolicy(Qt::NoFocus);
}

/* The thin rule used both under a section caption and as a standalone divider. */
QFrame *separatorLine(QWidget *parent)
{
    QFrame *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("QFrame { border-color:" + G::disabledColor.name() + ";"
                        "border-width:0.5px; border-style:inset; }");
    return line;
}

/* A section header: bold caption over a thin rule, matching the mock-up's grouping
   without the heavy border a QGroupBox would draw. */
QWidget *sectionHeader(const QString &title, QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    QVBoxLayout *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 8, 0, 2);
    v->setSpacing(2);
    QLabel *lab = new QLabel(title, w);
    QFont f = lab->font();
    f.setBold(true);
    lab->setFont(f);
    v->addWidget(lab);
    v->addWidget(separatorLine(w));
    return w;
}
} // namespace

ExportDlg::ExportDlg(ImageExporter *exporter,
                     ExportPresets *presets,
                     const QStringList &srcPaths,
                     const QString &currentPath,
                     QMap<QString, QString> &filenameTemplates,
                     Mode mode,
                     QWidget *parent)
    : QDialog(parent),
      exporter(exporter),
      presets(presets),
      srcPaths(srcPaths),
      currentPath(currentPath),
      filenameTemplates(filenameTemplates),
      mode(mode)
{
    setWindowTitle(mode == Mode::Develop ? tr("Export Developed Images")
                                         : tr("Export Images"));
    setModal(true);

    current = presets ? presets->readLast() : ExportSettings();
    /* No destination remembered yet: start in the source image's folder, which is the
       least surprising default and always writable if the source is. */
    if (current.folderPath.isEmpty() && !srcPaths.isEmpty())
        current.folderPath = QFileInfo(srcPaths.first()).dir().path();

    /* Before buildUi, not after: the sheet changes label and control metrics, and buildUi
       measures captions to line the File naming and Format grids up with each other. A
       stylesheet set on the dialog still reaches children created later, so nothing is
       lost by applying it first. */
    setStyleSheet(G::css);

    buildUi();
    reloadPresetCombo();
    settingsToUi();
    updateEnabledStates();
    updateExample();

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
    fitToContents();
}

void ExportDlg::fitToContents()
{
/*
    The dialog is a fixed-size form. Every field is either a fixed width or sized by the
    grid, so there is nothing a wider or taller window would show more of -- dragging it
    about would only pool space under the last section.

    The WIDTH is the design number (kDialogWidth). The HEIGHT is asked of the layout: the
    sizeHint is the height at which the trailing stretch contributes nothing, so the scope
    row keeps the same gap below it (to the footer rule) as above it. It also tracks what
    actually varies -- the mode, the font size, what G::css adds to the controls, and
    whether the Quality row is showing.

    Which is why it is recomputed rather than set once in the constructor:
    updateEnabledStates hides whole rows as the format changes. The height constraint has
    to be released before measuring, or the layout hands back the height already in force
    instead of the one the contents now want.
*/
    setFixedWidth(kDialogWidth);
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    layout()->activate();
    setFixedHeight(qMax(sizeHint().height(), minimumSizeHint().height()));
}

QVBoxLayout *ExportDlg::addSection(QVBoxLayout *lay, const QString &title)
{
    if (!G::useExportDlgIngestStyle) {
        lay->addWidget(sectionHeader(title, this));
        QVBoxLayout *inner = new QVBoxLayout;
        inner->setContentsMargins(0, 0, 0, 0);
        lay->addLayout(inner);
        return inner;
    }

    /*
        IngestDlg groups its settings in QGroupBoxes whose title font is two points up on
        the body text (it hardcodes 14 against a 12pt default; taking the delta instead
        keeps this right when the user changes Winnow's font size).

        The title font is set on the BOX, and a widget's font propagates to its children,
        so the controls would inherit the enlarged title size. The inner `body` widget
        resets to the dialog font to stop that.
    */
    QGroupBox *gb = new QGroupBox(title, this);
    QFont tf = gb->font();
    tf.setPointSize(font().pointSize() + 2);
    gb->setFont(tf);

    /* The title straddles the box's top border (widgetcss QGroupBox::title raises it by
       half a font size), so the first control needs clearance below the title text or it
       reads as crowding it. Scale the gap with the title font rather than hardcoding a
       pixel count, so it holds up when the user changes Winnow's font size. */
    const int topGap = tf.pointSize() + 6;

    QVBoxLayout *outer = new QVBoxLayout(gb);
    outer->setContentsMargins(10, topGap, 10, 10);
    QWidget *body = new QWidget(gb);
    body->setFont(font());               // undo the title size for the controls
    QVBoxLayout *inner = new QVBoxLayout(body);
    inner->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(body);

    lay->addWidget(gb);
    return inner;
}

void ExportDlg::buildUi()
{
    QVBoxLayout *lay = new QVBoxLayout(this);

    /* ---- Preset section ---------------------------------------------------------------
       The preset controls act ON the rest of the dialog (they load and store the whole
       form) rather than being one of its settings, so both formattings set them apart --
       Ingest style with a QGroupBox like every other section, the original with its own
       bordered, tinted panel. Two rows either way -- the name, then the actions on it --
       so nothing is crowded however long a preset name is. */
    QVBoxLayout *presetLay = nullptr;
    if (G::useExportDlgIngestStyle) {
        presetLay = addSection(lay, tr("Preset"));
    }
    else {
        /* The panel shade must differ from BOTH the dialog background and the buttons
           sitting on it, or the buttons vanish into it. Winnow's theme is a greyscale
           ramp off G::backgroundShade (see widgetcss.cpp): buttons are shade-10, borders
           shade+40. So take the panel a step the OTHER way, shade+12, giving three
           distinct levels -- buttons darkest, dialog mid, panel lightest. Near-white
           themes have no headroom above, so those step down instead. */
        const int bgShade = G::backgroundShade;
        const int panelShade = qBound(0, bgShade <= 235 ? bgShade + 12 : bgShade - 12, 255);
        const QColor panelColor(panelShade, panelShade, panelShade);

        QFrame *presetFrame = new QFrame(this);
        presetFrame->setObjectName("presetFrame");
        presetFrame->setStyleSheet(
            "QFrame#presetFrame {"
                "border: 1px solid " + G::borderColor.name() + ";"
                "border-radius: 4px;"
                "background-color: " + panelColor.name() + ";"
            "}");
        presetLay = new QVBoxLayout(presetFrame);
        presetLay->setContentsMargins(10, 8, 10, 8);
        presetLay->setSpacing(6);
        lay->addWidget(presetFrame);
        lay->addSpacing(6);      // detach the panel from the settings sections below
    }

    QHBoxLayout *presetRow = new QHBoxLayout;
    /* The group box supplies its own "Preset" title in Ingest style, so the inline bold
       caption would just repeat it. */
    if (!G::useExportDlgIngestStyle) {
        QLabel *presetCaption = new QLabel(tr("Preset"), this);
        QFont pf = presetCaption->font();
        pf.setBold(true);
        presetCaption->setFont(pf);
        presetRow->addWidget(presetCaption);
    }
    presetCombo = new QComboBox(this);
    /* Fill the row. AdjustToContents would shrink it back to the longest preset name, so
       it is deliberately not used; an Expanding policy plus the layout stretch below lets
       the combo take all the slack the caption and state label leave. */
    presetCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    presetCombo->setMinimumWidth(220);
    presetRow->addWidget(presetCombo, 1);
    /* "modified" appears when the form has been edited away from the selected preset.
       The combo keeps naming the preset (Update writes back to it), so this label is the
       only thing distinguishing "these are its settings" from "these started as its
       settings". */
    presetStateLab = new QLabel(tr("modified"), this);
    presetStateLab->setStyleSheet("QLabel { color:" + G::disabledColor.name() + ";"
                                  "font-style: italic; border: none; }");
    presetStateLab->setVisible(false);
    presetRow->addWidget(presetStateLab);   // trails the combo at the right of the row
    presetLay->addLayout(presetRow);
    presetLay->addSpacing(10);       // breathing room between the name and its actions

    QHBoxLayout *presetBtnRow = new QHBoxLayout;
    presetBtnRow->setSpacing(6);
    presetNewBtn    = new QPushButton(tr("New"), this);
    presetUpdateBtn = new QPushButton(tr("Update"), this);
    presetRenameBtn = new QPushButton(tr("Rename"), this);
    presetDeleteBtn = new QPushButton(tr("Delete"), this);
    for (QPushButton *b : {presetNewBtn, presetUpdateBtn, presetRenameBtn, presetDeleteBtn}) {
        sizeButton(b);
        presetBtnRow->addWidget(b);
    }
    presetBtnRow->addStretch(1);
    presetLay->addLayout(presetBtnRow);

    presetNewBtn->setToolTip(tr("Save these settings as a new named export preset"));
    presetRenameBtn->setToolTip(tr("Rename the selected preset"));
    presetDeleteBtn->setToolTip(tr("Delete the selected preset"));

    /* ---- Destination ---- */
    QVBoxLayout *destLay = addSection(lay, tr("Destination"));
    QGridLayout *dest = new QGridLayout;
    /* Widest to narrowest scope: the source folder itself, a subfolder of it, then
       somewhere else entirely. Only the last two need a field beside them. */
    sourceFolderRadio = new QRadioButton(tr("Source folder"), this);
    sourceFolderRadio->setToolTip(tr("Write beside the original files, in the folder they "
                                     "came from"));
    subfolderRadio = new QRadioButton(tr("Source subfolder"), this);
    subfolderRadio->setToolTip(tr("Write to a subfolder of the folder each image came "
                                  "from, created if it does not exist"));
    folderRadio    = new QRadioButton(tr("Folder"), this);
    subfolderEdit  = new QLineEdit(this);
    folderEdit     = new QLineEdit(this);
    selectFolderBtn = new QPushButton(tr("Select..."), this);
    sizeButton(selectFolderBtn);
    dest->addWidget(sourceFolderRadio, 0, 0);
    dest->addWidget(subfolderRadio, 1, 0);
    dest->addWidget(subfolderEdit,  1, 1);
    dest->addWidget(folderRadio,    2, 0);
    dest->addWidget(folderEdit,     2, 1);
    dest->addWidget(selectFolderBtn, 2, 2);
    dest->setColumnStretch(1, 1);
    destLay->addLayout(dest);
    /* Tooltip and enabled state both depend on the destination, so updateEnabledStates
       owns them -- it runs once from the constructor and on every edit. */
    addToViewChk = new QCheckBox(tr("Insert exported images without rescanning the folder"),
                                 this);
    destLay->addWidget(addToViewChk);

    /* ---- File naming ----
       This grid and the Format grid below share the same (caption, field, caption, field)
       shape, and the captions are collected as they are made so the two can be given
       matching column widths once both exist -- see the alignment pass after Format. */
    QVector<QLabel *> capCol0, capCol2;
    auto cap = [&](const QString &text, QVector<QLabel *> &into) {
        QLabel *l = new QLabel(text, this);
        into.append(l);
        return l;
    };

    QVBoxLayout *nameLay = addSection(lay, tr("Name"));
    QGridLayout *name = new QGridLayout;
    /* Template and Suffix share one row on the Format group's four-column pattern
       (label, field, label, field) -- both name the output file, and neither needs the
       full width. */
    name->addWidget(cap(tr("Template:"), capCol0), 0, 0);
    templateCombo = new QComboBox(this);
    templateCombo->addItem(tr("Original file name"));      // index 0 == no template
    for (auto it = filenameTemplates.cbegin(); it != filenameTemplates.cend(); ++it)
        templateCombo->addItem(it.key(), it.value());
    templateCombo->setFixedWidth(kTemplateWidth);
    name->addWidget(templateCombo, 0, 1);
    name->addWidget(cap(tr("Suffix:"), capCol2), 0, 2);
    suffixEdit = new QLineEdit(this);
    /* A suffix is a handful of characters ("_dev"), so it is sized to that rather than
       taking column 3's full share the way the combos in the Format grid below do. */
    suffixEdit->setFixedWidth(165);
    name->addWidget(suffixEdit, 0, 3);
    name->addWidget(cap(tr("If the file exists:"), capCol0), 1, 0);
    QHBoxLayout *existsRow = new QHBoxLayout;
    overwriteRadio = new QRadioButton(tr("Overwrite"), this);
    uniqueRadio    = new QRadioButton(tr("Unique name"), this);
    skipRadio      = new QRadioButton(tr("Skip"), this);
    existsRow->addWidget(overwriteRadio);
    existsRow->addWidget(uniqueRadio);
    existsRow->addWidget(skipRadio);
    existsRow->addStretch(1);
    name->addLayout(existsRow, 1, 1, 1, 3);
    /* The example and the editor that changes it share the last row: the example grows
       from the left, the button is pushed to the right edge of the group, where the
       Destination group's Select button also sits. */
    exampleLab = new QLabel(this);
    exampleLab->setStyleSheet("QLabel { color:" + G::disabledColor.name() + "; }");
    templateEditorBtn = new QPushButton(tr("Template editor"), this);
    templateEditorBtn->setToolTip(tr("Create, rename or edit the filename templates. "
                                     "The templates are shared with Ingest and Rename."));
    sizeButton(templateEditorBtn, /*noFocus*/true, /*fitText*/true);
    QHBoxLayout *exampleRow = new QHBoxLayout;
    exampleRow->addWidget(exampleLab);
    exampleRow->addStretch(1);
    exampleRow->addWidget(templateEditorBtn);
    name->addLayout(exampleRow, 2, 1, 1, 3);
    nameLay->addLayout(name);

    /* ---- Format ---- */
    QVBoxLayout *fmtLay = addSection(lay, tr("Format"));
    QGridLayout *fmt = new QGridLayout;
    fmt->addWidget(cap(tr("Type:"), capCol0), 0, 0);
    typeCombo = new QComboBox(this);
    fmt->addWidget(typeCombo, 0, 1);
    depthCapLab = cap(tr("Depth:"), capCol2);
    fmt->addWidget(depthCapLab, 0, 2);
    depthCombo = new QComboBox(this);
    depthCombo->addItem(tr("8-bit"), 8);
    depthCombo->addItem(tr("16-bit"), 16);
    fmt->addWidget(depthCombo, 0, 3);
    fmt->addWidget(cap(tr("Space:"), capCol0), 1, 0);
    spaceCombo = new QComboBox(this);
    /* sRGB first: it is the safe default for anything shared or printed by a lab, and the
       wider two only pay off where the whole chain is managed. */
    spaceCombo->addItem("sRGB", int(OutputSpace::sRGB));
    spaceCombo->addItem("Display P3", int(OutputSpace::DisplayP3));
    spaceCombo->addItem("Adobe RGB", int(OutputSpace::AdobeRGB));
    spaceCombo->setToolTip(tr("Colour space of the exported file. sRGB is safest for the "
                              "web and print labs; Display P3 and Adobe RGB are wider, "
                              "for onward editing or a wide-gamut display.\n\n"
                              "8-bit Adobe RGB or Display P3 can band in smooth "
                              "gradients -- prefer 16-bit for those."));
    fmt->addWidget(spaceCombo, 1, 1);
    compressionCapLab = cap(tr("Compression:"), capCol2);
    fmt->addWidget(compressionCapLab, 1, 2);
    compressionCombo = new QComboBox(this);
    compressionCombo->addItem(tr("LZW"), 1);
    compressionCombo->addItem(tr("None"), 0);
    fmt->addWidget(compressionCombo, 1, 3);
    /* All four the same width: their contents are short ("JPEG", "16-bit", "LZW"), and
       four combos of one width read as one group where four different widths read as an
       accident. */
    for (QComboBox *c : {typeCombo, depthCombo, spaceCombo, compressionCombo})
        c->setFixedWidth(kFormatWidth);
    qualityCapLab = cap(tr("Quality:"), capCol0);
    fmt->addWidget(qualityCapLab, qualityRow, 0);
    qualitySlider = new QSlider(Qt::Horizontal, this);
    qualitySlider->setRange(1, 100);
    fmt->addWidget(qualitySlider, qualityRow, 1, 1, 2);
    qualityLab = new QLabel(this);
    qualityLab->setMinimumWidth(30);
    fmt->addWidget(qualityLab, qualityRow, 3);
    fmtLay->addLayout(fmt);
    fmtGrid = fmt;      // updateEnabledStates collapses the Quality row when it empties

    /* ---- Align the File naming and Format grids --------------------------------------
       They are separate layouts in separate group boxes, so nothing lines their
       columns up by itself: each caption column would size to its own widest
       caption, putting the Template combo somewhere else than Type and Space.

       Both boxes are the same width with the same margins, so it is enough to
       (a) pin both caption columns to the widest caption found in EITHER grid and
       (b) give both field columns the same stretch. Qt then hands each stretchy
       column the same share of what is left, and the fields land on the same x in
       both groups.

       Equal stretch on 1 and 3 also stops the template combo eating all the slack, which
       would leave column 3 too narrow for the Format grid's combos. (The suffix field
       itself no longer depends on that -- it is fixed-width, and simply sits at the left
       of its column.) */
    auto widestCap = [](const QVector<QLabel *> &labs) {
        int w = 0;
        for (QLabel *l : labs) w = qMax(w, l->sizeHint().width());
        return w;
    };
    const int capW0 = widestCap(capCol0);
    const int capW2 = widestCap(capCol2);
    for (QGridLayout *g : {name, fmt}) {
        g->setColumnMinimumWidth(0, capW0);
        g->setColumnMinimumWidth(2, capW2);
        g->setColumnStretch(1, 1);
        g->setColumnStretch(3, 1);
    }

    /* ---- One vertical rhythm across all three settings grids -------------------------
       A grid row is as tall as the tallest thing in it, so left alone the rows step
       about: Destination's first row holds only a radio (short), its last holds the
       Select button (tall), while File naming and Format are all combos and fields.
       Different row HEIGHTS read as different spacing even though the gap between
       rows is the same everywhere.

       The FIELDS set that height -- a line edit or a combo, which is what File naming and
       Format are made of and what already looked right. Deliberately NOT the tallest
       control in the dialog: sizeButton() gives Select a fixed 32 in Ingest style, and
       pinning every row to that stretches all three groups out. The button is refitted to
       the row instead, so it stops driving the geometry.

       (A fixed height does not show up in sizeHint(), which is what left the Select row
       taller than the two above it even after the rows were pinned -- hence measuring the
       larger of the hint and the enforced minimum.)

       Grids only, deliberately. The checkbox rows sit in the group boxes' own vertical
       layouts, where the standard gap already reads right against a pinned row. */
    auto effectiveH = [](QWidget *w) {
        return qMax(w->sizeHint().height(), w->minimumHeight());
    };
    int rowH = 0;
    for (QWidget *w : {static_cast<QWidget*>(subfolderEdit),
                       static_cast<QWidget*>(templateCombo),
                       static_cast<QWidget*>(typeCombo)})
        rowH = qMax(rowH, effectiveH(w));
    selectFolderBtn->setFixedHeight(rowH);      // fit the row, do not define it
    templateEditorBtn->setFixedHeight(rowH);    // ditto: keep the Example row's height
    for (QGridLayout *g : {dest, name, fmt})
        for (int r = 0; r < g->rowCount(); ++r)
            g->setRowMinimumHeight(r, rowH);
    fmtRowH = rowH;    // so a hidden Quality row can be pinned back to it when it returns
    copyMetadataChk = new QCheckBox(tr("Copy metadata from the original"), this);
    copyMetadataChk->setToolTip(tr("Copy EXIF, IPTC and the ICC profile from the source "
                                   "file. Adds roughly a fifth of a second per image."));
    embedThumbChk = new QCheckBox(tr("Embed thumbnail"), this);
    QHBoxLayout *metaRow = new QHBoxLayout;
    metaRow->addWidget(copyMetadataChk);
    metaRow->addWidget(embedThumbChk);
    metaRow->addStretch(1);
    fmtLay->addLayout(metaRow);

    /* ---- Image sizing ---- */
    QVBoxLayout *sizeLay = addSection(lay, tr("Size"));
    QHBoxLayout *size = new QHBoxLayout;
    fullSizeRadio = new QRadioButton(tr("Full size"), this);
    longEdgeRadio = new QRadioButton(tr("Long edge"), this);
    percentRadio  = new QRadioButton(tr("Percent"), this);
    longEdgeSpin  = new QSpinBox(this);
    longEdgeSpin->setRange(16, 60000);
    longEdgeSpin->setSuffix(" px");
    percentSpin = new QSpinBox(this);
    percentSpin->setRange(1, 100);
    percentSpin->setSuffix(" %");
    size->addWidget(fullSizeRadio);
    size->addWidget(longEdgeRadio);
    size->addWidget(longEdgeSpin);
    size->addWidget(percentRadio);
    size->addWidget(percentSpin);
    size->addStretch(1);
    sizeLay->addLayout(size);
    dontEnlargeChk = new QCheckBox(tr("Don't enlarge"), this);
    dontEnlargeChk->setToolTip(tr("Never scale an image UP to reach the target size"));
    sizeLay->addWidget(dontEnlargeChk);

    /* ---- Scope ----
       What gets exported, not how, so it sits outside the settings group boxes -- last,
       just above the footer, where it reads as part of the decision to press Export
       rather than as another setting the preset would carry. */
    QHBoxLayout *scopeRow = new QHBoxLayout;
    countLab = new QLabel(this);
    scopeRow->addWidget(countLab);
    scopeRow->addStretch(1);
    allSelectedRadio = new QRadioButton(tr("All selected"), this);
    currentOnlyRadio = new QRadioButton(tr("Current image only"), this);
    allSelectedRadio->setChecked(true);
    scopeRow->addWidget(allSelectedRadio);
    scopeRow->addWidget(currentOnlyRadio);
    lay->addSpacing(6);          // detach the row from the Image sizing box above
    lay->addLayout(scopeRow);
    /* One image selected: the choice is meaningless, so do not offer it. */
    const bool oneOnly = srcPaths.count() < 2;
    allSelectedRadio->setVisible(!oneOnly);
    currentOnlyRadio->setVisible(!oneOnly);
    countLab->setText(oneOnly ? tr("1 image")
                              : tr("%1 selected images").arg(srcPaths.count()));

    lay->addStretch(1);

    /* ---- Footer: progress + buttons ----
       Closed off from the settings above by its own rule, so the action row reads as the
       end of the dialog rather than as a continuation of the scope row. IngestDlg does the
       same (its full-width Line above the Cancel / OK pair), so both formattings keep it
       -- in Ingest style the group boxes already close themselves, but the rule still
       separates the settings from the actions. */
    lay->addSpacing(6);
    lay->addWidget(separatorLine(this));
    lay->addSpacing(6);

    QHBoxLayout *foot = new QHBoxLayout;
    progressBar = new QProgressBar(this);
    progressBar->setVisible(false);
    statusLab = new QLabel(this);
    exportBtn = new QPushButton(tr("Export"), this);
    exportBtn->setDefault(true);
    closeBtn  = new QPushButton(tr("Cancel"), this);
    /* Export keeps its focus (it is the default button); Cancel does not need it. */
    sizeButton(exportBtn, /*noFocus*/false);
    sizeButton(closeBtn);
    foot->addWidget(progressBar, 1);
    foot->addWidget(statusLab);
    foot->addStretch(1);
    foot->addWidget(closeBtn);
    foot->addWidget(exportBtn);
    lay->addLayout(foot);

    loadFormats();

    /* ---- Connections ---- */
    connect(selectFolderBtn, &QPushButton::clicked, this, &ExportDlg::onSelectFolder);
    connect(templateEditorBtn, &QPushButton::clicked, this, &ExportDlg::onTemplateEditor);
    connect(exportBtn, &QPushButton::clicked, this, &ExportDlg::onExportClicked);
    connect(closeBtn,  &QPushButton::clicked, this, &QDialog::reject);

    connect(presetCombo, &QComboBox::currentIndexChanged, this, &ExportDlg::onPresetChosen);
    connect(presetNewBtn,    &QPushButton::clicked, this, &ExportDlg::onPresetNew);
    connect(presetUpdateBtn, &QPushButton::clicked, this, &ExportDlg::onPresetUpdate);
    connect(presetRenameBtn, &QPushButton::clicked, this, &ExportDlg::onPresetRename);
    connect(presetDeleteBtn, &QPushButton::clicked, this, &ExportDlg::onPresetDelete);

    /* Every setting control funnels through one handler: pull the UI into `current`,
       re-evaluate what the format allows, refresh the example, and mark the preset
       modified.

       The selection is deliberately KEPT. Clearing the combo back to "(none)" here would
       destroy the only record of which preset Update should write to, making Update
       unreachable in the exact situation it exists for -- you can only want it after
       editing, and editing is what would have cleared it. */
    auto onEdited = [this]() {
        if (populating) return;
        uiToSettings();
        updateEnabledStates();
        updateExample();
        setPresetDirty(true);
    };
    for (QRadioButton *r : {sourceFolderRadio, subfolderRadio, folderRadio,
                            overwriteRadio, uniqueRadio,
                            skipRadio, fullSizeRadio, longEdgeRadio, percentRadio,
                            allSelectedRadio, currentOnlyRadio})
        connect(r, &QRadioButton::toggled, this, onEdited);
    for (QLineEdit *e : {subfolderEdit, folderEdit, suffixEdit})
        connect(e, &QLineEdit::textChanged, this, onEdited);
    for (QCheckBox *c : {addToViewChk, copyMetadataChk, embedThumbChk, dontEnlargeChk})
        connect(c, &QCheckBox::toggled, this, onEdited);
    for (QComboBox *c : {templateCombo, typeCombo, depthCombo, spaceCombo, compressionCombo})
        connect(c, &QComboBox::currentIndexChanged, this, onEdited);
    for (QSpinBox *sp : {longEdgeSpin, percentSpin})
        connect(sp, &QSpinBox::valueChanged, this, onEdited);
    connect(qualitySlider, &QSlider::valueChanged, this, [this, onEdited](int v) {
        qualityLab->setText(QString::number(v));
        onEdited();
    });

    /* Progress from the batch. */
    if (exporter) {
        connect(exporter, &ImageExporter::progress, this, [this](int done, int total) {
            progressBar->setMaximum(total);
            progressBar->setValue(done);
            statusLab->setText(tr("%1 of %2").arg(done).arg(total));
        });
        connect(exporter, &ImageExporter::finished, this,
                [this](const ImageExporter::Result &r) {
            setBusy(false);
            QString msg;
            if (r.aborted) msg = tr("Export cancelled after %1.").arg(r.written.count());
            else msg = tr("Exported %1.").arg(r.written.count());
            if (!r.skipped.isEmpty()) msg += tr(" %1 skipped.").arg(r.skipped.count());
            if (!r.failed.isEmpty())  msg += tr(" %1 failed.").arg(r.failed.count());
            statusLab->setText(msg);
            /* A clean run has nothing more to say: close. Anything unusual stays open so
               the count is readable -- unless the user already asked to leave (they hit
               Esc or the window close, which stopped the run instead of closing). */
            if (closeWhenDone) { QDialog::reject(); return; }
            if (!r.aborted && r.failed.isEmpty() && r.skipped.isEmpty()) accept();
        });
    }
}

void ExportDlg::loadFormats()
{
    QSignalBlocker block(typeCombo);
    typeCombo->clear();
    const QVector<ExportFormat> fmts = ExportFormats::available();
    for (const ExportFormat &f : fmts) typeCombo->addItem(f.label, f.key);
    if (typeCombo->count() == 0)          // no writer plugins at all: should not happen
        typeCombo->addItem("JPEG", "jpg");
}

void ExportDlg::settingsToUi()
{
    populating = true;

    sourceFolderRadio->setChecked(current.dest == ExportSettings::SourceFolder);
    subfolderRadio->setChecked(current.dest == ExportSettings::SubfolderOfSource);
    folderRadio->setChecked(current.dest == ExportSettings::ChosenFolder);
    subfolderEdit->setText(current.subfolderName);
    folderEdit->setText(current.folderPath);
    addToViewChk->setChecked(current.addToFolderView);

    /* The template combo stores the token string as item data; index 0 is "no template".
       A template that has since been deleted falls back to the original file name. */
    int tIdx = 0;
    for (int i = 1; i < templateCombo->count(); ++i)
        if (templateCombo->itemData(i).toString() == current.tokenTemplate) { tIdx = i; break; }
    templateCombo->setCurrentIndex(tIdx);
    suffixEdit->setText(current.suffix);
    overwriteRadio->setChecked(current.exists == ExportSettings::Overwrite);
    uniqueRadio->setChecked(current.exists == ExportSettings::UniqueName);
    skipRadio->setChecked(current.exists == ExportSettings::Skip);

    int fIdx = typeCombo->findData(current.format);
    if (fIdx < 0) fIdx = 0;                       // stored format not writable here
    typeCombo->setCurrentIndex(fIdx);
    depthCombo->setCurrentIndex(current.bitDepth == 16 ? 1 : 0);
    spaceCombo->setCurrentIndex(qMax(0, spaceCombo->findData(int(current.space))));
    compressionCombo->setCurrentIndex(current.tiffCompression == 0 ? 1 : 0);
    qualitySlider->setValue(current.quality);
    qualityLab->setText(QString::number(current.quality));
    copyMetadataChk->setChecked(current.copyMetadata);
    embedThumbChk->setChecked(current.embedThumbnail);

    fullSizeRadio->setChecked(current.sizing == ExportSettings::FullSize);
    longEdgeRadio->setChecked(current.sizing == ExportSettings::LongEdge);
    percentRadio->setChecked(current.sizing == ExportSettings::Percent);
    longEdgeSpin->setValue(current.longEdgePx);
    percentSpin->setValue(current.percent);
    dontEnlargeChk->setChecked(current.dontEnlarge);

    populating = false;
}

void ExportDlg::uiToSettings()
{
    current.dest = sourceFolderRadio->isChecked() ? ExportSettings::SourceFolder
                 : subfolderRadio->isChecked()    ? ExportSettings::SubfolderOfSource
                                                  : ExportSettings::ChosenFolder;
    current.subfolderName = subfolderEdit->text();
    current.folderPath = folderEdit->text();
    current.addToFolderView = addToViewChk->isChecked();

    current.tokenTemplate = templateCombo->currentIndex() > 0
                                ? templateCombo->currentData().toString()
                                : QString();
    current.suffix = suffixEdit->text();
    current.exists = overwriteRadio->isChecked() ? ExportSettings::Overwrite
                   : skipRadio->isChecked()      ? ExportSettings::Skip
                                                 : ExportSettings::UniqueName;

    current.format = typeCombo->currentData().toString();
    current.bitDepth = depthCombo->currentData().toInt();
    current.space = OutputSpace(spaceCombo->currentData().toInt());
    current.tiffCompression = compressionCombo->currentData().toInt();
    current.quality = qualitySlider->value();
    current.copyMetadata = copyMetadataChk->isChecked();
    current.embedThumbnail = embedThumbChk->isChecked();

    current.sizing = longEdgeRadio->isChecked() ? ExportSettings::LongEdge
                   : percentRadio->isChecked()  ? ExportSettings::Percent
                                                : ExportSettings::FullSize;
    current.longEdgePx = longEdgeSpin->value();
    current.percent = percentSpin->value();
    current.dontEnlarge = dontEnlargeChk->isChecked();
}

void ExportDlg::updateEnabledStates()
{
    const ExportFormat f = ExportFormats::find(current.format);

    /* Only what the format can actually do is SHOWN -- caption and control both. A greyed
       row still reads as something that could be switched on, and a format that simply
       has no such setting (JPEG has no compression choice, PNG no quality) has nothing
       to turn on. Nothing here names a format: the table in Export/exportsettings.h
       says what each one supports.

       Bit depth is also a develop-render property: the preview pixel source hands over an
       8-bit browse image, so offering 16-bit there would be a lie. */
    const bool showDepth   = f.depth16 && mode == Mode::Develop;
    const bool showQuality = f.hasQuality;
    const bool showComp    = f.hasTiffComp;
    depthCapLab->setVisible(showDepth);
    depthCombo->setVisible(showDepth);
    compressionCapLab->setVisible(showComp);
    compressionCombo->setVisible(showComp);
    qualityCapLab->setVisible(showQuality);
    qualitySlider->setVisible(showQuality);
    qualityLab->setVisible(showQuality);
    /* Depth and Compression share their rows with Type and Space, which never hide, so
       only Quality can leave a row empty -- and buildUi pinned every row to fmtRowH,
       which would hold that empty row open as a blank gap. */
    if (fmtGrid) {
        fmtGrid->setRowMinimumHeight(qualityRow, showQuality ? fmtRowH : 0);
        /* The form just got a row shorter or taller and the dialog is fixed-size, so it
           has to be re-measured or the difference shows up as dead space. */
        fitToContents();
    }
    spaceCombo->setEnabled(spaceCombo->count() > 1);

    subfolderEdit->setEnabled(subfolderRadio->isChecked());
    folderEdit->setEnabled(folderRadio->isChecked());
    selectFolderBtn->setEnabled(folderRadio->isChecked());

    /* This choice only means anything when the export lands in a folder Winnow already
       has open -- either way the new files END UP visible, so what it really picks is
       insert-in-place vs a folder rescan. The CHECKED state is left alone while disabled:
       it is the destination that made it irrelevant, and pointing at a loaded folder
       again should restore the user's choice rather than silently having cleared it. */
    const bool destLoaded =
        exporter && exporter->willTouchLoadedFolder(targetPaths(), current);
    addToViewChk->setEnabled(destLoaded);
    addToViewChk->setToolTip(
        destLoaded ? tr("The destination folder is open, so the exported files will appear "
                        "in it either way.\n\n"
                        "Ticked, they are inserted directly, keeping your place. Unticked, "
                        "the folder is rescanned to find them, which also picks up any "
                        "other changes on disk.")
                   : tr("The destination folder is not open in Winnow, so there is nothing "
                        "to insert into"));

    longEdgeSpin->setEnabled(longEdgeRadio->isChecked());
    percentSpin->setEnabled(percentRadio->isChecked());
    dontEnlargeChk->setEnabled(current.sizing != ExportSettings::FullSize);
}

void ExportDlg::updateExample()
{
    if (!exporter) return;
    const QStringList targets = targetPaths();
    if (targets.isEmpty()) { exampleLab->clear(); return; }
    const QString dst = exporter->previewDestination(targets.first(), current);
    exampleLab->setText(tr("Example: %1").arg(QFileInfo(dst).fileName()));
}

QStringList ExportDlg::targetPaths() const
{
    if (currentOnlyRadio->isVisible() && currentOnlyRadio->isChecked()
        && !currentPath.isEmpty())
        return QStringList() << currentPath;
    return srcPaths;
}

void ExportDlg::onTemplateEditor()
{
/*
    Open the shared token editor (Dialogs/tokendlg.h) on the filename templates, the same
    editor Ingest and Rename use. It edits filenameTemplates -- MW's one map -- in place,
    so a template added or renamed here appears in those dialogs too, and MW persists it.

    TokenDlg holds its arguments BY REFERENCE for its lifetime, so the token list, the
    example map and the index/key it reports back are locals of this (blocking) call.
*/
    /* The title also filters warnings -- see TokenDlg::updateUniqueFileNameWarning -- so
       it is not free text. */
    QString title = "Token Editor - Export file name";
    QMap<QString, QString> usingTokenMap;              // dummy: only Ingest tracks these
    QStringList tokens = TokenFileName::tokens();
    QMap<QString, QString> exampleMap = TokenFileName::exampleMap();
    /* TokenDlg indexes the template map; the combo offsets that by one for its
       "Original file name" entry, which is not a template. */
    int index = qMax(0, templateCombo->currentIndex() - 1);
    QString currentKey = templateCombo->currentIndex() > 0
                             ? templateCombo->currentText() : QString();

    TokenDlg tokenDlg(tokens, exampleMap, filenameTemplates, usingTokenMap,
                      index, currentKey, title, this);
    tokenDlg.exec();

    /* Rebuild the list and land back on the same template for continuity. Renaming is
       what makes this more than a refresh: the key moves while the token string does
       not, so a lost key falls back to matching the token string, and only then to
       "Original file name" (which is what a deleted template must land on).

       Signals are blocked because a rebuild is not an edit -- the preset only becomes
       dirty if the template the dialog will export with actually changed. */
    const QString wasTemplate = current.tokenTemplate;
    {
        QSignalBlocker block(templateCombo);
        templateCombo->clear();
        templateCombo->addItem(tr("Original file name"));      // index 0 == no template
        for (auto it = filenameTemplates.cbegin(); it != filenameTemplates.cend(); ++it)
            templateCombo->addItem(it.key(), it.value());
        int idx = currentKey.isEmpty() ? 0 : templateCombo->findText(currentKey);
        if (idx < 0) idx = templateCombo->findData(wasTemplate);
        templateCombo->setCurrentIndex(qMax(0, idx));
    }
    uiToSettings();
    updateExample();
    if (current.tokenTemplate != wasTemplate) setPresetDirty(true);
}

void ExportDlg::onSelectFolder()
{
    const QString start = folderEdit->text().isEmpty()
                              ? QDir::homePath() : folderEdit->text();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select the export folder"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;
    folderEdit->setText(dir);
    folderRadio->setChecked(true);
}

void ExportDlg::onExportClicked()
{
    if (running) {                       // the button is "Cancel" mid-batch
        if (exporter) exporter->abort();
        return;
    }
    uiToSettings();

    if (current.dest == ExportSettings::ChosenFolder && current.folderPath.isEmpty()) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Please choose a destination folder."));
        return;
    }
    const QStringList targets = targetPaths();
    if (targets.isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("There is nothing to export."));
        return;
    }
    if (!exporter) return;

    /* Writing back over the originals. Only possible when the destination resolves to the
       source folder AND nothing distinguishes the name -- no suffix, no template, and a
       format whose extension matches the source. That was already reachable by pointing
       "Folder" at the source folder; the Source folder option puts it one click away, so
       it is worth a confirmation. Only Overwrite actually destroys anything: Unique name
       and Skip already handle the collision. */
    if (current.exists == ExportSettings::Overwrite) {
        int clashes = 0;
        for (const QString &src : targets) {
            const QString dst = exporter->previewDestination(src, current);
            if (QFileInfo(dst).absoluteFilePath() == QFileInfo(src).absoluteFilePath())
                ++clashes;
        }
        if (clashes > 0) {
            const auto answer = QMessageBox::warning(
                this, tr("Export"),
                tr("%1 of the %2 images would be written back over the original file, "
                   "replacing it.\n\n"
                   "Add a suffix, choose a file name template, or set \"If the file "
                   "exists\" to Unique name to keep the originals.")
                    .arg(clashes).arg(targets.count()),
                QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel);
            if (answer != QMessageBox::Ok) return;
        }
    }

    setBusy(true);
    exporter->run(targets, current);
}

void ExportDlg::reject()
{
    if (running) {                       // Esc / Cancel mid-batch means Stop
        closeWhenDone = true;            // the user asked to leave: honour it on finish
        if (exporter) exporter->abort();
        return;
    }
    QDialog::reject();
}

void ExportDlg::closeEvent(QCloseEvent *event)
{
    if (running) {
        closeWhenDone = true;
        if (exporter) exporter->abort();
        event->ignore();                 // close once the run reports finished
        return;
    }
    QDialog::closeEvent(event);
}

void ExportDlg::setBusy(bool busy)
{
    running = busy;
    progressBar->setVisible(busy);
    exportBtn->setText(busy ? tr("Stop") : tr("Export"));
    closeBtn->setEnabled(!busy);
    /* Freeze the settings while a batch is running -- they were captured at Export and
       changing them mid-run would show something the output does not match. */
    for (QWidget *w : findChildren<QWidget*>()) {
        if (w == exportBtn || w == progressBar || w == statusLab) continue;
        if (qobject_cast<QLabel*>(w)) continue;
        w->setEnabled(!busy);
    }
    if (!busy) updateEnabledStates();
    /* The progress bar joining or leaving the footer row can change what the form needs,
       and a fixed-size dialog cannot absorb that by itself. */
    fitToContents();
}

QString ExportDlg::selectedPreset() const
{
    /* Index 0 is the "(none)" placeholder, not a preset. */
    return presetCombo->currentIndex() > 0 ? presetCombo->currentText() : QString();
}

void ExportDlg::setPresetDirty(bool dirty)
{
/*
    Show whether the settings still match the selected preset, and keep the three preset
    buttons honest about it. The combo goes on naming the preset while it is modified (it
    is the target of Update), so without the label there would be nothing on screen
    distinguishing "these ARE Web 2048's settings" from "these started as Web 2048".
*/
    presetDirty = dirty;
    const bool hasSel = !selectedPreset().isEmpty();

    presetStateLab->setVisible(hasSel && dirty);
    presetUpdateBtn->setEnabled(hasSel && dirty);
    /* Rename and Delete act on the STORED preset, so they do not care whether the form
       has been edited -- only that one is selected. */
    presetRenameBtn->setEnabled(hasSel);
    presetDeleteBtn->setEnabled(hasSel);

    presetUpdateBtn->setToolTip(
        hasSel ? (dirty ? tr("Overwrite \"%1\" with these settings").arg(selectedPreset())
                        : tr("These settings already match \"%1\"").arg(selectedPreset()))
               : tr("Select a preset to update, or use New"));
}

void ExportDlg::reloadPresetCombo(const QString &select)
{
    {
        QSignalBlocker block(presetCombo);
        presetCombo->clear();
        presetCombo->addItem(tr("(none)"));
        if (presets) presetCombo->addItems(presets->names());
        if (!select.isEmpty()) {
            const int i = presetCombo->findText(select);
            if (i > 0) presetCombo->setCurrentIndex(i);
        }
    }
    /* Refresh the buttons for the new selection WITHOUT deciding the modified state --
       only the caller knows whether the form now matches what is stored. Rebuilding after
       a Rename, for instance, leaves an edited form still modified. */
    setPresetDirty(presetDirty);
}

void ExportDlg::onPresetChosen(int index)
{
    if (index <= 0 || !presets) {          // "(none)": keep the settings, drop the link
        setPresetDirty(false);
        return;
    }
    current = presets->read(presetCombo->itemText(index));
    settingsToUi();
    updateEnabledStates();
    updateExample();
    setPresetDirty(false);                 // the form now IS the preset
}

void ExportDlg::onPresetNew()
{
    if (!presets) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Export Preset"),
                                               tr("Preset name:"), QLineEdit::Normal,
                                               QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (presets->contains(name)) {
        if (QMessageBox::question(this, tr("New Export Preset"),
                tr("\"%1\" already exists. Replace it?").arg(name))
            != QMessageBox::Yes) return;
    }
    uiToSettings();
    presets->write(name, current);
    reloadPresetCombo(name);               // select the preset just created
    setPresetDirty(false);                 // the form now matches what is stored
}

void ExportDlg::onPresetUpdate()
{
    const QString name = selectedPreset();
    if (!presets || name.isEmpty()) return;
    uiToSettings();
    presets->write(name, current);
    reloadPresetCombo(name);
    setPresetDirty(false);                 // the form now matches what is stored
}

void ExportDlg::onPresetRename()
{
    if (!presets || selectedPreset().isEmpty()) return;
    const QString from = presetCombo->currentText();
    bool ok = false;
    const QString to = QInputDialog::getText(this, tr("Rename Export Preset"),
                                             tr("New name:"), QLineEdit::Normal,
                                             from, &ok).trimmed();
    if (!ok || to.isEmpty() || to == from) return;
    if (!presets->rename(from, to)) {
        QMessageBox::warning(this, tr("Rename Export Preset"),
                             tr("\"%1\" already exists.").arg(to));
        return;
    }
    reloadPresetCombo(to);
}

void ExportDlg::onPresetDelete()
{
    const QString name = selectedPreset();
    if (!presets || name.isEmpty()) return;
    if (QMessageBox::question(this, tr("Delete Export Preset"),
                              tr("Delete the preset \"%1\"?").arg(name))
        != QMessageBox::Yes) return;
    presets->remove(name);
    /* The settings stay on the form -- deleting a preset should not disturb the export
       the user is set up to run. There is just no preset backing them any more. */
    reloadPresetCombo();
    setPresetDirty(false);
}
