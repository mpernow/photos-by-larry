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

// Simple per-channel gain shift rather than true Kelvin-based color science -
// consistent with the rest of this pipeline's "plain linear ops" approach.
// temperature/tint are both roughly -100..100; 0 leaves the image untouched.
// Caller only invokes this when at least one is nonzero.
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
// invokes this when at least one is nonzero.
//
// Whites/blacks move the white/black clipping points and linearly rescale
// everything between back to [0,255] - a classic levels adjustment.
// Highlights/shadows are an additive shift weighted by luminance (via a
// squared falloff, so the effect is concentrated at the tonal extreme it
// names and fades out toward the opposite end) rather than applying
// uniformly like brightness does.
cv::Mat applyTonalRange(const cv::Mat &source, double highlights, double shadows, double whites, double blacks)
{
    cv::Mat working = source;

    if (whites != 0.0 || blacks != 0.0) {
        constexpr double kEndpointStrength = 1.2; // ±100 -> clip point moves up to ±120, past full clip
        const double blackPoint = std::clamp(blacks * kEndpointStrength, -255.0, 255.0);
        const double whitePoint = std::clamp(255.0 + whites * kEndpointStrength, 1.0, 510.0);
        const double range = std::max(whitePoint - blackPoint, 1.0);
        const double alpha = 255.0 / range;
        const double beta = -blackPoint * alpha;
        cv::Mat leveled;
        working.convertTo(leveled, -1, alpha, beta); // convertTo saturates to [0,255]
        working = leveled;
    }

    if (highlights == 0.0 && shadows == 0.0)
        return working;

    cv::Mat gray;
    cv::cvtColor(working, gray, cv::COLOR_BGR2GRAY);
    cv::Mat luminance;
    gray.convertTo(luminance, CV_32F, 1.0 / 255.0);
    cv::Mat invLuminance = 1.0f - luminance;

    cv::Mat shadowWeight = invLuminance.mul(invLuminance);  // emphasizes dark pixels
    cv::Mat highlightWeight = luminance.mul(luminance);     // emphasizes bright pixels
    cv::Mat delta =
        shadowWeight * static_cast<double>(shadows) + highlightWeight * static_cast<double>(highlights);

    std::vector<cv::Mat> channels;
    cv::split(working, channels);
    for (cv::Mat &channel : channels) {
        cv::Mat channelFloat;
        channel.convertTo(channelFloat, CV_32F);
        channelFloat += delta;
        channelFloat.convertTo(channel, CV_8U); // convertTo saturates to [0,255]
    }
    cv::Mat result;
    cv::merge(channels, result);
    return result;
}

// Vibrance/saturation: -100..100 each, 0 = neutral. Caller only invokes this
// when at least one is nonzero. Both operate on HSV's saturation channel;
// saturation scales it uniformly, while vibrance's boost is weighted by how
// unsaturated a pixel already is, so it pushes muted colors harder than
// already-vivid ones instead of applying uniformly.
cv::Mat applyVibranceSaturation(const cv::Mat &source, double vibrance, double saturation)
{
    cv::Mat hsv;
    cv::cvtColor(source, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);

    cv::Mat s;
    channels[1].convertTo(s, CV_32F);

    if (saturation != 0.0)
        s *= (1.0 + saturation / 100.0); // ±100 -> ±100% multiplicative change

    if (vibrance != 0.0) {
        cv::Mat weight = (255.0f - s) / 255.0f;
        s += weight * (vibrance / 100.0 * 255.0);
    }

    s.convertTo(channels[1], CV_8U); // convertTo saturates to [0,255]
    cv::merge(channels, hsv);

    cv::Mat result;
    cv::cvtColor(hsv, result, cv::COLOR_HSV2BGR);
    return result;
}
} // namespace

cv::Mat ImageProcessor::apply(const cv::Mat &source, const EditParameters &params)
{
    if (source.empty())
        return source;

    cv::Mat working = source;
    bool freshBuffer = false; // whether `working` has already been detached from source's buffer

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
        freshBuffer = true;
    }

    if (!params.isFullCrop())
        working = working(cropRectPixels(params.cropRect, working.cols, working.rows)); // shares the parent's buffer

    if (params.temperature != 0.0 || params.tint != 0.0) {
        working = applyWhiteBalance(working, params.temperature, params.tint);
        freshBuffer = true;
    }

    if (params.highlights != 0.0 || params.shadows != 0.0 || params.whites != 0.0 || params.blacks != 0.0) {
        working = applyTonalRange(working, params.highlights, params.shadows, params.whites, params.blacks);
        freshBuffer = true;
    }

    if (params.brightness != 0.0 || params.contrast != 1.0) {
        cv::Mat result;
        // Classic linear brightness/contrast adjustment: out = contrast * in + brightness.
        // convertTo with alpha/beta does this in one pass, saturating to the valid range.
        working.convertTo(result, -1, params.contrast, params.brightness);
        working = result;
        freshBuffer = true;
    }

    if (params.vibrance != 0.0 || params.saturation != 0.0) {
        working = applyVibranceSaturation(working, params.vibrance, params.saturation);
        freshBuffer = true;
    }

    return freshBuffer ? working : working.clone(); // detach from source's buffer before handing back
}
