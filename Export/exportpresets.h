#ifndef EXPORTPRESETS_H
#define EXPORTPRESETS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include "Export/exportsettings.h"

/*
    Export presets -- the persistent store, and the sibling of Develop's preset store
    (Develop/Presets/developpresets.h). The two are deliberately shaped the same so they
    read the same way, but they hold different things and never mix: a DEVELOP preset is a
    partial recipe applied TO an image, an EXPORT preset is a complete ExportSettings
    describing how images leave Winnow.

    COMPLETE, not partial. A develop preset is partial on purpose (an absent key means
    "don't touch this adjustment"). An export preset is the opposite: it is a whole
    ExportSettings including the DESTINATION, so "Web 2048 JPEG" always means the same
    thing and can be run straight from Develop > Export with preset with no dialog at all.
    A missing key falls back to the ExportSettings default, which is what lets an old
    preset survive a new field.

    QSettings layout:

        Export Presets/<name>/version       = schema version of the keys below
        Export Presets/<name>/<field>       = one ExportSettings field per key

    VERSION / MIGRATION. A preset lives in QSettings indefinitely and is applied by
    assigning its stored values straight into an ExportSettings, so a preset key is
    effectively a PUBLISHED FORMAT. Adding keys is always safe (an absent key keeps the
    struct default), but renaming one silently drops that setting from every existing
    preset, and RESCALING or redefining one silently corrupts them -- the old number is
    written into the new field unchanged. So: add freely, never rename or rescale in
    place. When that is genuinely unavoidable, bump kVersion and convert in migrate(),
    which runs inside read() before any value reaches the struct. Presets written before
    the version key existed read back as version 1 -- they are v1 by definition, since v1
    is the schema they were written against. See notes/Documentation.txt "Stale presets
    and history (schema drift)".
*/
class ExportPresets : public QObject
{
    Q_OBJECT
public:
    explicit ExportPresets(QSettings *setting, QObject *parent = nullptr);

    /* Current schema version of the keys written by write(). */
    static constexpr int kVersion = 1;

    QStringList    names() const;                  // sorted, case-insensitive
    bool           contains(const QString &name) const;
    /* The named preset, or a default-constructed ExportSettings if it does not exist.
       Absent keys keep their struct default, so an older preset simply omits newer
       fields rather than zeroing them. */
    ExportSettings read(const QString &name) const;
    void           write(const QString &name, const ExportSettings &s);  // overwrites
    bool           rename(const QString &from, const QString &to);
    void           remove(const QString &name);

    /* The settings the dialog opens with when no preset is chosen: whatever was last
       used. Kept out of the named presets so a plain export never mutates one. */
    ExportSettings readLast() const;
    void           writeLast(const ExportSettings &s);

signals:
    /* Any change to the NAMED presets (write / rename / remove). The Develop menu's
       "Export with preset" submenu rebuilds on this. */
    void presetsChanged();

private:
    /* Convert an older stored schema in place, before any value is read into the struct.
       No-op at v1 -- it exists so the first real migration has an obvious home. */
    void migrate(const QString &name, int fromVersion) const;

    QString groupOf(const QString &name) const;

    QSettings *setting;
};

#endif // EXPORTPRESETS_H
