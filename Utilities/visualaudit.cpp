#include "Utilities/visualaudit.h"

#include <QDataStream>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <unordered_map>
#include <opencv2/core.hpp>

namespace VisualAudit {

QString foldKey(const QString &path)
{
    return path.trimmed().toCaseFolded();
}

QString kindName(Finding::Kind kind)
{
    switch (kind) {
    case Finding::Suspect:   return "suspect";
    case Finding::Missing:   return "missing";
    case Finding::Suggested: return "suggested";
    }
    return "missing";
}

QString verdictKey(const QString &pathKey, Finding::Kind kind, const QString &keyword)
{
    return pathKey + '|' + kindName(kind) + '|' + foldKey(keyword);
}

QString Suggestion::reason() const
{
    QStringList why;
    if (visual) {
        const int i = keyword.lastIndexOf('|');
        why << QString("Looks like your %1 images")
                   .arg(i < 0 ? keyword : keyword.mid(i + 1));
    }
    if (sessionOf > 0)
        why << QString("On %1 of %2 keyworded images from this session")
                   .arg(sessionHits).arg(sessionOf);
    return why.join(" · ");
}

namespace {

QString parentKey(const QString &key)
{
    const int i = key.lastIndexOf('|');
    return i < 0 ? QString() : key.left(i);
}

bool hasCol(const std::vector<int> &sorted, int c)
{
    return std::binary_search(sorted.begin(), sorted.end(), c);
}

/* Bursts: union adjacent frames of a folder, in path order, that are near-duplicates.
   Over the images im[idx[0..]]; returns the burst id of each POSITION in idx, compacted
   to 0..B-1. */
std::vector<int> findBursts(const QVector<Image> &im, const std::vector<int> &idx,
                            const float *E, int dim, const Params &p, int &count)
{
    const int n = int(idx.size());
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const Image &x = im[idx[a]], &y = im[idx[b]];
        const int f = QString::compare(x.folder, y.folder);
        return f != 0 ? f < 0 : x.pathKey < y.pathKey;
    });

    std::vector<int> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&](int i) {
        while (parent[i] != i) i = parent[i] = parent[parent[i]];
        return i;
    };
    for (int a = 0; a < n; ++a) {
        const int i = order[a];
        const Image &ii = im[idx[i]];
        for (int b = a + 1; b < std::min(a + 6, n); ++b) {
            const int j = order[b];
            const Image &jj = im[idx[j]];
            if (ii.folder != jj.folder) break;
            if (ii.captured > 0 && jj.captured > 0
                && std::llabs(ii.captured - jj.captured) > p.burstSecs)
                continue;
            const float *x = E + size_t(idx[i]) * dim, *y = E + size_t(idx[j]) * dim;
            double dot = 0;
            for (int d = 0; d < dim; ++d) dot += double(x[d]) * y[d];
            if (dot >= p.burstCos) parent[root(j)] = root(i);
        }
    }

    std::vector<int> id(n, -1), out(n);
    count = 0;
    for (int i = 0; i < n; ++i) {
        const int r = root(i);
        if (id[r] < 0) id[r] = count++;
        out[i] = id[r];
    }
    return out;
}

/* Midrank AUC from the two sorted score lists. */
double auc(const std::vector<float> &pos, const std::vector<float> &neg)
{
    if (pos.empty() || neg.empty()) return 0.5;
    double sum = 0;
    for (float s : pos) {
        const auto lo = std::lower_bound(neg.begin(), neg.end(), s);
        const auto hi = std::upper_bound(lo, neg.end(), s);
        sum += double(lo - neg.begin()) + 0.5 * double(hi - lo);
    }
    return sum / (double(pos.size()) * double(neg.size()));
}

struct Candidate
{
    int row;
    int col;
    double score;
};

}   // namespace

