// fsphotometric.cpp
#include "fsphotometric.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>

namespace FSPhotometric {

static inline bool isFinite(float v) { return std::isfinite(v); }

static inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static void ensureBGR3(cv::Mat& img)
{
    if (img.empty()) return;

    if (img.channels() == 1)
        cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
    else if (img.channels() == 4)
        cv::cvtColor(img, img, cv::COLOR_BGRA2BGR);
    // else assume 3ch
}

static bool valid5x1F32(const cv::Mat& m)
{
    return !m.empty() && m.type() == CV_32FC1 && m.rows == 5 && m.cols == 1;
}

static bool valid6x1F32(const cv::Mat& m)
{
    return !m.empty() && m.type() == CV_32FC1 && m.rows == 6 && m.cols == 1;
}

struct ContrastCoeffs
{
    float C0=1, Cx=0, Cx2=0, Cy=0, Cy2=0;
    bool  enabled=false;
};

struct WBCoeffs
{
    // FSAlign ordering: [B_off, B_gain, G_off, G_gain, R_off, R_gain]
    float Boff=0, Bgain=1, Goff=0, Ggain=1, Roff=0, Rgain=1;
    bool  enabled=false;
};

static ContrastCoeffs readContrastSafe(const cv::Mat& c5x1,
                                       float gainLo,
                                       float gainHi)
{
    ContrastCoeffs c;
    if (!valid5x1F32(c5x1)) return c;

    c.C0  = c5x1.at<float>(0,0);
    c.Cx  = c5x1.at<float>(1,0);
    c.Cx2 = c5x1.at<float>(2,0);
    c.Cy  = c5x1.at<float>(3,0);
    c.Cy2 = c5x1.at<float>(4,0);

    // If anything is NaN/Inf, disable contrast.
    if (!isFinite(c.C0) || !isFinite(c.Cx) || !isFinite(c.Cx2) || !isFinite(c.Cy) || !isFinite(c.Cy2))
        return ContrastCoeffs{};

    // Mild sanity clamps (these are *not* correctness, just guardrails)
    c.C0  = clampf(c.C0,  gainLo, gainHi);
    c.Cx  = clampf(c.Cx,  -2.0f, 2.0f);
    c.Cx2 = clampf(c.Cx2, -2.0f, 2.0f);
    c.Cy  = clampf(c.Cy,  -2.0f, 2.0f);
    c.Cy2 = clampf(c.Cy2, -2.0f, 2.0f);

    c.enabled = true;
    return c;
}

static WBCoeffs readWBSafe(const cv::Mat& wb6x1)
{
    WBCoeffs w;
    if (!valid6x1F32(wb6x1)) return w;

    w.Boff  = wb6x1.at<float>(0,0);
    w.Bgain = wb6x1.at<float>(1,0);
    w.Goff  = wb6x1.at<float>(2,0);
    w.Ggain = wb6x1.at<float>(3,0);
    w.Roff  = wb6x1.at<float>(4,0);
    w.Rgain = wb6x1.at<float>(5,0);

    if (!isFinite(w.Boff)  || !isFinite(w.Bgain) ||
        !isFinite(w.Goff)  || !isFinite(w.Ggain) ||
        !isFinite(w.Roff)  || !isFinite(w.Rgain))
        return WBCoeffs{};

    // Guardrails: if solver ever produces “near zero gain”, disable WB rather than black-out.
    // (You can tune this threshold.)
    const float minGain = 0.05f;
    if (std::abs(w.Bgain) < minGain || std::abs(w.Ggain) < minGain || std::abs(w.Rgain) < minGain)
        return WBCoeffs{};

    // Optional clamps to keep weird sources from blowing out:
    w.Bgain = clampf(w.Bgain, 0.25f, 4.0f);
    w.Ggain = clampf(w.Ggain, 0.25f, 4.0f);
    w.Rgain = clampf(w.Rgain, 0.25f, 4.0f);

    // Offsets: guardrails in "8-bit domain" units
    w.Boff  = clampf(w.Boff, -128.0f, 128.0f);
    w.Goff  = clampf(w.Goff, -128.0f, 128.0f);
    w.Roff  = clampf(w.Roff, -128.0f, 128.0f);

    w.enabled = true;
    return w;
}

// build x in [-1..1] and x^2 for speed
static void buildXRow(int w, cv::Mat& x32, cv::Mat& x2_32)
{
    x32.create(1, w, CV_32F);
    x2_32.create(1, w, CV_32F);

    float* x  = x32.ptr<float>(0);
    float* x2 = x2_32.ptr<float>(0);

    if (w <= 1) { x[0]=0.f; x2[0]=0.f; return; }

    for (int ix = 0; ix < w; ++ix)
    {
        float xn = (2.0f * float(ix) / float(w - 1)) - 1.0f;
        x[ix]  = xn;
        x2[ix] = xn * xn;
    }
}

static inline float computeGain(const ContrastCoeffs& c,
                                float x, float x2,
                                float yn, float y2n,
                                float gainLo, float gainHi)
{
    if (!c.enabled) return 1.0f;
    float g = c.C0 + c.Cx*x + c.Cx2*x2 + c.Cy*yn + c.Cy2*y2n;
    return clampf(g, gainLo, gainHi);
}

void applyPhotometricNormalizationOrig(cv::Mat& grayOrig,
                                       cv::Mat& colorOrig,
                                       const cv::Mat& contrast5x1,
                                       const cv::Mat& wb6x1,
                                       bool wbOffsetsAre8bitUnits,
                                       float gainLo,
                                       float gainHi)
{
    if (grayOrig.empty() || colorOrig.empty()) return;

    // Gray can be 8U or 16U, but we treat it as scalar luminance to apply contrast gain only.
    if (!(grayOrig.depth() == CV_8U || grayOrig.depth() == CV_16U) || grayOrig.channels() != 1)
        return;

    ensureBGR3(colorOrig);
    if (colorOrig.channels() != 3) return;
    if (colorOrig.size() != grayOrig.size()) return;

    const ContrastCoeffs C = readContrastSafe(contrast5x1, gainLo, gainHi);
    const WBCoeffs       W = readWBSafe(wb6x1);

    if (!C.enabled && !W.enabled) return;

    const int w = grayOrig.cols;
    const int h = grayOrig.rows;

    cv::Mat x32, x2_32;
    buildXRow(w, x32, x2_32);
    const float* x  = x32.ptr<float>(0);
    const float* x2 = x2_32.ptr<float>(0);

    const int cDepth = colorOrig.depth();
    if (!(cDepth == CV_8U || cDepth == CV_16U || cDepth == CV_32F))
        return;

    // WB offsets are produced by FSAlign using 8-bit samples (Vec3b) during fitting.
    // If applying to 16U or 32F representing 0..65535 or 0..1, convert offsets appropriately.
    const float offScale =
        (!W.enabled) ? 1.0f :
            (wbOffsetsAre8bitUnits
                 ? (cDepth == CV_8U  ? 1.0f :
                        cDepth == CV_16U ? 256.0f :
                        /*CV_32F*/         (1.0f / 255.0f))
                 : 1.0f);

    for (int y = 0; y < h; ++y)
    {
        const float yn  = (h > 1) ? ((2.0f * float(y) / float(h - 1)) - 1.0f) : 0.0f;
        const float y2n = yn * yn;

        // ---- Gray ----
        if (grayOrig.depth() == CV_8U)
        {
            uint8_t* g = grayOrig.ptr<uint8_t>(y);
            for (int ix = 0; ix < w; ++ix)
            {
                const float gain = computeGain(C, x[ix], x2[ix], yn, y2n, gainLo, gainHi);
                float gv = float(g[ix]) * gain;
                gv = clampf(gv, 0.0f, 255.0f);
                g[ix] = (uint8_t)(gv + 0.5f);
            }
        }
        else // CV_16U
        {
            uint16_t* g = grayOrig.ptr<uint16_t>(y);
            for (int ix = 0; ix < w; ++ix)
            {
                const float gain = computeGain(C, x[ix], x2[ix], yn, y2n, gainLo, gainHi);
                float gv = float(g[ix]) * gain;
                gv = clampf(gv, 0.0f, 65535.0f);
                g[ix] = (uint16_t)(gv + 0.5f);
            }
        }

        // ---- Color ----
        if (cDepth == CV_8U)
        {
            cv::Vec3b* c = colorOrig.ptr<cv::Vec3b>(y);
            for (int ix = 0; ix < w; ++ix)
            {
                const float gain = computeGain(C, x[ix], x2[ix], yn, y2n, gainLo, gainHi);

                float B = float(c[ix][0]) * gain;
                float G = float(c[ix][1]) * gain;
                float R = float(c[ix][2]) * gain;

                if (W.enabled)
                {
                    B = W.Bgain * B + W.Boff * offScale;
                    G = W.Ggain * G + W.Goff * offScale;
                    R = W.Rgain * R + W.Roff * offScale;
                }

                B = clampf(B, 0.0f, 255.0f);
                G = clampf(G, 0.0f, 255.0f);
                R = clampf(R, 0.0f, 255.0f);

                c[ix] = cv::Vec3b((uint8_t)(B + 0.5f),
                                  (uint8_t)(G + 0.5f),
                                  (uint8_t)(R + 0.5f));
            }
        }
        else if (cDepth == CV_16U)
        {
            cv::Vec<uint16_t,3>* c = colorOrig.ptr<cv::Vec<uint16_t,3>>(y);
            for (int ix = 0; ix < w; ++ix)
            {
                const float gain = computeGain(C, x[ix], x2[ix], yn, y2n, gainLo, gainHi);

                float B = float(c[ix][0]) * gain;
                float G = float(c[ix][1]) * gain;
                float R = float(c[ix][2]) * gain;

                if (W.enabled)
                {
                    B = W.Bgain * B + W.Boff * offScale;
                    G = W.Ggain * G + W.Goff * offScale;
                    R = W.Rgain * R + W.Roff * offScale;
                }

                B = clampf(B, 0.0f, 65535.0f);
                G = clampf(G, 0.0f, 65535.0f);
                R = clampf(R, 0.0f, 65535.0f);

                c[ix] = cv::Vec<uint16_t,3>((uint16_t)(B + 0.5f),
                                             (uint16_t)(G + 0.5f),
                                             (uint16_t)(R + 0.5f));
            }
        }
        else // CV_32F (assume normalized 0..1 unless your pipeline uses 0..255 float)
        {
            cv::Vec3f* c = colorOrig.ptr<cv::Vec3f>(y);
            for (int ix = 0; ix < w; ++ix)
            {
                const float gain = computeGain(C, x[ix], x2[ix], yn, y2n, gainLo, gainHi);

                float B = c[ix][0] * gain;
                float G = c[ix][1] * gain;
                float R = c[ix][2] * gain;

                if (W.enabled)
                {
                    B = W.Bgain * B + W.Boff * offScale;
                    G = W.Ggain * G + W.Goff * offScale;
                    R = W.Rgain * R + W.Roff * offScale;
                }

                // clamp to sane float display range
                B = clampf(B, 0.0f, 1.0f);
                G = clampf(G, 0.0f, 1.0f);
                R = clampf(R, 0.0f, 1.0f);

                c[ix] = cv::Vec3f(B, G, R);
            }
        }
    }
}

} // namespace FSPhotometric
