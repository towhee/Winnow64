#!/usr/bin/env python3
"""
Keyword Audit, Phase 0 prototype -- does SigLIP disagree with my keywords because I made
mistakes? See ~/.claude/plans/i-suspect-that-many-sparkling-dove.md. Standalone; reads
index.db and userdata.db READ-ONLY and touches no Winnow code.

The method judges each image against the user's OWN tagged images, not against SigLIP's
idea of the word. Every score for (image, keyword) comes from a model that never saw that
image -- or any frame of its burst -- labelled, so a wrong tag cannot vouch for itself:

    kNN    similarity-weighted vote of the k nearest images, burst-mates excluded
    probe  multi-label linear probe, 5-fold cross-fit, folds grouped by burst
    text   SigLIP zero-shot "a photo of <leaf>" (comparison only: is Phase 3 worth it?)

Scores are turned into RANKS so rare and common keywords share one threshold:

    suspect  a positive whose score is beaten by more than --suspect-fpr of the images
             that do NOT carry the keyword (it looks no more X than a non-X image)
    missing  a non-carrier that outscores more than --missing-tpr of the carriers

Both kNN and probe must agree before anything is reported. Keywords whose cross-fit AUC
is below --min-auc are not visual (people, events, "Favourite") and are skipped.

Usage (venv: tools/_kwaudit_venv):
    python tools/keyword_audit_proto.py embed --model base
    python tools/keyword_audit_proto.py audit --model base
    open tools/_kwaudit_out/report-base.html
"""
import argparse
import base64
import html
import json
import os
import sqlite3
import sys
import time
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from io import BytesIO

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "_kwaudit_out")
APPDATA = os.path.expanduser("~/Library/Application Support/Winnow")
INDEX_DB = os.path.join(APPDATA, "PreviewCache", "index.db")
USER_DB = os.path.join(APPDATA, "userdata.db")

MODELS = {
    "base":   ("google/siglip2-base-patch16-256", 256),
    "so400m": ("google/siglip2-so400m-patch14-384", 384),
}

NO_CAPTURE = 0  # index.db stores a 1904 sentinel (negative) for "unknown"


# ---------------------------------------------------------------- keyword paths
# Mirrors Metadata/keywordpaths.h: keywordEffectivePaths + keywordPrefixExpand.

def fold(s):
    return s.strip().casefold()


def nodes(path):
    return [p.strip() for p in path.split("|") if p.strip()]


def effective_paths(subject, hierarchical):
    out, seen, node_names = [], set(), set()
    for p in hierarchical:
        ns = nodes(p)
        if not ns:
            continue
        for n in ns:
            node_names.add(fold(n))
        key = fold("|".join(ns))
        if key not in seen:
            seen.add(key)
            out.append("|".join(ns))
    for s in subject:
        s = s.strip()
        if not s or fold(s) in node_names or fold(s) in seen:
            continue
        seen.add(fold(s))
        out.append(s)
    return out


def prefix_expand(paths):
    out, seen = [], set()
    for p in paths:
        prefix = ""
        for n in nodes(p):
            prefix = n if not prefix else prefix + "|" + n
            if fold(prefix) not in seen:
                seen.add(fold(prefix))
                out.append(prefix)
    return out


def drop_ancestors(paths):
    keys = [fold(p) for p in paths]
    return [p for p, k in zip(paths, keys)
            if not any(o != k and o.startswith(k + "|") for o in keys)]


def split_list(s):
    return [x for x in (s or "").split("\n") if x.strip()]


# ---------------------------------------------------------------- data

def connect_ro(path):
    return sqlite3.connect(f"file:{path}?mode=ro", uri=True)


