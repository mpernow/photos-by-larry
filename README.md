# Photos by Larry

A desktop photo browser/editor built with Qt (UI) and OpenCV (image processing).

## Building

Dependencies: CMake 3.16+, a C++17 compiler, Qt6 (Widgets + Concurrent), OpenCV 4.

On Ubuntu/Debian:

```sh
sudo apt install cmake build-essential qt6-base-dev libqt6opengl6-dev libopencv-dev
```

Configure and build:

```sh
cmake -B build
cmake --build build -j
./build/PhotosByLarry
```

## Architecture

The code is split into two layers under `src/`:

- **`src/core/`** — UI-independent model and processing logic. Depends on
  Qt Core (for `QString`/JSON) and OpenCV, but nothing from Qt Widgets.
- **`src/ui/`** — Qt Widgets views, wired together by `MainWindow`.

### Core types

- `Photo` — one image file plus its current `EditParameters` and favorite
  status. Favorite is deliberately not a field on `EditParameters`: it's
  browsing/curation metadata, not a value that feeds into rendering, so it
  never touches `ImageProcessor`. It's persisted in the same sidecar JSON as
  a flat, sibling `"favorite"` key though (not nested under the edit
  fields), so sidecars from before this feature existed still parse
  identically - a missing key just defaults to false.
- `PhotoLibrary` — the set of photos found in one opened directory.
- `EditParameters` — the adjustable values for a photo: brightness, contrast,
  white balance (`temperature`/`tint`, simple per-channel gain shifts rather
  than true Kelvin-based color science), a discrete rotation
  (`rotationQuarterTurns`, 0-3), and a crop rect normalized to `[0,1]`
  *relative to the rotated image* - resolution independent, so the same
  value crops the full-resolution photo and any downscaled preview
  identically. Serializes to/from JSON.
- `ImageProcessor::apply(source, params)` — pure function: original pixels +
  parameters → rendered pixels, applying rotate, then crop, then white
  balance, then brightness/contrast, in that order. Stateless, so it can run
  on the UI thread for live preview or off-thread for export/thumbnailing.
- `ImageConversion` — `cv::Mat` <-> `QImage` conversions (the seam between
  OpenCV and Qt).

Rotating a photo that already has a crop transforms the crop rect to match
(`EditParameters::rotatedClockwise/CounterClockwise`) rather than resetting
it, so the selected region keeps referring to the same content.

### Non-destructive editing

Original image files are never modified. Each photo's `EditParameters` are
persisted to a small JSON sidecar file next to it
(`IMG_0001.jpg` -> `IMG_0001.jpg.larryedit.json`). Opening a directory again
picks the sidecar back up, and every render re-applies the parameters to the
untouched original — so edits stay revisable and cheap. This keeps a
central catalog/database out of scope for now; if the library grows large
enough that per-file sidecars become unwieldy, that's the natural point to
introduce one, but the `Photo`/`PhotoLibrary` API wouldn't need to change to
do it.

### UI panels (`MainWindow`)

- `ThumbnailPanel` (left) — grid of thumbnails, backed by `ThumbnailModel`,
  a `QAbstractListModel` over `PhotoLibrary` that decodes each thumbnail
  lazily on a background thread (`QtConcurrent`) and caches it. A small star
  badge is painted directly onto a favorited photo's thumbnail image (baked
  in during that same background decode, not drawn by a delegate). Also has
  a "★ Favorites only" checkbox that filters the grid down to favorited
  photos.
- `ImageViewer` (center) — `QGraphicsView`-based pan/zoom display of the
  current rendered image. Also hosts a favorite-toggle star button anchored
  to the viewport's bottom-right corner - a real child `QWidget` of the
  viewport, not a `QGraphicsScene` item, specifically so it stays fixed in
  place regardless of the image's pan/zoom (a scene item would move and
  scale with the photo instead). Hidden while cropping, since a fresh crop
  rect commonly extends to that same corner, which would otherwise put two
  click-targets on top of each other.
- `AdjustmentsPanel` (right dock) — brightness/contrast/temperature/tint
  sliders, rotate buttons, and the crop tool for the current photo.
- `CropOverlayItem` — a `QGraphicsItem` drawn on top of the image in
  `ImageViewer` while cropping: darkens everything outside the selection and
  lets the user drag its body to move it or its edges/corners to resize it.

Two editing patterns coexist, depending on whether an operation has a
meaningful "in progress" state:

- **Brightness/contrast/temperature/tint** (continuous, dragged): slider move ->
  `AdjustmentsPanel::previewParametersChanged` -> `MainWindow::updatePreview`
  -> `ImageProcessor::apply` on the cached decoded source ->
  `ImageViewer::setImage`. Slider release additionally fires
  `parametersCommitted`, which updates the `Photo` and writes its sidecar.
- **Rotate** (discrete, no dragging): clicking a rotate button commits
  immediately - there's no meaningful "live preview" for a 90° turn, so it
  skips straight to updating the `Photo`, writing its sidecar, and
  re-rendering.
- **Crop** (a mode, not a single action): clicking "Crop" hands the full
  (uncropped) rotated frame to `ImageViewer::beginCropping`, seeded with any
  existing crop. The user drags the overlay freely with nothing persisted;
  "Apply" reads the overlay's rect back via
  `ImageViewer::currentCropNormalizedRect()`, commits it, and re-renders;
  "Cancel" just discards it. Switching photos or opening a new directory
  mid-crop cancels it first (`MainWindow::cancelCropIfActive`).
