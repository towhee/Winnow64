#ifndef DEVELOPSCHEMA_H
#define DEVELOPSCHEMA_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

/*
    The develop settings SCHEMA VERSION and its migration chain.

    One version number covers every store a develop recipe lives in: the XMP sidecar,
    the History snapshots in index.db (both EditStack JSON, read through
    EditStack::fromJson), the develop presets and the copy / paste clipboard (both read
    through DevelopPresets::read). All of them use the EditStack JSON field names, which
    is what lets ONE ordered list of steps serve them all.

    AUTO-MIGRATE ON LOAD (decided 2026-10-28). Only the newest develop maths exists in
    the code. When a build renames, rescales or redefines a setting, changes its default,
    or retunes an op so the same value renders differently, it bumps kVersion and appends
    ONE Step that rewrites old values so the old look is reproduced (or approximated)
    under the new meaning. A recipe stamped with an older version runs through every
    newer step, in order, before any value reaches EditParams.

    THE RULES
      - Steps are append-only. Never edit or delete a released step: a sidecar written
        three versions ago still depends on it.
      - Steps run on RAW JSON, before paramsFromJson, so a step can tell an ABSENT key
        (which meant the old default) from a present one. When a DEFAULT changes, the
        step writes the old default into a full recipe that lacks the key.
      - A preset is PARTIAL on purpose (an absent key means "don't touch"), so a step
        must not insert keys when `partial` is true.
      - A step only remaps values. If the op's PIXELS changed, also bump
        DevelopRevision::kEdited (Develop/developrevision.h) so cached previews of edited
        images are rebuilt.
      - Never reuse a retired key name (cameraProfileLook, maskHaloMode).
      - `summary` is shown to the user ("What changed" in the Develop panel), so write
        it for a photographer, not a programmer.

    A recipe stamped NEWER than this build (written by a later Winnow) is passed
    through untouched; EditStack::fromBase64 reports it.

    Header-only, Qt Core only: editstack.h includes it, and so does every unit test that
    reads a sidecar.
*/

namespace DevelopSchema {

/* The schema version this build reads and writes. Must equal the `to` of the last
   released step (tst_developschema checks). */
constexpr int kVersion = 1;

struct Step {
    int         to;         // the version this step produces (previous + 1)
    const char *date;       // when it shipped, yyyy-mm-dd
    const char *summary;    // one user-facing line for "What changed"
    /* Rewrites one EditParams JSON object (a sidecar scope's "params", or a preset's
       params / globals). May be null. `partial` = a preset: do not insert keys. */
    void (*params)(QJsonObject &params, bool partial);
    /* Rewrites the stack root (geometry, fill, scope fields outside "params",
       mask components). Sidecar / History only. May be null. */
    void (*stack)(QJsonObject &stack);
};

/* THE RELEASED STEPS, oldest first. Empty while kVersion is 1. A step looks like

       { 2, "2026-11-03", "Shadows recalibrated; your values were converted to match",
         [](QJsonObject &p, bool) {
             if (p.contains("shadows"))
                 p["shadows"] = p.value("shadows").toDouble() * 0.8;
         },
         nullptr },
*/
inline const QVector<Step> &releasedSteps()
{
    static const QVector<Step> steps;
    return steps;
}

/* Test hook: tst_developschema substitutes its own chain to exercise the machinery
   while no real step exists. Null in the app. */
inline const QVector<Step> *&testSteps()
{
    static const QVector<Step> *p = nullptr;
    return p;
}

inline const QVector<Step> &steps()
{
    return testSteps() ? *testSteps() : releasedSteps();
}

/* The version a recipe is brought up to and stamped with: kVersion in the app, the
   last injected step's `to` under test. */
inline int current()
{
    const QVector<Step> &s = steps();
    if (testSteps()) return s.isEmpty() ? 1 : s.last().to;
    return kVersion;
}

/* Runs every step newer than `from` on one params object. Returns the version it is
   now at (`from` unchanged when it is already current or newer). */
inline int migrateParams(QJsonObject &params, int from, bool partial)
{
    const int cur = current();
    if (from >= cur) return from;
    for (const Step &st : steps())
        if (st.to > from && st.to <= cur && st.params) st.params(params, partial);
    return cur;
}

/* Brings a whole EditStack JSON object up to current(), in place, and restamps its
   "version". Steps are applied one at a time to the root AND every scope's params, so
   a later step always sees the output of the earlier ones. Returns the version the
   stack was migrated FROM, or 0 when nothing was done (current, or newer). An absent
   "version" is 1: the stamp has been written since the first sidecar. */
inline int migrateStack(QJsonObject &stack)
{
    const int from = stack.value("version").toInt(1);
    const int cur = current();
    if (from >= cur) return 0;
    for (const Step &st : steps()) {
        if (st.to <= from || st.to > cur) continue;
        if (st.stack) st.stack(stack);
        if (!st.params) continue;
        QJsonArray scopes = stack.value("scopes").toArray();
        for (int i = 0; i < scopes.size(); ++i) {
            QJsonObject scope = scopes[i].toObject();
            QJsonObject params = scope.value("params").toObject();
            st.params(params, false);
            scope["params"] = params;
            scopes[i] = scope;
        }
        stack["scopes"] = scopes;
    }
    stack["version"] = cur;
    return from;
}

/* The user-facing lines for every step a recipe stamped `from` went through, oldest
   first, each prefixed with its date. */
inline QStringList summariesSince(int from)
{
    QStringList out;
    const int cur = current();
    for (const Step &st : steps())
        if (st.to > from && st.to <= cur)
            out << QString::fromLatin1(st.date) + "  " + QString::fromUtf8(st.summary);
    return out;
}

} // namespace DevelopSchema

#endif // DEVELOPSCHEMA_H
