#include "core/ImageProcessor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace
{
// cv::Vec3b doesn't provide operator==, so pixel-exact checks compare
// channels individually rather than the whole Vec3b at once.
void expectPixelEq(const cv::Vec3b &actual, const cv::Vec3b &expected)
{
    EXPECT_EQ(actual[0], expected[0]);
    EXPECT_EQ(actual[1], expected[1]);
    EXPECT_EQ(actual[2], expected[2]);
}

// A small BGR test image where every pixel is distinct, so geometry
// operations (rotate/crop) can be checked against exact expected layouts
// rather than just "didn't crash". 2 rows x 3 cols:
//   (0,0)=black   (0,1)=red    (0,2)=green
//   (1,0)=blue    (1,1)=white  (1,2)=gray
cv::Mat makeMarkerImage()
{
    cv::Mat image(2, 3, CV_8UC3);
    image.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 0, 0);       // black
    image.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 0, 255);     // red (BGR)
    image.at<cv::Vec3b>(0, 2) = cv::Vec3b(0, 255, 0);     // green
    image.at<cv::Vec3b>(1, 0) = cv::Vec3b(255, 0, 0);     // blue
    image.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 255, 255); // white
    image.at<cv::Vec3b>(1, 2) = cv::Vec3b(128, 128, 128); // gray
    return image;
}

// A larger synthetic gradient covering the full 0..255 range per channel,
// with channels offset from each other - enough variety for tonal/color
// adjustments to have something meaningful to act on.
cv::Mat makeGradientImage(int size = 64)
{
    cv::Mat image(size, size, CV_8UC3);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const uchar b = static_cast<uchar>((x * 255) / (size - 1));
            const uchar g = static_cast<uchar>(((x + y) * 255) / (2 * (size - 1)));
            const uchar r = static_cast<uchar>(255 - b);
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(b, g, r);
        }
    }
    return image;
}

cv::Mat solidColor(int size, cv::Vec3b color)
{
    return cv::Mat(size, size, CV_8UC3, cv::Scalar(color[0], color[1], color[2]));
}

double maxAbsDiff(const cv::Mat &a, const cv::Mat &b)
{
    return cv::norm(a, b, cv::NORM_INF);
}
} // namespace

TEST(ImageProcessorTest, EmptySourceReturnsEmpty)
{
    const cv::Mat result = ImageProcessor::apply(cv::Mat(), EditParameters{});
    EXPECT_TRUE(result.empty());
}

TEST(ImageProcessorTest, IdentityParametersPreservesPixelValuesExactly)
{
    const cv::Mat source = makeGradientImage();
    const cv::Mat result = ImageProcessor::apply(source, EditParameters{});

    ASSERT_EQ(result.size(), source.size());
    ASSERT_EQ(result.type(), source.type());
    EXPECT_DOUBLE_EQ(maxAbsDiff(source, result), 0.0);
}

TEST(ImageProcessorTest, OutputIsAlwaysEightBitBgr)
{
    const cv::Mat source = makeGradientImage();
    EditParameters params;
    params.brightness = 30;
    params.saturation = 20;
    params.highlights = -10;

    const cv::Mat result = ImageProcessor::apply(source, params);
    EXPECT_EQ(result.type(), CV_8UC3);
}

TEST(ImageProcessorTest, HandlesSixteenBitSourceAndNormalizesCorrectly)
{
    // No 16-bit source exists yet in the app (a future RAW decoder will
    // produce one) - this protects the depth-normalization path added
    // ahead of that, so it isn't only ever exercised for the first time
    // once a real decoder lands.
    constexpr int kSize = 4;
    const cv::Mat source16(kSize, kSize, CV_16UC3, cv::Scalar(32768, 32768, 32768)); // mid-gray, 16-bit

    const cv::Mat result = ImageProcessor::apply(source16, EditParameters{});

    ASSERT_EQ(result.type(), CV_8UC3);
    const cv::Vec3b pixel = result.at<cv::Vec3b>(0, 0);
    // 32768/65535 * 255 ~= 127.5; allow a couple of levels for rounding.
    EXPECT_NEAR(pixel[0], 128, 2);
    EXPECT_NEAR(pixel[1], 128, 2);
    EXPECT_NEAR(pixel[2], 128, 2);
}

