#pragma once

#include <QJsonObject>

// All adjustments that can be applied to a photo, non-destructively.
// This is the thing that gets persisted to a sidecar file per photo, and
// re-applied to the original pixel data on top whenever the photo is shown
// or exported. Add new adjustment fields here as editing features grow.
struct EditParameters
{
    double brightness = 0.0; // additive term, roughly -100..100
    double contrast = 1.0;   // multiplicative term, roughly 0.0..3.0 (1.0 = unchanged)

    bool isIdentity() const { return brightness == 0.0 && contrast == 1.0; }

    bool operator==(const EditParameters &other) const
    {
        return brightness == other.brightness && contrast == other.contrast;
    }
    bool operator!=(const EditParameters &other) const { return !(*this == other); }

    QJsonObject toJson() const;
    static EditParameters fromJson(const QJsonObject &object);
};