def load_rows():
    """Every live, keyworded image that has a cached thumbnail, in path order."""
    db = connect_ro(INDEX_DB)
    q = """SELECT i.pathkey, i.path, i.folder, i.captured,
                  i.keywords_literal, i.keywordpaths
           FROM image i JOIN thumb t ON t.pathkey = i.pathkey
           WHERE i.live = 1 AND (i.keywords_literal <> '' OR i.keywordpaths <> '')
           ORDER BY i.folder, i.path"""
    rows = []
    for pk, path, folder, cap, lit, kp in db.execute(q):
        eff = effective_paths(split_list(lit), split_list(kp))
        rows.append({
            "pathkey": pk, "path": path, "folder": folder,
            "captured": cap if cap and cap > NO_CAPTURE else None,
            "explicit": drop_ancestors(eff),
            "expanded": prefix_expand(eff),
        })
    db.close()
    return rows


def thumb_jpg(db, pathkey):
    r = db.execute("SELECT jpg FROM thumb WHERE pathkey = ?", (pathkey,)).fetchone()
    return r[0] if r else None


def load_vocab():
    """folded path -> display path, for the user's keyword list."""
    db = connect_ro(USER_DB)
    v = {fold(p): p for (p,) in db.execute("SELECT path FROM vocab")}
    db.close()
    return v


def pooled(out):
    """transformers 5 returns an output object from get_*_features; 4.x a tensor."""
    return out if hasattr(out, "float") else out.pooler_output


def device():
    import torch
    return "mps" if torch.backends.mps.is_available() else "cpu"


# ---------------------------------------------------------------- embed

def cmd_embed(args):
    import torch
    from PIL import Image
    from transformers import AutoModel

    os.makedirs(OUT, exist_ok=True)
    name, size = MODELS[args.model]
    rows = load_rows()
    keys = [r["pathkey"] for r in rows]
    print(f"{len(rows)} keyworded images with thumbnails")

    emb_path = os.path.join(OUT, f"emb-{args.model}.npy")
    keys_path = os.path.join(OUT, f"emb-{args.model}.keys.json")
    done = {}
    if os.path.exists(emb_path) and os.path.exists(keys_path):
        old = np.load(emb_path)
        for k, v in zip(json.load(open(keys_path)), old):
            done[k] = v
    todo = [k for k in keys if k not in done]
    if args.limit:
        todo = todo[:args.limit]
    print(f"{len(done)} already embedded, {len(todo)} to go")

    dev = device()
    model = AutoModel.from_pretrained(name, dtype=torch.float16).to(dev).eval()

    def prep(jpg):
        # SigLIP fixed-res: squash to size x size (no crop), bicubic, mean = std = 0.5.
        im = Image.open(BytesIO(jpg)).convert("RGB").resize((size, size), Image.BICUBIC)
        a = np.asarray(im, dtype=np.float32) / 255.0
        return ((a - 0.5) / 0.5).transpose(2, 0, 1)

    db = connect_ro(INDEX_DB)
    pool = ThreadPoolExecutor(8)
    t0, bs = time.time(), args.batch
    for start in range(0, len(todo), bs):
        chunk = todo[start:start + bs]
        jpgs = [thumb_jpg(db, k) for k in chunk]
        x = np.stack(list(pool.map(prep, jpgs)))
        with torch.no_grad():
            f = model.get_image_features(
                pixel_values=torch.from_numpy(x).to(dev, torch.float16))
            f = torch.nn.functional.normalize(pooled(f).float(), dim=-1)
        for k, v in zip(chunk, f.cpu().numpy()):
            done[k] = v
        n = start + len(chunk)
        if n % (bs * 64) < bs or n == len(todo):
            rate = n / (time.time() - t0)
            print(f"  {n}/{len(todo)}  {rate:.0f} img/s  "
                  f"eta {(len(todo) - n) / rate / 60:.1f} min", flush=True)
            save_embeddings(done, emb_path, keys_path)
    save_embeddings(done, emb_path, keys_path)
    print("done")


def save_embeddings(done, emb_path, keys_path):
    ks = list(done.keys())
    np.save(emb_path + ".tmp.npy", np.stack([done[k] for k in ks]).astype(np.float16))
    os.replace(emb_path + ".tmp.npy", emb_path)
    json.dump(ks, open(keys_path, "w"))


