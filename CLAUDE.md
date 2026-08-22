# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

Dependencies: CMake 3.16+, a C++17 compiler, Qt6 (Widgets + Concurrent), OpenCV 4.

```sh
# Ubuntu/Debian dependency install
sudo apt install cmake build-essential qt6-base-dev libqt6opengl6-dev libopencv-dev

# Configure + build
cmake -B build
cmake --build build -j

# Run
./build/PhotosByLarry
```

`CMakeLists.txt` also has basic
macOS support - `MACOSX_BUNDLE` on the target (a no-op on other platforms,
so it doesn't affect the Linux build) and a default
`CMAKE_OSX_DEPLOYMENT_TARGET` - believed correct from Qt/CMake's own docs
but unverified on real macOS; see the README's macOS section (dependencies
via Homebrew, `open build/PhotosByLarry.app` to run, what's still needed for
a distributable - not just locally-runnable - bundle).

There is no test suite and no linter/formatter configured in this repo yet.
Verify changes by building (it must compile cleanly with no new warnings)
and, where possible, running the app.

## Architecture

The code is split into two layers under `src/`:

- **`src/core/`** — UI-independent model and processing logic. Depends on Qt
  Core (for `QString`/JSON) and OpenCV, but nothing from Qt Widgets.
- **`src/ui/`** — Qt Widgets views, wired together by `MainWindow`.

### Core types

- `Photo` — one image file plus its current `EditParameters` and favorite
  status. Favorite is a field on `Photo`, not `EditParameters` - it's
  curation metadata that never feeds into `ImageProcessor`. It's persisted
  as a flat, sibling `"favorite"` key in the same sidecar JSON (not nested
  under the edit fields), so old sidecars without that key still parse fine.
- `PhotoLibrary` — the set of photos found in one opened directory.
- `EditParameters` — the adjustable values for a photo: brightness, contrast,
  tonal range (`highlights`/`shadows`/`whites`/`blacks`, layered on top of
  brightness/contrast rather than replacing them), white balance
  (`temperature`/`tint` — simple per-channel gain shifts, not true
  Kelvin-based color science), presence (`vibrance`/`saturation`), a discrete
  rotation (`rotationQuarterTurns`, 0-3), and a crop rect normalized to
  `[0,1]` _relative to the rotated image_ — resolution independent, so the
  same value crops the full-resolution photo and any downscaled preview
  identically. Serializes to/from JSON. Rotating a photo that already has a
  crop transforms the crop rect to match
  (`rotatedClockwise`/`rotatedCounterClockwise`) rather than resetting it, so
  the selected region keeps referring to the same content.
- `ImageProcessor::apply(source, params)` — pure function: original pixels +
  parameters → rendered pixels, applying rotate, then crop, then white
  balance, then tonal range (highlights/shadows/whites/blacks), then
  brightness/contrast, then vibrance/saturation, in that order. Stateless, so
  it can run on the UI thread for live preview or off-thread for
  export/thumbnailing. Highlights/shadows and vibrance/saturation are
  weighted rather than uniform adjustments - highlights/shadows shift
  additively based on how bright/dark a pixel already is (via a
  luminance-squared falloff), and vibrance's saturation boost is weighted
  toward already-muted colors - so each is a step up from brightness's flat
  additive shift without introducing full masking/regional editing.
- `ImageConversion` — `cv::Mat` <-> `QImage` conversions (the seam between
  OpenCV and Qt); every conversion deep-copies.

### Non-destructive editing

Original image files are never modified. Each photo's `EditParameters` are
persisted to a small JSON sidecar file next to it
(`IMG_0001.jpg` -> `IMG_0001.jpg.larryedit.json`). Opening a directory again
picks the sidecar back up, and every render re-applies the parameters to the
untouched original. There's no central catalog/database — if per-file
sidecars ever become unwieldy, that's the natural point to introduce one, but
the `Photo`/`PhotoLibrary` API wouldn't need to change to do it.

### UI panels (`MainWindow`)

