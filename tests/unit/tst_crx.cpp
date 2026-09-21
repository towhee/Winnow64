/*
    Canon CRX decode -- both bitstreams, pinned by content against the libraw oracle.

    WHY THIS TEST EXISTS. Canon writes two different CRX bitstreams behind the same .cr3
    extension. "RAW" is level-0 and is decoded clean-room in crx.cpp; "C-RAW" wavelet-
    transforms each plane first (levels 1-3) and is decoded by the LibRaw port in
    crxcodec.cpp. C-RAW is the DEFAULT on current bodies, so it is the case that matters
    most -- and before crxcodec.cpp landed it did not decode at all: UnpackCfa returned
    false, ImageDecoder silently fell back to the embedded JPEG, and because that preview
    is tagged kWorking rather than CameraNative, every camera-profile and raw-only control
    in Develop quietly did nothing on those files. A SILENT fallback is exactly the kind of
    regression nobody notices, so the decode is pinned by CONTENT, not by "it returned
    true": a decoder that comes back with a plausible-looking but wrong mosaic fails here.

    WHAT IS PINNED. A CRC32 over the full sensor mosaic for one file of each kind, plus its
    geometry and bit depth. The expected values were NOT taken from this decoder -- they
    were derived independently from the rawpy/libraw oracle, so the pin cross-checks the
    port rather than merely recording whatever it happens to emit. (The port was validated
    bit-exact against that oracle on 43 EOS R5 files, 41 C-RAW and 2 RAW; these two are the
    committed representatives of that sweep.)

    WHAT IS NOT COVERED HERE, and why. The test drives CrxCodec::Decode, not
    CanonCR3Raw::UnpackCfa: UnpackCfa's base class lives in rawformat.cpp, whose link
    closure is the whole raw pipeline (Develop, OpenCV, PMRID, every vendor decoder's
    vtable), which is far more than a unit test should drag in -- see the note on keeping
    WINNOW_CORE_TEST_SOURCES bounded in tests/CMakeLists.txt. So the ENTROPY/WAVELET CODEC
    is pinned here; the active-area crop, black calibration and white balance that
    UnpackCfa layers on top are not. The container walk below is the test's OWN, written
    independently of the one in crx.cpp -- if the two ever disagree about which track or
    which CMP1 payload is the full-raw one, this test stops finding the stream.

    RUNNING IT. The sample files are 28-62 MB, far too large to commit, so the test is
    SKIPPED unless WINNOW_TEST_CR3_DIR points at a directory holding them:

        WINNOW_TEST_CR3_DIR=~/Pictures/CR3 ctest --test-dir build/mac-debug -R tst_crx

    Any .cr3 in that directory is decoded and sanity-checked; the pinned files are
    additionally checksummed if present. To add a body, decode one of its files with the
    oracle and append a row to kPinned.
*/

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstring>
#include <functional>
#include <vector>

#include "ImageFormats/Canon/crxcodec.h"

