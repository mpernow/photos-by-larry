#pragma once

#include <QJsonObject>
#include <QRectF>

// All adjustments that can be applied to a photo, non-destructively.
// This is the thing that gets persisted to a sidecar file per photo, and
// re-applied to the original pixel data on top whenever the photo is shown
// or exported. Add new adjustment fields here as editing features grow.
struct EditParameters
{
    double brightness = 0.0; // additive term, roughly -100..100
    double contrast = 1.0;   // multiplicative term, roughly 0.0..3.0 (1.0 = unchanged)

    // White balance, as simple per-channel gain shifts rather than true
    // Kelvin-based color science - both roughly -100..100, 0 = neutral.
    double temperature = 0.0; // positive = warmer (more red/less blue), negative = cooler
    double tint = 0.0;        // positive = more magenta, negative = more green

    int rotationQuarterTurns = 0; // number of 90-degree clockwise turns, 0..3

    // Normalized [0,1] crop rect, relative to the ROTATED image (i.e. after
    // rotationQuarterTurns has been applied) - resolution-independent, so the
    // same value crops the full-resolution photo and any downscaled preview
    // of it identically. (0,0,1,1) means "no crop".
    QRectF cropRect = {0.0, 0.0, 1.0, 1.0};

    bool isFullCrop() const;
    bool isIdentity() const
    {
        return brightness == 0.0 && contrast == 1.0 && temperature == 0.0 && tint == 0.0 &&
               rotationQuarterTurns == 0 && isFullCrop();
    }

    // Rotating the photo also has to carry any existing crop selection along
    // with it, transformed into the newly-rotated frame - otherwise a crop
    // rect computed for the old orientation would select the wrong region
    // (or an out-of-bounds one) once the dimensions change.
    EditParameters rotatedClockwise() const;
    EditParameters rotatedCounterClockwise() const;

    bool operator==(const EditParameters &other) const;
    bool operator!=(const EditParameters &other) const { return !(*this == other); }

    QJsonObject toJson() const;
    static EditParameters fromJson(const QJsonObject &object);
};
