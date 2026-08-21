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

void Photo::loadSidecarIfPresent()
{
    QFile file(sidecarPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject())
        m_params = EditParameters::fromJson(doc.object());
}

void Photo::saveSidecar()
{
    if (!m_dirty)
        return;

    QFile file(sidecarPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(m_params.toJson()).toJson());
    m_dirty = false;
}