- **Copy/Paste Settings** (discrete, no dragging): "Copy Settings" snapshots
  the current photo's `EditParameters` into `MainWindow::m_copiedParameters`
  (an in-memory, session-only clipboard - not written anywhere, and not tied
  to the source photo once copied, so it works across directories too).
  "Paste Settings" applies that snapshot to whichever photo is currently
  selected and commits immediately, *except* the crop rect: crop is specific
  to the photo it was drawn on, so pasting keeps the destination's own crop
  rather than overwriting it with a rectangle that has no relationship to
  its content. "Paste" is disabled until something has been copied, and
  while a crop is in progress (it would re-render the background out from
  under the crop overlay); "Copy" is read-only and stays available even
  mid-crop.

Committing any edit also invalidates that photo's cached thumbnail
(`ThumbnailModel::invalidateThumbnail`) so the thumbnail strip reflects
rotate/crop/brightness/etc. changes too, not just the main viewer.

### Export

The "Export..." button sits in the status bar's permanent-widget area (the
window's bottom-right corner) rather than a menu, per how this was asked
for. It renders the current photo through `ImageProcessor::apply` at full
resolution with its last *committed* `EditParameters` and writes the result
to a file the user picks (`cv::imwrite`) - it never touches the sidecar, so
exporting doesn't affect how revisable the edit stays afterward. The
suggested filename defaults to `<name>_edited.<ext>` next to the original
rather than reusing the original's name, so a quick Export doesn't sit one
dialog-confirm away from overwriting the untouched original. Disabled
whenever there's no photo loaded, and while a crop is in progress but not
yet applied (exporting then would silently ignore it, since it isn't part
of `EditParameters` until "Apply" commits it).

Export always acts on the single active photo - there's no batch/export-all
yet, partly because the thumbnail grid is single-selection only
(`QAbstractItemView::SingleSelection`), so there's no multi-photo selection
to drive one.

### Favoriting

Two controls toggle the same underlying flag, kept in sync with each other:
the checkable "Photo > Toggle Favorite" menu action (shortcut `F`, chosen for
discoverability - Qt shows it right in the menu), and `ImageViewer`'s corner
star button. Both funnel into `MainWindow::onFavoriteToggled`, the single
place that actually persists the change (`Photo::setFavorite` + `saveSidecar`
+ thumbnail invalidation) and pushes the resulting value back out to
whichever control *didn't* originate the click (via `QSignalBlocker`, so
that sync doesn't loop back around as another toggle). Enabled whenever a
photo is loaded, including mid-crop - unlike Export/Paste, favoriting never
touches the rendered image, so there's no consistency reason to lock it out.

### Filtering the thumbnail grid to favorites

`ThumbnailPanel`'s "★ Favorites only" checkbox filters `ThumbnailModel`
directly (`ThumbnailPanel::favoritesOnlyToggled` -> `setFavoritesOnlyFilter`)
- a pure browsing/view setting, so `MainWindow` doesn't mediate it. This is
the one place the model's row numbering gets more involved: `ThumbnailModel`
now has two distinct notions of "index" -

- **view row** — this model's row (0..N-1 over whatever subset is currently
  visible), tracked in `m_visibleIndices` (view row -> library index).
- **library index** — the photo's stable position in `PhotoLibrary`,
  unaffected by filtering.

Everything *outside* `ThumbnailModel` - `MainWindow`, `ThumbnailPanel`'s
selection handling - deals exclusively in library indices, fetched via
`PhotoIndexRole` (defined from the start, but unused until this feature
needed it) rather than a `QModelIndex`'s raw `row()`. This is what keeps
`MainWindow` completely unaffected by filtering existing at all: `loadPhoto`,
`invalidateThumbnail`, `m_currentRow`, etc. all still just mean "index into
`PhotoLibrary`," exactly as before.

The thumbnail cache is also keyed by library index rather than view row, so
toggling the filter never forces re-decoding anything - `setFavoritesOnlyFilter`
only recomputes which already-cached rows are currently visible
(`rebuildVisibleIndices`), while a genuinely new directory
(`onLibraryChanged`) is the only thing that clears the cache outright.
Favoriting/unfavoriting the current photo calls
`notifyFavoriteChanged` rather than `invalidateThumbnail`: under the filter,
that photo may need to appear or disappear from the grid entirely (a full
`rebuildVisibleIndices`), not just have its thumbnail repainted.

Known rough edge: toggling the filter resets the model (`beginResetModel`),
which drops the grid's current selection even for a photo that remains
visible under the new filter state - the main viewer keeps showing whatever
was selected, it just loses its highlight in the grid. Fixing that would
mean diffing old/against new `m_visibleIndices` and using targeted
`beginInsertRows`/`beginRemoveRows` instead of a blanket reset, which felt
like more complexity than this first cut warranted.

### What's deliberately not built yet

This is infrastructure, not a feature-complete editor. Brightness, contrast,
white balance, rotate, crop, and export are wired up end-to-end as a proof
that the whole pipeline works - adding another adjustment in the same style
means: a field on `EditParameters` (+ JSON read/write), a case in
`ImageProcessor::apply`, and a control in `AdjustmentsPanel` — no changes
needed to `Photo`, `PhotoLibrary`, or the persistence mechanism. White
balance (temperature/tint) is a real example of exactly that extension path.

Not yet implemented: arbitrary-angle straightening, undo/redo history, RAW
support, batch export, multi-select/batch editing, EXIF-based sorting,
graduated star ratings (favoriting is currently a plain boolean - a
1-5 rating would need its own UI rather than a checkable toggle).
