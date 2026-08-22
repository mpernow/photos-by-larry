#include "core/EditParameters.h"

#include <gtest/gtest.h>

namespace
{
// A params value with every field set to something distinct from the
// identity default, so serialization/rotation tests can't accidentally pass
// just because a field was left at its default.
EditParameters makeNonIdentityParams()
{
    EditParameters params;
    params.brightness = 12.5;
    params.contrast = 1.4;
    params.highlights = -30.0;
    params.shadows = 45.0;
    params.whites = 10.0;
    params.blacks = -15.0;
    params.temperature = 22.0;
    params.tint = -8.0;
    params.vibrance = 33.0;
    params.saturation = -17.0;
    params.rotationQuarterTurns = 2;
    params.cropRect = QRectF(0.1, 0.2, 0.3, 0.4);
    return params;
}
} // namespace

TEST(EditParametersTest, DefaultConstructedIsIdentity)
{
    EXPECT_TRUE(EditParameters{}.isIdentity());
}

TEST(EditParametersTest, DefaultConstructedIsFullCrop)
{
    EXPECT_TRUE(EditParameters{}.isFullCrop());
}

TEST(EditParametersTest, EachFieldIndividuallyBreaksIdentity)
{
    {
        EditParameters p;
        p.brightness = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.contrast = 1.5;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.highlights = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.shadows = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.whites = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.blacks = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.temperature = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.tint = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.vibrance = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.saturation = 1.0;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.rotationQuarterTurns = 1;
        EXPECT_FALSE(p.isIdentity());
    }
    {
        EditParameters p;
        p.cropRect = QRectF(0.1, 0.1, 0.5, 0.5);
        EXPECT_FALSE(p.isIdentity());
        EXPECT_FALSE(p.isFullCrop());
    }
}

TEST(EditParametersTest, OperatorEqualsUsesFuzzyComparison)
{
    EditParameters a = makeNonIdentityParams();
    EditParameters b = a;
    b.brightness += 1e-9; // well under the internal epsilon
    EXPECT_EQ(a, b);
}

TEST(EditParametersTest, OperatorEqualsCatchesRealDifferences)
{
    EditParameters a = makeNonIdentityParams();
    EditParameters b = a;
    b.saturation += 5.0;
    EXPECT_NE(a, b);
}

TEST(EditParametersTest, ToJsonFromJsonRoundTrips)
{
    const EditParameters original = makeNonIdentityParams();
    const EditParameters restored = EditParameters::fromJson(original.toJson());
    EXPECT_EQ(original, restored);
}

TEST(EditParametersTest, FromJsonOfEmptyObjectIsIdentity)
{
    // Covers old sidecars written before a field existed: a JSON object
    // missing some (or all) of these keys must still parse to a sane,
    // identity-equivalent EditParameters rather than garbage or a crash.
    const EditParameters restored = EditParameters::fromJson(QJsonObject{});
    EXPECT_TRUE(restored.isIdentity());
}

TEST(EditParametersTest, FromJsonWithOnlySomeKeysLeavesRestAtDefault)
{
    QJsonObject object;
    object["brightness"] = 42.0;
    object["saturation"] = -10.0;

    const EditParameters restored = EditParameters::fromJson(object);

    EXPECT_DOUBLE_EQ(restored.brightness, 42.0);
    EXPECT_DOUBLE_EQ(restored.saturation, -10.0);
    EXPECT_DOUBLE_EQ(restored.contrast, 1.0);
    EXPECT_DOUBLE_EQ(restored.highlights, 0.0);
    EXPECT_DOUBLE_EQ(restored.temperature, 0.0);
    EXPECT_TRUE(restored.isFullCrop());
}

TEST(EditParametersTest, RotatedClockwiseTransformsCropRectAndIncrementsTurns)
{
    EditParameters params;
    params.rotationQuarterTurns = 0;
    params.cropRect = QRectF(0.1, 0.2, 0.3, 0.4);

    const EditParameters rotated = params.rotatedClockwise();

    EXPECT_EQ(rotated.rotationQuarterTurns, 1);
    // (x,y) -> (1-y,x) applied to the rect's corner, per the documented
    // mapping: new_x = 1 - y - h, new_y = x, new_w = h, new_h = w.
    EXPECT_NEAR(rotated.cropRect.x(), 0.4, 1e-9);
    EXPECT_NEAR(rotated.cropRect.y(), 0.1, 1e-9);
    EXPECT_NEAR(rotated.cropRect.width(), 0.4, 1e-9);
    EXPECT_NEAR(rotated.cropRect.height(), 0.3, 1e-9);
}

TEST(EditParametersTest, RotatedCounterClockwiseUndoesRotatedClockwise)
{
    const EditParameters original = makeNonIdentityParams();
    const EditParameters roundTripped = original.rotatedClockwise().rotatedCounterClockwise();
    EXPECT_EQ(original, roundTripped);
}

TEST(EditParametersTest, FourClockwiseRotationsReturnToOriginalOrientation)
{
    EditParameters rotated = makeNonIdentityParams();
    for (int i = 0; i < 4; ++i)
        rotated = rotated.rotatedClockwise();

    const EditParameters original = makeNonIdentityParams();
    EXPECT_EQ(rotated.rotationQuarterTurns, original.rotationQuarterTurns);
    EXPECT_EQ(rotated, original);
}

TEST(EditParametersTest, RotationDoesNotAffectNonGeometryFields)
{
    const EditParameters original = makeNonIdentityParams();
    const EditParameters rotated = original.rotatedClockwise();

    EXPECT_DOUBLE_EQ(rotated.brightness, original.brightness);
    EXPECT_DOUBLE_EQ(rotated.contrast, original.contrast);
    EXPECT_DOUBLE_EQ(rotated.vibrance, original.vibrance);
    EXPECT_DOUBLE_EQ(rotated.saturation, original.saturation);
}
