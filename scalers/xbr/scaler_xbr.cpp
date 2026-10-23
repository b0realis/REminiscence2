#include "scaler_xbr.h"

static void xbrNx(int factor, uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h) {
    xbrz::scale(factor, src, dst, w, h, xbrz::ColorFormat::RGB);
}

const Scaler EXPORT *getScaler() {
    return new Scaler {
        SCALER_TAG,
        "xbrNx",
        2, 6,
        xbrNx,
    };
}