Result run(const QVector<Image> &images, const std::vector<float> &emb, int dim,
           const QHash<QString, QString> &vocab, const QSet<QString> &dismissed,
           const QSet<QString> &skipped, const Params &p, const ProgressFn &progress,
           int trainCount, const std::vector<char> *inScope)
{
    Result res;
    const int n = trainCount < 0 ? int(images.size())
                                 : std::min(trainCount, int(images.size()));
    if (n == 0 || dim <= 0 || emb.size() < size_t(n) * dim) return res;
    if (inScope && inScope->size() < size_t(n)) inScope = nullptr;
    const float *E = emb.data();

    auto report = [&](const QString &stage, int pct) {
        if (progress && !progress(stage, pct)) {
            res.cancelled = true;
            return false;
        }
        return true;
    };

    /* ---------------------------------------------------------------- columns */
    if (!report("Grouping bursts", 0)) return res;
    QHash<QString, int> carriers;
    for (int i = 0; i < n; ++i)                     // the training images only
        for (const QString &path : images[i].expandedPaths) {
            const QString key = foldKey(path);
            if (vocab.contains(key)) ++carriers[key];
        }
    std::vector<QString> colKey;
    for (auto it = carriers.cbegin(); it != carriers.cend(); ++it)
        if (it.value() >= p.minPositives) colKey.push_back(it.key());
    std::sort(colKey.begin(), colKey.end());
    const int K = int(colKey.size());
    if (K == 0) {
        res.ok = true;
        return res;
    }
    QHash<QString, int> col;
    for (int c = 0; c < K; ++c) col.insert(colKey[c], c);

    std::vector<std::vector<int>> rowCols(n), rowExplicit(n), pos(K);
    for (int i = 0; i < n; ++i) {
        for (const QString &path : images[i].expandedPaths) {
            const int c = col.value(foldKey(path), -1);
            if (c >= 0) rowCols[i].push_back(c);
        }
        for (const QString &path : images[i].explicitPaths) {
            const int c = col.value(foldKey(path), -1);
            if (c >= 0) rowExplicit[i].push_back(c);
        }
        std::sort(rowCols[i].begin(), rowCols[i].end());
        rowCols[i].erase(std::unique(rowCols[i].begin(), rowCols[i].end()),
                         rowCols[i].end());
        std::sort(rowExplicit[i].begin(), rowExplicit[i].end());
        for (int c : rowCols[i]) pos[c].push_back(i);
    }

    QHash<QString, QVector<int>> children;          // parent key -> columns
    for (int c = 0; c < K; ++c) children[parentKey(colKey[c])] << c;

    /* ---------------------------------------------------------------- bursts */
    std::vector<int> all(n);
    std::iota(all.begin(), all.end(), 0);
    const std::vector<int> burst = findBursts(images, all, E, dim, p, res.bursts);
    std::vector<std::vector<int>> burstMembers(res.bursts);
    for (int i = 0; i < n; ++i) burstMembers[burst[i]].push_back(i);
    std::vector<int> fold(n);
    for (int i = 0; i < n; ++i)
        fold[i] = int((quint64(burst[i]) * 2654435761ULL % 4294967296ULL) % quint64(p.folds));

    /* ---------------------------------------------------------------- kNN */
    /*  Neighbours are found once; each row's vote is kept SPARSE (the columns its
        neighbours carry), because a dense images x keywords matrix is 600 MB at 87k x
        1.7k and almost all of it is zero. */
    const int k = std::min(p.k, n - 1);
    std::vector<std::vector<std::pair<int, float>>> knn(n);
    {
        const cv::Mat X(n, dim, CV_32F, const_cast<float *>(E));
        const int chunk = 256;
        for (int s = 0; s < n && k > 0; s += chunk) {
            const int e = std::min(n, s + chunk);
            cv::Mat S;
            cv::gemm(X.rowRange(s, e), X, 1.0, cv::noArray(), 0.0, S, cv::GEMM_2_T);
            cv::parallel_for_(cv::Range(s, e), [&](const cv::Range &r) {
                std::vector<std::pair<float, int>> heap;
                heap.reserve(k + 1);
                for (int i = r.start; i < r.end; ++i) {
                    float *row = S.ptr<float>(i - s);
                    for (int j : burstMembers[burst[i]]) row[j] = -2.0f;
                    heap.clear();
                    auto gt = std::greater<std::pair<float, int>>();
                    for (int j = 0; j < n; ++j) {
                        if (int(heap.size()) < k) {
                            heap.emplace_back(row[j], j);
                            std::push_heap(heap.begin(), heap.end(), gt);
                        }
                        else if (row[j] > heap.front().first) {
                            std::pop_heap(heap.begin(), heap.end(), gt);
                            heap.back() = {row[j], j};
                            std::push_heap(heap.begin(), heap.end(), gt);
                        }
                    }
                    float top = -2.0f;
                    for (const auto &h : heap) top = std::max(top, h.first);
                    double wsum = 0;
                    std::vector<double> w(heap.size());
                    for (size_t m = 0; m < heap.size(); ++m)
                        wsum += w[m] = std::exp((heap[m].first - top) / p.tau);
                    std::map<int, double> vote;
                    for (size_t m = 0; m < heap.size(); ++m)
                        for (int c : rowCols[heap[m].second]) vote[c] += w[m] / wsum;
                    /*  QUANTISED, so a saturated vote is an exact tie. Most carriers
                        of a well-kept keyword score ~1, and without this float
                        rounding (0.9999998 vs 1.0000001) decided whether a lookalike
                        "outscored" them -- ranks of noise. Ties count in the
                        lookalike's favour in both rank tests. */
                    knn[i].clear();
                    for (const auto &[c, v] : vote)
                        knn[i].emplace_back(c, float(std::round(v * 1e4) / 1e4));
                }
            });
            if (!report("Finding similar images", 5 + int(45.0 * e / n))) return res;
        }
    }
    auto knnScore = [&](int i, int c) {
        const auto &v = knn[i];
        const auto it = std::lower_bound(v.begin(), v.end(), std::make_pair(c, -1e9f));
        return it != v.end() && it->first == c ? it->second : 0.0f;
    };

    /* ---------------------------------------------------------------- ridge */
    if (!report("Learning keywords", 50)) return res;
    const int da = dim + 1;                           // + bias
    std::vector<cv::Mat> Xf(p.folds);                  // rows of each fold, float
    std::vector<std::vector<int>> foldRows(p.folds);
    std::vector<int> rowInFold(n);
    for (int i = 0; i < n; ++i) {
        rowInFold[i] = int(foldRows[fold[i]].size());
        foldRows[fold[i]].push_back(i);
    }
    std::vector<cv::Mat> W(p.folds);
    cv::Mat Wfull;                                     // every row: for suggestions
    {
        std::vector<cv::Mat> G(p.folds), B(p.folds);
        cv::Mat Gt = cv::Mat::zeros(da, da, CV_64F), Bt = cv::Mat::zeros(da, K, CV_64F);
        for (int f = 0; f < p.folds; ++f) {
            const auto &rows = foldRows[f];
            Xf[f].create(int(rows.size()), da, CV_32F);
            for (int r = 0; r < int(rows.size()); ++r) {
                float *d = Xf[f].ptr<float>(r);
                std::copy(E + size_t(rows[r]) * dim, E + size_t(rows[r] + 1) * dim, d);
                d[dim] = 1.0f;
            }
            cv::Mat Xd;
            Xf[f].convertTo(Xd, CV_64F);
            cv::gemm(Xd, Xd, 1.0, cv::noArray(), 0.0, G[f], cv::GEMM_1_T);
            /*  B = X^T T with T = +1 for carriers, -1 otherwise, so each column is
                2 * (sum of its carriers) - (sum of every row) -- no dense T. */
            cv::Mat all;
            cv::reduce(Xd, all, 0, cv::REDUCE_SUM, CV_64F);   // 1 x da
            B[f].create(da, K, CV_64F);
            for (int c = 0; c < K; ++c)
                for (int d = 0; d < da; ++d) B[f].at<double>(d, c) = -all.at<double>(0, d);
            for (int r = 0; r < int(rows.size()); ++r) {
                const double *x = Xd.ptr<double>(r);
                for (int c : rowCols[rows[r]])
                    for (int d = 0; d < da; ++d) B[f].at<double>(d, c) += 2.0 * x[d];
            }
            Gt += G[f];
            Bt += B[f];
        }
        cv::Mat reg = cv::Mat::eye(da, da, CV_64F) * p.ridgeLambda;
        reg.at<double>(dim, dim) = 0;                        // bias unregularised
        for (int f = 0; f < p.folds; ++f) {
            cv::Mat Wd;
            cv::solve(Gt - G[f] + reg, Bt - B[f], Wd, cv::DECOMP_CHOLESKY);
            Wd.convertTo(W[f], CV_32F);
        }
        /*  NO FOLD HELD OUT. Cross-fitting stops a carrier vouching for itself; an image
            being suggested for was never a label, so the model it is scored by may as
            well have seen everything. One more solve with the totals already summed. */
        cv::Mat Wd;
        cv::solve(Gt + reg, Bt, Wd, cv::DECOMP_CHOLESKY);
        Wd.convertTo(Wfull, CV_32F);
    }
    auto ridgeScore = [&](int i, int c) {
        const float *x = Xf[fold[i]].ptr<float>(rowInFold[i]);
        const cv::Mat &w = W[fold[i]];
        double s = 0;
        for (int d = 0; d < da; ++d) s += double(x[d]) * w.at<float>(d, c);
        return float(s);
    };

    /* ---------------------------------------------------------------- score */
    /*  Columns in blocks: a block's ridge scores are one gemm per fold, its kNN scores
        are filled from the sparse votes, and each column is then ranked on its own. */
    std::vector<double> aucK(K), aucR(K);
    std::vector<std::vector<float>> posK(K), posR(K);  // sorted carrier scores, kept
    std::vector<std::vector<Candidate>> sus(K), mis(K);
    std::vector<std::vector<int>> exemplars(K);         // best-scoring carriers
    const int block = 64;
    for (int c0 = 0; c0 < K; c0 += block) {
        const int c1 = std::min(K, c0 + block), bc = c1 - c0;
        cv::Mat SR(n, bc, CV_32F), SK = cv::Mat::zeros(n, bc, CV_32F);
        for (int f = 0; f < p.folds; ++f) {
            if (Xf[f].empty()) continue;
            cv::Mat s;
            cv::gemm(Xf[f], W[f].colRange(c0, c1), 1.0, cv::noArray(), 0.0, s);
            for (int r = 0; r < s.rows; ++r)
                s.row(r).copyTo(SR.row(foldRows[f][r]));
        }
        for (int i = 0; i < n; ++i)
            for (const auto &v : knn[i])
                if (v.first >= c0 && v.first < c1) SK.at<float>(i, v.first - c0) = v.second;

        cv::parallel_for_(cv::Range(c0, c1), [&](const cv::Range &r) {
            std::vector<char> carried(n);
            for (int c = r.start; c < r.end; ++c) {
                std::fill(carried.begin(), carried.end(), 0);
                for (int i : pos[c]) carried[i] = 1;
                const int bcol = c - c0;
                std::vector<float> nk, nr;
                nk.reserve(n - pos[c].size());
                nr.reserve(n - pos[c].size());
                posK[c].clear();
                posR[c].clear();
                for (int i = 0; i < n; ++i) {
                    const float sk = SK.at<float>(i, bcol), sr = SR.at<float>(i, bcol);
                    if (carried[i]) { posK[c].push_back(sk); posR[c].push_back(sr); }
                    else { nk.push_back(sk); nr.push_back(sr); }
                }
                std::sort(nk.begin(), nk.end());
                std::sort(nr.begin(), nr.end());
                std::sort(posK[c].begin(), posK[c].end());
                std::sort(posR[c].begin(), posR[c].end());
                /* The review shows these beside a finding: "this is what your Egret
                   pictures look like". Ridge, not kNN, so they are typical rather than
                   merely clustered. */
                std::vector<std::pair<float, int>> best;
                for (int i : pos[c]) best.emplace_back(-SR.at<float>(i, bcol), i);
                const size_t nb = std::min<size_t>(3, best.size());
                std::partial_sort(best.begin(), best.begin() + nb, best.end());
                for (size_t b = 0; b < nb; ++b) exemplars[c].push_back(best[b].second);

                aucK[c] = auc(posK[c], nk);
                aucR[c] = auc(posR[c], nr);
                if (nk.empty()) continue;

                /* Suspect: an explicit carrier beaten by suspectFpr of non-carriers. */
                for (int i : pos[c]) {
                    if (!hasCol(rowExplicit[i], c)) continue;
                    const float sk = SK.at<float>(i, bcol), sr = SR.at<float>(i, bcol);
                    const double fk = 1.0 - double(std::lower_bound(nk.begin(), nk.end(), sk)
                                                   - nk.begin()) / nk.size();
                    const double fr = 1.0 - double(std::lower_bound(nr.begin(), nr.end(), sr)
                                                   - nr.begin()) / nr.size();
                    if (fk >= p.suspectFpr && fr >= p.suspectFpr)
                        sus[c].push_back({i, c, (fk + fr) / 2});
                }
                /* Missing: a non-carrier that outscores missingTpr of the carriers. */
                const auto &pk = posK[c], &pr = posR[c];
                for (int i = 0; i < n; ++i) {
                    if (carried[i]) continue;
                    const float sk = SK.at<float>(i, bcol);
                    const double tk = double(std::upper_bound(pk.begin(), pk.end(), sk)
                                             - pk.begin()) / pk.size();
                    if (tk < p.missingTpr) continue;
                    const float sr = SR.at<float>(i, bcol);
                    const double tr = double(std::upper_bound(pr.begin(), pr.end(), sr)
                                             - pr.begin()) / pr.size();
                    if (tr >= p.missingTpr) mis[c].push_back({i, c, (tk + tr) / 2});
                }
            }
        });
        if (!report("Scoring keywords", 55 + int(40.0 * c1 / K))) return res;
    }

    /* ---------------------------------------------------------------- collect */
    if (!report("Collecting findings", 95)) return res;
    std::vector<char> visual(K);
    for (int c = 0; c < K; ++c)
        visual[c] = aucK[c] >= p.minAuc && aucR[c] >= p.minAuc
                    && !skipped.contains(colKey[c]);
    auto tpr = [](const std::vector<float> &sorted, float s) {
        return sorted.empty() ? 0.0
               : double(std::upper_bound(sorted.begin(), sorted.end(), s)
                        - sorted.begin()) / sorted.size();
    };

    struct Group { Finding f; int col; };
    std::map<std::tuple<int, int, int>, Group> groups;      // (burst, kind, col)
    auto add = [&](int i, int c, Finding::Kind kind, double score,
                   const QString &alt, const QStringList &sib) {
        if (inScope && !(*inScope)[i]) return;          // learned from, not reported on
        const QString vk = verdictKey(images[i].pathKey, kind, colKey[c]);
        if (dismissed.contains(vk)) return;
        auto [it, fresh] = groups.try_emplace({burst[i], int(kind), c});
        Group &g = it->second;
        if (fresh) {
            g.col = c;
            g.f.kind = kind;
            g.f.keyword = vocab.value(colKey[c]);
            g.f.alternative = alt;
            g.f.siblings = sib;
            g.f.score = score;
        }
        if (score > g.f.score) {
            g.f.score = score;
            if (!alt.isEmpty()) g.f.alternative = alt;
        }
        g.f.members << i;
    };

    /*  (row, column) of every alternative offered. "Tagged Heron, looks like Egret" and
        "missing Egret" on the same image are ONE mistake, reported once as the former. */
    std::set<std::pair<int, int>> offered;
    for (int c = 0; c < K; ++c) {
        if (!visual[c]) continue;
        const QVector<int> sibs = children.value(parentKey(colKey[c]));
        for (const Candidate &s : sus[c]) {
            int alt = -1;
            double best = 0;
            for (int o : sibs) {
                if (o == c || !visual[o] || hasCol(rowCols[s.row], o)) continue;
                const double t = std::min(tpr(posK[o], knnScore(s.row, o)),
                                          tpr(posR[o], ridgeScore(s.row, o)));
                if (t > best) { best = t; alt = o; }
            }
            if (best < p.altTpr) alt = -1;
            if (alt >= 0)
                for (int m : burstMembers[burst[s.row]]) offered.insert({m, alt});
            add(s.row, c, Finding::Suspect, s.score,
                alt >= 0 ? vocab.value(colKey[alt]) : QString(), {});
        }
    }

    /* Missing, deepest keyword only per image. */
    std::vector<std::vector<Candidate>> missByRow(n);
    for (int c = 0; c < K; ++c)
        if (visual[c])
            for (const Candidate &m : mis[c]) missByRow[m.row].push_back(m);
    for (int i = 0; i < n; ++i) {
        const auto &cands = missByRow[i];
        for (const Candidate &m : cands) {
            const QString prefix = colKey[m.col] + '|';
            bool deeper = false;
            for (const Candidate &o : cands)
                if (colKey[o.col].startsWith(prefix)) { deeper = true; break; }
            if (deeper || offered.count({i, m.col})) continue;
            QStringList sib;
            for (int o : children.value(parentKey(colKey[m.col])))
                if (hasCol(rowCols[i], o)) sib << vocab.value(colKey[o]);
            add(i, m.col, Finding::Missing, m.score, {}, sib);
        }
    }

    std::vector<int> nSus(K), nMis(K);
    for (auto &[key, g] : groups) {
        (g.f.kind == Finding::Suspect ? nSus : nMis)[g.col]++;
        res.findings << g.f;
    }
    std::stable_sort(res.findings.begin(), res.findings.end(),
                     [](const Finding &a, const Finding &b) { return a.score > b.score; });

    for (int c = 0; c < K; ++c) {
        KeywordStat st;
        st.keyword = vocab.value(colKey[c]);
        st.images = int(pos[c].size());
        st.aucKnn = aucK[c];
        st.aucRidge = aucR[c];
        st.visual = visual[c];
        st.suspects = nSus[c];
        st.missing = nMis[c];
        st.exemplars = QVector<int>(exemplars[c].begin(), exemplars[c].end());
        res.stats << st;
    }
    std::stable_sort(res.stats.begin(), res.stats.end(),
                     [](const KeywordStat &a, const KeywordStat &b) {
                         return a.images > b.images;
                     });

    /* ---------------------------------------------------------------- model */
    /*  The visual columns only, with their carriers' CROSS-FITTED ridge scores: a new
        image scored by the full fit is ranked against what the carriers scored when
        each was unseen, which is the like-for-like comparison. */
    {
        SuggestModel &m = res.model;
        m.learned = QDateTime::currentDateTime();
        m.images = n;
        m.dim = dim;
        m.minTpr = p.missingTpr;
        std::vector<int> cols;
        for (int c = 0; c < K; ++c)
            if (visual[c] && !posR[c].empty()) cols.push_back(c);
        const int kv = int(cols.size());
        m.W.assign(size_t(da) * kv, 0.0f);
        for (int j = 0; j < kv; ++j) {
            const int c = cols[j];
            m.keywords << vocab.value(colKey[c]);
            for (int d = 0; d < da; ++d)
                m.W[size_t(d) * kv + j] = Wfull.at<float>(d, c);
            m.carrierScores.push_back(posR[c]);
        }
    }
    res.ok = true;
    report("Done", 100);
    return res;
}

