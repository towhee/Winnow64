#include "rawbase.h"

RawBase::RawBase(QString srcPath, QObject* parent)
    : QObject(parent), fPath(std::move(srcPath)) {}

RawBase::~RawBase() { unloadContainer(); }

bool RawBase::parse(MetadataParameters &p,
                    ImageMetadata &m,
                    IFD *ifd,
                    IRB *irb,
                    IPTC *iptc,
                    Xmp *xmp,
                    GPS *gps)
{
    Q_UNUSED(irb); Q_UNUSED(iptc); Q_UNUSED(xmp); Q_UNUSED(gps);
    if (!loadContainer(p)) return false;
    const bool ok = readIFDsAndTags(p, ifd);
    unloadContainer();
    if (!ok) return false;

    // Minimal fields mirrored to ImageMetadata for consistency
    m.width = rawW; m.height = rawH; /*m.bps = bps*/; // adjust to your struct
    return true;
}

bool RawBase::parse(ImageMetadata &m, QString path)
{
    fPath = std::move(path);
    MetadataParameters p; // minimal; supply defaults as needed
    IFD dummyIfd;         // caller often passes one anyway
    return parse(p, m, &dummyIfd, nullptr, nullptr, nullptr, nullptr);
}

bool RawBase::parseForDecoding(MetadataParameters &p, IFD *ifd)
{
    if (!loadContainer(p)) return false;
    const bool ok = readIFDsAndTags(p, ifd);
    // keep container open for decode stage that may follow
    return ok;
}

bool RawBase::decode(MetadataParameters &p, QImage &image, int newSize)
{
    Q_UNUSED(newSize);
    RawPlaneView src;
    if (!mapData && !loadContainer(p)) return false;
    if (!readIFDsAndTags(p, nullptr)) return false; // allow quick path

    // Try fast-path: embedded preview for thumbnails
    if (tryExtractEmbeddedPreview(p, image, newSize)) {
        unloadContainer();
        return true;
    }

    if (!extractRawPlane(p, src)) { unloadContainer(); return false; }

    QVector<float> r, g, b;
    if (!unpackAndLinearize(src, r, g, b)) { unloadContainer(); return false; }
    if (!demosaic(src, r, g, b))          { unloadContainer(); return false; }
    const bool ok = colorCorrect(r, g, b, image, /*to16bit*/true);
    unloadContainer();
    return ok;
}

bool RawBase::decode(ImageMetadata &m, QString &path, QImage &image,
                     bool thumb, int maxDim)
{
    Q_UNUSED(m); Q_UNUSED(thumb);
    fPath = path;
    MetadataParameters p;
    if (maxDim > 0 && tryExtractEmbeddedPreview(p, image, maxDim)) return true;
    return decode(p, image, /*newSize*/maxDim);
}

bool RawBase::decode(QString path, quint32 /*offset*/, QImage &image)
{
    fPath = path; MetadataParameters p; return decode(p, image, 0);
}

bool RawBase::unloadContainer()
{
    if (mapData) {
        // QFile::unmap is called where mapping happened; keep here if needed
        mapData = nullptr; mapSize = 0;
    }
    if (file.isOpen()) file.close();
    return true;
}

bool RawBase::tryExtractEmbeddedPreview(MetadataParameters &p, QImage &image, int maxDim)
{
    Q_UNUSED(p); Q_UNUSED(image); Q_UNUSED(maxDim);
    // Default: not available. Subclasses override to read SubIFD/preview strips or IRB JPEG.
    return false;
}

bool RawBase::unpackAndLinearize(const RawPlaneView &src,
                                 QVector<float> &r, QVector<float> &g, QVector<float> &b)
{
    // Simple nearest unpack into per-channel planes (Bayer RGGB assumed unless subclass provides cfa)
    const int W = src.width, H = src.height;
    r.resize(W*H); g.resize(W*H); b.resize(W*H);

    auto at = [&](int x, int y)->float {
        if (src.dataU16) return src.dataU16[(size_t)y*(src.strideBytes/2) + x] / float((1<<src.bitsPerSample)-1);
        if (src.dataU8)  return src.dataU8 [(size_t)y* src.strideBytes      + x] / 255.f;
        return 0.f;
    };

    const CFAPattern pat = src.cfa == CFAPattern::Unknown ? CFAPattern::RGGB : src.cfa;
    for (int y=0; y<H; ++y) {
        for (int x=0; x<W; ++x) {
            const bool rpos = ((y&1)==0) && ((x&1)==0);
            const bool bpos = ((y&1)==1) && ((x&1)==1);
            const float v = at(x,y);
            switch (pat) {
            case CFAPattern::RGGB:  if (rpos) r[y*W+x]=v; else if (bpos) b[y*W+x]=v; else g[y*W+x]=v; break;
            case CFAPattern::BGGR:  if (rpos) b[y*W+x]=v; else if (bpos) r[y*W+x]=v; else g[y*W+x]=v; break;
            case CFAPattern::GRBG:  if (((y&1)==0 && (x&1)==1)) r[y*W+x]=v; else if (((y&1)==1 && (x&1)==0)) b[y*W+x]=v; else g[y*W+x]=v; break;
            case CFAPattern::GBRG:  if (((y&1)==0 && (x&1)==1)) b[y*W+x]=v; else if (((y&1)==1 && (x&1)==0)) r[y*W+x]=v; else g[y*W+x]=v; break;
            default: g[y*W+x]=v; break;
            }
        }
    }
    return true;
}

