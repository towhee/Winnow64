#ifndef WORKINGDUMP_H
#define WORKINGDUMP_H

#include <QFile>
#include <QString>
#include <QtGlobal>

#include "Develop/workingimage.h"

/*
    THE CALIBRATION DUMP (.wkg): a WorkingImage written as raw float32, so a tone operator
    can be fitted against Lightroom without an export per try. What is dumped is the image
    a Develop export develops, converted to the working space WITH its white balance
    (stage 0) but before exposure and every edit -- unclipped scene-linear data, which no
    8/16-bit export can carry past white.

    Three writers/readers share this one definition, so they cannot drift:
        dumpWorkingForCalibration (Main/mainwindow.cpp)  a Develop export, when
                                                         WINNOW_DUMP_WORKING is a folder
        Winnow --dumpworking <raw> <out.wkg> (main.cpp)  headless, no window or catalog
        developrender (tests/unit/developrender.cpp)     renders a dump through
                                                         Develop::Apply with any recipe

    Format, little-endian: "WNWK", int32 version (1), int32 width, int32 height, float32
    white, int32 sceneReferred, int32 orientation in degrees (the rotation the loupe
    applies), then width * height * 3 float32 RGB.
*/
namespace WorkingDump {

inline bool write(const WorkingImage &w, int degrees, const QString &path)
{
    if (!w.isValid()) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const qint32 hdr[3] = {1, w.width, w.height};
    const float white = w.white;
    const qint32 tail[2] = {w.sceneReferred ? 1 : 0, degrees};
    const qint64 bytes = qint64(w.rgb.size() * sizeof(float));
    return f.write("WNWK", 4) == 4 &&
           f.write(reinterpret_cast<const char *>(hdr), sizeof(hdr)) == sizeof(hdr) &&
           f.write(reinterpret_cast<const char *>(&white), sizeof(white)) == sizeof(white) &&
           f.write(reinterpret_cast<const char *>(tail), sizeof(tail)) == sizeof(tail) &&
           f.write(reinterpret_cast<const char *>(w.rgb.data()), bytes) == bytes;
}

/* Reads into w (working space, white and sceneReferred from the header); degrees gets the
   orientation. False for a missing, foreign or truncated file. */
inline bool read(const QString &path, WorkingImage &w, int &degrees)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.read(4) != "WNWK") return false;
    qint32 hdr[3];
    float white = 1.0f;
    qint32 tail[2];
    if (f.read(reinterpret_cast<char *>(hdr), sizeof(hdr)) != sizeof(hdr) ||
        f.read(reinterpret_cast<char *>(&white), sizeof(white)) != sizeof(white) ||
        f.read(reinterpret_cast<char *>(tail), sizeof(tail)) != sizeof(tail))
        return false;
    if (hdr[0] != 1 || hdr[1] <= 0 || hdr[2] <= 0) return false;
    w = WorkingImage();
    w.width = hdr[1];
    w.height = hdr[2];
    w.white = white;
    w.sceneReferred = tail[0] != 0;
    w.space = ColorSpaceMath::kWorking;
    w.rgb.resize(size_t(w.width) * size_t(w.height) * 3);
    const qint64 bytes = qint64(w.rgb.size() * sizeof(float));
    if (f.read(reinterpret_cast<char *>(w.rgb.data()), bytes) != bytes) return false;
    degrees = tail[1];
    return true;
}

} // namespace WorkingDump

#endif // WORKINGDUMP_H
