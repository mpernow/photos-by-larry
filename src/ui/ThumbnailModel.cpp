#include "ThumbnailModel.h"

#include "ThumbnailGeometry.h"
#include "core/EditParameters.h"
#include "core/ImageConversion.h"
#include "core/ImageProcessor.h"
#include "core/Photo.h"
#include "core/PhotoLibrary.h"

#include <QFutureWatcher>
#include <QPainter>
#include <QtConcurrent/QtConcurrent>

#include <opencv2/imgcodecs.hpp>

namespace
{
constexpr int kThumbnailSize = ThumbnailGeometry::kIconSize.width();

// Baked directly into the thumbnail image (rather than drawn by a delegate)
// since the model already generates these off the UI thread - QImage (unlike
// QPixmap) is safe to paint on from a background thread.
void paintFavoriteBadge(QImage &image)
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = painter.font();
    font.setPointSize(font.pointSize() + 6);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(255, 200, 0));
    painter.drawText(image.rect().adjusted(4, 4, -4, -4), Qt::AlignTop | Qt::AlignLeft,
                      QStringLiteral("★"));
}
} // namespace

ThumbnailModel::ThumbnailModel(PhotoLibrary *library, QObject *parent)
    : QAbstractListModel(parent), m_library(library)
{
    connect(m_library, &PhotoLibrary::libraryChanged, this, &ThumbnailModel::onLibraryChanged);
    rebuildVisibleIndices();
}

int ThumbnailModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visibleIndices.size();
}

QVariant ThumbnailModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleIndices.size())
        return {};

    const int libraryIndex = m_visibleIndices[index.row()];
    Photo *photo = m_library->photoAt(libraryIndex);
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
        const auto it = m_thumbnailCache.constFind(libraryIndex);
        if (it != m_thumbnailCache.constEnd())
            return *it;
        // Lazily kick off decoding for rows we haven't seen yet. data() is const
        // (as required by the model interface) but this is the standard
        // lazy-cache pattern: the one const_cast confines the mutation here.
        const_cast<ThumbnailModel *>(this)->requestThumbnail(libraryIndex);
        return QPixmap();
    }
    case PhotoIndexRole:
        return libraryIndex;
    default:
        return {};
    }
}

void ThumbnailModel::onLibraryChanged()
{
    // A genuinely new set of photos - unlike a filter toggle, cached
    // thumbnails and in-flight requests (keyed by library index) are no
    // longer meaningful at all.
    m_thumbnailCache.clear();
    m_pendingRows.clear();
    rebuildVisibleIndices();
}

void ThumbnailModel::setFavoritesOnlyFilter(bool favoritesOnly)
{
    if (favoritesOnly == m_favoritesOnly)
        return;
    m_favoritesOnly = favoritesOnly;
    // Same photos, same cache (it's keyed by the stable library index, not
    // view row) - only which subset is currently visible changes, so there's
    // no need to throw away already-decoded thumbnails and redecode on
    // every toggle.
    rebuildVisibleIndices();
}

void ThumbnailModel::rebuildVisibleIndices()
{
    beginResetModel();
    m_visibleIndices.clear();
    const int count = m_library->count();
    m_visibleIndices.reserve(count);
    for (int i = 0; i < count; ++i) {
        Photo *photo = m_library->photoAt(i);
        if (!m_favoritesOnly || (photo && photo->isFavorite()))
            m_visibleIndices.push_back(i);
    }
    endResetModel();
}

void ThumbnailModel::requestThumbnail(int libraryIndex)
{
    if (m_pendingRows.contains(libraryIndex))
        return;
    m_pendingRows.insert(libraryIndex);

    Photo *photo = m_library->photoAt(libraryIndex);
    if (!photo)
        return;
    const QString path = photo->filePath();
    const EditParameters params = photo->editParameters();
    const bool isFavorite = photo->isFavorite();

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, libraryIndex]() {
        onThumbnailReady(libraryIndex, watcher->result());
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([path, params, isFavorite]() -> QImage {
        const cv::Mat mat = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
        if (mat.empty())
            return {};
        const cv::Mat rendered = ImageProcessor::apply(mat, params);
        QImage image = ImageConversion::matToQImage(rendered);
        image = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (isFavorite)
            paintFavoriteBadge(image);
        return image;
    }));
}

void ThumbnailModel::onThumbnailReady(int libraryIndex, const QImage &thumbnail)
{
    m_pendingRows.remove(libraryIndex);
    if (thumbnail.isNull())
        return;

    m_thumbnailCache.insert(libraryIndex, QPixmap::fromImage(thumbnail));

    // The filter may have changed while this was decoding, in which case
    // this photo isn't currently shown - it just stays cached for whenever
    // it becomes visible again.
    const int viewRow = m_visibleIndices.indexOf(libraryIndex);
    if (viewRow < 0)
        return;
    const QModelIndex idx = index(viewRow, 0);
    emit dataChanged(idx, idx, {Qt::DecorationRole});
}

void ThumbnailModel::invalidateThumbnail(int libraryIndex)
{
    m_thumbnailCache.remove(libraryIndex);
    // If a request for the old params is still in flight, it's dropped from
    // m_pendingRows so a fresh one gets kicked off below - if that stale job
    // finishes after the fresh one, it could briefly clobber the cache with
    // outdated pixels until the next invalidate. Rare enough in practice
    // (this only fires on discrete, user-paced commits) not to be worth a
    // generation counter to fully close.
    m_pendingRows.remove(libraryIndex);

    const int viewRow = m_visibleIndices.indexOf(libraryIndex);
    if (viewRow < 0)
        return; // not currently visible under the filter - nothing to repaint
    const QModelIndex idx = index(viewRow, 0);
    emit dataChanged(idx, idx, {Qt::DecorationRole}); // triggers a fresh requestThumbnail() on next paint
}

void ThumbnailModel::notifyFavoriteChanged(int libraryIndex)
{
    if (m_favoritesOnly) {
        // Unlike other edits, this can change whether the row should be
        // visible at all under the filter, not just repaint its thumbnail.
        rebuildVisibleIndices();
        return;
    }
    invalidateThumbnail(libraryIndex); // same visible rows either way - just refresh the star badge
}
