#include "focuswavelet.h"
#include <algorithm>
using std::vector;

namespace {

// ----------------------------------------------------------
// Utility helpers
// ----------------------------------------------------------

inline void splitBGR32F(const cv::Mat& img32f, vector<cv::Mat>& ch) {
    ch.resize(3);
    cv::split(img32f, ch);
}
inline cv::Mat joinBGR32F(const vector<cv::Mat>& ch) {
    cv::Mat out; cv::merge(ch, out); return out;
}

static void buildFullResBase(
    const std::vector<cv::Mat>& stack32f,
    const cv::Mat& mu32f, const cv::Mat& sig2, const cv::Mat& amp32f,
    const std::vector<cv::Mat>& validFull,
    cv::Mat& baseFull /*CV_32F, 1 or 3 ch*/)
{
    const int n = (int)stack32f.size();
    CV_Assert(n > 0);
    cv::Size fs = stack32f[0].size();

    // Gates per slice at full-res
    std::vector<cv::Mat> wFull(n);
    for (int i = 0; i < n; ++i) {
        cv::Mat diff; cv::absdiff(mu32f, cv::Scalar(float(i)), diff);
        cv::Mat gate; cv::exp(-(diff.mul(diff)) / (2.0f * sig2), gate);
        gate = gate * 0.99f + 0.01f;
        cv::max(gate, 1e-3f, gate);

        cv::Mat w = gate;
        if (!amp32f.empty())  w = w.mul(amp32f);
        if (!validFull.empty()) w = w.mul(validFull[i]);

        // Soften weights a touch to avoid seams
        cv::GaussianBlur(w, w, cv::Size(5,5), 1.2);
        wFull[i] = w;
    }

    // Normalize weights to sum=1
    cv::Mat sumW = wFull[0].clone();
    for (int i = 1; i < n; ++i) sumW += wFull[i];
    cv::max(sumW, 1e-6f, sumW);

    // Build normalized weights
    for (int i = 0; i < n; ++i) wFull[i] = wFull[i] / sumW;

    // Weighted average in image domain (full-res)
    baseFull = cv::Mat::zeros(fs, stack32f[0].type());
    for (int i = 0; i < n; ++i) {
        if (baseFull.channels() == 3) {
            std::vector<cv::Mat> w3(3, wFull[i]);
            cv::Mat W3; cv::merge(w3, W3);
            baseFull += stack32f[i].mul(W3);
        } else {
            baseFull += stack32f[i].mul(wFull[i]);
        }
    }
}

static cv::Rect padToMultiple(const cv::Mat& in, int mult, cv::Mat& out)
{
    const int H = in.rows, W = in.cols;
    const int Ht = ((H + mult - 1) / mult) * mult;
    const int Wt = ((W + mult - 1) / mult) * mult;

    const int top = 0, left = 0;
    const int bottom = Ht - H, right = Wt - W;

    cv::copyMakeBorder(in, out, top, bottom, left, right, cv::BORDER_REFLECT_101);
    return cv::Rect(0, 0, W, H); // original ROI for cropping later
}

static cv::Mat padEven(const cv::Mat& in)
{
    int addX = in.cols % 2;
    int addY = in.rows % 2;
    cv::Mat out;
    cv::copyMakeBorder(in, out, 0, addY, 0, addX, cv::BORDER_REFLECT_101);
    return out;
}

static cv::Mat cropToROI(const cv::Mat& in, const cv::Rect& roi)
{
    return in(roi).clone();
}

// ----------------------------------------------------------
// Forward: separable Haar downsample
// ----------------------------------------------------------
static void haarDown(const cv::Mat& in,
                     cv::Mat& low,
                     cv::Mat& highH,
                     cv::Mat& highV,
                     cv::Mat& highHV)
{
    CV_Assert(in.type() == CV_32FC1 || in.type() == CV_32FC3);

    int rows = in.rows;
    int cols = in.cols;
    int ch   = in.channels();

    auto down1D = [&](const cv::Mat& src, cv::Mat& L, cv::Mat& H, bool horizontal)
    {
        int srows = src.rows, scols = src.cols, sch = src.channels();
        int dcols2 = horizontal ? (scols + 1) / 2 : scols;
        int drows2 = horizontal ? srows : (srows + 1) / 2;

        L.create(drows2, dcols2, src.type());
        H.create(drows2, dcols2, src.type());
        L.setTo(0);
        H.setTo(0);

        if (horizontal)
        {
            for (int y = 0; y < srows; ++y) {
                const float* s = src.ptr<float>(y);
                float* l = L.ptr<float>(y);
                float* h = H.ptr<float>(y);
                for (int x = 0; x < scols; x += 2) {
                    for (int c = 0; c < sch; ++c) {
                        float a = s[(x + 0) * sch + c];
                        float b = (x + 1 < scols) ? s[(x + 1) * sch + c] : a;
                        // Scale so that inverse (a=L+H, b=L−H) perfectly conserves mean
                        l[(x / 2) * sch + c] = 0.5f * (a + b);
                        h[(x / 2) * sch + c] = 0.5f * (a - b);
                    }
                }
            }
        }
        else
        {
            for (int y = 0; y < srows; y += 2) {
                const float* s0 = src.ptr<float>(y + 0);
                const float* s1 = (y + 1 < srows) ? src.ptr<float>(y + 1) : s0;
                float* l = L.ptr<float>(y / 2);
                float* h = H.ptr<float>(y / 2);
                for (int x = 0; x < scols; ++x) {
                    for (int c = 0; c < sch; ++c) {
                        float a = s0[x * sch + c];
                        float b = s1[x * sch + c];
                        l[x * sch + c] = 0.5f * (a + b);
                        h[x * sch + c] = 0.5f * (a - b);
                    }
                }
            }
        }
    };

    // Horizontal then vertical
    cv::Mat inPad;
    // Always pad symmetrically so top/left edges stay centered
    int padY = (in.rows % 2);
    int padX = (in.cols % 2);
    cv::copyMakeBorder(in, inPad,
                       padY / 2, padY - padY / 2,
                       padX / 2, padX - padX / 2,
                       cv::BORDER_REFLECT_101);    cv::Mat Lh, Hh;
    down1D(inPad, Lh, Hh, true);                  // use padded input

    cv::Mat Lv, Hv;
    down1D(Lh, low,  Lv, false);
    cv::Mat Vv, Vh;
    down1D(Hh, Vv,   Vh, false);

    highH  = Vv;  // HL
    highV  = Lv;  // LH
    highHV = Vh;  // HH

    cv::patchNaNs(low, 0.0);
    cv::patchNaNs(highH, 0.0);
    cv::patchNaNs(highV, 0.0);
    cv::patchNaNs(highHV, 0.0);
}

// ----------------------------------------------------------
// Inverse: exact manual Haar reconstruction (tile-safe)
// ----------------------------------------------------------
inline cv::Mat haarUpAdd(const cv::Mat& low,
                         const cv::Mat& hl,
                         const cv::Mat& lh,
                         const cv::Mat& hh)
{
    CV_Assert(low.size() == hl.size());
    CV_Assert(hl.size() == lh.size());
    CV_Assert(lh.size() == hh.size());
    CV_Assert(low.type() == hl.type());
    CV_Assert(low.type() == CV_32FC1 || low.type() == CV_32FC3);

    const int rows = low.rows, cols = low.cols, ch = low.channels();

    cv::Mat l0(rows, cols, low.type()),
        l1(rows, cols, low.type()),
        h0(rows, cols, low.type()),
        h1(rows, cols, low.type());

    // undo vertical
    for (int y = 0; y < rows; ++y) {
        const float* L  = low.ptr<float>(y);
        const float* LH = lh .ptr<float>(y);
        const float* HL = hl .ptr<float>(y);
        const float* HH = hh .ptr<float>(y);

        float* L0 = l0.ptr<float>(y);
        float* L1 = l1.ptr<float>(y);
        float* H0 = h0.ptr<float>(y);
        float* H1 = h1.ptr<float>(y);

        for (int x = 0; x < cols * ch; ++x) {
            const float ll = L[x], lh_ = LH[x], hl_ = HL[x], hh_ = HH[x];
            L0[x] = ll + lh_;
            L1[x] = ll - lh_;
            H0[x] = hl_ + hh_;
            H1[x] = hl_ - hh_;
        }
    }

    // undo horizontal
    cv::Mat out(rows * 2, cols * 2, low.type());
    for (int y = 0; y < rows; ++y) {
        const float* L0 = l0.ptr<float>(y);
        const float* L1 = l1.ptr<float>(y);
        const float* H0 = h0.ptr<float>(y);
        const float* H1 = h1.ptr<float>(y);

        float* o0 = out.ptr<float>(2 * y + 0);
        float* o1 = out.ptr<float>(2 * y + 1);

        for (int x = 0; x < cols; ++x) {
            for (int c = 0; c < ch; ++c) {
                const int si = x * ch + c;
                const int di0 = (2 * x + 0) * ch + c;
                const int di1 = (2 * x + 1) * ch + c;

                const float a0 = L0[si] + H0[si]; // even col
                const float b0 = L0[si] - H0[si]; // odd  col
                const float a1 = L1[si] + H1[si];
                const float b1 = L1[si] - H1[si];

                o0[di0] = a0;
                o0[di1] = b0;
                o1[di0] = a1;
                o1[di1] = b1;
            }
        }
    }

    // optional debug (keep if useful)
    // cv::Mat dbg; cv::normalize(out, dbg, 0, 1, cv::NORM_MINMAX);
    // FSUtils::debugSaveMat(dbg, ".../recon_blurtest.png");

    return out;
}

static cv::Mat mag2(const cv::Mat& a, const cv::Mat& b, const cv::Mat& c)
{
    cv::Mat s; cv::add(a.mul(a), b.mul(b), s); cv::add(s, c.mul(c), s); return s;
}

} // anon