- `ThumbnailPanel` (left) — grid of thumbnails, backed by `ThumbnailModel`, a
  `QAbstractListModel` over `PhotoLibrary` that decodes each thumbnail
  lazily on a background thread (`QtConcurrent`) and caches it. Thumbnails
  are rendered through `ImageProcessor` with the photo's current
  `EditParameters`, so they reflect rotate/crop/brightness/etc. edits too. A
  favorited photo also gets a star badge painted directly onto its thumbnail
  during that same background decode. Also has a "Favorites only" checkbox
  that filters the grid - see "View row vs. library index" below.
- `ImageViewer` (center) — `QGraphicsView`-based pan/zoom display of the
  current rendered image; also hosts `CropOverlayItem` while cropping, and a
  favorite-toggle star button as a real child `QWidget` of the viewport
  (not a scene item) anchored to the bottom-right corner, so pan/zoom never
  moves it. Hidden while cropping - a fresh crop rect commonly extends to
  that same corner, which would otherwise put the crop handle and the
  button on top of each other.
- `AdjustmentsPanel` (right dock) — brightness/contrast/highlights/shadows/
  whites/blacks/temperature/tint/vibrance/saturation sliders, rotate
  buttons, the crop tool, and Copy/Paste Settings for the current photo.
- `CropOverlayItem` — a `QGraphicsItem` drawn on top of the image while
  cropping: darkens everything outside the selection and lets the user drag
  its body to move it or its edges/corners to resize it. `ImageViewer` only
  deals in normalized `[0,1]` crop fractions at its API boundary
  (`beginCropping`/`currentCropNormalizedRect`/`endCropping`) — callers never
  need to know the displayed image's pixel dimensions.

Two editing patterns coexist, depending on whether an operation has a
meaningful "in progress" state:

- **Brightness/contrast/highlights/shadows/whites/blacks/temperature/tint/
  vibrance/saturation** (continuous, dragged): slider move
  -> `AdjustmentsPanel::previewParametersChanged` ->
  `MainWindow::updatePreview` -> `ImageProcessor::apply` ->
  `ImageViewer::setImage`. Slider release additionally fires
  `parametersCommitted`, which updates the `Photo` and writes its sidecar.
  `AdjustmentsPanel` tracks a full `m_baseParameters` (not just the slider
  values) so committing a slider change carries the photo's existing
  rotation/crop forward instead of wiping it.
- **Rotate** (discrete, no dragging): clicking a rotate button commits
  immediately — skips straight to updating the `Photo`, writing its sidecar,
  and re-rendering.
- **Crop** (a mode, not a single action): "Crop" hands the full (uncropped)
  rotated frame to `ImageViewer::beginCropping`, seeded with any existing
  crop; the overlay is dragged freely with nothing persisted; "Apply" reads
  the overlay's rect back and commits it; "Cancel" discards it. Switching
  photos or opening a new directory mid-crop cancels it first
  (`MainWindow::cancelCropIfActive`).
- **Copy/Paste Settings** (discrete): "Copy" snapshots the current photo's
  `EditParameters` into an in-memory `MainWindow::m_copiedParameters` —
  session-only, not written anywhere, not tied to the source photo, so it
  works across directories. "Paste" applies it to whatever's currently
  selected _except_ the crop rect, which is left as the destination's own
  (a crop drawn for one photo's content/aspect ratio has no reason to mean
  anything on another). Paste is disabled until something's copied and
  while mid-crop; Copy is read-only and stays available even then.