/* ================================================================ suggestions */

namespace {

constexpr quint32 kModelMagic = 0x57534B4D;     // "WSKM"
constexpr qint32 kModelVersion = 1;

/* Every ancestor of every path, folded, the paths themselves included. */
QSet<QString> expandFold(const QStringList &paths)
{
    QSet<QString> out;
    for (const QString &p : paths) {
        QString f = foldKey(p);
        while (!f.isEmpty()) {
            out.insert(f);
            f = parentKey(f);
        }
    }
    return out;
}

}   // namespace

bool SuggestModel::save(const QString &file) const
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream s(&f);
    s.setVersion(QDataStream::Qt_6_0);
    s << kModelMagic << kModelVersion << modelTag << learned << qint32(images)
      << qint32(dim) << minTpr << keywords;
    s << quint64(W.size());
    s.writeRawData(reinterpret_cast<const char *>(W.data()),
                   int(W.size() * sizeof(float)));
    s << quint32(carrierScores.size());
    for (const std::vector<float> &v : carrierScores) {
        s << quint32(v.size());
        s.writeRawData(reinterpret_cast<const char *>(v.data()),
                       int(v.size() * sizeof(float)));
    }
    if (s.status() != QDataStream::Ok) { f.cancelWriting(); return false; }
    return f.commit();
}

