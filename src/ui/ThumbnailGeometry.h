#pragma once

#include <QSize>

// Shared sizing for the thumbnail grid. The model's declared per-item
// Qt::SizeHintRole and the view's actual grid cell must always match exactly:
// if they drift apart, Qt falls back to computing size from content (icon +
// text) instead of using the fixed hint, which re-opens two problems at once
// - cropped items before thumbnails decode, and every row's icon being
// decoded up front just to size the layout. See ThumbnailModel::data() and
// ThumbnailPanel's constructor.
namespace ThumbnailGeometry
{
constexpr QSize kIconSize(140, 140);
constexpr QSize kCellSize(160, 172); // icon + spacing + room for the filename below it
} // namespace ThumbnailGeometry
