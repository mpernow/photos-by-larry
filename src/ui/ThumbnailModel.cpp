#include "ThumbnailModel.h"

#include "ThumbnailGeometry.h"
#include "core/ImageConversion.h"
#include "core/Photo.h"
#include "core/PhotoLibrary.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include <opencv2/imgcodecs.hpp>

namespace
{
constexpr int kThumbnailSize = ThumbnailGeometry::kIconSize.width();
}

ThumbnailModel::ThumbnailModel(PhotoLibrary *library, QObject *parent)
    : QAbstractListModel(parent), m_library(library)
{
    connect(m_library, &PhotoLibrary::libraryChanged, this, &ThumbnailModel::onLibraryChanged);
}

int ThumbnailModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_library->count();
}

QVariant ThumbnailModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    const int row = index.row();
    Photo *photo = m_library->photoAt(row);
    if (!photo)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return photo->fileName();
    case Qt::SizeHintRole:
        // Fixed and content-independent, so the delegate's sizeHint() can
        // return immediately without looking at the (possibly not-yet-decoded)
        // icon - see ThumbnailGeometry.h for why that matters.
        return ThumbnailGeometry::kCellSize;
    case Qt::DecorationRole: {
        const auto it = m_thumbnailCache.constFind(row);
        if (it != m_thumbnailCache.constEnd())
            return *it;
        // Lazily kick off decoding for rows we haven't seen yet. data() is const
        // (as required by the model interface) but this is the standard
        // lazy-cache pattern: the one const_cast confines the mutation here.
        const_cast<ThumbnailModel *>(this)->requestThumbnail(row);
        return QPixmap();
    }
    case PhotoIndexRole:
        return row;
    default:
        return {};
    }
}

void ThumbnailModel::onLibraryChanged()
{
    beginResetModel();
    m_thumbnailCache.clear();
    m_pendingRows.clear();
    endResetModel();
}

void ThumbnailModel::requestThumbnail(int row)
{
    if (m_pendingRows.contains(row))
        return;
    m_pendingRows.insert(row);

    Photo *photo = m_library->photoAt(row);
    if (!photo)
        return;
    const QString path = photo->filePath();

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, row]() {
        onThumbnailReady(row, watcher->result());
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([path]() -> QImage {
        const cv::Mat mat = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
        if (mat.empty())
            return {};
        const QImage image = ImageConversion::matToQImage(mat);
        return image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }));
}

void ThumbnailModel::onThumbnailReady(int row, const QImage &thumbnail)
{
    m_pendingRows.remove(row);
    if (thumbnail.isNull())
        return;

    m_thumbnailCache.insert(row, QPixmap::fromImage(thumbnail));
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {Qt::DecorationRole});
}