bool SuggestModel::load(const QString &file)
{
/*
    All or nothing: a file from another version, or one cut short, leaves the model
    EMPTY rather than half-read -- an empty model suggests nothing, a garbled one would
    suggest nonsense.
*/
    *this = SuggestModel();
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QDataStream s(&f);
    s.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    qint32 version = 0, imgs = 0, d = 0;
    SuggestModel m;
    s >> magic >> version;
    if (magic != kModelMagic || version != kModelVersion) return false;
    s >> m.modelTag >> m.learned >> imgs >> d >> m.minTpr >> m.keywords;
    m.images = imgs;
    m.dim = d;
    quint64 nw = 0;
    s >> nw;
    if (d <= 0 || nw != quint64(d + 1) * quint64(m.keywords.size())) return false;
    m.W.resize(nw);
    if (s.readRawData(reinterpret_cast<char *>(m.W.data()), int(nw * sizeof(float)))
        != int(nw * sizeof(float))) return false;
    quint32 nc = 0;
    s >> nc;
    if (nc != quint32(m.keywords.size())) return false;
    m.carrierScores.resize(nc);
    for (quint32 c = 0; c < nc; ++c) {
        quint32 len = 0;
        s >> len;
        m.carrierScores[c].resize(len);
        if (len && s.readRawData(reinterpret_cast<char *>(m.carrierScores[c].data()),
                                 int(len * sizeof(float))) != int(len * sizeof(float)))
            return false;
    }
    if (s.status() != QDataStream::Ok) return false;
    *this = std::move(m);
    return true;
}