TEST(ImageProcessorTest, RotateClockwiseMapsPixelsToExpectedPositions)
{
    const cv::Mat source = makeMarkerImage(); // 2 rows x 3 cols
    EditParameters params;
    params.rotationQuarterTurns = 1; // 90 degrees clockwise

    const cv::Mat result = ImageProcessor::apply(source, params);

    ASSERT_EQ(result.rows, 3);
    ASSERT_EQ(result.cols, 2);
    // A 90-degree clockwise rotation: the source's top-left (black) ends up
    // at the rotated image's top-right; source's bottom-left (blue) ends up
    // at the rotated image's top-left.
    expectPixelEq(result.at<cv::Vec3b>(0, 0), cv::Vec3b(255, 0, 0)); // blue
    expectPixelEq(result.at<cv::Vec3b>(0, 1), cv::Vec3b(0, 0, 0));   // black
    expectPixelEq(result.at<cv::Vec3b>(2, 1), cv::Vec3b(0, 255, 0)); // green
}

TEST(ImageProcessorTest, CropExtractsExpectedRegion)
{
    const cv::Mat source = makeMarkerImage(); // 2 rows x 3 cols
    EditParameters params;
    // Select just the middle column (x in [1/3, 2/3)).
    params.cropRect = QRectF(1.0 / 3.0, 0.0, 1.0 / 3.0, 1.0);

    const cv::Mat result = ImageProcessor::apply(source, params);

    ASSERT_EQ(result.cols, 1);
    ASSERT_EQ(result.rows, 2);
    expectPixelEq(result.at<cv::Vec3b>(0, 0), cv::Vec3b(0, 0, 255));     // red
    expectPixelEq(result.at<cv::Vec3b>(1, 0), cv::Vec3b(255, 255, 255)); // white
}

TEST(ImageProcessorTest, PositiveBrightnessIncreasesPixelValues)
{
    const cv::Mat source = solidColor(4, cv::Vec3b(100, 100, 100));
    EditParameters params;
    params.brightness = 50;

    const cv::Mat result = ImageProcessor::apply(source, params);
    const cv::Vec3b pixel = result.at<cv::Vec3b>(0, 0);
    EXPECT_NEAR(pixel[0], 150, 1);
}

TEST(ImageProcessorTest, NegativeBrightnessDecreasesPixelValues)
{
    const cv::Mat source = solidColor(4, cv::Vec3b(100, 100, 100));
    EditParameters params;
    params.brightness = -50;

    const cv::Mat result = ImageProcessor::apply(source, params);
    const cv::Vec3b pixel = result.at<cv::Vec3b>(0, 0);
    EXPECT_NEAR(pixel[0], 50, 1);
}

TEST(ImageProcessorTest, BrightnessSaturatesAtWhiteAndBlack)
{
    EditParameters brighten;
    brighten.brightness = 100;
    const cv::Mat brightened = ImageProcessor::apply(solidColor(4, cv::Vec3b(230, 230, 230)), brighten);
    expectPixelEq(brightened.at<cv::Vec3b>(0, 0), cv::Vec3b(255, 255, 255));

    EditParameters darken;
    darken.brightness = -100;
    const cv::Mat darkened = ImageProcessor::apply(solidColor(4, cv::Vec3b(20, 20, 20)), darken);
    expectPixelEq(darkened.at<cv::Vec3b>(0, 0), cv::Vec3b(0, 0, 0));
}

