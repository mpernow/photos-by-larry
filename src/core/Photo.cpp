#include "Photo.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
constexpr const char *kSidecarSuffix = ".larryedit.json";
}

Photo::Photo(QString filePath) : m_filePath(std::move(filePath))
{
    loadSidecarIfPresent();
}

QString Photo::fileName() const
{
    return QFileInfo(m_filePath).fileName();
}

QString Photo::sidecarPath() const
{
    return m_filePath + kSidecarSuffix;
}

void Photo::setEditParameters(const EditParameters &params)
{
    if (params == m_params)
        return;
    m_params = params;
    m_dirty = true;
}

void Photo::setFavorite(bool favorite)
{
    if (favorite == m_isFavorite)
        return;
    m_isFavorite = favorite;
    m_dirty = true;
}

void Photo::loadSidecarIfPresent()
{
    QFile file(sidecarPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        m_params = EditParameters::fromJson(object);
        m_isFavorite = object.value("favorite").toBool(false);
    }
}

void Photo::saveSidecar()
{
    if (!m_dirty)
        return;

    QFile file(sidecarPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QJsonObject object = m_params.toJson();
    object["favorite"] = m_isFavorite;
    file.write(QJsonDocument(object).toJson());
    m_dirty = false;
}