QVector<Suggestion> SuggestModel::score(const float *vec, const QSet<QString> &carriedFold,
                                        const QSet<QString> &dismissedFold) const
{
    if (!isValid() || !vec) return {};
    const int kv = keywords.size();
    std::vector<double> s(kv, 0.0);
    for (int d = 0; d < dim; ++d) {
        const double x = vec[d];
        const float *w = W.data() + size_t(d) * kv;
        for (int c = 0; c < kv; ++c) s[c] += x * w[c];
    }
    const float *bias = W.data() + size_t(dim) * kv;
    std::vector<float> raw(kv);
    for (int c = 0; c < kv; ++c) raw[c] = float(s[c] + bias[c]);
    return rank(raw.data(), carriedFold, dismissedFold);
}

QVector<Suggestion> SuggestModel::rank(const float *raw, const QSet<QString> &carriedFold,
                                       const QSet<QString> &dismissedFold) const
{
    QVector<Suggestion> out;
    if (!isValid() || !raw) return out;
    const int kv = keywords.size();
    for (int c = 0; c < kv; ++c) {
        const std::vector<float> &carriers = carrierScores[c];
        if (carriers.empty()) continue;
        /*  The share of the keyword's own images this one outscores -- the rule Missing
            uses, so "suggested" means the same thing in the panel and in the audit. */
        const double tpr = double(std::upper_bound(carriers.begin(), carriers.end(), raw[c])
                                  - carriers.begin()) / carriers.size();
        if (tpr < minTpr) continue;
        const QString fold = foldKey(keywords[c]);
        if (carriedFold.contains(fold) || dismissedFold.contains(fold)) continue;
        Suggestion sg;
        sg.keyword = keywords[c];
        sg.score = tpr;
        sg.visual = true;
        out << sg;
    }
    return out;
}