- **Favoriting** (discrete): two controls - the checkable "Photo > Toggle
  Favorite" menu action (shortcut `F`) and `ImageViewer`'s corner button -
  both feed `MainWindow::onFavoriteToggled`, the single place that persists
  the change and syncs the _other_ control to match (via `QSignalBlocker`,
  so the sync doesn't loop back as another toggle). Enabled whenever a photo
  is loaded, including mid-crop - unlike Export/Paste, it never touches the
  rendered image, so there's no reason to lock it out.

Committing any edit invalidates that photo's cached thumbnail
(`ThumbnailModel::invalidateThumbnail`) - except a favorite toggle, which
goes through `notifyFavoriteChanged` instead, since under the favorites-only
filter that photo may need to appear/disappear from the grid entirely, not
just have its thumbnail repainted.

The "Export..." button lives in the status bar's permanent-widget area
(bottom-right corner) rather than a menu — that placement was a specific
ask, not a default, so preserve it rather than "cleaning it up" into a menu
action. It renders `m_currentSource` through `ImageProcessor::apply` with
the photo's last _committed_ `EditParameters` and writes the result via
`cv::imwrite` to a file the user picks; it never touches the sidecar, so
exporting doesn't affect how revisable the edit stays. Acts on the single
active photo only — the thumbnail grid is single-selection, so there's no
multi-photo selection to drive a batch export from yet.

### View row vs. library index (`ThumbnailModel`)

The favorites-only filter means `ThumbnailModel` has two distinct notions of
"index": the _view row_ (this model's row, 0..N-1 over whatever subset is
currently visible - tracked in `m_visibleIndices`, view row -> library
index) and the _library index_ (a photo's stable position in `PhotoLibrary`,
unaffected by filtering). Everything outside this model - `MainWindow`,
`ThumbnailPanel`'s selection handling - deals exclusively in library
indices, fetched via `PhotoIndexRole` (defined from the very start of the
model, unused until this feature needed it) rather than a `QModelIndex`'s
raw `row()`. That's what keeps `MainWindow` completely unaffected by
filtering existing at all: `loadPhoto`, `invalidateThumbnail`,
`m_currentRow`, etc. all still just mean "index into `PhotoLibrary`."

The thumbnail cache is keyed by library index too, so toggling the filter
(`setFavoritesOnlyFilter`) never forces re-decoding anything - it only
recomputes which already-cached rows are visible (`rebuildVisibleIndices`).
Only a genuinely new directory (`onLibraryChanged`) clears the cache
outright. One known rough edge: toggling the filter still does a full
`beginResetModel`, which drops the grid's selection highlight even for a
photo that remains visible under the new filter - fixing that would mean
diffing old/new `m_visibleIndices` with targeted
`beginInsertRows`/`beginRemoveRows` instead.

### Preview vs. full-resolution rendering

`MainWindow` keeps two decoded copies of the selected photo: `m_currentSource`
(full resolution) and `m_previewSource` (downscaled to a ~1600px long edge).
Live slider dragging renders from `m_previewSource` so every mouse-move stays
cheap regardless of the photo's actual resolution; slider release, rotate,
and crop-apply all re-render from `m_currentSource` for a crisp result.
`ImageViewer` only re-fits/re-centers when the displayed pixmap's _dimensions_
change, so switching between preview- and full-resolution renders of the same
photo doesn't reset the user's pan/zoom.

### Qt gotchas already worked around here

- `ImageViewer` tracks `m_userAdjustedZoom` and only auto-fits when it's
  false. Without this, zooming in past the fit level makes a scrollbar
  appear, which resizes the viewport, which re-enters `resizeEvent()`, which
  re-fits the view right back to "fit" — so zooming in looks like it silently
  does nothing (zooming out never shows a scrollbar, so it isn't affected).
- `ThumbnailModel` provides a fixed `Qt::SizeHintRole` per row. Without it,
  `QListView` calling the delegate's `sizeHint()` for every row (not just
  visible ones) falls through to reading `Qt::DecorationRole` to size the
  item — which is exactly what triggers this model's background thumbnail
  decode, so _every_ photo in a directory would start decoding the instant
  it's opened instead of just the visible ones.

### Planned, not yet implemented

- **Multi-select + batch operations** — Export and Paste Settings both
  currently act on the single active photo only, since `ThumbnailPanel`'s
  grid is single-selection (`QAbstractItemView::SingleSelection`). Extending
  this to batch export and batch-apply-copied-settings across several
  selected photos is the main workflow gap versus editing a whole shoot at
  once.
