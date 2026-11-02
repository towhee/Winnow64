#ifndef EXPORTDLG_H
#define EXPORTDLG_H

#include <QtWidgets>
#include "Export/exportpresets.h"
#include "Export/exportsettings.h"

class ImageExporter;

/*
    The one export dialog. It collects an ExportSettings and hands it back; it does not
    export anything itself -- ImageExporter does that -- and it knows nothing about where
    the pixels come from. That is what lets the same dialog serve "Export Images" and
    File > "Save Preview as", which differ only in the pixel source the caller pairs with
    the settings.

    LAYOUT: a preset bar across the top (combo + New / Update / Rename / Delete), then one
    column of grouped sections -- Source (Develop only), Embellish, Destination, File
    naming, Format, Image sizing -- and the buttons. Hand-built rather than a .ui file, following the
    newest dialogs (SaveDevelopPresetDlg): the form is generated from the format table and
    reacts to it, which a static .ui cannot express.

    The dialog CLOSES when Export is pressed: it accepts, and the caller hands targetPaths()
    and settings() to ImageExporter, whose progress shows in the status bar's "Export" row
    (Esc stops it). A batch is seconds per raw, and a modal dialog sitting over the window
    for its length blocked everything else for no reason. The no-dialog quick export
    (Develop > Export with preset) reports through the same row.

    Mode::Preview hides the rows that only mean something for a develop render (bit depth,
    colour space), so "Save Preview as" is not offering settings its pixel source cannot
    honour.
*/
class ExportDlg : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Develop, Preview };

    /* exporter is borrowed, not owned: the dialog asks it about names and destinations
       but never runs it.
       imageCount / firstPath drive the header line and the live example name. */
    ExportDlg(ImageExporter *exporter,
              ExportPresets *presets,
              const QStringList &srcPaths,
              const QString &currentPath,
              QMap<QString, QString> &filenameTemplates,
              Mode mode = Mode::Develop,
              QWidget *parent = nullptr);

    /* The settings as last shown -- the caller persists them as the "last used" slot. */
    ExportSettings settings() const { return current; }
    /* The paths to export: the whole selection, or just the current image. */
    QStringList targetPaths() const;

private slots:
    void onExportClicked();          // validate, then accept: the caller runs the export
    void onSelectFolder();
    void onPresetChosen(int index);
    void onPresetNew();
    void onPresetUpdate();
    void onPresetRename();
    void onPresetDelete();
    /* Open the shared token editor on the filename templates. Not const: the editor edits
       the caller's template map in place, so Ingest and Rename see the change too. */
    void onTemplateEditor();