QVector<Suggestion> sessionSuggestions(const SessionImage &me,
                                       const QVector<SessionImage> &candidates,
                                       const QHash<QString, QString> &vocab,
                                       const QSet<QString> &carriedFold,
                                       const QSet<QString> &dismissedFold,
                                       const Params &p)
{
/*
    Counted on the EXPANDED paths of each neighbour, so a session tagged
    "Event|Wedding|Ceremony" on some frames and "Event|Wedding|Reception" on others still
    agrees on "Event|Wedding" -- mergeSuggestions then keeps the deeper one wherever a
    deeper one also passes.
*/
    QVector<Suggestion> out;
    if (me.captured <= 0 || me.folder.isEmpty()) return out;
    QHash<QString, int> hits;
    int of = 0;
    for (const SessionImage &c : candidates) {
        if (c.folder != me.folder || c.captured <= 0 || c.explicitPaths.isEmpty()) continue;
        if (std::llabs(c.captured - me.captured) > p.sessionSecs) continue;
        ++of;
        for (const QString &f : expandFold(c.explicitPaths))
            if (vocab.contains(f)) hits[f]++;
    }
    if (of < p.sessionMinImages) return out;
    for (auto it = hits.cbegin(); it != hits.cend(); ++it) {
        const double share = double(it.value()) / of;
        if (share < p.sessionShare) continue;
        if (carriedFold.contains(it.key()) || dismissedFold.contains(it.key())) continue;
        Suggestion sg;
        sg.keyword = vocab.value(it.key());
        sg.score = share;
        sg.sessionHits = it.value();
        sg.sessionOf = of;
        out << sg;
    }
    return out;
}

