#include "FusionPyr.h"
#include "fsutilities.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace FusionPyr
{

namespace
{
// Build a 0/255 edge mask from a CV_16U label image.
// Edge if label differs from any 4-neighbor.
static cv::Mat labelEdgeMask4N_8u(const cv::Mat& idx16)
{
    CV_Assert(idx16.type() == CV_16U);

    const int rows = idx16.rows;
    const int cols = idx16.cols;

    cv::Mat edge8(idx16.size(), CV_8U, cv::Scalar(0));

    for (int y = 0; y < rows; ++y)
    {
        const uint16_t* row   = idx16.ptr<uint16_t>(y);
        const uint16_t* rowUp = (y > 0) ? idx16.ptr<uint16_t>(y - 1) : nullptr;
        const uint16_t* rowDn = (y + 1 < rows) ? idx16.ptr<uint16_t>(y + 1) : nullptr;

        uint8_t* out = edge8.ptr<uint8_t>(y);

        for (int x = 0; x < cols; ++x)
        {
            const uint16_t v = row[x];
            bool diff = false;

            if (x > 0)        diff |= (v != row[x - 1]);
            if (x + 1 < cols) diff |= (v != row[x + 1]);
            if (rowUp)        diff |= (v != rowUp[x]);
            if (rowDn)        diff |= (v != rowDn[x]);

            if (diff) out[x] = 255;
        }
    }

    return edge8;
}

// Downsample a binary-ish mask (0/255) to next pyramid level using:
// 1) convert to float 0..1
// 2) Gaussian blur (optional)
// 3) pyrDown
// 4) threshold to 0/255
static cv::Mat downsampleVetoMask(const cv::Mat& src8,
                                  float blurSigma,
                                  float thresh01)
{
    CV_Assert(src8.type() == CV_8U);
    CV_Assert(thresh01 >= 0.0f && thresh01 <= 1.0f);

    cv::Mat f32;
    src8.convertTo(f32, CV_32F, 1.0 / 255.0);

    if (blurSigma > 0.0f)
        cv::GaussianBlur(f32, f32, cv::Size(0, 0), blurSigma, blurSigma, cv::BORDER_REFLECT);

    cv::Mat down32;
    cv::pyrDown(f32, down32); // size halves (rounded)

    cv::Mat out8 = (down32 > thresh01);
    out8.convertTo(out8, CV_8U, 255.0);
    return out8;
}

static inline int resolveLevelParamLocal(int raw, int levels, int defaultIfNegOne)
{
    if (levels <= 0) return 0;
    if (raw == -1) raw = defaultIfNegOne;
    if (raw < 0) raw = 0;
    if (raw > levels - 1) raw = levels - 1;
    return raw;
}

} // namespace

// ----------------------------
// Gaussian / Laplacian pyramids
// ----------------------------
void buildGaussianPyr(const cv::Mat& img32, int levels, std::vector<cv::Mat>& gp)
{
    CV_Assert(levels >= 1);
    gp.clear();
    gp.reserve(levels);

    gp.push_back(img32);
    for (int i = 1; i < levels; ++i)
    {
        cv::Mat down;
        cv::pyrDown(gp.back(), down);
        gp.push_back(down);
    }
}

void buildLaplacianPyr(const cv::Mat& img32, int levels, std::vector<cv::Mat>& lp)
{
    std::vector<cv::Mat> gp;
    buildGaussianPyr(img32, levels, gp);

    lp.resize(levels);
    for (int i = 0; i < levels - 1; ++i)
    {
        cv::Mat up;
        cv::pyrUp(gp[i + 1], up, gp[i].size());
        lp[i] = gp[i] - up;
    }
    lp[levels - 1] = gp[levels - 1];
}

cv::Mat collapseLaplacianPyr(const std::vector<cv::Mat>& lp)
{
    CV_Assert(!lp.empty());
    cv::Mat cur = lp.back().clone();

    for (int i = (int)lp.size() - 2; i >= 0; --i)
    {
        cv::Mat up;
        cv::pyrUp(cur, up, lp[i].size());
        cur = up + lp[i];
    }
    return cur;
}

// ----------------------------
// Label pyramid helpers
// ----------------------------
cv::Mat downsampleIdx16_Mode2x2(const cv::Mat& src16)
{
    CV_Assert(src16.type() == CV_16U);

    const int outW = (src16.cols + 1) / 2;
    const int outH = (src16.rows + 1) / 2;
    cv::Mat dst16(outH, outW, CV_16U);

    for (int y = 0; y < outH; ++y)
    {
        const int y0 = 2 * y;
        const int y1 = std::min(y0 + 1, src16.rows - 1);

        const uint16_t* r0 = src16.ptr<uint16_t>(y0);
        const uint16_t* r1 = src16.ptr<uint16_t>(y1);
        uint16_t* out = dst16.ptr<uint16_t>(y);

        for (int x = 0; x < outW; ++x)
        {
            const int x0 = 2 * x;
            const int x1 = std::min(x0 + 1, src16.cols - 1);

            const uint16_t a = r0[x0];
            const uint16_t b = r0[x1];
            const uint16_t c = r1[x0];
            const uint16_t d = r1[x1];

            uint16_t v = a;
            if (b == a || c == a || d == a) v = a;
            else if (c == b || d == b)      v = b;
            else if (d == c)                v = c;

            out[x] = v;
        }
    }
    return dst16;
}

void buildIndexPyrNearest(const cv::Mat& idx16Level0, int levels, std::vector<cv::Mat>& idxPyr)
{
    CV_Assert(idx16Level0.type() == CV_16U);
    CV_Assert(levels >= 1);

    idxPyr.clear();
    idxPyr.reserve(levels);

    idxPyr.push_back(idx16Level0);
    for (int i = 1; i < levels; ++i)
        idxPyr.push_back(downsampleIdx16_Mode2x2(idxPyr.back()));
}

// ----------------------------
// Depth-edge veto pyramid — ORIGINAL behavior
// ----------------------------
void buildDepthEdgeVetoPyr(const std::vector<cv::Mat>& idxPyr16,
                           std::vector<cv::Mat>& vetoPyr8,
                           const DepthEdgeVetoParams& p)
{
    const int levels = (int)idxPyr16.size();
    CV_Assert(levels >= 1);

    // ORIGINAL: vetoFromLevel < 0 => residual only (levels-1)
    const int start =
        (p.vetoFromLevel < 0)
            ? (levels - 1)
            : std::max(0, std::min(levels - 1, p.vetoFromLevel));

    vetoPyr8.assign(levels, cv::Mat());

    for (int i = 0; i < levels; ++i)
    {
        CV_Assert(!idxPyr16[i].empty());
        CV_Assert(idxPyr16[i].type() == CV_16U);

        if (i < start)
        {
            vetoPyr8[i] = cv::Mat::zeros(idxPyr16[i].size(), CV_8U);
            continue;
        }

        cv::Mat idxSm16;
        if (idxPyr16[i].cols >= 5 && idxPyr16[i].rows >= 5)
            cv::medianBlur(idxPyr16[i], idxSm16, 5);
        else
            idxSm16 = idxPyr16[i];

        cv::Mat depthF;
        idxSm16.convertTo(depthF, CV_32F);

        cv::Mat dx, dy;
        cv::Sobel(depthF, dx, CV_32F, 1, 0, 3);
        cv::Sobel(depthF, dy, CV_32F, 0, 1, 3);

        cv::Mat grad = cv::abs(dx) + cv::abs(dy);

        // Normalize Sobel gain to “index units”
        grad = grad * 0.25f;

        cv::Mat veto = (grad > p.depthGradThresh); // CV_8U 0/255

        if (p.vetoDilatePx > 0) {
            cv::dilate(veto, veto, FSUtilities::seEllipse(p.vetoDilatePx));
        }

        vetoPyr8[i] = veto;
    }
}

// ----------------------------
// PyrAccum
// ----------------------------
void PyrAccum::reset(const cv::Size& sz, int levels_)
{
    CV_Assert(levels_ >= 1);
    levels = levels_;

    num3.resize(levels);
    den1.resize(levels);

    cv::Size s = sz;
    for (int i = 0; i < levels; ++i)
    {
        num3[i] = cv::Mat::zeros(s, CV_32FC3);
        den1[i] = cv::Mat::zeros(s, CV_32F);

        if (i < levels - 1)
            s = cv::Size((s.width + 1) / 2, (s.height + 1) / 2);
    }
}

// ----------------------------
// Accumulate / finalize (simple)
// ----------------------------
void accumulateSlicePyr(PyrAccum& A,
                        const cv::Mat& color32FC3,
                        const cv::Mat& weight32F,
                        int levels,
                        float weightBlurSigma)
{
    CV_Assert(levels >= 1);
    CV_Assert(A.levels == levels);
    CV_Assert(color32FC3.type() == CV_32FC3);
    CV_Assert(weight32F.type() == CV_32F);
    CV_Assert(color32FC3.size() == weight32F.size());

    cv::Mat w = weight32F;
    if (weightBlurSigma > 0.0f)
        cv::GaussianBlur(w, w, cv::Size(0, 0),
                         weightBlurSigma, weightBlurSigma, cv::BORDER_REFLECT);

    std::vector<cv::Mat> lpColor;
    buildLaplacianPyr(color32FC3, levels, lpColor);

    std::vector<cv::Mat> gpW;
    buildGaussianPyr(w, levels, gpW);

    for (int i = 0; i < levels; ++i)
    {
        cv::Mat w3;
        cv::Mat ch[] = { gpW[i], gpW[i], gpW[i] };
        cv::merge(ch, 3, w3);

        A.num3[i] += lpColor[i].mul(w3);
        A.den1[i] += gpW[i];
    }
}

// ----------------------------
// Accumulate (DMap)
// ----------------------------
void accumulateSlicePyr(PyrAccum& A,
                        const cv::Mat& color32FC3,
                        const cv::Mat& weight32F,
                        const std::vector<cv::Mat>& idxPyr16,
                        const std::vector<cv::Mat>* vetoPyr8_orNull,
                        int sliceIndex,
                        const AccumDMapParams& p,
                        int levels,
                        float weightBlurSigma)
{
    CV_Assert(levels >= 1);
    CV_Assert(A.levels == levels);
    CV_Assert(color32FC3.type() == CV_32FC3);
    CV_Assert(weight32F.type() == CV_32F);
    CV_Assert(color32FC3.size() == weight32F.size());
    CV_Assert((int)idxPyr16.size() == levels);

    if (vetoPyr8_orNull)
        CV_Assert((int)vetoPyr8_orNull->size() == levels);

    const int hardFromResolved = resolveLevelParamLocal(
        p.hardFromLevel, levels, std::max(0, levels - 2));

    const int vetoFromResolved = resolveLevelParamLocal(
        p.vetoFromLevel, levels, std::max(0, levels - 2));

    cv::Mat w = weight32F;
    if (weightBlurSigma > 0.0f)
        cv::GaussianBlur(w, w, cv::Size(0, 0),
                         weightBlurSigma, weightBlurSigma, cv::BORDER_REFLECT);

    std::vector<cv::Mat> lpColor;
    buildLaplacianPyr(color32FC3, levels, lpColor);

    std::vector<cv::Mat> gpW;
    buildGaussianPyr(w, levels, gpW);

    for (int l = 0; l < levels; ++l)
    {
        CV_Assert(gpW[l].type() == CV_32F);
        CV_Assert(lpColor[l].type() == CV_32FC3);
        CV_Assert(idxPyr16[l].type() == CV_16U);

        if (p.enableHardWeightsOnLowpass && l >= hardFromResolved)
        {
            cv::Mat winMask = (idxPyr16[l] == (uint16_t)sliceIndex); // 0/255
            cv::Mat winMask32;
            winMask.convertTo(winMask32, CV_32F, 1.0 / 255.0);
            gpW[l] = gpW[l].mul(winMask32);
        }

        if (p.enableDepthGradLowpassVeto &&
            vetoPyr8_orNull &&
            l >= vetoFromResolved)
        {
            const cv::Mat& veto8 = (*vetoPyr8_orNull)[l];
            CV_Assert(veto8.type() == CV_8U);
            CV_Assert(veto8.size() == gpW[l].size());

            if (p.vetoStrength > 0.0f)
            {
                cv::Mat veto01;
                veto8.convertTo(veto01, CV_32F, 1.0 / 255.0);
                gpW[l] = gpW[l].mul(1.0f - (float)p.vetoStrength * veto01);
            }
        }

        if (p.wMin > 0.0f)
            cv::max(gpW[l], (float)p.wMin, gpW[l]);

        cv::Mat w3;
        cv::Mat ch[] = { gpW[l], gpW[l], gpW[l] };
        cv::merge(ch, 3, w3);

        A.num3[l] += lpColor[l].mul(w3);
        A.den1[l] += gpW[l];
    }
}

cv::Mat finalizeBlend(const PyrAccum& A, float eps)
{
    CV_Assert(A.levels >= 1);
    CV_Assert(eps >= 0.0f);

    std::vector<cv::Mat> lpOut(A.levels);

    for (int i = 0; i < A.levels; ++i)
    {
        cv::Mat den = A.den1[i] + eps;

        cv::Mat den3;
        cv::Mat ch[] = { den, den, den };
        cv::merge(ch, 3, den3);

        lpOut[i] = A.num3[i] / den3;
    }

    return collapseLaplacianPyr(lpOut);
}

} // namespace FusionPyr
