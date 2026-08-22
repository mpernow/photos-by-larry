#include "ImageProcessor.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
// Converts a normalized [0,1] crop rect into a pixel rect on an image of the
// given size, clamped so it always stays a valid, in-bounds region.
cv::Rect cropRectPixels(const QRectF &normalized, int width, int height)
{
    const int x = std::clamp(static_cast<int>(std::lround(normalized.x() * width)), 0, width - 1);
    const int y = std::clamp(static_cast<int>(std::lround(normalized.y() * height)), 0, height - 1);
    const int w = std::clamp(static_cast<int>(std::lround(normalized.width() * width)), 1, width - x);
    const int h = std::clamp(static_cast<int>(std::lround(normalized.height() * height)), 1, height - y);
    return cv::Rect(x, y, w, h);
}

// Converts to this pipeline's normalized [0,1] float working representation,
// regardless of the source's actual bit depth - 8-bit today (cv::imread),
// 16-bit later (a RAW decoder). Every operation below works in this common
// domain, so none of them need to know or care what depth the original
// pixels came from; a 16-bit source keeps its extra headroom all the way
// through instead of losing it the moment editing starts.
cv::Mat toNormalizedFloat(const cv::Mat &mat)
{
    double scale = 1.0;
    switch (mat.depth()) {
    case CV_8U:
        scale = 1.0 / 255.0;
        break;
    case CV_16U:
        scale = 1.0 / 65535.0;
        break;
    default:
        break; // already float (or something unexpected) - pass through as-is
    }
    cv::Mat result;
    mat.convertTo(result, CV_32FC3, scale);
    return result;
}

// Simple per-channel gain shift rather than true Kelvin-based color science -
// consistent with the rest of this pipeline's "plain linear ops" approach.
// temperature/tint are both roughly -100..100; 0 leaves the image untouched.
// Caller only invokes this when at least one is nonzero. Operates on (and
// returns) the normalized float working representation - pure multiplicative
// gains, so the [0,1] convention doesn't otherwise affect this math.
cv::Mat applyWhiteBalance(const cv::Mat &source, double temperature, double tint)
{
    constexpr double kTemperatureStrength = 0.003; // ±100 temperature -> up to ±30% red/blue gain
    constexpr double kTintStrength = 0.003;         // ±100 tint -> up to ±30% green gain

    const double blueGain = 1.0 - temperature * kTemperatureStrength + tint * kTintStrength * 0.5;
    const double greenGain = 1.0 - tint * kTintStrength;
    const double redGain = 1.0 + temperature * kTemperatureStrength + tint * kTintStrength * 0.5;

    std::vector<cv::Mat> channels; // BGR order, matching cv::imread's default
    cv::split(source, channels);
    channels[0].convertTo(channels[0], -1, blueGain, 0.0);
    channels[1].convertTo(channels[1], -1, greenGain, 0.0);
    channels[2].convertTo(channels[2], -1, redGain, 0.0);

    cv::Mat result;
    cv::merge(channels, result);
    return result;
}

// Highlights/shadows/whites/blacks: -100..100 each, 0 = neutral. Caller only
// invokes this when at least one is nonzero. Operates on (and returns) the
// normalized float working representation - unlike the old 8-bit pipeline,
// nothing here saturates to a fixed range; values are free to briefly land
// outside [0,1] and get resolved once, at the very end of ImageProcessor::apply.
//
// Whites/blacks move the white/black clipping points and linearly rescale
// everything between back to [0,1] - a classic levels adjustment. Positive
// pushes toward *more* clipping at that extreme (whites brightens/clips
// highlights harder, blacks darkens/clips shadows harder), negative pulls
// back from it (recovers highlights, lifts shadows) - matching every other
// slider's "positive = more of the named effect" convention.
// Highlights/shadows are an additive shift weighted by luminance (via a
// squared falloff, so the effect is concentrated at the tonal extreme it
// names and fades out toward the opposite end) rather than applying
// uniformly like brightness does.
cv::Mat applyTonalRange(const cv::Mat &source, double highlights, double shadows, double whites, double blacks)
{
    cv::Mat working = source;

    if (whites != 0.0 || blacks != 0.0) {
        // 1.2/255 carries forward the same tuning as the original 0-255-scale
        // version of this formula, just re-expressed in [0,1] units: ±100 ->
        // clip point moves by just under half the full range, past full clip.
        constexpr double kEndpointStrength = 1.2 / 255.0;
        // Increasing blacks *raises* the black point (narrows the range from
        // the bottom -> crushes shadows darker). Increasing whites has to
        // *lower* the white point the same way (narrow the range from the
        // top -> blows highlights brighter) to match - hence the minus sign
        // here where blackPoint uses a plus.
        const double blackPoint = std::clamp(blacks * kEndpointStrength, -1.0, 1.0);
        const double whitePoint = std::clamp(1.0 - whites * kEndpointStrength, 1.0 / 255.0, 2.0);
        const double range = std::max(whitePoint - blackPoint, 1.0 / 255.0);
        const double alpha = 1.0 / range;
        const double beta = -blackPoint * alpha;
        cv::Mat leveled;
        working.convertTo(leveled, -1, alpha, beta);
        working = leveled;
    }

    if (highlights == 0.0 && shadows == 0.0)
        return working;

    // BGR2GRAY on a float [0,1] source produces float luminance already in
    // [0,1] - no separate rescale needed, unlike the old 8-bit version of
    // this function.
    cv::Mat luminance;
    cv::cvtColor(working, luminance, cv::COLOR_BGR2GRAY);
    cv::Mat invLuminance = 1.0f - luminance;

    cv::Mat shadowWeight = invLuminance.mul(invLuminance);  // emphasizes dark pixels
    cv::Mat highlightWeight = luminance.mul(luminance);     // emphasizes bright pixels
    // /255 rescales shadows/highlights (still -100..100, an old 0-255-scale
    // additive shift) into this pipeline's [0,1] units.
    cv::Mat delta = (shadowWeight * static_cast<double>(shadows) + highlightWeight * static_cast<double>(highlights)) /
                     255.0;

    std::vector<cv::Mat> channels;
    cv::split(working, channels);
    for (cv::Mat &channel : channels)
        channel += delta;
    cv::Mat result;
    cv::merge(channels, result);
    return result;
}