QVector<Suggestion> mergeSuggestions(const QVector<Suggestion> &visual,
                                     const QVector<Suggestion> &session, const Params &p)
{
    QHash<QString, Suggestion> byFold;
    auto take = [&](const Suggestion &s) {
        const QString f = foldKey(s.keyword);
        auto it = byFold.find(f);
        if (it == byFold.end()) { byFold.insert(f, s); return; }
        it->visual |= s.visual;
        if (s.sessionOf > 0) { it->sessionHits = s.sessionHits; it->sessionOf = s.sessionOf; }
        it->score = std::max(it->score, s.score);
    };
    for (const Suggestion &s : visual) take(s);
    for (const Suggestion &s : session) take(s);

    /*  DEEPEST WINS. "Fauna|Bird" beside "Fauna|Bird|Heron" says nothing the second does
        not; prefix expansion puts it on the image anyway. */
    QVector<Suggestion> out;
    for (auto it = byFold.cbegin(); it != byFold.cend(); ++it) {
        const QString prefix = it.key() + '|';
        bool deeper = false;
        for (auto jt = byFold.cbegin(); jt != byFold.cend(); ++jt)
            if (jt.key().startsWith(prefix)) { deeper = true; break; }
        if (!deeper) out << it.value();
    }
    /*  Both reasons first, then the stronger: two independent signals agreeing beat
        either alone. */
    std::sort(out.begin(), out.end(), [](const Suggestion &a, const Suggestion &b) {
        const bool ba = a.visual && a.sessionOf > 0, bb = b.visual && b.sessionOf > 0;
        if (ba != bb) return ba;
        if (a.score != b.score) return a.score > b.score;
        return a.keyword < b.keyword;
    });
    if (out.size() > p.maxSuggestions) out.resize(p.maxSuggestions);
    return out;
}

