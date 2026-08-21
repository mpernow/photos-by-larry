#include "PhotoLibrary.h"

#include "Photo.h"

#include <QDir>
#include <QFileInfo>

PhotoLibrary::PhotoLibrary(QObject *parent) : QObject(parent) {}

PhotoLibrary::~PhotoLibrary() = default;

void PhotoLibrary::openDirectory(const QString &directoryPath)
{
    m_photos.clear();
    m_directoryPath = directoryPath;

    QDir dir(directoryPath);
    dir.setNameFilters({"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tif", "*.tiff"});
    dir.setFilter(QDir::Files | QDir::Readable);
    dir.setSorting(QDir::Name);

    const QFileInfoList entries = dir.entryInfoList();
    m_photos.reserve(entries.size());
    for (const QFileInfo &info : entries)
        m_photos.push_back(std::make_unique<Photo>(info.absoluteFilePath()));

    emit libraryChanged();
}

int PhotoLibrary::count() const
{
    return static_cast<int>(m_photos.size());
}

Photo *PhotoLibrary::photoAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_photos.size()))
        return nullptr;
    return m_photos[index].get();
}
