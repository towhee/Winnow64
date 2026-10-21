#ifndef EDITPARAMS_H
#define EDITPARAMS_H

#include <QString>
#include "Develop/tonecurve.h"
#include "Develop/pointcolor.h"

/*
    Parametric, non-destructive develop adjustments (Lightroom-style). One versioned
    struct, identity defaults, so an unset / absent EditParams is a no-op and changes
    nothing. This is the SINGLE source of truth for all adjustment values: every stage
    that needs an adjustment (the generic Develop processor, and for RAW the white-
    balance / denoise steps inside RawFormat) reads from this one struct.

    Persisted per-image (datamodel column + sidecar) and bound to the slider UI -- both
    deferred. See notes/Documentation.txt "DEVELOP / IMAGE EDIT".
*/
struct EditParams {
    /* White balance, ABSOLUTE (Lightroom-style), resolved against the source's own colour
       characterisation -- see Develop/whitebalance.h.

         temp  correlated colour temperature in KELVIN (2000..50000). The sentinel 0 means
               "as shot": the image has no white-balance edit, so it stays at its decoded
               balance and reads as identity. It does NOT mean 0 K.
         tint  green/magenta offset (-150..+150, positive = magenta), absolute like temp.

       The panel writes both together -- moving Tint alone still commits the resolved
       Kelvin -- so temp == 0 only ever means a pristine image and the pair can never
       disagree about their reference.

       wbPreset is the WB dropdown's selection (a WhiteBalance::Preset cast to int; 0 =
       As Shot). Kept only so the dropdown can show what the user picked: temp/tint above
       are what actually render, and any manual slider move switches this to Custom. */
    float temp = 0.0f;
    float tint = 0.0f;
    int   wbPreset = 0;

    /* Tone. All 0 = identity. */
    float exposure   = 0.0f;    // EV
    float contrast   = 0.0f;
    float highlights = 0.0f;
    float shadows    = 0.0f;
    float whites     = 0.0f;
    float blacks     = 0.0f;

    /* Tone-region split positions (perceptual 0..1), set by the histogram region slider. They
       move WHERE the shadows/highlights controls act and how far each reaches; blacks (0) and
       whites (1) stay pinned at the ends. Defaults 0.25/0.50/0.75 reproduce the fixed centres
       the tone curve used before, so they are a no-op until moved (and only matter when a tone
       slider is non-zero -- hence not part of isIdentity()). */
    float toneShadowCenter    = 0.25f;
    float toneCrossover       = 0.50f;
    float toneHighlightCenter = 0.75f;

