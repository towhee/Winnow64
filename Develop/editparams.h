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

    /* Presence. */
    float texture = 0.0f;
    float dehaze  = 0.0f;

    /* Noise reduction. */
    float denoiseLuma   = 0.0f;
    float denoiseChroma = 0.0f;

    int version = 1;

    /* True when nothing would change, letting callers skip the Develop stage and
       serve the cached WorkingImage directly. */
    bool isIdentity() const {
        return temp == 0.0f && tint == 0.0f &&
               exposure == 0.0f && contrast == 0.0f &&
               highlights == 0.0f && shadows == 0.0f &&
               whites == 0.0f && blacks == 0.0f &&
               texture == 0.0f && dehaze == 0.0f &&
               denoiseLuma == 0.0f && denoiseChroma == 0.0f;
    }
};

#endif // EDITPARAMS_H