namespace {

/* ---- the test's own ISO-BMFF walk (deliberately independent of crx.cpp's) ------------- */

using Bytes = std::vector<uint8_t>;

uint16_t be16(const uint8_t *b) { return uint16_t((uint16_t(b[0]) << 8) | b[1]); }
uint32_t be32(const uint8_t *b) {
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
}
uint64_t be64(const uint8_t *b) { return (uint64_t(be32(b)) << 32) | be32(b + 4); }

void forEachBox(const Bytes &d, size_t begin, size_t end,
                const std::function<void(const char *, size_t, size_t)> &fn)
{
    size_t o = begin;
    while (o + 8 <= end) {
        uint64_t size = be32(&d[o]);
        size_t hdr = 8;
        if (size == 1) { if (o + 16 > end) break; size = be64(&d[o + 8]); hdr = 16; }
        else if (size == 0) size = end - o;
        if (size < hdr || o + size > end) break;
        fn(reinterpret_cast<const char *>(&d[o + 4]), o + hdr, o + size);
        o += size;
    }
}

/* The full-resolution CRAW track: its CMP1 payload and the extent of its CRX bitstream. */
struct Track {
    bool ok = false;
    size_t cmp1 = 0, cmp1End = 0;
    uint64_t mdat = 0, len = 0;
    uint32_t fW = 0, fH = 0;
};

Track findCrawTrack(const Bytes &d)
{
    Track best;
    forEachBox(d, 0, d.size(), [&](const char *ty, size_t b, size_t e) {
        if (std::memcmp(ty, "moov", 4)) return;
        forEachBox(d, b, e, [&](const char *t2, size_t tb, size_t te) {
            if (std::memcmp(t2, "trak", 4)) return;
            Track t;
            uint32_t tileW = 0, tileH = 0;
            forEachBox(d, tb, te, [&](const char *a, size_t ab, size_t ae) {
              if (std::memcmp(a, "mdia", 4)) return;
              forEachBox(d, ab, ae, [&](const char *bb, size_t bbb, size_t bbe) {
                if (std::memcmp(bb, "minf", 4)) return;
                forEachBox(d, bbb, bbe, [&](const char *c, size_t cb, size_t ce) {
                  if (std::memcmp(c, "stbl", 4)) return;
                  forEachBox(d, cb, ce, [&](const char *g, size_t gb, size_t ge) {
                    if (!std::memcmp(g, "stsd", 4)) {
                      forEachBox(d, gb + 8, ge, [&](const char *s, size_t sb, size_t se) {
                        if (std::memcmp(s, "CRAW", 4)) return;
                        for (size_t o = sb; o + 8 <= se; ++o)
                          if (!std::memcmp(&d[o], "CMP1", 4)) {
                            const size_t p = o + 4;
                            t.cmp1 = p; t.cmp1End = se;
                            t.fW = be32(&d[p + 8]);  t.fH = be32(&d[p + 12]);
                            tileW = be32(&d[p + 16]); tileH = be32(&d[p + 20]);
                          }
                      });
                    }
                    else if (!std::memcmp(g, "co64", 4)) { if (be32(&d[gb + 4]) >= 1) t.mdat = be64(&d[gb + 8]); }
                    else if (!std::memcmp(g, "stco", 4)) { if (be32(&d[gb + 4]) >= 1) t.mdat = be32(&d[gb + 8]); }
                    else if (!std::memcmp(g, "stsz", 4)) {
                        const uint32_t ss = be32(&d[gb + 4]);
                        t.len = ss ? ss : (be32(&d[gb + 8]) >= 1 ? be32(&d[gb + 12]) : 0);
                    }
                  });
                });
              });
            });
            /* Single-tile full-raw track only, largest wins -- the same rule crx.cpp uses. */
            if (t.cmp1 && t.fW && tileW == t.fW && tileH == t.fH)
                if (!best.ok || uint64_t(t.fW) * t.fH > uint64_t(best.fW) * best.fH) {
                    t.ok = true; best = t;
                }
        });
    });
    return best;
}

/* ---- pinned expectations (from the rawpy/libraw oracle) ------------------------------- */

struct Pinned {
    const char *file;
    int width, height, bits;
    quint32 crc;        // zlib CRC32 over the mosaic as uint16 little-endian, row-major
};

/* EOS R5. 0064 is C-RAW (CMP1 imageLevels 3), 0120 is lossless RAW (level 0) -- one of
   each bitstream, so a regression in either path fails here. */
const Pinned kPinned[] = {
    {"2021-01-28_0064.cr3", 8352, 5586, 14, 0x4d4765d0u},
    {"2020-08-25_0120.cr3", 8352, 5586, 14, 0xd0c36beeu},
};

const Pinned *pinnedFor(const QString &name)
{
    for (const Pinned &p : kPinned)
        if (name.compare(QString::fromLatin1(p.file), Qt::CaseInsensitive) == 0) return &p;
    return nullptr;
}

/* zlib-compatible CRC32 -- the oracle's checksums come from Python's zlib.crc32. */
quint32 crc32(const uchar *data, qsizetype len)
{
    static quint32 table[256];
    static bool built = false;
    if (!built) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (qsizetype i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

bool decodeFile(const QString &path, std::vector<uint16_t> &out, int &w, int &h, int &bits,
                QString &why)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { why = "cannot open"; return false; }
    const QByteArray all = f.readAll();
    if (all.size() < 16) { why = "too small"; return false; }
    Bytes d(reinterpret_cast<const uint8_t *>(all.constData()),
            reinterpret_cast<const uint8_t *>(all.constData()) + all.size());

    const Track t = findCrawTrack(d);
    if (!t.ok) { why = "no single-tile full-raw CRAW track"; return false; }

    const char *err = "";
    if (!CrxCodec::Decode(d.data(), qint64(d.size()), &d[t.cmp1], qint64(t.cmp1End - t.cmp1),
                          qint64(t.mdat), qint64(t.len), out, w, h, bits, &err)) {
        why = QString::fromLatin1(err);
        return false;
    }
    return true;
}

} // namespace

class TestCrx : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void everySampleDecodesToAPlausibleMosaic();
    void pinnedFilesMatchTheOracleChecksum();

private:
    QStringList files;
};