    /* Tone curve (Curves panel) -- Lightroom's Tone Curve. FOUR independent point curves:
       channel 0 is the RGB composite (applied to every channel) and 1/2/3 are Red / Green
       / Blue (applied on top of the composite, to that channel alone). Control points are
       (x, y) in the PERCEPTUAL 0..1 domain, ordered by x with both endpoints pinned at
       x = 0 and x = 1 (y free, which is what gives the black-/white-point moves).

       Fixed-size arrays rather than a container so operator== stays defaulted, the copies
       this struct takes per render and per history snapshot stay cheap, and
       EditStack::sanitizeParams stays purely numeric. (cameraProfile below is the one
       non-scalar member; a QString copy is a refcount, so the cost argument holds.) curveN[c] == 2 with the diagonal
       is identity, and the defaults below are exactly that -- so an untouched image
       writes no sidecar. UNLIKE the tone splits above, the curve IS a primary control
       and so DOES count towards isIdentity(). Maths, repair and the shared string
       encoding are all in Develop/tonecurve.h. */
    int   curveN[ToneCurve::kChannels] = {2, 2, 2, 2};
    float curveX[ToneCurve::kChannels][ToneCurve::kMaxPts] =
        {{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};
    float curveY[ToneCurve::kChannels][ToneCurve::kMaxPts] =
        {{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};

    /* True when all four curves are the 2-point diagonal. */
    bool curvesAreIdentity() const {
        for (int c = 0; c < ToneCurve::kChannels; ++c)
            if (!ToneCurve::isIdentity(curveX[c], curveY[c], curveN[c])) return false;
        return true;
    }

    /* Presence -- three local-contrast controls at three radii, all -100..100, all stored
       RAW (no divisor: the panel writes the slider value straight through).

         texture  ~0.0015 x long edge (~13px on 8640)   fine detail contrast
         clarity  ~0.007  x long edge (~60px on 8640)   midtone punch / glow
         dehaze   ~0.02   x long edge (~170px on 8640)  atmospheric veiling

       An order of magnitude apart, so they stack rather than compete. Clarity is midtone-
       weighted and halo-guarded, which is what separates it from a wider Texture -- see
       Develop::Clarity and Develop/localcontrast.h. Sharpening (Detail panel) is the same
       family again at ~1px absolute. */
    float texture = 0.0f;
    float clarity = 0.0f;
    float dehaze  = 0.0f;

    /* Colour -- RGB. Per-channel scene-linear gain (-100..100). Folded into the fused point
       pass's per-channel gain alongside white balance + exposure, so they cost nothing extra.
       0 = no change; positive lifts that channel, negative attenuates it. */
    float red   = 0.0f;
    float green = 0.0f;
    float blue  = 0.0f;

    /* Colour -- HSL (global, -100..100). Cross-channel point op applied after the tone
       curve in the same fused pass. hue = rotation about the neutral (gray) axis;
       saturation = chroma scale about luma (-100 -> grayscale, +100 -> 2x); vibrance =
       saturation weighted by how muted each pixel already is (boosts low-chroma pixels,
       protects saturated ones); luminance = uniform brightness gain. */
    float hue        = 0.0f;
    float saturation = 0.0f;
    float vibrance   = 0.0f;
    float luminance  = 0.0f;

    /* Camera calibration (Calibrate panel) -- rotates the three PRIMARIES of the working
       space: each of R/G/B gets a hue shift (-100..100 -> +/-30 deg about the neutral
       axis) and a saturation scale (-100..100 -> chroma x0..x2 about the primary's own
       luma). Built into one 3x3 matrix whose columns are the re-pointed primaries, so
       neutrals stay neutral. Applied in LINEAR light before the tone curve -- distinct
       from the Color panel's red/green/blue, which are per-channel gains folded into
       white balance, and from its HSL hue, which runs after the curve. Math is
       header-only in Develop/calibrate.h. Identity = all six zero. */
    float calRedHue   = 0.0f, calRedSat   = 0.0f;
    float calGreenHue = 0.0f, calGreenSat = 0.0f;
    float calBlueHue  = 0.0f, calBlueSat  = 0.0f;
    /* Calibration's Shadows Tint (-100..100): green (-) / magenta (+) in the SHADOWS
       only, luma-preserving, in the same linear slot as the primaries -- Lightroom's
       Calibration > Shadows > Tint. Math in Develop/calibrate.h (shadowTintGains). */
    float calShadowTint = 0.0f;

    /* The VIEW TRANSFORM (the first row of the Basic panel) -- how scene-linear data is
       mapped to a displayable range. An OutputTransform::ViewTransform cast to int:
       0 = None, 1 = Filmic, 2 = AgX. Stored as an int for the same reason wbPreset
       is: it rides the existing int machinery (sanitizeParams clamp, kIntFields, the
       preset round trip) instead of being the first non-scalar field in EditParams.

       NOT A DEVELOP OP. Every other field here is consumed by Develop::Apply; this one
       is consumed by OutputTransform, three stages later. It lives in EditParams anyway
       because it has to persist to the sidecar, travel in a preset and propagate across
       a multi-image selection like any other adjustment.

       It applies to the WHOLE IMAGE, not to a mask: only scope 0 (Global) is read. See
       "THE VIEW TRANSFORM" in notes/Documentation.txt.

       Values are a PUBLISHED FORMAT once written to a sidecar -- add freely, never
       renumber. The IDENTITY is still None (0), and it has to be: a value of zero is
       what "no tone mapping" means to every sidecar ever written.

       THE DEFAULT IS NOT THE IDENTITY, which is the one place this field differs from
       every other. A scene-referred raw is radiometrically linear and renders dark and
       flat with no view transform -- measurably so: against Lightroom's Adobe Standard
       rendering of the same file, an untransformed render's luminance spread is about
       two thirds as wide (std 24 vs 36 of 255) and its midtone sits ~0.7 EV low. That
       is not a neutral starting point, it is a broken one, and it is what an untouched
       raw looked like while this defaulted to None.

       Filmic is the default because it MATCHES: fitted against Lightroom's luminance
       histogram on the same raw it lands within ~1 level of 255 across every percentile
       (None is off by 6, AgX by 5). A raw therefore opens looking like the raw the rest
       of the world renders, and None remains one click away for anyone who wants the
       linear data.

       A PROFILE'S TONE CURVE IS ONE OF THESE CHOICES, not an override. The Camera *
       profiles carry a ProfileToneCurve; it applies only when this is ProfileCurve, and
       otherwise the profile contributes its colour (matrices, HueSatMap, LookTable) under
       whichever tone mapping is chosen. Adobe Standard and Winnow Standard carry no
       curve -- Lightroom supplies its own baseline curve in exactly that case, and
       Standard roll-off is Winnow's. CameraContrast is the camera-style middle ground: a
       per-maker curve with Winnow's roll-off (Develop/cameracurve.h).

       A JPEG IS UNAFFECTED whatever this says: OutputTransform forces None when
       img.sceneReferred is false, since a display-referred file already carries its
       camera's tone curve and mapping it again would tone-map twice. */
    static constexpr int kDefaultViewTransform = 1;     // OutputTransform::ViewTransform::Filmic
    int   viewTransform = kDefaultViewTransform;

    /*
        THE CAMERA PROFILE (the first row of the Basic panel) -- which characterisation of
        the SENSOR the raw is rendered through. Lightroom's Profile control: "Adobe
        Standard", "Camera Vivid", and so on. Empty is the default and means Winnow's
        own built-in matrix, which is what every image rendered with before profiles
        existed -- so an untouched image is identity and writes no sidecar.

        STORED BY NAME, NOT BY PATH. A profile lives in the Adobe install, or in Winnow's
        own folder, and the path to it differs per machine and per Adobe version; the name
        plus the image's own camera model resolves it wherever it is. A name that no longer
        resolves (a profile uninstalled, or a sidecar carried from another machine) renders
        with the built-in matrix and SAYS SO in the panel -- see the greyed-reason rule --
        rather than silently rendering as though nothing had been chosen.

        A PROFILE IS APPLIED WHOLE -- its matrix, its HueSatMap, and its look (LookTable,
        tone curve, exposure offset) if it has one. There is no switch for the look half.
        Adobe ships ONE colorimetric base per camera generation with a different look on
        each of its "Camera *" profiles, so "that profile without its look" is not a
        property of the profile -- it is one shared rendering, offered as its own entry
        named "Camera Base". See CameraProfileStore::deriveBases.

        RAW ONLY. A rendered JPEG has no sensor to characterise (cam.valid is false), and
        the Apple Core Image decoder has already applied a profile of its own, so the row
        is disabled with its reason in both cases.

        NOT A DEVELOP OP in the usual sense: it replaces stage 0, the camera-native ->
        working matrix, and it CARRIES THE WHITE BALANCE with it -- under DNG the chosen
        white sets the matrix rather than a gain in front of it. buildPointCoeffs therefore
        drops the per-channel WB gains on this path; applying both would white-balance
        twice. See Develop/cameraprofile.h.

        Like viewTransform it is a WHOLE-IMAGE property: only scope 0 is read, and the
        panel always writes scope 0 whatever scope is active. A profile characterises the
        camera, and one part of a picture cannot have been taken by a different one.
    */
    QString cameraProfile;


    /* Point Color (the Point Color panel, Lightroom's Color Mixer > Point Color) -- up to
       PointColor::kMaxPts sampled colours, each shifting the hue / saturation / lightness
       of the pixels near it. A fixed array for the same reasons as the tone curve
       (defaulted operator==, cheap copies); slots past pointColorN are ALWAYS default, so
       the comparison never sees stale data -- PointColor::decode and the panel's delete
       both keep that true. Applied in the fused pass after HSL, before Color Grade; maths
       and the shared string encoding in Develop/pointcolor.h. Identity = every point
       inert (a sample with no shift changes nothing) -- but isIdentity() counts any
       sample, so it persists. */
    int   pointColorN = 0;
    PointColor::Point pointColor[PointColor::kMaxPts] = {};

    /* VIEW STATE, NOT AN EDIT: the index of the point whose selection "Visualize Range"
       is showing (the rest of the image goes grey), or -1. Never serialised
       (paramsToJson skips it), never in a preset or the multi-image diff, and set only
       on the INTERACTIVE render job (DevelopProperties::stackJob) -- an export or a
       devPreview renders the stored recipe, where it is always -1. */
    int   pointColorVisualize = -1;

    bool pointColorIsIdentity() const {
        return PointColor::isIdentity(pointColorN, pointColor);
    }

    /* Colour grading (Color Grade panel) -- tonal-range tinting, the Lightroom "teal
       shadows / orange highlights" look. Three ranges (shadows / midtones / highlights);
       each ADDS a chroma tint of the given hue at the given saturation and NUDGES that
       range's luminance. Applied as a point op after HSL in the fused pass, weighted by
       smooth per-pixel tonal windows. hue is 0..360 (degrees), sat 0..1 (0 = no tint, so
       hue is irrelevant there), lum -100..100. Identity = every sat and lum 0. */
    float gradeShadowHue = 0.0f, gradeShadowSat = 0.0f, gradeShadowLum = 0.0f;
    float gradeMidHue    = 0.0f, gradeMidSat    = 0.0f, gradeMidLum    = 0.0f;
    float gradeHighHue   = 0.0f, gradeHighSat   = 0.0f, gradeHighLum   = 0.0f;

    /* A fourth grading range that is NOT tone-selective: Global applies its tint and
       luminance to every pixel at full weight, on top of the three windowed ranges. */
    float gradeGlobalHue = 0.0f, gradeGlobalSat = 0.0f, gradeGlobalLum = 0.0f;

    /* Shape of the three tonal windows, panel-wide rather than per-range.
       gradeBlending (0..100, default 50) widens or narrows the OVERLAP between shadows
       and highlights: 50 reproduces the fixed split the panel had before these existed,
       0 pulls them apart (a broader pure-midtone band), 100 pushes them together.
       gradeBalance (-100..100, default 0) slides the whole split -- positive favours the
       highlight range, negative the shadow range. Both are inert while every range is at
       sat/lum 0, so neither is part of isIdentity. */
    float gradeBlending = 50.0f;
    float gradeBalance  = 0.0f;

    /* Noise reduction -- GLOBAL, applied in the raw decode pipeline (RawFormat / the Apple
       engine) during demosaic, alongside start WB / black / white. Not maskable: these are
       baked into WorkingImage before Develop runs. 0 = engine default. */
    /* The "Denoise raw" amounts, 0..1 -- or -1 = UNSET: follow the REMEMBERED amounts
       (G::denoiseLumaDefault / G::denoiseChromaDefault, the last values the user set,
       persisted in QSettings). Same tri-state idea as denoiseRaw below: an image the user
       never adjusted takes the current remembered amounts, one they did keeps its own.
       Unset is the default and is OMITTED from the sidecar, so an untouched image keeps
       the recipe (and the devPreview key) it has always had.

       Read them through denoiseLumaOr / denoiseChromaOr, never directly.

       The FACTORY values below seed the remembered amounts for a new user. Color is full:
       chroma noise is objectionable and chroma detail is low-frequency, so baking 100 at
       decode is the right baseline. Luma is 0.75: strong enough to clean sensor grain
       while leaving fine detail intact. Sidecars written before 2026-10-13 always stored
       both amounts explicitly (usually these factory values), so those images keep them
       rather than following the remembered amounts -- reading a stored factory value as
       unset would have re-keyed every edited image's devPreview. */
    static constexpr float kDefaultDenoiseLuma   = 0.75f;
    static constexpr float kDefaultDenoiseChroma = 1.0f;
    float denoiseLuma   = -1.0f;
    float denoiseChroma = -1.0f;
    float denoiseLumaOr(float remembered) const {
        return denoiseLuma >= 0.0f ? denoiseLuma : remembered;
    }
    float denoiseChromaOr(float remembered) const {
        return denoiseChroma >= 0.0f ? denoiseChroma : remembered;
    }

    /*
        WHETHER that denoise runs at all -- TRI-STATE, because the amounts above cannot
        say it: their defaults are NON-ZERO, so "amount > 0" is true for every raw,
        including one nobody has touched. Before this the intent lived only in
        G::autoRunDenoise (a global preference) and in a session cache filled by the manual
        "Denoise" checkbox, so a manual run was invisible to the exporter, to the devPreview
        builder and to the next launch -- the loupe showed a denoised image while the stored
        preview stayed clean and its recipe hash claimed the two matched.

          -1  unset  follow the "Auto run" preference. THE DEFAULT, and omitted from the
                     sidecar (see paramsToJson), so an untouched raw keeps the recipe --
                     and therefore the devPreview key -- it has always had, and browsing a
                     folder of raws in Develop still writes no sidecars.
           0  off    not denoised -- unless Auto run is on.
           1  on     denoised, even with Auto run off.

        AUTO RUN OVERRIDES (2026-10-13). With Auto run on, every raw is denoised and the
        Raw panel hides the per-image "Run on this image" box, so a stored 0 cannot also
        win: it would render clean with nothing on screen saying why. Before this an
        explicit 0 beat Auto run. The values and the key are unchanged; only what 0 means
        under Auto run moved, so the field is effectively "opted in on its own".

        Read it through wantsDenoiseRaw(), never directly: every render site needs the
        same rule, and spelling that out per site is how they drift apart.
    */
    int denoiseRaw = -1;

    /* Does this recipe want the raw denoise? `autoRun` is the ambient preference
       (G::autoRunDenoise), passed in rather than read here so EditParams stays free of
       Main/global.h -- the unit tests and the standalone decoders include this header. */
    bool wantsDenoiseRaw(bool autoRun) const {
        return autoRun || denoiseRaw > 0;
    }

    /*
        RAW RENDERER ("Render using" on the Raw panel), Global scope only: which decoder
        turns this image's raw file into pixels. PER IMAGE, and so per version (virtual
        copy) -- a version can render with Apple Raw 9 while its master uses Winnow.

        rawEngine, TRI-STATE like denoiseRaw and for the same reason (an untouched raw
        must keep its recipe and devPreview key):
          -1  unset  follow the app-wide default (Preferences; G::decodeRawEngine and
                     AppleRawDecode::VersionPin). THE DEFAULT, omitted from the sidecar.
           0  Winnow the in-house decoder.
           1  Apple  Core Image (macOS only; Winnow elsewhere).
        appleRawVersion -- with rawEngine 1 only: 0 = the newest version the file
        supports, N = Apple Raw N (the newest <= N the file has; see
        AppleRawDecode::VersionPin for the rule).

        It changes the DECODE, not the develop pass: the cached pre-develop base is
        per renderer (WorkingImageCache tags). Resolve it through RawRender::resolve
        (Develop/rawrender.h), never by reading these directly.
    */
    int rawEngine       = -1;
    int appleRawVersion = 0;

    /*
        LENS CORRECTIONS (Develop/lenscorrect.h, Develop/lensprofile.h), Global scope
        only, applied to the BASE image before any scope develops -- so every mask, guide
        and scope sees corrected edges. Set from the Global scope's Lens section.

        removeCA -- lateral chromatic aberration, TRI-STATE like denoiseRaw and for the
        same reason (an untouched raw must keep its recipe and devPreview key):
          -1  unset  follow G::autoRemoveCA for a raw; off for anything else. DEFAULT,
                     omitted from the sidecar.
           0  off    not corrected, even with the preference on.
           1  on     corrected (the radial R/B scale is estimated from the image).
        Read it through wantsRemoveCA().

        defringePurple / defringeGreen -- edge colour fringing, 0..1, 0 = off. Purple
        covers cyan-blue through magenta (the Dempster ridge line was cyan-blue); green is
        off by default in practice, as Lightroom's is.
    */
    int   removeCA       = -1;
    float defringePurple = 0.0f;
    float defringeGreen  = 0.0f;

    bool wantsRemoveCA(bool autoRaw, bool isRaw) const {
        return removeCA >= 0 ? removeCA > 0 : (autoRaw && isRaw);
    }
    bool wantsDefringe() const { return defringePurple > 0.0f || defringeGreen > 0.0f; }

    /*
        LENS PROFILE (lensfun calibration; Develop/lensdb.h finds it, lensprofile.h
        applies it): distortion, transverse CA and vignetting for the lens in the EXIF.

        lensProfile -- TRI-STATE, removeCA's pattern and reason:
          -1  unset  follow G::autoLensProfile for a raw; off for anything else (a JPEG
                     was usually corrected in camera). DEFAULT, omitted from the sidecar.
           0  off
           1  on
        Read it through wantsLensProfile().
        lensDistortion / lensVignetting -- how much of the profile's correction, 0..2,
        1 = as calibrated (the panel shows 0..200).
        lensProfileId -- the lens the user picked (LensDb::Lens::id, "maker|model|crop");
        empty = matched automatically from the EXIF.

        The profile's TCA follows removeCA: with Remove CA on, a profile that has TCA
        replaces the estimate measured from the image.

        MANUAL (Lightroom's Manual tab; LensProfile::Amounts::manual*) -- need no profile
        and apply whether it is on, off or missing, on top of it when it is on:
        lensManualDistortion -- -1..1 (panel -100..100), 0 = none; + corrects barrel.
        lensManualVignetting -- -1..1 (panel -100..100), 0 = none; + brightens corners.
        lensManualMidpoint   -- 0..1 (panel 0..100), 0.5 default; how far in the
                                vignetting reaches (inert until Vignetting is non-zero,
                                so it is not in isIdentity).
    */
    int     lensProfile    = -1;
    float   lensDistortion = 1.0f;
    float   lensVignetting = 1.0f;
    QString lensProfileId;
    float   lensManualDistortion = 0.0f;
    float   lensManualVignetting = 0.0f;
    float   lensManualMidpoint   = 0.5f;

    bool wantsLensProfile(bool autoRaw, bool isRaw) const {
        return lensProfile >= 0 ? lensProfile > 0 : (autoRaw && isRaw);
    }

    /*
        LENS BLUR (Develop/lensblur.h, run by MW on the composited frame, before the
        geometry): a depth-of-field blur from the AI depth field (Develop/depthmask.h).
        Unlike the corrections above it does NOT touch the base, so the base and the
        stack caches ignore it (MW strips it from the job, as it does the kaleidoscope).

        Global scope:
        lensBlurAmount -- 0..1 (panel 0..100), 0 = off; the aperture. The only one in
                          isIdentity; the rest are inert until it is non-zero.
        lensBlurFocus  -- 0..1 depth of the focal plane, 0 = nearest .. 1 = farthest
                          (panel 0..100, or click the photo). Default 0: near in focus.
        lensBlurRange  -- 0..1 depth range kept sharp, centred on the focus (panel
                          0..100). Default 0.1.
        lensBlurShape  -- LensBlur::Shape: 0 circle, 1 five blades, 2 six blades,
                          3 cat's eye, 4 soap bubble, 5 anamorphic.
        lensBlurBoost  -- 0..1 (panel 0..100) highlight boost.

        Mask scope:
        lensBlurRefine -- -1..1 (panel -100..100), 0 = none. Over the mask, - pulls the
                          blur toward in focus, + pushes it toward a strong blur: the
                          fix for a depth estimate that got something wrong. In
                          isIdentity, since on its own it is an edit (it only shows
                          while Global's Lens Blur is on).
    */
    float lensBlurAmount = 0.0f;
    float lensBlurFocus  = 0.0f;
    float lensBlurRange  = 0.1f;
    int   lensBlurShape  = 0;
    float lensBlurBoost  = 0.0f;
    float lensBlurRefine = 0.0f;
    bool hasLensBlur() const { return lensBlurAmount > 0.0f; }

    /* Local (maskable) NR -- Develop SPATIAL ops layered on TOP of the global baseline, operating
       on the already-decoded WorkingImage (see Develop::Denoise and notes/Documentation.txt
       "Scope & masking model"). localDenoiseLuma = luminance NR (ratio-preserving); localDenoise-
       Chroma = colour/chroma NR (opponent-chroma blur, luma kept exact). 0 = off. Range 0..1. */
    float localDenoiseLuma = 0.0f;
    float localDenoiseChroma = 0.0f;

    /* Sharpen (Detail SPATIAL op, runs after Vignette and before Grain -- so film grain
       is never itself sharpened and the vignette's darkened corners do not get
       amplitude-boosted detail). Capture sharpening: an unsharp mask on perceptual
       luminance, ratio-preserving like Texture and Denoise.

       This is the ONLY Develop op with an ABSOLUTE pixel radius. Every other spatial op
       scales its radius to max(w,h) so the proxy preview matches the full-res render;
       acutance is a property of the pixel grid, so sharpening cannot. It reads
       WorkingImage::renderScale instead and is judged at 1:1 -- see Develop/sharpen.h.

       Distinct from Texture, which is a resolution-PROPORTIONAL mid-frequency band
       (~13 px on an 8640 px edge, ungated): the two occupy disjoint bands and stack.

         sharpenAmount   0..1.5  strength. 0 = off. The primary control (isIdentity).
         sharpenRadius   0.5..3  unsharp radius in PIXELS at full resolution.
         sharpenDetail   0..1    how much of the finest band survives: low soft-thresholds
                                 the smallest amplitudes away (treats them as noise), high
                                 passes everything through (more bite, more halo).
         sharpenMasking  0..1    edge gate. 0 sharpens everything, 1 edges only -- this
                                 is what stops sharpening amplifying sky/skin noise.

       Radius / detail / masking are inert while sharpenAmount is 0, so they are NOT part
       of isIdentity() -- same rule as vignetteFeather and grainSize. There is no
       negative amount ("soften"): negative Texture and Denoise already cover it. */
    float sharpenAmount  = 0.0f;
    float sharpenRadius  = 1.0f;
    float sharpenDetail  = 0.25f;
    float sharpenMasking = 0.0f;

    /* Vignette (Effects SPATIAL op, runs last). A radial exposure falloff about the image
       centre: vignetteExposure is the EV applied at the corners (negative darkens = the
       classic vignette, positive brightens), ramping smoothly to 0 at the centre.
       vignetteFeather (0..1) shapes the falloff -- high spreads the effect inward
       (gradual), low concentrates it in the corners. For anything off-centre or shaped,
       the user paints radial masks instead, so these two sliders are all the global
       vignette needs. Feather is not part of isIdentity (it only matters when
       vignetteExposure is non-zero). */
    float vignetteExposure = 0.0f;
    float vignetteFeather  = 0.5f;

    /* Grain (Effects SPATIAL op, runs last -- after Vignette). Monochromatic film grain
       added to luminance (ratio-preserving), most visible in the midtones and fading
       toward black / white. grainAmount (0..1) is the strength; grainSize (0..1) the
       particle size, scaled to the image so the proxy preview matches full res;
       grainRoughness (0..1) the irregularity of the grain amplitude (0 = uniform). Size
       and roughness only matter when grainAmount is non-zero, so they are not part of
       isIdentity(). */
    float grainAmount    = 0.0f;
    float grainSize      = 0.25f;
    float grainRoughness = 0.5f;

    /* Kaleidoscope (Effects, a GEOMETRIC op -- not run by Develop::Apply). After the crop
       (MW::developCompositeStack, CropTransform::applyKaleidoscope*), the frame is folded
       around a centre: kaleidoSegments wedges (0 = off, else EVEN 2..32 so every seam is
       a mirror), kaleidoX / kaleidoY the centre normalized in the CROPPED frame,
       kaleidoAngle (deg, -180..180, y-down) turns the mirror lines and the sampled slice
       together, kaleidoSource (deg) turns only the slice, kaleidoZoom magnifies the
       slice about the centre (the photo is sampled at radius r / zoom; 1 = as is, 0.25
       .. 8), kaleidoOpacity (0..1) blends the fold with the unfolded frame. Per scope: on a mask scope only
       the masked pixels are folded (unmasked = transparent) and laid over the frame;
       Global (and a mask-less scope) folds the whole frame, last. Only kaleidoSegments is
       in isIdentity (the rest are inert while it is off). */
    int   kaleidoSegments = 0;
    float kaleidoX        = 0.5f;
    float kaleidoY        = 0.5f;
    float kaleidoAngle    = 0.0f;
    float kaleidoSource   = 0.0f;
    float kaleidoZoom     = 1.0f;
    float kaleidoOpacity  = 1.0f;
    bool hasKaleido() const { return kaleidoSegments >= 2; }

    /* Blur (Effects, a SPATIAL Develop op after Sharpen, before Grain -- Develop::Blur,
       Develop/artblur.h). The ARTISTIC blurs; a depth / bokeh lens blur belongs to the
       Lens panel. blurType picks one (ArtBlur::Type: 0 Gaussian, 1 Motion, 2 Spin,
       3 Zoom, 4 Tilt-shift, 5 Glow); blurAmount (0..1) is its strength and the only field
       in isIdentity. blurAngle (deg, y-down) is the Motion direction and the Tilt-shift
       band; blurX / blurY the centre (Spin, Zoom, Tilt-shift), normalized in the ORIENTED
       frame like a mask -- MW converts both to the source frame before the develop;
       blurSize (0..1) the Glow radius / Tilt-shift band width; blurFeather (0..1) the
       Tilt-shift transition. Distances are fractions of the long edge. */
    int   blurType    = 0;
    float blurAmount  = 0.0f;
    float blurAngle   = 0.0f;
    float blurX       = 0.5f;
    float blurY       = 0.5f;
    float blurSize    = 0.5f;
    float blurFeather = 0.5f;

    int version = 1;

    /* Adjustment groups, matching the Develop panel's section headers (Basic / Color / Effects).
       These drive the per-section Preview (show/ignore) and Reset (restore defaults) controls: a
       group is a fixed subset of the fields above. Note the grouping follows the UI, not the field
       comments -- texture/dehaze/vibrance/saturation sit under Basic, and the tone splits
       belong to Basic for Reset (they are irrelevant to Preview since the tone sliders they modulate are zeroed anyway).
       localDenoiseLuma ("Denoise", local post-demosaic NR) is under Detail. denoiseLuma/denoiseChroma
       are decode-time global NR (the Global scope's "Denoise raw", baked before Develop runs) so they
       are in NO group and cannot be previewed/reset via params. */
    /* ColorGrade = the nine colour-grading fields, Calibrate = the six primary fields;
       each is its own group so its panel's Preview/Reset are independent of the Color
       panel's RGB/HSL group. */
    /* Detail = sharpening + the local (post-demosaic) NR pair. The two fight each other
       -- NR removes the detail sharpening amplifies -- so they share a panel, as they do
       in Lightroom. localDenoise* moved here from Effects; the saved preset format is
       unaffected (presets store a flat map of field names, section titles only group the
       checklist UI), but a scope's showDetail rather than showEffects now gates them. */
    /* Lens = the Global-only lens corrections (profile, removeCA, defringe) and Lens
       Blur, the Global scope's Lens section. They sat under Effects until the Lens
       section existed; the preset format is unaffected (presets store field names, not
       sections). A mask scope never sets them, except lensBlurRefine -- the one row its
       own Lens section shows. */
    enum class Group { Basic, Curves, Color, Calibrate, ColorGrade, Detail, Effects,
                       PointColor, Lens };

    /* Force one group's fields back to their identity defaults, in place. The defaults come from a
       fresh EditParams{} so the non-zero tone-split defaults (0.25/0.50/0.75) restore correctly.
       Reset calls this on the STORED params (destructive); the Preview fold calls it on a COPY. */
    static void resetGroup(EditParams &p, Group g) {
        const EditParams def;
        switch (g) {
        case Group::Basic:
            p.temp = def.temp; p.tint = def.tint; p.wbPreset = def.wbPreset;
            p.exposure = def.exposure; p.contrast = def.contrast;
            p.highlights = def.highlights; p.shadows = def.shadows;
            p.whites = def.whites; p.blacks = def.blacks;
            p.texture = def.texture; p.clarity = def.clarity;
            p.dehaze = def.dehaze;
            p.vibrance = def.vibrance; p.saturation = def.saturation;
            p.toneShadowCenter = def.toneShadowCenter;
            p.toneCrossover = def.toneCrossover;
            p.toneHighlightCenter = def.toneHighlightCenter;
            p.viewTransform = def.viewTransform;
            p.cameraProfile = def.cameraProfile;
            break;
        case Group::Curves:
            /* Every channel back to the diagonal. The tone splits are NOT reset here:
               they are shared with Basic's shadows/highlights (the Curves panel only
               mirrors them), and Basic owns them for Reset. */
            for (int c = 0; c < ToneCurve::kChannels; ++c)
                ToneCurve::setIdentity(p.curveN[c], p.curveX[c], p.curveY[c]);
            break;
        case Group::Color:
            p.red = def.red; p.green = def.green; p.blue = def.blue;
            p.hue = def.hue; p.luminance = def.luminance;
            break;
        case Group::PointColor:
            p.pointColorN = def.pointColorN;
            for (int i = 0; i < PointColor::kMaxPts; ++i) p.pointColor[i] = def.pointColor[i];
            p.pointColorVisualize = def.pointColorVisualize;
            break;
        case Group::Calibrate:
            p.calRedHue = def.calRedHue;     p.calRedSat = def.calRedSat;
            p.calGreenHue = def.calGreenHue; p.calGreenSat = def.calGreenSat;
            p.calBlueHue = def.calBlueHue;   p.calBlueSat = def.calBlueSat;
            p.calShadowTint = def.calShadowTint;
            break;
        case Group::ColorGrade:
            p.gradeShadowHue = def.gradeShadowHue; p.gradeShadowSat = def.gradeShadowSat;
            p.gradeShadowLum = def.gradeShadowLum;
            p.gradeMidHue = def.gradeMidHue; p.gradeMidSat = def.gradeMidSat;
            p.gradeMidLum = def.gradeMidLum;
            p.gradeHighHue = def.gradeHighHue; p.gradeHighSat = def.gradeHighSat;
            p.gradeHighLum = def.gradeHighLum;
            p.gradeGlobalHue = def.gradeGlobalHue;
            p.gradeGlobalSat = def.gradeGlobalSat;
            p.gradeGlobalLum = def.gradeGlobalLum;
            p.gradeBlending = def.gradeBlending; p.gradeBalance = def.gradeBalance;
            break;
        case Group::Detail:
            p.sharpenAmount = def.sharpenAmount;             // "Sharpening" (capture USM)
            p.sharpenRadius = def.sharpenRadius;
            p.sharpenDetail = def.sharpenDetail;
            p.sharpenMasking = def.sharpenMasking;
            p.localDenoiseLuma = def.localDenoiseLuma;       // "Denoise" (local NR)
            p.localDenoiseChroma = def.localDenoiseChroma;   // "Denoise Color" (chroma)
            break;
        case Group::Effects:
            p.vignetteExposure = def.vignetteExposure;       // "Vignette" (radial)
            p.vignetteFeather = def.vignetteFeather;
            p.grainAmount = def.grainAmount;                 // "Grain" (film grain)
            p.grainSize = def.grainSize;
            p.grainRoughness = def.grainRoughness;
            p.kaleidoSegments = def.kaleidoSegments;         // "Kaleidoscope"
            p.kaleidoX = def.kaleidoX;
            p.kaleidoY = def.kaleidoY;
            p.kaleidoAngle = def.kaleidoAngle;
            p.kaleidoSource = def.kaleidoSource;
            p.kaleidoZoom = def.kaleidoZoom;
            p.kaleidoOpacity = def.kaleidoOpacity;
            p.blurType = def.blurType;                       // "Blur"
            p.blurAmount = def.blurAmount;
            p.blurAngle = def.blurAngle;
            p.blurX = def.blurX;
            p.blurY = def.blurY;
            p.blurSize = def.blurSize;
            p.blurFeather = def.blurFeather;
            break;
        case Group::Lens:
            p.lensProfile = def.lensProfile;
            p.lensDistortion = def.lensDistortion;
            p.lensVignetting = def.lensVignetting;
            p.lensProfileId = def.lensProfileId;
            p.lensManualDistortion = def.lensManualDistortion;
            p.lensManualVignetting = def.lensManualVignetting;
            p.lensManualMidpoint = def.lensManualMidpoint;
            p.removeCA = def.removeCA;
            p.defringePurple = def.defringePurple;
            p.defringeGreen = def.defringeGreen;
            p.lensBlurAmount = def.lensBlurAmount;
            p.lensBlurFocus = def.lensBlurFocus;
            p.lensBlurRange = def.lensBlurRange;
            p.lensBlurShape = def.lensBlurShape;
            p.lensBlurBoost = def.lensBlurBoost;
            p.lensBlurRefine = def.lensBlurRefine;
            break;
        }
    }

    /* Field-wise equality (all members are scalars), used by groupIsDefault below. */
    bool operator==(const EditParams &) const = default;

    /* True when every field in one group still holds its identity default, i.e. the
       panel has no edits. Derived from resetGroup on a copy so it can never drift out
       of step with the group membership above: resetGroup touches only that group's
       fields, so an unchanged copy means the group is pristine. The Develop panel uses
       this to flag edited sections in their header ("Basic *"). */
    static bool groupIsDefault(const EditParams &p, Group g) {
        EditParams q = p;
        resetGroup(q, g);
        return q == p;
    }

    /* True when nothing would change, letting callers skip the Develop stage and
       serve the cached WorkingImage directly. */
    bool isIdentity() const {
        return temp == 0.0f && tint == 0.0f &&
               exposure == 0.0f && contrast == 0.0f &&
               highlights == 0.0f && shadows == 0.0f &&
               whites == 0.0f && blacks == 0.0f &&
               texture == 0.0f && clarity == 0.0f && dehaze == 0.0f &&
               red == 0.0f && green == 0.0f && blue == 0.0f &&
               calRedHue == 0.0f && calRedSat == 0.0f &&
               calGreenHue == 0.0f && calGreenSat == 0.0f &&
               calBlueHue == 0.0f && calBlueSat == 0.0f && calShadowTint == 0.0f &&
               /* Against the DEFAULT, not against zero: the default view transform is
                  Filmic, not the identity None, so a literal 0 here would read every
                  untouched raw as edited (and every raw explicitly set to None as
                  untouched). Same rule as denoiseLuma/denoiseChroma below. */
               viewTransform == kDefaultViewTransform && cameraProfile.isEmpty() &&
               hue == 0.0f && saturation == 0.0f && vibrance == 0.0f && luminance == 0.0f &&
               gradeShadowSat == 0.0f && gradeShadowLum == 0.0f &&
               gradeMidSat == 0.0f && gradeMidLum == 0.0f &&
               gradeHighSat == 0.0f && gradeHighLum == 0.0f &&
               gradeGlobalSat == 0.0f && gradeGlobalLum == 0.0f &&
               denoiseLuma < 0.0f && denoiseChroma < 0.0f &&   // unset = remembered
               denoiseRaw < 0 &&        // unset = "follow the preference" = untouched
               rawEngine < 0 &&         // unset = the default renderer = untouched
               removeCA < 0 && defringePurple == 0.0f && defringeGreen == 0.0f &&
               lensProfile < 0 && lensDistortion == 1.0f && lensVignetting == 1.0f &&
               lensProfileId.isEmpty() &&
               lensManualDistortion == 0.0f && lensManualVignetting == 0.0f &&
               lensBlurAmount == 0.0f && lensBlurRefine == 0.0f &&
               localDenoiseLuma == 0.0f && localDenoiseChroma == 0.0f &&
               sharpenAmount == 0.0f &&
               vignetteExposure == 0.0f && grainAmount == 0.0f &&
               kaleidoSegments < 2 && blurAmount == 0.0f &&
               curvesAreIdentity() &&
               /* A sample with no shift moves no pixel, but it IS the user's work: count
                  it, or the sidecar is never written and the sample is gone on return. */
               pointColorN == 0;
    }
};

#endif // EDITPARAMS_H
