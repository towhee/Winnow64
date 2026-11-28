#ifndef GREYCARD_H
#define GREYCARD_H

#include <QString>
#include <QStringList>
#include <atomic>

/*
    GREYCARD TRIALS -- the switches for every change ported or derived from greycard
    (github.com/jessolmstead/greycard, GPL-3.0-or-later), kept apart from the shipping
    pipeline until each is adopted or dropped. The plan, the measurements and the order
    are in notes/GreycardPlan.md; Documentation.md has a section per trial once built.

    PRE-RELEASE AND G::isRory ONLY. active() is false for every other user whatever the
    stored settings say, so nothing here can reach a release build's renders. The
    Preferences group that sets these (Preferences::addGreycard) is likewise built only
    for isRory. The one exception is the headless calibration tools (--dumpworking,
    developrender), which opt in per run with greycard=1 (parseToolArg below).

    A/B. Hold ` in Develop to see A (MW::setGreycardHold); the status-bar badge says
    which side is up. Headless: dump with gc.sc=off and gc.sc=dual, render both.

    GLOBAL STATE, NOT SIDECAR KEYS. A trial is a comparison, not an edit, so it must not
    be written into a recipe a later build would have to keep honoring. A trial that is
    adopted becomes an ordinary EditParams key (with its migration) at that point.

    EVERY RENDER KEY FOLDS key(): a devPreview or sidecar thumbnail built with a trial on
    must miss when it is off, and vice versa (Metadata::renderKey).

    Kept out of Main/global.h on purpose: almost every translation unit includes that, and
    each trial adds fields here. Only the code a trial touches includes this header.
*/
namespace G {

extern bool isRory;     // Main/global.h; redeclared so this header stands alone

struct Greycard {
    /* C1 + C2: how Winnow Standard (no camera profile selected) turns camera RGB into
       the working space.
         Off             the shipping path: libraw's ONE D65 matrix with the file's
                         camera WB, and slider WB as working-space gains.
         CameraWb        the same D65 matrix, but the white balance moves INSIDE the
                         input matrix and is re-derived per temperature, exactly as a
                         DNG profile with one illuminant does.
         DualIlluminant  CameraWb, plus a StdA matrix interpolated with the D65 one in
                         mired at the chosen temperature, as a two-illuminant DNG
                         profile does. A DNG uses its own ColorMatrix1/2.
       Built as an in-memory DNG profile (CameraProfileStore::winnowStandard), so it rides
       the tested DCP path rather than adding a third color path. */
    enum class StandardColor { Off = 0, CameraWb = 1, DualIlluminant = 2 };

    bool enabled = false;                                       // master switch
    StandardColor standardColor = StandardColor::DualIlluminant;

    /* The headless tools (Winnow --dumpworking, developrender) are not the Rory build and
       read no settings; they opt in with greycard=1 on their command line, which sets
       this as well as enabled. Never set by the app. */
    bool allowHeadless = false;

    /* HOLD-TO-COMPARE: while ` is held in Develop (MW::setGreycardHold), every trial
       renders as A, the shipping behavior. Static and atomic because the renders that
       read it run on worker threads while the GUI thread flips it; a static member keeps
       the struct copyable for the tests. */
    static inline std::atomic<bool> holdA{false};

    /* Are trials switched on in the app (Preferences)? What the status-bar badge and the
       hold key test. Unaffected by the hold. */
    bool trialsOn() const { return enabled && isRory; }

    /* Is the B side rendering right now? What every trial's code tests. */
    bool active() const {
        return enabled && (isRory || allowHeadless)
               && !holdA.load(std::memory_order_relaxed);
    }

    StandardColor effectiveStandardColor() const {
        return active() ? standardColor : StandardColor::Off;
    }

    /* The render-key part: empty when nothing is active, so every existing key and cache
       entry is unchanged for a user who never turns a trial on. */
    QString key() const {
        if (!active()) return QString();
        return QString("|greycard=1:sc%1").arg(int(standardColor));
    }
};

extern Greycard greycard;       // defined in Main/global.cpp

/*
    One command-line argument of a headless tool, shared so --dumpworking and
    developrender spell the trials the same way:

        greycard=1          opt in (sets enabled and allowHeadless); 0 opts out
        gc.sc=off|camwb|dual   C1/C2 "Winnow Standard color" (or 0|1|2)

    Returns 1 when it was a greycard argument and was applied, 0 when the key is not a
    greycard one (the caller handles it), -1 when the value is bad (err says why).
    inputStage is set for a trial that acts on the INPUT stage (the camera matrix and
    white balance): a .wkg dump is written after that stage, so such a trial has to be
    given to --dumpworking, and developrender refuses it.
*/
inline int parseToolArg(Greycard &g, const QString &key, const QString &value,
                        QString &err, bool &inputStage)
{
    inputStage = false;
    if (key == QLatin1String("greycard")) {
        if (value != QLatin1String("0") && value != QLatin1String("1")) {
            err = QString("greycard=%1: expected 0 or 1").arg(value);
            return -1;
        }
        g.enabled = g.allowHeadless = value == QLatin1String("1");
        return 1;
    }
    if (key == QLatin1String("gc.sc")) {
        inputStage = true;
        static const QStringList names = {"off", "camwb", "dual"};
        int sc = names.indexOf(value.toLower());
        if (sc < 0) {
            bool ok = false;
            sc = value.toInt(&ok);
            if (!ok || sc < 0 || sc > 2) sc = -1;
        }
        if (sc < 0) {
            err = QString("gc.sc=%1: expected off, camwb or dual").arg(value);
            return -1;
        }
        g.standardColor = static_cast<Greycard::StandardColor>(sc);
        return 1;
    }
    if (key.startsWith(QLatin1String("gc."))) {
        err = QString("unknown greycard trial %1").arg(key);
        return -1;
    }
    return 0;
}

} // namespace G

#endif // GREYCARD_H
