#ifndef CACHEADAPT_H
#define CACHEADAPT_H

#pragma once
#include <algorithm>
#include <optional>
#include <cmath>
#include <cstddef>

// --- Tiny math helpers (header-only) ---------------------------------
struct Ewma {
    double alpha{0.2};
    std::optional<double> y;
    void add(double x) { if (!y) y = x; else y = alpha * x + (1.0 - alpha) * (*y); }
    double value(double fallback=0.0) const { return y.value_or(fallback); }
};

struct OnlineVar {
    size_t n{0}; double mean{0}, m2{0};
    void add(double x){ ++n; double d=x-mean; mean+=d/double(n); double d2=x-mean; m2+=d*d2; }
    double sigma() const { return (n>1) ? std::sqrt(m2/double(n-1)) : 0.0; }
};

// Jain–Chlamtac P² single-quantile estimator (P90/P95)
class P2 {
public:
    explicit P2(double p=0.9) : p_(std::clamp(p, 1e-6, 1.0-1e-6)) { initDn(); }
    void add(double x);
    double value(double fallback=0.0) const;
private:
    void initDn(){ dn_[0]=0; dn_[1]=p_/2; dn_[2]=p_; dn_[3]=(1+p_)/2; dn_[4]=1; }
    double parabolic(int i, int d) const {
        double qi=q_[i], qim1=q_[i-1], qip1=q_[i+1];
        double ni=n_[i], nim1=n_[i-1], nip1=n_[i+1];
        return qi + d * (((ni-nim1+d)*(qip1-qi)/(nip1-ni) + (nip1-ni-d)*(qi-qim1)/(ni-nim1)) / (nip1-nim1));
    }
    double linear(int i, int d) const { return q_[i] + d*(q_[i+d]-q_[i])/(n_[i+d]-n_[i]); }
    // state
    double p_, dn_[5]{}, q_[5]{}, n_[5]{}, np_[5]{};
    bool inited=false; int cnt=0; double buf_[5]{};
};

// P2 impl
inline void P2::add(double x) {
    if (!inited) {
        buf_[cnt++] = x;
        if (cnt==5) {
            std::sort(buf_, buf_+5);
            for (int i=0;i<5;++i){ q_[i]=buf_[i]; n_[i]=i; }
            np_[0]=0; np_[1]=2*p_; np_[2]=4*p_; np_[3]=2+2*p_; np_[4]=4;
            inited=true;
        }
        return;
    }
    int k;
    if (x<q_[0]){ q_[0]=x; k=0; }
    else if (x<q_[1]) k=0;
    else if (x<q_[2]) k=1;
    else if (x<q_[3]) k=2;
    else { q_[4]=std::max(q_[4],x); k=3; }
    for (int i=k+1;i<5;++i) n_[i]+=1;
    for (int i=0;i<5;++i)   np_[i]+=dn_[i];
    for (int i=1;i<4;++i) {
        double d = np_[i]-n_[i];
        if ((d>=1.0 && n_[i+1]-n_[i]>1) || (d<=-1.0 && n_[i-1]-n_[i]<-1)) {
            int sign = d>=1.0? +1 : -1;
            double qi = parabolic(i, sign);
            if (q_[i-1] < qi && qi < q_[i+1]) q_[i] = qi;
            else q_[i] = linear(i, sign);
            n_[i] += sign;
        }
    }
}
inline double P2::value(double fb) const {
    if (!inited) { if (cnt==0) return fb; double tmp[5]; std::copy(buf_,buf_+cnt,tmp);
        std::sort(tmp, tmp+cnt); int idx=int(std::round(p_ * double(cnt-1)));
        return tmp[std::clamp(idx,0,cnt-1)]; }
    return q_[2];
}

// --- Cache adaptation model (header-only) -----------------------------
struct CacheStats {
    Ewma tMean{0.2}; OnlineVar tVar; P2 tP95{0.95};
    P2 bytesP90{0.90};
    Ewma vMean{0.3}; double vInst{0.0};
    Ewma revRatio{0.2};
    void addDecode(double sec, std::size_t bytes){ tMean.add(sec); tVar.add(sec); tP95.add(sec); bytesP90.add(double(bytes)); }
    void addNavRate(double v){ vInst=v; vMean.add(v); }
    void addReverse(bool r){ revRatio.add(r?1.0:0.0); }
    double sigma() const { return tVar.sigma(); }
    double p95()  const { return tP95.value(tMean.value(0.03)); }
    double perImgP90(std::size_t fb) const { return bytesP90.value(double(fb)); }
};

struct CacheTargets {
    int ahead{2}; int behind{1};
    double horizonSec{0.50}; double marginImg{1.0};
};

struct MemCaps {
    std::size_t cacheLimitBytes{std::size_t(4ull<<30)};
    std::size_t headroomBytes{std::size_t(512ull<<20)};
    std::size_t defaultPerImageBytes{std::size_t(50ull<<20)};
};

struct CacheController {
    double jitterK{1.0}; double hMin{0.20}, hMax{1.00}; double dUp{0.15}, dDown{0.05};
    int baseBehind{1}, maxBehind{5}, baseDeficit{2};
    void update(const CacheStats& S, CacheTargets& T, const MemCaps& M) {
        const double vstar = std::max(S.vMean.value(0.0), S.vInst);
        const double meanT = S.tMean.value(0.03);
        const double Sthr  = meanT>1e-6 ? 1.0/meanT : 1e6;
        const double t95   = S.p95();
        const double jit   = jitterK * S.sigma();
        const double H     = std::clamp(T.horizonSec, hMin, hMax);

        int aheadLatency = (int)std::ceil(vstar * (t95 + jit) + T.marginImg);
        int aheadDeficit = (Sthr < vstar) ? (int)std::ceil((vstar - Sthr) * H) + baseDeficit : 0;
        int aheadTarget  = std::max(aheadLatency, aheadDeficit);

        int behindTarget = std::clamp(baseBehind + int(std::round(S.revRatio.value(0.0)*2.0)), 1, maxBehind);

        const std::size_t perImg = std::max<std::size_t>(1, (std::size_t)S.perImgP90(M.defaultPerImageBytes));
        const std::size_t budget = (M.cacheLimitBytes > M.headroomBytes) ? (M.cacheLimitBytes - M.headroomBytes) : 0;
        const int capImages = (budget>0) ? std::max(2, int(budget / perImg)) : 2;

        // ahead is capped elsewhere using current ready/inflight/planned; here just clamp sanely:
        aheadTarget  = std::max(1, std::min(aheadTarget, capImages-1 - behindTarget));

        T.ahead = aheadTarget; T.behind = behindTarget;
    }
    void onUnderrun(CacheTargets& T){ T.horizonSec=std::min(T.horizonSec+dUp, hMax); T.marginImg=std::min(T.marginImg+0.5,3.0); }
    void onSmooth(CacheTargets& T){ T.horizonSec=std::max(T.horizonSec-dDown, hMin); T.marginImg=std::max(T.marginImg-0.25,1.0); }
};
#endif // CACHEADAPT_H
