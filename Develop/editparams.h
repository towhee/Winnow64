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

    /* Noise reduction -- GLOBAL, applied in the raw decode pipeline (RawFormat / the Apple
       engine) during demosaic, alongside start WB / black / white. Not maskable: these are
       baked into WorkingImage before Develop runs. 0 = engine default. */
    float denoiseLuma   = 0.0f;
    float denoiseChroma = 0.0f;

    /* Local (maskable) luminance NR -- a Develop SPATIAL op layered on TOP of the global
       baseline, operating on the already-decoded WorkingImage (see Develop::Denoise and
       notes/Documentation.txt "Layer & masking model"). 0 = off. Range 0..1. */
    float localDenoiseLuma = 0.0f;

    int version = 1;

    /* True when nothing would change, letting callers skip the Develop stage and
       serve the cached WorkingImage directly. */
    bool isIdentity() const {
        return temp == 0.0f && tint == 0.0f &&
               exposure == 0.0f && contrast == 0.0f &&
               highlights == 0.0f && shadows == 0.0f &&
               whites == 0.0f && blacks == 0.0f &&
               texture == 0.0f && dehaze == 0.0f &&
               denoiseLuma == 0.0f && denoiseChroma == 0.0f &&
               localDenoiseLuma == 0.0f;
    }
};

#endif // EDITPARAMS_H
