#include "EditParameters.h"

#include <cmath>

namespace
{
constexpr double kEpsilon = 1e-6;
bool fuzzyEquals(double a, double b)
{
    return std::abs(a - b) < kEpsilon;
}
} // namespace

bool EditParameters::isFullCrop() const
{
    return fuzzyEquals(cropRect.x(), 0.0) && fuzzyEquals(cropRect.y(), 0.0) &&
           fuzzyEquals(cropRect.width(), 1.0) && fuzzyEquals(cropRect.height(), 1.0);
}

EditParameters EditParameters::rotatedClockwise() const
{
    EditParameters result = *this;
    result.rotationQuarterTurns = (rotationQuarterTurns + 1) % 4;
    // A 90-degree clockwise turn of the whole image maps a normalized point
    // (x,y) to (1-y,x); apply that to the crop rect's corners and take the
    // new bounding box.
    result.cropRect = QRectF(1.0 - cropRect.y() - cropRect.height(), cropRect.x(), cropRect.height(),
                              cropRect.width());
    return result;
}

EditParameters EditParameters::rotatedCounterClockwise() const
{
    EditParameters result = *this;
    result.rotationQuarterTurns = (rotationQuarterTurns + 3) % 4;
    // Inverse of the clockwise mapping above: (x,y) -> (y,1-x).
    result.cropRect = QRectF(cropRect.y(), 1.0 - cropRect.x() - cropRect.width(), cropRect.height(),
                              cropRect.width());
    return result;
}

bool EditParameters::operator==(const EditParameters &other) const
{
    return fuzzyEquals(brightness, other.brightness) && fuzzyEquals(contrast, other.contrast) &&
           fuzzyEquals(temperature, other.temperature) && fuzzyEquals(tint, other.tint) &&
           rotationQuarterTurns == other.rotationQuarterTurns &&
           fuzzyEquals(cropRect.x(), other.cropRect.x()) && fuzzyEquals(cropRect.y(), other.cropRect.y()) &&
           fuzzyEquals(cropRect.width(), other.cropRect.width()) &&
           fuzzyEquals(cropRect.height(), other.cropRect.height());
}

QJsonObject EditParameters::toJson() const
{
    QJsonObject object;
    object["brightness"] = brightness;
    object["contrast"] = contrast;
    object["temperature"] = temperature;
    object["tint"] = tint;
    object["rotationQuarterTurns"] = rotationQuarterTurns;
    object["cropX"] = cropRect.x();
    object["cropY"] = cropRect.y();
    object["cropWidth"] = cropRect.width();
    object["cropHeight"] = cropRect.height();
    return object;
}

EditParameters EditParameters::fromJson(const QJsonObject &object)
{
    EditParameters params;
    params.brightness = object.value("brightness").toDouble(params.brightness);
    params.contrast = object.value("contrast").toDouble(params.contrast);
    params.temperature = object.value("temperature").toDouble(params.temperature);
    params.tint = object.value("tint").toDouble(params.tint);
    params.rotationQuarterTurns = object.value("rotationQuarterTurns").toInt(params.rotationQuarterTurns);
    params.cropRect = QRectF(object.value("cropX").toDouble(params.cropRect.x()),
                              object.value("cropY").toDouble(params.cropRect.y()),
                              object.value("cropWidth").toDouble(params.cropRect.width()),
                              object.value("cropHeight").toDouble(params.cropRect.height()));
    return params;
}