TEST(ImageProcessorTest, ContrastAboveOneWidensSpreadBetweenTones)
{
    cv::Mat source(2, 1, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(100, 100, 100);
    source.at<cv::Vec3b>(1, 0) = cv::Vec3b(150, 150, 150);

    EditParameters params;
    params.contrast = 2.0;
    const cv::Mat result = ImageProcessor::apply(source, params);

    const int spreadBefore = 150 - 100;
    const int spreadAfter = result.at<cv::Vec3b>(1, 0)[0] - result.at<cv::Vec3b>(0, 0)[0];
    EXPECT_GT(spreadAfter, spreadBefore);
}

TEST(ImageProcessorTest, ContrastBelowOneNarrowsSpreadBetweenTones)
{
    cv::Mat source(2, 1, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(100, 100, 100);
    source.at<cv::Vec3b>(1, 0) = cv::Vec3b(150, 150, 150);

    EditParameters params;
    params.contrast = 0.5;
    const cv::Mat result = ImageProcessor::apply(source, params);

    const int spreadBefore = 150 - 100;
    const int spreadAfter = result.at<cv::Vec3b>(1, 0)[0] - result.at<cv::Vec3b>(0, 0)[0];
    EXPECT_LT(spreadAfter, spreadBefore);
}

TEST(ImageProcessorTest, PositiveTemperatureWarmsImage)
{
    const cv::Mat source = solidColor(4, cv::Vec3b(128, 128, 128));
    EditParameters params;
    params.temperature = 80;

    const cv::Mat result = ImageProcessor::apply(source, params);
    const cv::Vec3b pixel = result.at<cv::Vec3b>(0, 0); // BGR
    EXPECT_LT(pixel[0], 128);                            // less blue
    EXPECT_GT(pixel[2], 128);                             // more red
}

TEST(ImageProcessorTest, NegativeTemperatureCoolsImage)
{
    const cv::Mat source = solidColor(4, cv::Vec3b(128, 128, 128));
    EditParameters params;
    params.temperature = -80;

    const cv::Mat result = ImageProcessor::apply(source, params);
    const cv::Vec3b pixel = result.at<cv::Vec3b>(0, 0); // BGR
    EXPECT_GT(pixel[0], 128);                            // more blue
    EXPECT_LT(pixel[2], 128);                             // less red
}

TEST(ImageProcessorTest, HighlightsAffectBrightPixelsMoreThanDarkOnes)
{
    cv::Mat source(2, 1, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(20, 20, 20);   // dark
    source.at<cv::Vec3b>(1, 0) = cv::Vec3b(230, 230, 230); // bright

    EditParameters params;
    params.highlights = 60;
    const cv::Mat result = ImageProcessor::apply(source, params);

    const int darkChange = std::abs(result.at<cv::Vec3b>(0, 0)[0] - source.at<cv::Vec3b>(0, 0)[0]);
    const int brightChange = std::abs(result.at<cv::Vec3b>(1, 0)[0] - source.at<cv::Vec3b>(1, 0)[0]);
    EXPECT_GT(brightChange, darkChange);
}

TEST(ImageProcessorTest, ShadowsAffectDarkPixelsMoreThanBrightOnes)
{
    cv::Mat source(2, 1, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(20, 20, 20);   // dark
    source.at<cv::Vec3b>(1, 0) = cv::Vec3b(230, 230, 230); // bright

    EditParameters params;
    params.shadows = 60;
    const cv::Mat result = ImageProcessor::apply(source, params);

    const int darkChange = std::abs(result.at<cv::Vec3b>(0, 0)[0] - source.at<cv::Vec3b>(0, 0)[0]);
    const int brightChange = std::abs(result.at<cv::Vec3b>(1, 0)[0] - source.at<cv::Vec3b>(1, 0)[0]);
    EXPECT_GT(darkChange, brightChange);
}

TEST(ImageProcessorTest, PositiveWhitesBrightensNearWhitePixels)
{
    // Matches every other slider's "positive = more of the named effect"
    // convention: positive whites should push toward *more* clipping at the
    // white end (brighter highlights), not recover them.
    const cv::Mat source = solidColor(4, cv::Vec3b(200, 200, 200));
    EditParameters params;
    params.whites = 80;

    const cv::Mat result = ImageProcessor::apply(source, params);
    EXPECT_GT(result.at<cv::Vec3b>(0, 0)[0], 200);
}

TEST(ImageProcessorTest, NegativeWhitesRecoversNearWhitePixels)
{
    const cv::Mat source = solidColor(4, cv::Vec3b(200, 200, 200));
    EditParameters params;
    params.whites = -80;

    const cv::Mat result = ImageProcessor::apply(source, params);
    EXPECT_LT(result.at<cv::Vec3b>(0, 0)[0], 200);
}

TEST(ImageProcessorTest, PositiveBlacksDarkensNearBlackPixels)
{
    // Mirrors Whites: positive blacks should push toward *more* clipping at
    // the black end (darker/crushed shadows), not lift them.
    const cv::Mat source = solidColor(4, cv::Vec3b(60, 60, 60));
    EditParameters params;
    params.blacks = 80;

    const cv::Mat result = ImageProcessor::apply(source, params);
    EXPECT_LT(result.at<cv::Vec3b>(0, 0)[0], 60);
}

TEST(ImageProcessorTest, NegativeBlacksLiftsNearBlackPixels)
{
    const cv::Mat source = solidColor(4, cv::Vec3b(60, 60, 60));
    EditParameters params;
    params.blacks = -80;

    const cv::Mat result = ImageProcessor::apply(source, params);
    EXPECT_GT(result.at<cv::Vec3b>(0, 0)[0], 60);
}

TEST(ImageProcessorTest, SaturationOfMinusOneHundredProducesGrayscale)
{
    const cv::Mat source = makeGradientImage();
    EditParameters params;
    params.saturation = -100;

    const cv::Mat result = ImageProcessor::apply(source, params);
    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            const cv::Vec3b pixel = result.at<cv::Vec3b>(y, x);
            EXPECT_NEAR(pixel[0], pixel[1], 1) << "at (" << x << "," << y << ")";
            EXPECT_NEAR(pixel[1], pixel[2], 1) << "at (" << x << "," << y << ")";
        }
    }
}

TEST(ImageProcessorTest, PositiveSaturationIncreasesChannelSpread)
{
    const cv::Mat source = solidColor(4, cv::Vec3b(80, 140, 200)); // clearly non-gray
    EditParameters params;
    params.saturation = 60;

    const cv::Mat result = ImageProcessor::apply(source, params);
    const cv::Vec3b before = source.at<cv::Vec3b>(0, 0);
    const cv::Vec3b after = result.at<cv::Vec3b>(0, 0);

    const int spreadBefore = *std::max_element(before.val, before.val + 3) - *std::min_element(before.val, before.val + 3);
    const int spreadAfter = *std::max_element(after.val, after.val + 3) - *std::min_element(after.val, after.val + 3);
    EXPECT_GT(spreadAfter, spreadBefore);
}

TEST(ImageProcessorTest, VibranceBoostsMutedColorsMoreThanAlreadyVividOnes)
{
    const cv::Mat vividSource = solidColor(4, cv::Vec3b(0, 0, 255));      // pure red, fully saturated
    const cv::Mat mutedSource = solidColor(4, cv::Vec3b(120, 130, 140)); // nearly gray

    EditParameters params;
    params.vibrance = 80;

    const cv::Mat vividResult = ImageProcessor::apply(vividSource, params);
    const cv::Mat mutedResult = ImageProcessor::apply(mutedSource, params);

    const double vividChange = maxAbsDiff(vividSource, vividResult);
    const double mutedChange = maxAbsDiff(mutedSource, mutedResult);
    EXPECT_GT(mutedChange, vividChange);
}

TEST(ImageProcessorTest, CombinedEditsProduceValidEightBitOutput)
{
    const cv::Mat source = makeGradientImage();
    EditParameters params;
    params.brightness = 15;
    params.contrast = 1.2;
    params.highlights = -30;
    params.shadows = 25;
    params.whites = 15;
    params.blacks = -10;
    params.temperature = -20;
    params.tint = 10;
    params.vibrance = 20;
    params.saturation = 10;
    params.rotationQuarterTurns = 3;
    params.cropRect = QRectF(0.1, 0.1, 0.5, 0.5);

    const cv::Mat result = ImageProcessor::apply(source, params);

    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.type(), CV_8UC3);
}