// Vibrance/saturation: -100..100 each, 0 = neutral. Caller only invokes this
// when at least one is nonzero. Both operate on HSV's saturation channel,
// which for a float [0,1] BGR source is itself already [0,1] (unlike 8-bit
// HSV, where it's 0-255) - saturation scales it uniformly, while vibrance's
// boost is weighted by how unsaturated a pixel already is, so it pushes
// muted colors harder than already-vivid ones instead of applying uniformly.
cv::Mat applyVibranceSaturation(const cv::Mat &source, double vibrance, double saturation)
{
    cv::Mat hsv;
    cv::cvtColor(source, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    cv::Mat &s = channels[1];

    if (saturation != 0.0)
        s *= (1.0 + saturation / 100.0); // ±100 -> ±100% multiplicative change

    if (vibrance != 0.0) {
        cv::Mat weight = 1.0f - s;
        s += weight * (vibrance / 100.0);
    }

    cv::merge(channels, hsv);
    cv::Mat result;
    cv::cvtColor(hsv, result, cv::COLOR_HSV2BGR);
    return result;
}
} // namespace

// Converts source pixels to normalized [0,1] float once, applies every edit
// in that common working space, and quantizes back to 8-bit once at the very
// end - the single point where values actually clip to a fixed range.
// Everything in between is free to carry values slightly outside [0,1]
// without being cut off prematurely the way repeated per-step 8-bit
// saturating casts used to. `source` may be 8-bit (cv::imread today) or
// 16-bit (a future RAW decoder); the result is always CV_8UC3, ready for
// display or export.
cv::Mat ImageProcessor::apply(const cv::Mat &source, const EditParameters &params)
{
    if (source.empty())
        return source;

    cv::Mat working = source;

    if (const int turns = ((params.rotationQuarterTurns % 4) + 4) % 4; turns != 0) {
        cv::Mat rotated;
        switch (turns) {
        case 1:
            cv::rotate(working, rotated, cv::ROTATE_90_CLOCKWISE);
            break;
        case 2:
            cv::rotate(working, rotated, cv::ROTATE_180);
            break;
        case 3:
            cv::rotate(working, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        }
        working = rotated;
    }

    if (!params.isFullCrop())
        working = working(cropRectPixels(params.cropRect, working.cols, working.rows)); // shares the parent's buffer

    // Rotate/crop happen on the source's original (smaller, cheaper) depth;
    // everything from here on works in the wider float representation.
    working = toNormalizedFloat(working);

    if (params.temperature != 0.0 || params.tint != 0.0)
        working = applyWhiteBalance(working, params.temperature, params.tint);

    if (params.highlights != 0.0 || params.shadows != 0.0 || params.whites != 0.0 || params.blacks != 0.0)
        working = applyTonalRange(working, params.highlights, params.shadows, params.whites, params.blacks);

    if (params.brightness != 0.0 || params.contrast != 1.0) {
        cv::Mat result;
        // Classic linear brightness/contrast adjustment: out = contrast * in + brightness.
        // /255 rescales brightness (still -100..100, an old 0-255-scale additive
        // shift) into this pipeline's [0,1] units; contrast is already unitless.
        working.convertTo(result, -1, params.contrast, params.brightness / 255.0);
        working = result;
    }

    if (params.vibrance != 0.0 || params.saturation != 0.0)
        working = applyVibranceSaturation(working, params.vibrance, params.saturation);

    cv::Mat result;
    // The one and only place values actually clip to a fixed range: back to
    // 8-bit, saturating via convertTo's rounding + clamping.
    working.convertTo(result, CV_8UC3, 255.0, 0.0);
    return result;
}
