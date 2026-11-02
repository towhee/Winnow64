/*
    Unit tests for ExportPresets::effective and the Embellish preset keys.

    effective() is what the Export dialog shows and what MW::runExport runs: with an
    Embellish template the size / depth / space are forced, and with "Use the template's
    export settings" the template's own Export section (Embel/Templates/<name>/Export/)
    replaces the destination, naming and format. A temporary INI file stands in for the
    app's QSettings.
*/

#include <QtTest>
#include <QTemporaryDir>
#include "Export/exportpresets.h"

class TstExportEmbellish : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void noTemplate_unchanged();
    void template_forcesSizeDepthSpace();
    void templateOutput_subfolder();
    void templateOutput_otherFolder();
    void templateOutput_defaults();
    void ownOutput_keptWhenUnticked();
    void templates_listSkipsPlaceholder();
    void presetKeys_roundTrip();

private:
    QTemporaryDir *dir = nullptr;
    QSettings *setting = nullptr;
    ExportPresets *presets = nullptr;
    ExportSettings userSettings() const;
};

void TstExportEmbellish::init()
{
    dir = new QTemporaryDir;
    setting = new QSettings(dir->filePath("winnow.ini"), QSettings::IniFormat);
    presets = new ExportPresets(setting);

    const QString sub = "Embel/Templates/Web/Export/";
    setting->setValue(sub + "fileType", "PNG");
    setting->setValue(sub + "fileQuality", 80);
    setting->setValue(sub + "saveMethod", "Subfolder");
    setting->setValue(sub + "subfolder", "Embellished");
    setting->setValue(sub + "suffix", "_web");
    setting->setValue(sub + "copyMetadata", true);
    setting->setValue(sub + "overwriteFiles", true);

    const QString other = "Embel/Templates/Print/Export/";
    setting->setValue(other + "saveMethod", "Another folder somewhere else");
    setting->setValue(other + "folderPath", "/tmp/print");

    setting->setValue("Embel/Templates/Bare/isCurrent", false);   // no Export section
    setting->setValue("Embel/Templates/Do not Embellish/isCurrent", true);
}

void TstExportEmbellish::cleanup()
{
    delete presets;
    delete setting;
    delete dir;
}

ExportSettings TstExportEmbellish::userSettings() const
{
    ExportSettings s;
    s.dest = ExportSettings::ChosenFolder;
    s.folderPath = "/Users/me/out";
    s.suffix = "_dev";
    s.tokenTemplate = "{YYYY}-{ORIGINAL FILENAME}";
    s.format = "tif";
    s.bitDepth = 16;
    s.space = OutputSpace::AdobeRGB;
    s.sizing = ExportSettings::LongEdge;
    s.longEdgePx = 2048;
    s.quality = 70;
    s.exists = ExportSettings::Skip;
    return s;
}

void TstExportEmbellish::noTemplate_unchanged()
{
    const ExportSettings s = userSettings();
    const ExportSettings e = presets->effective(s);
    QCOMPARE(e.format, s.format);
    QCOMPARE(e.bitDepth, 16);
    QCOMPARE(e.sizing, ExportSettings::LongEdge);
    QCOMPARE(e.folderPath, s.folderPath);
}

void TstExportEmbellish::template_forcesSizeDepthSpace()
{
    ExportSettings s = userSettings();
    s.embellish = "Web";
    s.embellishUseTemplateOutput = false;
    const ExportSettings e = presets->effective(s);
    QCOMPARE(e.sizing, ExportSettings::FullSize);
    QCOMPARE(e.bitDepth, 8);
    QCOMPARE(e.space, OutputSpace::sRGB);
}

void TstExportEmbellish::templateOutput_subfolder()
{
    ExportSettings s = userSettings();
    s.embellish = "Web";
    const ExportSettings e = presets->effective(s);
    QCOMPARE(e.format, QString("png"));
    QCOMPARE(e.quality, 80);
    QCOMPARE(e.dest, ExportSettings::SubfolderOfSource);
    QCOMPARE(e.subfolderName, QString("Embellished"));
    QCOMPARE(e.suffix, QString("_web"));
    QVERIFY(e.tokenTemplate.isEmpty());
    QCOMPARE(e.exists, ExportSettings::Overwrite);
    QVERIFY(e.copyMetadata);
    QVERIFY(e.embedThumbnail);
}

void TstExportEmbellish::templateOutput_otherFolder()
{
    ExportSettings s = userSettings();
    s.embellish = "Print";
    const ExportSettings e = presets->effective(s);
    QCOMPARE(e.dest, ExportSettings::ChosenFolder);
    QCOMPARE(e.folderPath, QString("/tmp/print"));
}

void TstExportEmbellish::templateOutput_defaults()
{
    /* EmbelProperties' defaults for an absent key: JPG, 100, Subfolder, "_<name>",
       no overwrite, no metadata. */
    ExportSettings s = userSettings();
    s.embellish = "Bare";
    const ExportSettings e = presets->effective(s);
    QCOMPARE(e.format, QString("jpg"));
    QCOMPARE(e.quality, 100);
    QCOMPARE(e.dest, ExportSettings::SubfolderOfSource);
    QVERIFY(e.subfolderName.isEmpty());
    QCOMPARE(e.suffix, QString("_Bare"));
    QCOMPARE(e.exists, ExportSettings::UniqueName);
    QVERIFY(!e.copyMetadata);
}

void TstExportEmbellish::ownOutput_keptWhenUnticked()
{
    ExportSettings s = userSettings();
    s.embellish = "Web";
    s.embellishUseTemplateOutput = false;
    const ExportSettings e = presets->effective(s);
    QCOMPARE(e.format, s.format);
    QCOMPARE(e.folderPath, s.folderPath);
    QCOMPARE(e.suffix, s.suffix);
    QCOMPARE(e.tokenTemplate, s.tokenTemplate);
    QCOMPARE(e.exists, ExportSettings::Skip);
}

void TstExportEmbellish::templates_listSkipsPlaceholder()
{
    QCOMPARE(presets->embellishTemplates(),
             QStringList({"Bare", "Print", "Web"}));
}

void TstExportEmbellish::presetKeys_roundTrip()
{
    ExportSettings s = userSettings();
    s.embellish = "Web";
    s.embellishUseTemplateOutput = false;
    presets->write("Embellished web", s);
    const ExportSettings r = presets->read("Embellished web");
    QCOMPARE(r.embellish, QString("Web"));
    QVERIFY(!r.embellishUseTemplateOutput);

    /* A preset saved before the keys existed: no template, template output on. */
    presets->write("Old", ExportSettings());
    setting->remove("Export Presets/Old/embellish");
    setting->remove("Export Presets/Old/embellishUseTemplateOutput");
    const ExportSettings o = presets->read("Old");
    QVERIFY(o.embellish.isEmpty());
    QVERIFY(o.embellishUseTemplateOutput);
}

QTEST_MAIN(TstExportEmbellish)
#include "tst_exportembellish.moc"