private:
    void buildUi();
    /* Pin the dialog to its design width and to whatever height the controls currently
       need. The dialog is NOT resizable -- see the comment on the definition. */
    void fitToContents();
    /* Add a titled section to `lay` and return the layout its rows go in: a QGroupBox
       with an enlarged title, matching IngestDlg. Callers add their rows to the returned
       layout and never see the container. */
    QVBoxLayout *addSection(QVBoxLayout *lay, const QString &title);
    void loadFormats();              // fill the type combo from what this build can write
    void settingsToUi();             // push `current` into the controls
    void uiToSettings();             // pull the controls into `current`
    /* Show only what the chosen format (and mode) can actually do, and grey what the
       current destination / sizing choice makes irrelevant. */
    void updateEnabledStates();
    void updateExample();            // the live "Example:" line
    void reloadPresetCombo(const QString &select = QString());
    /* Editing a control makes the settings differ from the selected preset. The selection
       is KEPT (Update has to know which preset to write back to) and the difference is
       shown instead -- see presetDirty. */
    void setPresetDirty(bool dirty);
    QString selectedPreset() const;  // "" when the combo is on "(none)"
    /* What `current` exports with: ExportPresets::effective, so with an Embellish
       template the rows the template decides show the template's values. `current`
       keeps the user's own, which come back when the template is cleared. */
    ExportSettings shown() const;
    void onEmbellishEdited();

    ImageExporter *exporter;
    ExportPresets *presets;
    QStringList    srcPaths;
    QString        currentPath;
    /* The one shared template map (MW::filenameTemplates), by reference: the Template
       editor button writes back into it, and MW persists it at exit. */
    QMap<QString, QString> &filenameTemplates;
    Mode           mode;

    ExportSettings current;
    bool           populating = false;   // suppress control signals while loading

    /* Preset bar. presetDirty = the settings have been edited away from the selected
       preset; presetStateLab shows that, since the combo still names the preset. */
    bool         presetDirty = false;
    QComboBox   *presetCombo = nullptr;
    QLabel      *presetStateLab = nullptr;
    QPushButton *presetNewBtn = nullptr;
    QPushButton *presetUpdateBtn = nullptr;
    QPushButton *presetRenameBtn = nullptr;
    QPushButton *presetDeleteBtn = nullptr;

    /* Source (Mode::Develop only). sourceNoteLab is the inline reason when the choice
       does not reach every image: no raw in the export, or some that are not raw. */
    QRadioButton *developedRadio = nullptr;
    QRadioButton *cameraPreviewRadio = nullptr;
    QLabel       *sourceNoteLab = nullptr;

    /* Embellish. embelNoteLab is the inline reason the rows the template decides are
       greyed. */
    QComboBox    *embelCombo = nullptr;
    QCheckBox    *embelUseOutputChk = nullptr;
    QLabel       *embelNoteLab = nullptr;

    /* Scope */
    QLabel       *countLab = nullptr;
    QRadioButton *allSelectedRadio = nullptr;
    QRadioButton *currentOnlyRadio = nullptr;

    /* Destination */
    QRadioButton *sourceFolderRadio = nullptr;
    QRadioButton *subfolderRadio = nullptr;
    QRadioButton *folderRadio = nullptr;
    QLineEdit    *subfolderEdit = nullptr;
    QLineEdit    *folderEdit = nullptr;
    QPushButton  *selectFolderBtn = nullptr;
    QCheckBox    *addToViewChk = nullptr;

    /* Naming */
    QComboBox    *templateCombo = nullptr;
    QPushButton  *templateEditorBtn = nullptr;
    QLineEdit    *suffixEdit = nullptr;
    QRadioButton *overwriteRadio = nullptr;
    QRadioButton *uniqueRadio = nullptr;
    QRadioButton *skipRadio = nullptr;
    QLabel       *exampleLab = nullptr;

    /* Format. The caption labels are kept because a setting the chosen format does not
       have is HIDDEN, caption and all -- see updateEnabledStates. Quality and Depth share
       the same cells (no format has both), so hiding either never empties a row. */
    QComboBox    *typeCombo = nullptr;
    QComboBox    *depthCombo = nullptr;
    QComboBox    *spaceCombo = nullptr;
    QComboBox    *compressionCombo = nullptr;
    QSlider      *qualitySlider = nullptr;
    QLabel       *qualityLab = nullptr;
    QLabel       *depthCapLab = nullptr;
    QLabel       *compressionCapLab = nullptr;
    QLabel       *qualityCapLab = nullptr;
    QCheckBox    *copyMetadataChk = nullptr;
    QCheckBox    *embedThumbChk = nullptr;

    /* Sizing */
    QRadioButton *fullSizeRadio = nullptr;
    QRadioButton *longEdgeRadio = nullptr;
    QRadioButton *percentRadio = nullptr;
    QSpinBox     *longEdgeSpin = nullptr;
    QSpinBox     *percentSpin = nullptr;
    QCheckBox    *dontEnlargeChk = nullptr;

    /* Footer */
    QPushButton  *exportBtn = nullptr;
    QPushButton  *closeBtn = nullptr;
};

#endif // EXPORTDLG_H