QVector<Finding> suggest(const QVector<Image> &images, const std::vector<float> &emb,
                         int dim, const QVector<int> &rows, const SuggestModel &model,
                         const QHash<QString, QString> &vocab,
                         const QSet<QString> &dismissed, const QSet<QString> &exclude,
                         const Params &p, const ProgressFn &progress)
{
/*
    BLOCKS OF ROWS, EACH ONE GEMM AND ONE PARALLEL PASS. The visual scores of a block are
    (rows x dim+1) x (dim+1 x keywords) -- the shape run() already scores in -- instead of
    a dot product per image per keyword on one core; ranking, the session window and the
    merge are per image and independent, so they run across cores into one slot each.
    Only grouping by burst, which joins images, stays serial. A block of 2,048 rows
    against ~2,000 keywords is 16 MB of scores, so a whole catalog never holds more.
*/
    QVector<Finding> out;
    if (rows.isEmpty() || dim <= 0 || emb.size() < size_t(images.size()) * dim) return out;
    const float *E = emb.data();
    const int nr = rows.size();

    /*  The session index: per folder, every keyworded image with a capture time, in
        time order, so a row's session is a window rather than a scan of the folder.
        Read-only from here on, which is what lets the rows share it across threads.

        KEYWORDS AS IDS, folded and expanded ONCE per image. sessionSuggestions takes
        strings, which is right for the panel's one image; here every image visits a
        whole session of neighbours (hundreds, at a shoot's pace), and re-folding their
        paths for each visit was three quarters of the step (3.3 of 4.4 s measured at
        158k images). The rule is the same -- see sessionSuggestions. */
    QHash<QString, int> vocabId;
    QStringList idDisplay;
    for (auto it = vocab.cbegin(); it != vocab.cend(); ++it) {
        vocabId.insert(it.key(), idDisplay.size());
        idDisplay << it.value();
    }
    std::vector<std::vector<int>> kwIds(images.size());
    QHash<QString, std::vector<std::pair<qint64, int>>> byFolder;
    for (int i = 0; i < images.size(); ++i) {
        if (images[i].captured <= 0 || images[i].explicitPaths.isEmpty()) continue;
        for (const QString &f : expandFold(images[i].explicitPaths)) {
            const int id = vocabId.value(f, -1);
            if (id >= 0) kwIds[size_t(i)].push_back(id);
        }
        byFolder[images[i].folder].emplace_back(images[i].captured, i);
    }
    for (auto &v : byFolder) std::sort(v.begin(), v.end());

    const bool visual = model.isValid() && model.dim == dim;
    const int kv = visual ? model.keywords.size() : 0;
    const int da = dim + 1;
    const cv::Mat Wm = visual ? cv::Mat(da, kv, CV_32F, const_cast<float *>(model.W.data()))
                              : cv::Mat();

    std::vector<QVector<Suggestion>> perRow(nr);
    constexpr int kBlock = 2048;
    for (int b0 = 0; b0 < nr; b0 += kBlock) {
        const int b1 = std::min(nr, b0 + kBlock), bn = b1 - b0;
        cv::Mat S;
        if (visual) {
            cv::Mat X(bn, da, CV_32F);
            for (int r = 0; r < bn; ++r) {
                float *d = X.ptr<float>(r);
                const float *v = E + size_t(rows[b0 + r]) * dim;
                std::copy(v, v + dim, d);
                d[dim] = 1.0f;                                  // the bias row of W
            }
            cv::gemm(X, Wm, 1.0, cv::noArray(), 0.0, S);
        }

        cv::parallel_for_(cv::Range(0, bn), [&](const cv::Range &range) {
            std::unordered_map<int, int> hits;                  // per thread: id -> count
            for (int r = range.start; r < range.end; ++r) {
                const int i = rows[b0 + r];
                const Image &im = images[i];
                const QSet<QString> carried = expandFold(im.expandedPaths);
                const QVector<Suggestion> vis =
                    visual ? model.rank(S.ptr<float>(r), carried, {}) : QVector<Suggestion>();

                QVector<Suggestion> ses;
                const auto fit = byFolder.constFind(im.folder);
                if (im.captured > 0 && fit != byFolder.cend()) {
                    const auto &v = *fit;
                    auto lo = std::lower_bound(
                        v.begin(), v.end(), std::make_pair(im.captured - p.sessionSecs, -1));
                    int of = 0;
                    hits.clear();
                    for (auto it = lo;
                         it != v.end() && it->first <= im.captured + p.sessionSecs; ++it) {
                        if (it->second == i) continue;
                        ++of;
                        for (int id : kwIds[size_t(it->second)]) ++hits[id];
                    }
                    if (of >= p.sessionMinImages)
                        for (const auto &[id, n] : hits) {
                            const double share = double(n) / of;
                            if (share < p.sessionShare) continue;
                            const QString &kw = idDisplay[id];
                            if (carried.contains(foldKey(kw))) continue;
                            Suggestion sg;
                            sg.keyword = kw;
                            sg.score = share;
                            sg.sessionHits = n;
                            sg.sessionOf = of;
                            ses << sg;
                        }
                }
                perRow[size_t(b0 + r)] = mergeSuggestions(vis, ses, p);
            }
        });
        if (progress && !progress("Suggesting keywords", int(100.0 * b1 / nr))) return {};
    }

    /* ---------------------------------------------------------------- group */
    std::vector<int> idx(rows.begin(), rows.end());
    int bursts = 0;
    const std::vector<int> burst = findBursts(images, idx, E, dim, p, bursts);

    std::map<std::pair<int, QString>, Finding> groups;     // (burst, fold)
    for (int r = 0; r < nr; ++r) {
        const int i = idx[size_t(r)];
        const Image &im = images[i];
        for (const Suggestion &s : perRow[size_t(r)]) {
            const QString fold = foldKey(s.keyword);
            if (exclude.contains(QString::number(i) + '|' + fold)) continue;
            if (dismissed.contains(verdictKey(im.pathKey, Finding::Suggested, s.keyword)))
                continue;
            auto [it, fresh] = groups.try_emplace({burst[size_t(r)], fold});
            Finding &f = it->second;
            if (fresh) {
                f.kind = Finding::Suggested;
                f.keyword = s.keyword;
                f.reason = s.reason();
                f.score = s.score;
            }
            else if (s.score > f.score) {
                f.score = s.score;
                f.reason = s.reason();
            }
            f.members << i;
        }
    }
    for (auto &[key, f] : groups) out << f;
    std::stable_sort(out.begin(), out.end(),
                     [](const Finding &a, const Finding &b) { return a.score > b.score; });
    return out;
}

}   // namespace VisualAudit
