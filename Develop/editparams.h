#ifndef EDITPARAMS_H
#define EDITPARAMS_H

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
    /* White balance. 0,0 = as-shot / no change. */
    float temp = 0.0f;
    float tint = 0.0f;

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

    /* Presence. */
    float texture = 0.0f;
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

    /* Noise reduction -- GLOBAL, applied in the raw decode pipeline (RawFormat / the Apple
       engine) during demosaic, alongside start WB / black / white. Not maskable: these are
       baked into WorkingImage before Develop runs. 0 = engine default. */
    /* Default color (chroma) NR is full: chroma noise is objectionable and chroma
       detail is low-frequency, so baking 100 at decode is the right baseline.
       Default luma NR is 0.75: strong enough to clean sensor grain while leaving
       fine detail intact. A fresh raw carrying these defaults still counts as
       identity (see isIdentity); a saved recipe overrides them per image. */
    static constexpr float kDefaultDenoiseLuma   = 0.75f;
    static constexpr float kDefaultDenoiseChroma = 1.0f;
    float denoiseLuma   = kDefaultDenoiseLuma;
    float denoiseChroma = kDefaultDenoiseChroma;

    /* Local (maskable) NR -- Develop SPATIAL ops layered on TOP of the global baseline, operating
       on the already-decoded WorkingImage (see Develop::Denoise and notes/Documentation.txt
       "Layer & masking model"). localDenoiseLuma = luminance NR (ratio-preserving); localDenoise-
       Chroma = colour/chroma NR (opponent-chroma blur, luma kept exact). 0 = off. Range 0..1. */
    float localDenoiseLuma = 0.0f;
    float localDenoiseChroma = 0.0f;

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

    int version = 1;

    /* Adjustment groups, matching the Develop panel's section headers (Basic / Color / Effects).
       These drive the per-section Preview (show/ignore) and Reset (restore defaults) controls: a
       group is a fixed subset of the fields above. Note the grouping follows the UI, not the field
       comments -- texture/dehaze sit under Basic, and the tone splits belong to Basic for
       Reset (they are irrelevant to Preview since the tone sliders they modulate are zeroed anyway).
       localDenoiseLuma ("Denoise", local post-demosaic NR) is under Effects. denoiseLuma/denoiseChroma
       are decode-time global NR (the Base layer's "Denoise raw", baked before Develop runs) so they
       are in NO group and cannot be previewed/reset via params. */
    enum class Group { Basic, Color, Effects };

    /* Force one group's fields back to their identity defaults, in place. The defaults come from a
       fresh EditParams{} so the non-zero tone-split defaults (0.25/0.50/0.75) restore correctly.
       Reset calls this on the STORED params (destructive); the Preview fold calls it on a COPY. */
    static void resetGroup(EditParams &p, Group g) {
        const EditParams def;
        switch (g) {
        case Group::Basic:
            p.temp = def.temp; p.tint = def.tint;
            p.exposure = def.exposure; p.contrast = def.contrast;
            p.highlights = def.highlights; p.shadows = def.shadows;
            p.whites = def.whites; p.blacks = def.blacks;
            p.texture = def.texture; p.dehaze = def.dehaze;
            p.toneShadowCenter = def.toneShadowCenter;
            p.toneCrossover = def.toneCrossover;
            p.toneHighlightCenter = def.toneHighlightCenter;
            break;
        case Group::Color:
            p.red = def.red; p.green = def.green; p.blue = def.blue;
            p.hue = def.hue; p.saturation = def.saturation;
            p.vibrance = def.vibrance; p.luminance = def.luminance;
            break;
        case Group::Effects:
            p.localDenoiseLuma = def.localDenoiseLuma;       // "Denoise" (local NR)
            p.localDenoiseChroma = def.localDenoiseChroma;   // "Denoise Color" (chroma)
            p.vignetteExposure = def.vignetteExposure;       // "Vignette" (radial)
            p.vignetteFeather = def.vignetteFeather;
            p.grainAmount = def.grainAmount;                 // "Grain" (film grain)
            p.grainSize = def.grainSize;
            p.grainRoughness = def.grainRoughness;
            break;
        }
    }

    /* True when nothing would change, letting callers skip the Develop stage and
       serve the cached WorkingImage directly. */
    bool isIdentity() const {
        return temp == 0.0f && tint == 0.0f &&
               exposure == 0.0f && contrast == 0.0f &&
               highlights == 0.0f && shadows == 0.0f &&
               whites == 0.0f && blacks == 0.0f &&
               texture == 0.0f && dehaze == 0.0f &&
               red == 0.0f && green == 0.0f && blue == 0.0f &&
               hue == 0.0f && saturation == 0.0f && vibrance == 0.0f && luminance == 0.0f &&
               denoiseLuma == kDefaultDenoiseLuma && denoiseChroma == kDefaultDenoiseChroma &&
               localDenoiseLuma == 0.0f && localDenoiseChroma == 0.0f &&
               vignetteExposure == 0.0f && grainAmount == 0.0f;
    }
};

#endif // EDITPARAMS_H
