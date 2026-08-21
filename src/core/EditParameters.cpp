#include "EditParameters.h"

QJsonObject EditParameters::toJson() const
{
    QJsonObject object;
    object["brightness"] = brightness;
    object["contrast"] = contrast;
    return object;
}

EditParameters EditParameters::fromJson(const QJsonObject &object)
{
    EditParameters params;
    params.brightness = object.value("brightness").toDouble(params.brightness);
    params.contrast = object.value("contrast").toDouble(params.contrast);
    return params;
}