bool RawBase::demosaic(const RawPlaneView &src,
                       QVector<float> &r, QVector<float> &g, QVector<float> &b)
{
    Q_UNUSED(src);
    // Very simple bilinear demosaic to keep skeleton runnable (replace later)
    const int W = rawW, H = rawH;
    auto clamp = [&](int v,int a,int b){return v<a?a:(v>b?b:v);} ;
    auto interp = [&](QVector<float>& ch){
        QVector<float> out = ch; // in-place bilinear fill zeros
        for (int y=0;y<H;++y){
            for(int x=0;x<W;++x){
                float &v = out[y*W+x];
                if (v>0.f) continue;
                float s=0; int n=0;
                for (int dy=-1; dy<=1; ++dy)
                    for (int dx=-1; dx<=1; ++dx){ if (!dx && !dy) continue; int xx=clamp(x+dx,0,W-1), yy=clamp(y+dy,0,H-1); s += ch[yy*W+xx]; ++n; }
                v = n? s/float(n) : 0.f;
            }
        }
        ch.swap(out);
    };
    interp(r); interp(g); interp(b);
    return true;
}

bool RawBase::colorCorrect(const QVector<float> &r,
                           const QVector<float> &g,
                           const QVector<float> &b,
                           QImage &out, bool to16bit)
{
    const int W = rawW, H = rawH;
    if (to16bit) {
        out = QImage(W, H, QImage::Format_RGBA64);
        for (int y=0; y<H; ++y){
            auto *row = reinterpret_cast<quint16*>(out.scanLine(y));
            for (int x=0; x<W; ++x){
                const int i=y*W+x;
                float R=r[i], G=g[i], B=b[i];
                // Apply simple identity matrix (replace with camToRGB)
                float RR = camToRGB.m[0]*R + camToRGB.m[1]*G + camToRGB.m[2]*B;
                float GG = camToRGB.m[3]*R + camToRGB.m[4]*G + camToRGB.m[5]*B;
                float BB = camToRGB.m[6]*R + camToRGB.m[7]*G + camToRGB.m[8]*B;
                quint16 r16 = qBound<quint16>(0, quint16(RR*65535.f+0.5f), 65535);
                quint16 g16 = qBound<quint16>(0, quint16(GG*65535.f+0.5f), 65535);
                quint16 b16 = qBound<quint16>(0, quint16(BB*65535.f+0.5f), 65535);
                row[4*x+0]=r16; row[4*x+1]=g16; row[4*x+2]=b16; row[4*x+3]=65535;
            }
        }
    } else {
        out = QImage(W, H, QImage::Format_RGBA8888);
        for (int y=0; y<H; ++y){
            auto *row = reinterpret_cast<quint8*>(out.scanLine(y));
            for (int x=0; x<W; ++x){
                const int i=y*W+x;
                float RR = camToRGB.m[0]*r[i] + camToRGB.m[1]*g[i] + camToRGB.m[2]*b[i];
                float GG = camToRGB.m[3]*r[i] + camToRGB.m[4]*g[i] + camToRGB.m[5]*b[i];
                float BB = camToRGB.m[6]*r[i] + camToRGB.m[7]*g[i] + camToRGB.m[8]*b[i];
                row[4*x+0]=quint8(qBound(0, int(RR*255.f+0.5f), 255));
                row[4*x+1]=quint8(qBound(0, int(GG*255.f+0.5f), 255));
                row[4*x+2]=quint8(qBound(0, int(BB*255.f+0.5f), 255));
                row[4*x+3]=255;
            }
        }
    }
    return true;
}

CFAPattern RawBase::cfaFromTag(quint32, const QByteArray &pattern)
{
    // Very small helper for DNG CFARepeatPatternDim/CFAPlaneColor/CFAPattern
    if (pattern.size()>=4) {
        // Try to infer common Bayer layouts from first 4 bytes
        const uchar *p = reinterpret_cast<const uchar*>(pattern.constData());
        // Many DNGs encode pattern as indices 0=Red,1=Green,2=Blue
        if (p[0]==0 && p[1]==1 && p[2]==1 && p[3]==2) return CFAPattern::RGGB;
        if (p[0]==2 && p[1]==1 && p[2]==1 && p[3]==0) return CFAPattern::BGGR;
        if (p[0]==1 && p[1]==0 && p[2]==2 && p[3]==1) return CFAPattern::GRBG;
        if (p[0]==1 && p[1]==2 && p[2]==0 && p[3]==1) return CFAPattern::GBRG;
    }
    return CFAPattern::Unknown;
}

void RawBase::endianSwap16(void* data, int elements)
{
    auto *p = reinterpret_cast<quint16*>(data);
    for (int i=0;i<elements;++i) p[i] = qbswap(p[i]);
}
