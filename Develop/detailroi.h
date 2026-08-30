#ifndef DETAILROI_H
#define DETAILROI_H

#include <algorithm>

/*
    Geometry for the Detail panel's 1:1 preview -- the small window that shows a patch of
    the image at ONE screen pixel per image pixel, so capture sharpening and noise
    reduction can be judged without zooming the loupe to 100%.

    Pure integer math, no Qt and no OpenCV, so it is header-only and unit-testable in
    isolation (the same split as sharpen.h / maskfalloff.h). The render itself lives in
    MW::renderDetailRoi.

    WHY THIS EXISTS AT ALL. Sharpening is the one Develop op whose radius is in ABSOLUTE
    pixels (see Develop/sharpen.h), so it is the one op the interactive proxy cannot show:
    the proxy is ~1/3 scale, effectiveSigma() collapses to the kMinSigma floor, and the
    slider moves acutance by a few percent instead of the ~70% it moves at full
    resolution. The preview renders a small ROI of the FULL-RESOLUTION image instead --
    renderScale 1.0, a few hundred pixels square -- which is both honest and cheap.

    THE THREE SPACES, and why the mapping is not a no-op:

      source    the WorkingImage as decoded: w x h, NOT rotated. This is what the render
                reads, so an ROI must end up here.
      oriented  the source turned by the EXIF rotation (degrees): fw x fh, with the axes
                swapped at 90 / 270. This is the frame masks are normalized over
                (ImageView::developGeomSrc) and therefore the frame the picked point
                arrives in.
      displayed what is on screen, i.e. oriented THEN cropped / warped by the geometry
                stage. ImageView::maskItemToNorm already brings a click back from here to
                a normalized point in the oriented frame, so this header never sees it.

    The ROI is chosen in the ORIENTED frame (that is where the user pointed) and mapped
    back to SOURCE pixels for the crop. Geometry (crop / warp) is deliberately NOT applied
    to the preview: crop does not resample, so 1:1 pixels are identical either way, and
    reproducing a warp for a 300px patch would cost more than it tells the user. The
    preview therefore shows the sharpening of the UNWARPED pixels, which is what the
    sliders act on.
*/
namespace DetailRoi {

/* A rectangle in whichever space the caller is working in. Kept as a plain struct rather
   than QRect so this header stays Qt-free; MW converts at the boundary. */
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool isEmpty() const { return w <= 0 || h <= 0; }
    bool operator==(const Rect &o) const
    { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

/* The preview window's side, in full-resolution pixels. The widget is square and this is
   what it can show at 1:1; a larger patch would only be shrunk to fit, which would defeat
   the entire point of the preview. */
constexpr int kMinSize = 32;
constexpr int kMaxSize = 1024;

/* Oriented frame size for a source of w x h turned by `degrees` (0/90/180/270). */
inline void orientedSize(int w, int h, int degrees, int &fw, int &fh)
{
    if (degrees == 90 || degrees == 270) { fw = h; fh = w; }
    else                                 { fw = w; fh = h; }
}

/* The ROI, in ORIENTED pixels, centred on a normalized point.

   nx / ny are the picked point (0..1 over the oriented frame). The rect is clamped to
   the frame rather than centred blindly: a point near an edge yields a full-size window
   flush with that edge, which is what the user expects from a magnifier -- an ROI that
   shrank at the edges would change the amount of image shown as they moved the point.
   A frame smaller than the requested size simply yields the whole frame. */
inline Rect orientedRoi(double nx, double ny, int fw, int fh, int size)
{
    Rect r;
    if (fw <= 0 || fh <= 0) return r;
    const int s = std::clamp(size, kMinSize, kMaxSize);
    r.w = std::min(s, fw);
    r.h = std::min(s, fh);
    const double cx = std::clamp(nx, 0.0, 1.0) * fw;
    const double cy = std::clamp(ny, 0.0, 1.0) * fh;
    r.x = static_cast<int>(cx) - r.w / 2;
    r.y = static_cast<int>(cy) - r.h / 2;
    r.x = std::clamp(r.x, 0, fw - r.w);
    r.y = std::clamp(r.y, 0, fh - r.h);
    return r;
}

/* Map an ORIENTED rect back to SOURCE pixels, undoing the EXIF rotation.

   Forward (source -> oriented), for a source pixel (x, y) of a w x h image:
      90 : (ox, oy) = (h - 1 - y, x)          fw x fh = h x w
      180: (ox, oy) = (w - 1 - x, h - 1 - y)  fw x fh = w x h
      270: (ox, oy) = (y, w - 1 - x)          fw x fh = h x w

   Inverting that for a RECT (not a point) is where the sign errors live: at 90 the
   oriented rect's LEFT edge comes from the source's BOTTOM edge, so the origin is
   computed from the far corner (x + w), not from x. The axes also swap, so the returned
   rect's w/h are exchanged at 90 / 270. */
inline Rect orientedRectToSource(const Rect &r, int w, int h, int degrees)
{
    Rect s;
    if (r.isEmpty() || w <= 0 || h <= 0) return s;
    switch (((degrees % 360) + 360) % 360) {
    case 90:
        /* ox = h - 1 - y  ->  y = h - ox - rw ;  oy = x  ->  x = oy */
        s.x = r.y;
        s.y = h - r.x - r.w;
        s.w = r.h;
        s.h = r.w;
        break;
    case 180:
        s.x = w - r.x - r.w;
        s.y = h - r.y - r.h;
        s.w = r.w;
        s.h = r.h;
        break;
    case 270:
        /* ox = y  ->  y = ox ;  oy = w - 1 - x  ->  x = w - oy - rh */
        s.x = w - r.y - r.h;
        s.y = r.x;
        s.w = r.h;
        s.h = r.w;
        break;
    default:
        s = r;
        break;
    }
    /* Clamp into the source. The rotation above is exact, so this only ever trims a rect
       the caller built against a mismatched frame size -- but an out-of-range crop is a
       read past the end of the pixel buffer, so it is checked rather than trusted. */
    s.x = std::clamp(s.x, 0, std::max(0, w - 1));
    s.y = std::clamp(s.y, 0, std::max(0, h - 1));
    s.w = std::clamp(s.w, 0, w - s.x);
    s.h = std::clamp(s.h, 0, h - s.y);
    return s;
}

/* Convenience: oriented point -> source rect, the whole mapping in one call. */
inline Rect sourceRoi(double nx, double ny, int w, int h, int degrees, int size)
{
    int fw = 0, fh = 0;
    orientedSize(w, h, degrees, fw, fh);
    return orientedRectToSource(orientedRoi(nx, ny, fw, fh, size), w, h, degrees);
}

/* Where a source-space ROI lands in a PROXY of the same image, for sampling a mask that
   was rasterized at proxy resolution. Returns the proxy-pixel origin and step per source
   pixel; the caller walks the ROI and samples. Kept here so the scale factor is derived
   in one place rather than at each call site. */
inline double proxyScale(int proxyW, int fullW)
{
    if (fullW <= 0 || proxyW <= 0) return 1.0;
    return double(proxyW) / double(fullW);
}

} // namespace DetailRoi

#endif // DETAILROI_H