namespace FSWavelet {

Pyramid decompose(const cv::Mat& img32f, int levels)
{
    CV_Assert(img32f.type()==CV_32FC1 || img32f.type()==CV_32FC3);
    Pyramid pyr; pyr.reserve(levels);
    cv::Mat cur = img32f.clone();

    for (int i=0; i<levels; ++i) {
        Level L;
        haarDown(cur, L.low, L.hl, L.lh, L.hh);
        pyr.push_back(L);
        cur = L.low;
        if (cur.rows < 4 || cur.cols < 4) break;
    }
    return pyr;
}

cv::Mat reconstruct(const Pyramid& pyr)
{
    CV_Assert(!pyr.empty());
    cv::Mat cur = pyr.back().low.clone();

    for (int i = (int)pyr.size() - 1; i >= 0; --i) {
        const Level& L = pyr[i];

        // --- Perform synthesis for this level ---
        cv::Mat recon = haarUpAdd(cur, L.hl, L.lh, L.hh);

        // --- NEW: remove any per-tile brightness drift ---
        cv::Scalar meanIn  = cv::mean(cur);
        cv::Scalar meanOut = cv::mean(recon);
        recon = recon - (meanOut - meanIn);

        // --- Optional debug output ---
        cv::Mat dbg;
        cv::normalize(recon, dbg, 0, 1, cv::NORM_MINMAX);
        QString path = QString("/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/output/fusion_debug/recon_L%1.png").arg(i);
        FSUtils::debugSaveMat(dbg, path);

        cur = recon.clone();
    }

    return cur;
}

// ----------------------------------------------------------
// Diagnostic: verify Haar round-trip accuracy
// ----------------------------------------------------------
void testHaarRoundTrip(const cv::Mat& img32f)
{
    QString src = "testHaarRoundTrip";
    qDebug() << src << "starting test";

    const int levels = 4;
    auto pyr = FSWavelet::decompose(img32f, levels);

    cv::Mat cur = pyr.back().low.clone();
    std::vector<double> means;
    means.reserve(pyr.size() + 1);
    means.push_back(cv::mean(img32f)[0]);

    // --- Reconstruct step-by-step to observe drift ---
    for (int i = (int)pyr.size() - 1; i >= 0; --i) {
        const Level& L = pyr[i];
        cv::Mat recon = haarUpAdd(cur, L.hl, L.lh, L.hh);
        cv::Scalar m = cv::mean(recon);
        means.push_back(m[0]);

        qDebug().noquote()
            << QString("Level %1  mean=%2  Δmean=%3")
                   .arg(i)
                   .arg(m[0], 0, 'f', 8)
                   .arg((means.back() - means[means.size() - 2]), 0, 'f', 8);

        // Save normalized view for inspection
        cv::Mat dbg; cv::normalize(recon, dbg, 0, 1, cv::NORM_MINMAX);
        FSUtils::debugSaveMat(dbg,
                              QString("/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/output/fusion_debug/haar_roundtrip_L%1.png").arg(i));

        cur = recon.clone();
    }

    // --- Final comparison to input ---
    cv::Mat diff = cv::abs(cur - img32f);
    double minErr, maxErr;
    cv::minMaxLoc(diff.reshape(1), &minErr, &maxErr);
    cv::Mat sq; cv::multiply(diff, diff, sq);
    double mse = cv::mean(sq)[0];
    double psnr = (mse <= 1e-12) ? 99.0 : 10.0 * std::log10(1.0 / mse);

    qDebug() << "[Haar round-trip]" << "minErr:" << minErr << "maxErr:" << maxErr
             << "PSNR:" << psnr << "dB";

    cv::Mat diffVis; cv::normalize(diff, diffVis, 0, 1, cv::NORM_MINMAX);
    FSUtils::debugSaveMat(diffVis,
                          "/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/output/fusion_debug/haar_roundtrip_diff.png");

    qDebug() << src << "test complete";
}

cv::Mat focusEnergy(const Pyramid& pyr)
{
    QString src = "focusEnergy";
    // upsample each level’s |detail|^2 to full size and sum
    CV_Assert(!pyr.empty());
    cv::Size full = pyr.front().low.size()*2; // level 0 reconstruct size
    // Recompute full size robustly:
    cv::Mat probe = reconstruct(pyr); full = probe.size();

    cv::Mat acc(probe.rows, probe.cols, CV_32F, cv::Scalar(0));
    cv::Mat cur = pyr.back().low.clone(); // just for sizing
    for (int i=(int)pyr.size()-1; i>=0; --i) {
        const Level& L = pyr[i];
        cv::Mat e = mag2(cv::abs(L.lh), cv::abs(L.hl), cv::abs(L.hh));
        // Upsample to full res by repeated upsampling
        cv::Mat up = e;
        for (int j=i; j>=0; --j) cv::pyrUp(up, up);
        cv::resize(up, up, full, 0, 0, cv::INTER_LINEAR);
        if (up.channels() == 3) {
            vector<cv::Mat> ch; cv::split(up, ch);
            cv::Mat g; cv::cvtColor(joinBGR32F(ch), g, cv::COLOR_BGR2GRAY);
            up = g;
        }
        acc += up;
    }
    // Normalize to [0..1]
    double mn, mx; cv::minMaxLoc(acc, &mn, &mx);
    if (mx > mn) acc = (acc - mn) / (float)(mx - mn);
    return acc;
}

cv::Mat FSWavelet::fuseWavelet(const std::vector<cv::Mat>& stack32f,
                               const cv::Mat& mu32f_in,
                               const cv::Mat& sigma32f_in,
                               const cv::Mat& amp32f_in,
                               int levels)
{
    QString src = "FSWavelet::fuseWavelet";
    qDebug() << src;
    CV_Assert(!stack32f.empty());
    const int n = (int)stack32f.size();
    const cv::Size fullSize = stack32f[0].size();

    // --- Prepare guidance maps -------------------------------------------
    auto ensure32Fsingle = [&](cv::Mat &m) {
        if (m.empty()) return;
        if (m.channels() > 1)
            cv::cvtColor(m, m, cv::COLOR_BGR2GRAY);
        if (m.size() != fullSize)
            cv::resize(m, m, fullSize, 0, 0, cv::INTER_LINEAR);
        if (m.type() != CV_32F)
            m.convertTo(m, CV_32F, 1.0 / 255.0);
    };

    cv::Mat mu32f = mu32f_in.clone();
    cv::Mat sigma32f = sigma32f_in.clone();
    cv::Mat amp32f = amp32f_in.clone();
    ensure32Fsingle(mu32f);
    ensure32Fsingle(sigma32f);
    ensure32Fsingle(amp32f);

    // Clamp σ to reasonable range and pre-square
    cv::min(sigma32f, 2.5f, sigma32f);
    cv::max(sigma32f, 0.5f, sigma32f);
    cv::GaussianBlur(sigma32f, sigma32f, cv::Size(3,3), 0.6);
    cv::Mat sig2 = sigma32f.mul(sigma32f);

    // Normalize amplitude (or fill with 1)
    double aMin=0,aMax=0;
    if (!amp32f.empty()) {
        cv::minMaxLoc(amp32f,&aMin,&aMax);
        if (aMax <= 1e-8)
            amp32f = cv::Mat(fullSize,CV_32F,cv::Scalar(1.f));
        else {
            amp32f.convertTo(amp32f,CV_32F,1.0/aMax);
            cv::max(amp32f,0.1f,amp32f);
        }
    } else amp32f = cv::Mat(fullSize,CV_32F,cv::Scalar(1.f));

    // --- Validity masks --------------------------------------------------
    std::vector<cv::Mat> validFull;
    validFull.reserve(n);
    for (const cv::Mat& im : stack32f) {
        cv::Mat gray, mask;
        if (im.channels()==3) cv::cvtColor(im,gray,cv::COLOR_BGR2GRAY);
        else gray = im;
        cv::compare(gray,1e-6,mask,cv::CMP_GT);
        mask.convertTo(mask,CV_32F,1.0/255.0);
        cv::erode(mask,mask,cv::Mat(),cv::Point(-1,-1),1);
        validFull.push_back(mask);
    }

    // --- Full-resolution base blend -------------------------------------
    cv::Mat baseFull;
    buildFullResBase(stack32f, mu32f, sig2, amp32f, validFull, baseFull);
    cv::Mat baseFullEven = padEven(baseFull);
    Pyramid basePyr = decompose(baseFullEven, levels);
    FSUtils::debugSaveMat(baseFull, "/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/output/fusion_debug/baseFull.png");

    // --- Build pyramids for each stack image -----------------------------
    std::vector<Pyramid> pyrs; pyrs.reserve(n);
    for (auto &im : stack32f) {
        cv::Mat imgEven = padEven(im);
        pyrs.push_back(decompose(imgEven, levels));
    }

    // --- Initialize output pyramid --------------------------------------
    Pyramid out = pyrs[0];
    for (auto &L : out) {
        L.low.setTo(0);
        L.lh.setTo(0);
        L.hl.setTo(0);
        L.hh.setTo(0);
    }

    // --- Fuse detail bands -----------------------------------------------
    for (int lev=0; lev<(int)out.size(); ++lev) {
        cv::Size sz = out[lev].low.size();

        // Downscale validity + maps
        std::vector<cv::Mat> validL(n);
        for (int i=0;i<n;++i) {
            cv::resize(validFull[i],validL[i],sz,0,0,cv::INTER_AREA);
            cv::threshold(validL[i],validL[i],0.5,1.0,cv::THRESH_BINARY);
        }

        cv::Mat muL,sig2L,ampL;
        cv::resize(mu32f, muL, sz,0,0,cv::INTER_AREA);
        cv::resize(sig2,  sig2L,sz,0,0,cv::INTER_AREA);
        if(!amp32f.empty()) cv::resize(amp32f,ampL,sz,0,0,cv::INTER_AREA);
        else ampL=cv::Mat(sz,CV_32F,cv::Scalar(1.f));

        // weight per slice
        std::vector<cv::Mat> w_i(n);
        for (int i=0;i<n;++i) {
            const Level& L = pyrs[i][lev];
            cv::Mat diff; cv::absdiff(muL,cv::Scalar((float)i),diff);
            cv::Mat gate; cv::exp(-(diff.mul(diff))/(2.0f*sig2L),gate);
            gate = gate*0.99f + 0.01f;
            cv::max(gate,1e-3f,gate);

            cv::Mat e = cv::abs(L.lh)+cv::abs(L.hl)+cv::abs(L.hh);
            if(e.channels()==3){
                std::vector<cv::Mat> ch; cv::split(e,ch);
                e=(ch[0]+ch[1]+ch[2])/3.0f;
            }
            e+=1e-4f; cv::GaussianBlur(e,e,cv::Size(3,3),0.6);
            cv::Mat w = e.mul(gate).mul(ampL).mul(validL[i]);
            w+=1e-4f; w_i[i]=w;
        }

        // weighted sum
        for (int i=0;i<n;++i){
            const Level& Li=pyrs[i][lev];
            cv::Mat w=w_i[i];
            if(Li.lh.channels()==3){
                std::vector<cv::Mat> W3(3,w);
                cv::Mat w3; cv::merge(W3,w3);
                out[lev].lh+=Li.lh.mul(w3);
                out[lev].hl+=Li.hl.mul(w3);
                out[lev].hh+=Li.hh.mul(w3);
            } else {
                out[lev].lh+=Li.lh.mul(w);
                out[lev].hl+=Li.hl.mul(w);
                out[lev].hh+=Li.hh.mul(w);
            }
        }

        // normalize coefficients
        cv::Mat sumW=w_i[0].clone();
        for(int i=1;i<n;++i) sumW+=w_i[i];
        cv::max(sumW,1e-6f,sumW);

        if(out[lev].lh.channels()==3){
            std::vector<cv::Mat> S3(3,sumW);
            cv::Mat sumW3; cv::merge(S3,sumW3);
            cv::divide(out[lev].lh,sumW3,out[lev].lh);
            cv::divide(out[lev].hl,sumW3,out[lev].hl);
            cv::divide(out[lev].hh,sumW3,out[lev].hh);
        } else {
            cv::divide(out[lev].lh,sumW,out[lev].lh);
            cv::divide(out[lev].hl,sumW,out[lev].hl);
            cv::divide(out[lev].hh,sumW,out[lev].hh);
        }

        // --- soft blend of base low and averaged per-level low ---
        out[lev].low = basePyr[lev].low.clone();

        // --- match gain of base lowpass to current level ---
        // cv::Scalar meanOut = cv::mean(out[lev].low);
        // cv::Scalar meanBase = cv::mean(basePyr[lev].low);
        // double scale = (meanBase[0] > 1e-6) ? (meanOut[0] / meanBase[0]) : 1.0;

        // // copy base lowpass but rescale to match current brightness
        // out[lev].low = basePyr[lev].low * scale;
    }

    FSUtils::debugSaveMat(out.back().low,
                          "/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/output/fusion_debug/lowpass_final_level.png");

    // --- Reconstruction --------------------------------------------------
    qDebug() << src << "Reconstruct final fused image";
    cv::Mat fusedPadded = reconstruct(out);

    // crop to original size (remove any padding)
    const cv::Size orig = fullSize;
    cv::Mat fused = fusedPadded(cv::Rect(0,0,orig.width,orig.height)).clone();

    // cleanup
    cv::patchNaNs(fused,0.0);
    cv::max(fused,0.0f,fused);
    cv::min(fused,1.0f,fused);

    FSUtils::debugSaveMat(fused,
                          "/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/output/fusion_debug/fused_reconstruct.png");
    qDebug() << src << "Finished, returning fused";
    return fused;
}

} // namespace FSWavelet