void TestCrx::initTestCase()
{
    QString dir = qEnvironmentVariable("WINNOW_TEST_CR3_DIR");
    if (dir.isEmpty()) return;
    if (dir.startsWith('~')) dir.replace(0, 1, QDir::homePath());
    QDir d(dir);
    if (!d.exists()) return;
    const QStringList names = d.entryList({"*.cr3", "*.CR3"}, QDir::Files, QDir::Name);
    for (const QString &n : names) files << d.filePath(n);
}

void TestCrx::everySampleDecodesToAPlausibleMosaic()
{
    if (files.isEmpty())
        QSKIP("set WINNOW_TEST_CR3_DIR to a directory of .cr3 files to run this");

    /* Cap the sweep: a folder of several hundred raws is a soak test, not a unit test.
       The pinned cases below are checked whatever the cap drops. */
    const int limit = int(qMin(files.size(), qsizetype(8)));
    for (int i = 0; i < limit; ++i) {
        const QString path = files.at(i);
        const QString name = QFileInfo(path).fileName();

        std::vector<uint16_t> mosaic; int w = 0, h = 0, bits = 0; QString why;
        QVERIFY2(decodeFile(path, mosaic, w, h, bits, why),
                 qPrintable(QString("%1: %2").arg(name, why)));

        QCOMPARE(mosaic.size(), size_t(w) * size_t(h));
        QVERIFY(w > 0 && h > 0);
        QVERIFY(bits > 8 && bits <= 16);

        /* A decode that "succeeds" into a flat or out-of-range mosaic is the failure mode
           a bare bool would hide. */
        const auto mm = std::minmax_element(mosaic.begin(), mosaic.end());
        QVERIFY2(*mm.first != *mm.second,
                 qPrintable(name + ": mosaic is a constant -- decode produced no image"));
        QVERIFY2(*mm.second < (1u << bits),
                 qPrintable(name + ": sample exceeds the declared bit depth"));
    }
}

void TestCrx::pinnedFilesMatchTheOracleChecksum()
{
    if (files.isEmpty())
        QSKIP("set WINNOW_TEST_CR3_DIR to a directory of .cr3 files to run this");

    int checked = 0;
    for (const QString &path : std::as_const(files)) {
        const Pinned *pin = pinnedFor(QFileInfo(path).fileName());
        if (!pin) continue;

        std::vector<uint16_t> mosaic; int w = 0, h = 0, bits = 0; QString why;
        QVERIFY2(decodeFile(path, mosaic, w, h, bits, why),
                 qPrintable(QString("%1: %2").arg(pin->file, why)));

        QCOMPARE(w, pin->width);
        QCOMPARE(h, pin->height);
        QCOMPARE(bits, pin->bits);

        const quint32 got = crc32(reinterpret_cast<const uchar *>(mosaic.data()),
                                  qsizetype(mosaic.size() * sizeof(uint16_t)));
        QVERIFY2(got == pin->crc,
                 qPrintable(QString("%1: mosaic CRC32 0x%2, oracle says 0x%3")
                            .arg(QString::fromLatin1(pin->file))
                            .arg(got, 8, 16, QChar('0'))
                            .arg(pin->crc, 8, 16, QChar('0'))));
        ++checked;
    }

    if (!checked)
        QSKIP("none of the pinned sample files are in WINNOW_TEST_CR3_DIR");
}

QTEST_MAIN(TestCrx)
#include "tst_crx.moc"