# ---------------------------------------------------------------- bursts

def find_bursts(rows, E, max_dt, min_cos):
    """Union adjacent frames (same folder, path order) that are near-duplicates."""
    parent = list(range(len(rows)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for i in range(len(rows)):
        for j in range(i + 1, min(i + 6, len(rows))):
            a, b = rows[i], rows[j]
            if a["folder"] != b["folder"]:
                break
            if a["captured"] and b["captured"] and abs(a["captured"] - b["captured"]) > max_dt:
                continue
            if float(E[i] @ E[j]) >= min_cos:
                parent[root(j)] = root(i)
    ids = np.array([root(i) for i in range(len(rows))])
    _, ids = np.unique(ids, return_inverse=True)
    return ids


# ---------------------------------------------------------------- scoring

def knn_scores(E, Y, burst, k, tau):
    """Cross-fit by construction: an image's own burst is never among its neighbours."""
    import torch
    dev = device()
    Et = torch.from_numpy(E).to(dev, torch.float32)
    Yt = torch.from_numpy(Y).to(dev, torch.float16)
    bt = torch.from_numpy(burst).to(dev)
    out = np.zeros(Y.shape, dtype=np.float32)
    for s in range(0, len(E), 2048):
        q = Et[s:s + 2048]
        sim = q @ Et.T
        same = bt[s:s + 2048, None] == bt[None, :]
        sim = sim.masked_fill(same, -2.0)
        v, idx = sim.topk(k, dim=1)
        w = torch.softmax(v / tau, dim=1).to(torch.float16)
        out[s:s + 2048] = torch.einsum("nk,nkc->nc", w, Yt[idx]).float().cpu().numpy()
    return out


def probe_scores(E, Y, burst, folds, epochs, wd):
    """Multi-label linear probe, cross-fit: each fold is scored by a probe trained on
    the other folds. Folds are grouped by burst so near-duplicates cannot leak."""
    import torch
    dev = device()
    X = torch.from_numpy(E).to(dev, torch.float32) * 10.0
    Yt = torch.from_numpy(Y).to(dev, torch.float32)
    fold_of = torch.from_numpy(fold_of_burst(burst, folds)).to(dev)
    out = np.zeros(Y.shape, dtype=np.float32)
    for f in range(folds):
        tr, te = fold_of != f, fold_of == f
        lin = torch.nn.Linear(X.shape[1], Y.shape[1]).to(dev)
        opt = torch.optim.AdamW(lin.parameters(), lr=1e-2, weight_decay=wd)
        Xtr, Ytr = X[tr], Yt[tr]
        for _ in range(epochs):
            opt.zero_grad()
            loss = torch.nn.functional.binary_cross_entropy_with_logits(lin(Xtr), Ytr)
            loss.backward()
            opt.step()
        with torch.no_grad():
            out[te.cpu().numpy()] = torch.sigmoid(lin(X[te])).cpu().numpy()
        print(f"  probe fold {f + 1}/{folds}  loss {loss.item():.4f}", flush=True)
    return out


def fold_of_burst(burst, folds):
    return (burst * 2654435761 % 2**32) % folds


def ridge_scores(E, Y, burst, folds, lam):
    """Closed-form one-vs-rest ridge regression, cross-fit by subtraction: the Gram
    matrices are summed per fold once, and each fold's probe solves with the totals
    minus its own share. This is what the C++ port does -- no training loop."""
    X = np.hstack([E.astype(np.float64), np.ones((len(E), 1))])
    T = Y.astype(np.float64) * 2 - 1
    f = fold_of_burst(burst, folds)
    G = [X[f == k].T @ X[f == k] for k in range(folds)]
    B = [X[f == k].T @ T[f == k] for k in range(folds)]
    Gt, Bt = sum(G), sum(B)
    reg = lam * np.eye(X.shape[1])
    reg[-1, -1] = 0
    out = np.zeros(Y.shape, dtype=np.float32)
    for k in range(folds):
        W = np.linalg.solve(Gt - G[k] + reg, Bt - B[k])
        out[f == k] = X[f == k] @ W
    return out


def text_scores(E, keywords, model_name):
    import torch
    from transformers import AutoModel, AutoTokenizer
    dev = device()
    model = AutoModel.from_pretrained(model_name, dtype=torch.float32).to(dev).eval()
    tok = AutoTokenizer.from_pretrained(model_name)
    prompts = [f"a photo of {nodes(k)[-1]}." for k in keywords]
    T = []
    with torch.no_grad():
        for s in range(0, len(prompts), 256):
            t = tok(prompts[s:s + 256], padding="max_length", max_length=64,
                    truncation=True, return_tensors="pt").to(dev)
            T.append(torch.nn.functional.normalize(
                pooled(model.get_text_features(**t)), dim=-1))
        T = torch.cat(T)
        Et = torch.from_numpy(E).to(dev, torch.float32)
        logits = (Et @ T.T) * model.logit_scale.exp() + model.logit_bias
        return torch.sigmoid(logits).cpu().numpy()


def column_auc(scores, Y):
    from sklearn.metrics import roc_auc_score
    return np.array([roc_auc_score(Y[:, c], scores[:, c]) for c in range(Y.shape[1])])


def neg_fpr(scores, Y):
    """For each positive: the fraction of NEGATIVES scoring at least as high."""
    out = np.full(Y.shape, np.nan, dtype=np.float32)
    for c in range(Y.shape[1]):
        pos = Y[:, c] > 0
        neg = np.sort(scores[~pos, c])
        out[pos, c] = 1.0 - np.searchsorted(neg, scores[pos, c], side="left") / len(neg)
    return out


def pos_tpr(scores, Y):
    """For each negative: the fraction of POSITIVES scoring at or below it."""
    out = np.full(Y.shape, np.nan, dtype=np.float32)
    for c in range(Y.shape[1]):
        pos = Y[:, c] > 0
        p = np.sort(scores[pos, c])
        out[~pos, c] = np.searchsorted(p, scores[~pos, c], side="right") / len(p)
    return out


# ---------------------------------------------------------------- audit

def cmd_audit(args):
    name, _ = MODELS[args.model]
    rows_all = load_rows()
    E_all = np.load(os.path.join(OUT, f"emb-{args.model}.npy")).astype(np.float32)
    keys = json.load(open(os.path.join(OUT, f"emb-{args.model}.keys.json")))
    at = {k: i for i, k in enumerate(keys)}
    rows = [r for r in rows_all if r["pathkey"] in at]
    E = E_all[[at[r["pathkey"]] for r in rows]]
    print(f"{len(rows)} images")

    # Keyword columns: vocab nodes with enough carriers.
    vocab = load_vocab()
    counts = defaultdict(int)
    unfiled = set()
    for r in rows:
        for p in r["expanded"]:
            if fold(p) in vocab:
                counts[fold(p)] += 1
            else:
                unfiled.add(fold(p))
    cols = sorted(k for k, n in counts.items() if n >= args.min_pos)
    col = {k: c for c, k in enumerate(cols)}
    names = [vocab[k] for k in cols]
    print(f"{len(cols)} keywords with >= {args.min_pos} images "
          f"({len(counts)} in vocab, {len(unfiled)} unfiled skipped)")

    Y = np.zeros((len(rows), len(cols)), dtype=np.float32)
    for i, r in enumerate(rows):
        for p in r["expanded"]:
            c = col.get(fold(p))
            if c is not None:
                Y[i, c] = 1

    burst = find_bursts(rows, E, args.burst_dt, args.burst_cos)
    print(f"{burst.max() + 1} burst units")

    t = time.time()
    S_knn = knn_scores(E, Y, burst, args.k, args.tau)
    print(f"kNN {time.time() - t:.0f}s")
    t = time.time()
    if args.probe == "ridge":
        S_probe = ridge_scores(E, Y, burst, 5, args.ridge)
    else:
        S_probe = probe_scores(E, Y, burst, 5, args.epochs, args.wd)
    print(f"probe {time.time() - t:.0f}s")
    S_text = text_scores(E, names, name) if not args.no_text else None

    auc_knn, auc_probe = column_auc(S_knn, Y), column_auc(S_probe, Y)
    auc_text = column_auc(S_text, Y) if S_text is not None else None
    visual = (auc_knn >= args.min_auc) & (auc_probe >= args.min_auc)
    print(f"{visual.sum()} visual keywords (AUC >= {args.min_auc}), "
          f"{(~visual).sum()} skipped")

    fpr_k, fpr_p = neg_fpr(S_knn, Y), neg_fpr(S_probe, Y)
    tpr_k, tpr_p = pos_tpr(S_knn, Y), pos_tpr(S_probe, Y)

    # Tree helpers over the columns.
    parent_of = {k: fold("|".join(nodes(vocab[k])[:-1])) for k in cols}
    children = defaultdict(list)
    for k in cols:
        children[parent_of[k]].append(col[k])

    findings = []

    # SUSPECT: an explicitly carried leaf that looks less like its keyword than a
    # meaningful share of non-carriers. The alternative is the best-scoring sibling.
    for i, r in enumerate(rows):
        for p in r["explicit"]:
            c = col.get(fold(p))
            if c is None or not visual[c]:
                continue
            if fpr_k[i, c] < args.suspect_fpr or fpr_p[i, c] < args.suspect_fpr:
                continue
            alt, alt_tpr = None, 0.0
            for s in children[parent_of[cols[c]]]:
                if s != c and visual[s] and Y[i, s] == 0:
                    v = min(tpr_k[i, s], tpr_p[i, s])
                    if v > alt_tpr:
                        alt, alt_tpr = s, v
            findings.append({
                "i": i, "kind": "suspect", "c": c,
                "alt": alt if alt_tpr >= args.alt_tpr else None, "alt_tpr": alt_tpr,
                "score": (fpr_k[i, c] + fpr_p[i, c]) / 2,
            })

    # MISSING: a non-carrier that outscores most carriers -- deepest keyword only, so
    # "Fauna|Bird|Heron" is reported rather than also "Fauna|Bird".
    cand = defaultdict(list)
    vis_cols = np.where(visual)[0]
    hit = (np.nan_to_num(tpr_k[:, vis_cols]) >= args.missing_tpr) & \
          (np.nan_to_num(tpr_p[:, vis_cols]) >= args.missing_tpr)
    for i, j in zip(*np.nonzero(hit)):
        cand[i].append(vis_cols[j])
    for i, cs in cand.items():
        ks = {cols[c] for c in cs}
        for c in cs:
            if any(o.startswith(cols[c] + "|") for o in ks):
                continue
            sib = [names[s] for s in children[parent_of[cols[c]]] if Y[i, s] > 0]
            findings.append({
                "i": i, "kind": "missing", "c": c, "sibling": sib,
                "score": (tpr_k[i, c] + tpr_p[i, c]) / 2,
            })

    # One finding per burst unit.
    grouped = {}
    for f in findings:
        key = (burst[f["i"]], f["kind"], f["c"])
        g = grouped.setdefault(key, dict(f, members=[]))
        g["members"].append(f["i"])
        g["score"] = max(g["score"], f["score"])
    findings = sorted(grouped.values(), key=lambda f: -f["score"])
    print(f"{sum(f['kind'] == 'suspect' for f in findings)} suspect, "
          f"{sum(f['kind'] == 'missing' for f in findings)} missing (burst units)")

    fkeys = [f"{rows[f['i']]['pathkey']}|{f['kind']}|{cols[f['c']]}" for f in findings]
    json.dump(fkeys, open(os.path.join(OUT, f"findings-{args.model}-{args.probe}.json"),
                          "w"))
    write_report(args, rows, names, cols, Y, findings, S_knn, S_probe, S_text,
                 auc_knn, auc_probe, auc_text, visual)


# ---------------------------------------------------------------- report

def write_report(args, rows, names, cols, Y, findings, S_knn, S_probe, S_text,
                 auc_knn, auc_probe, auc_text, visual):
    db = connect_ro(INDEX_DB)

    def img(i):
        jpg = thumb_jpg(db, rows[i]["pathkey"])
        return "data:image/jpeg;base64," + base64.b64encode(jpg).decode() if jpg else ""

    shown = [f for f in findings if f["kind"] == "suspect"][:args.top] + \
            [f for f in findings if f["kind"] == "missing"][:args.top]
    shown.sort(key=lambda f: -f["score"])

    # Three exemplars per referenced keyword: its most confidently-scored carriers.
    exemplar = {}
    for f in shown:
        for c in (f["c"], f.get("alt")):
            if c is None or c in exemplar:
                continue
            pos = np.where(Y[:, c] > 0)[0]
            best = pos[np.argsort(-S_probe[pos, c])[:3]]
            exemplar[c] = [img(i) for i in best]

    def strip(c):
        return "".join(f'<img src="{s}">' for s in exemplar.get(c, []))

    cards = []
    for n, f in enumerate(shown):
        i, c = f["i"], f["c"]
        r = rows[i]
        fid = f"{r['pathkey']}|{f['kind']}|{cols[c]}"
        txt = f"{S_text[i, c]:.2f}" if S_text is not None else "-"
        if f["kind"] == "suspect":
            claim = f"Tagged <b>{html.escape(names[c])}</b> but doesn't look like it"
            if f["alt"] is not None:
                claim += (f"<br>Looks like <b>{html.escape(names[f['alt']])}</b> "
                          f"(beats {f['alt_tpr']:.0%} of its images)")
        else:
            claim = f"Not tagged but looks like <b>{html.escape(names[c])}</b>"
            if f["sibling"]:
                claim += "<br>Has sibling: " + html.escape(", ".join(f["sibling"]))
        burst = f" &middot; burst of {len(f['members'])}" if len(f["members"]) > 1 else ""
        alt_strip = (f'<div class="ex"><span>{html.escape(nodes(names[f["alt"]])[-1])}'
                     f'</span>{strip(f["alt"])}</div>') if f.get("alt") is not None else ""
        cards.append(f"""
<div class="card {f['kind']}" data-id="{html.escape(fid)}" data-kind="{f['kind']}"
     data-kw="{html.escape(names[c])}">
  <img class="main" src="{img(i)}">
  <div class="body">
    <div class="kind">{f['kind'].upper()} &middot; score {f['score']:.2f}{burst}</div>
    <div>{claim}</div>
    <div class="nums">kNN {S_knn[i, c]:.2f} &middot; probe {S_probe[i, c]:.2f}
      &middot; text {txt}</div>
    <div class="ex"><span>{html.escape(nodes(names[c])[-1])}</span>{strip(c)}</div>
    {alt_strip}
    <div class="path">{html.escape(r['path'])}</div>
    <div class="tags">{html.escape(', '.join(r['explicit']))}</div>
    <div class="verdict">
      <label><input type="radio" name="v{n}" value="mistake"> My mistake</label>
      <label><input type="radio" name="v{n}" value="correct"> Keywords are right</label>
      <label><input type="radio" name="v{n}" value="unsure"> Unsure</label>
    </div>
  </div>
</div>""")

    table = []
    order = np.argsort(-Y.sum(0))
    for c in order:
        at = f"{auc_text[c]:.2f}" if auc_text is not None else "-"
        ns = sum(1 for f in findings if f["c"] == c and f["kind"] == "suspect")
        nm = sum(1 for f in findings if f["c"] == c and f["kind"] == "missing")
        table.append(f"<tr class=\"{'' if visual[c] else 'skip'}\">"
                     f"<td>{html.escape(names[c])}</td><td>{int(Y[:, c].sum())}</td>"
                     f"<td>{auc_knn[c]:.2f}</td><td>{auc_probe[c]:.2f}</td><td>{at}</td>"
                     f"<td>{ns}</td><td>{nm}</td></tr>")

    params = html.escape(json.dumps({k: v for k, v in vars(args).items()
                                     if k != "func"}))
    page = TEMPLATE.replace("%PARAMS%", params) \
                   .replace("%CARDS%", "".join(cards)) \
                   .replace("%TABLE%", "".join(table)) \
                   .replace("%MODEL%", args.model)
    tag = "" if args.probe == "logistic" else "-" + args.probe
    path = os.path.join(OUT, f"report-{args.model}{tag}.html")
    open(path, "w").write(page)
    print(f"wrote {path}")


TEMPLATE = """<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Keyword Audit</title>
<style>
:root { --bg:#f6f6f4; --card:#fff; --fg:#222; --mute:#777; --line:#ddd;
        --sus:#c0392b; --mis:#2471a3; }
@media (prefers-color-scheme: dark) { :root { --bg:#1b1b1d; --card:#26262a;
        --fg:#e8e8e8; --mute:#999; --line:#3a3a3e; --sus:#e57368; --mis:#6fa8dc; } }
body { background:var(--bg); color:var(--fg); font:14px -apple-system, sans-serif;
       margin:0; padding:16px; }
header { position:sticky; top:0; background:var(--bg); padding:8px 0;
         border-bottom:1px solid var(--line); z-index:1; }
.stats { font-size:15px; margin:4px 0; }
.card { display:flex; gap:12px; background:var(--card); border:1px solid var(--line);
        border-left:4px solid var(--sus); border-radius:6px; padding:10px;
        margin:10px 0; }
.card.missing { border-left-color:var(--mis); }
.card.done { opacity:.55; }
.main { width:256px; height:auto; align-self:flex-start; border-radius:3px; }
.body { flex:1; min-width:0; }
.kind { font-size:12px; color:var(--mute); letter-spacing:.04em; }
.nums, .path, .tags { font-size:12px; color:var(--mute); overflow-wrap:anywhere; }
.ex { display:flex; align-items:center; gap:4px; margin:6px 0; }
.ex span { font-size:12px; color:var(--mute); width:110px; }
.ex img { height:56px; border-radius:2px; }
.verdict { margin-top:8px; display:flex; gap:14px; flex-wrap:wrap; }
table { border-collapse:collapse; font-size:12px; }
td, th { padding:2px 8px; border-bottom:1px solid var(--line); text-align:right; }
td:first-child, th:first-child { text-align:left; }
tr.skip { color:var(--mute); }
select, button { font:inherit; }
@media (max-width:640px) { .card { flex-direction:column; } .main { width:100%; } }
</style></head><body>
<header>
  <b>Keyword Audit &middot; %MODEL%</b>
  <div class="stats" id="stats"></div>
  <select id="kind"><option value="">All findings</option>
    <option value="suspect">Suspect only</option>
    <option value="missing">Missing only</option></select>
  <select id="kw"><option value="">All keywords</option></select>
  <label><input type="checkbox" id="hide"> Hide reviewed</label>
  <button id="export">Export verdicts</button>
</header>
<div id="cards">%CARDS%</div>
<details><summary>Per-keyword AUC (grey = skipped as not visual)</summary>
<table><tr><th>Keyword</th><th>Images</th><th>AUC kNN</th><th>AUC probe</th>
<th>AUC text</th><th>Suspect</th><th>Missing</th></tr>%TABLE%</table></details>
<details><summary>Parameters</summary><pre>%PARAMS%</pre></details>
<script>
const KEY = "kwaudit-%MODEL%";
let verdicts = {};
try { verdicts = JSON.parse(localStorage.getItem(KEY) || "{}"); } catch (e) {}
const cards = [...document.querySelectorAll(".card")];
const kwSel = document.getElementById("kw");
[...new Set(cards.map(c => c.dataset.kw))].sort().forEach(k => {
  const o = document.createElement("option"); o.value = o.textContent = k;
  kwSel.appendChild(o);
});
function save() { try { localStorage.setItem(KEY, JSON.stringify(verdicts)); }
                  catch (e) {} }
function render() {
  const kind = document.getElementById("kind").value, kw = kwSel.value;
  const hide = document.getElementById("hide").checked;
  const tally = { suspect:{mistake:0, correct:0, unsure:0},
                  missing:{mistake:0, correct:0, unsure:0} };
  cards.forEach(c => {
    const v = verdicts[c.dataset.id];
    if (v) tally[c.dataset.kind][v]++;
    c.classList.toggle("done", !!v);
    c.style.display = (kind && c.dataset.kind !== kind) || (kw && c.dataset.kw !== kw)
                      || (hide && v) ? "none" : "";
  });
  const pct = t => { const n = t.mistake + t.correct;
                     return n ? Math.round(100 * t.mistake / n) + "%" : "-"; };
  document.getElementById("stats").textContent =
    `Precision (mistakes / judged): suspect ${pct(tally.suspect)} ` +
    `(${tally.suspect.mistake}/${tally.suspect.mistake + tally.suspect.correct}), ` +
    `missing ${pct(tally.missing)} ` +
    `(${tally.missing.mistake}/${tally.missing.mistake + tally.missing.correct}) ` +
    `· ${cards.length} findings shown`;
}
cards.forEach(c => {
  const v = verdicts[c.dataset.id];
  c.querySelectorAll("input").forEach(inp => {
    if (inp.value === v) inp.checked = true;
    inp.addEventListener("change", () => { verdicts[c.dataset.id] = inp.value;
                                           save(); render(); });
  });
});
["kind", "kw", "hide"].forEach(id =>
  document.getElementById(id).addEventListener("change", render));
document.getElementById("export").addEventListener("click", () => {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([JSON.stringify(verdicts, null, 1)],
                                        {type:"application/json"}));
  a.download = "kwaudit-verdicts-%MODEL%.json"; a.click();
});
render();
</script></body></html>"""


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(required=True)

    e = sub.add_parser("embed", help="embed every keyworded thumbnail (resumable)")
    e.add_argument("--model", choices=MODELS, default="base")
    e.add_argument("--batch", type=int, default=64)
    e.add_argument("--limit", type=int, default=0, help="embed at most N (testing)")
    e.set_defaults(func=cmd_embed)

    a = sub.add_parser("audit", help="score keywords and write the HTML report")
    a.add_argument("--model", choices=MODELS, default="base")
    a.add_argument("--min-pos", type=int, default=8)
    a.add_argument("--min-auc", type=float, default=0.80)
    a.add_argument("--k", type=int, default=20)
    a.add_argument("--tau", type=float, default=0.05)
    a.add_argument("--epochs", type=int, default=300)
    a.add_argument("--wd", type=float, default=1e-4)
    a.add_argument("--probe", choices=["logistic", "ridge"], default="logistic")
    a.add_argument("--ridge", type=float, default=1.0, help="ridge lambda")
    a.add_argument("--burst-dt", type=int, default=60, help="seconds")
    a.add_argument("--burst-cos", type=float, default=0.95)
    a.add_argument("--suspect-fpr", type=float, default=0.10)
    a.add_argument("--missing-tpr", type=float, default=0.60)
    a.add_argument("--alt-tpr", type=float, default=0.25)
    a.add_argument("--top", type=int, default=400, help="cards per kind")
    a.add_argument("--no-text", action="store_true", help="skip zero-shot text")
    a.set_defaults(func=cmd_audit)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    sys.exit(main())
