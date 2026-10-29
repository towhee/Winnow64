#include "ImageFormats/Dng/dngjxl.h"

#ifdef WINNOW_HAVE_JXL
#include <jxl/decode.h>
#endif

namespace DngJxl {

bool Available()
{
#ifdef WINNOW_HAVE_JXL
    return true;
#else
    return false;
#endif
}

bool Decode(const uint8_t *data, size_t size, int channels,
            std::vector<float> &out, int &width, int &height, QString *err)
{
    width = height = 0;
    out.clear();
#ifdef WINNOW_HAVE_JXL
    auto fail = [err](const QString &msg) {
        if (err) *err = msg;
        return false;
    };
    if (!data || !size) return fail("empty codestream");
    if (channels != 1 && channels != 3) return fail("unsupported channel count");

    JxlDecoder *dec = JxlDecoderCreate(nullptr);
    if (!dec) return fail("JxlDecoderCreate failed");
    /* Owns dec on every return path. */
    struct Guard { JxlDecoder *d; ~Guard() { JxlDecoderDestroy(d); } } guard{dec};

    if (JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE)
        != JXL_DEC_SUCCESS)
        return fail("JxlDecoderSubscribeEvents failed");
    if (JxlDecoderSetInput(dec, data, size) != JXL_DEC_SUCCESS)
        return fail("JxlDecoderSetInput failed");
    JxlDecoderCloseInput(dec);

    const JxlPixelFormat fmt { uint32_t(channels), JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0 };
    for (;;) {
        const JxlDecoderStatus st = JxlDecoderProcessInput(dec);
        if (st == JXL_DEC_BASIC_INFO) {
            JxlBasicInfo info;
            if (JxlDecoderGetBasicInfo(dec, &info) != JXL_DEC_SUCCESS)
                return fail("JxlDecoderGetBasicInfo failed");
            if (int(info.num_color_channels) != channels)
                return fail(QString("codestream has %1 colour channels, expected %2")
                            .arg(info.num_color_channels).arg(channels));
            width = int(info.xsize);
            height = int(info.ysize);
        }
        else if (st == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t bytes = 0;
            if (JxlDecoderImageOutBufferSize(dec, &fmt, &bytes) != JXL_DEC_SUCCESS)
                return fail("JxlDecoderImageOutBufferSize failed");
            out.resize(bytes / sizeof(float));
            if (JxlDecoderSetImageOutBuffer(dec, &fmt, out.data(), bytes) != JXL_DEC_SUCCESS)
                return fail("JxlDecoderSetImageOutBuffer failed");
        }
        else if (st == JXL_DEC_FULL_IMAGE) {
            continue;
        }
        else if (st == JXL_DEC_SUCCESS) {
            break;
        }
        else {
            return fail("JPEG XL decode error");
        }
    }
    if (width <= 0 || height <= 0
        || out.size() != size_t(width) * size_t(height) * size_t(channels))
        return fail("JPEG XL decode produced no image");
    return true;
#else
    Q_UNUSED(data) Q_UNUSED(size) Q_UNUSED(channels)
    if (err) *err = "built without JPEG XL support (libjxl)";
    return false;
#endif
}

} // namespace DngJxl
