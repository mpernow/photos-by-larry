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

There is no test suite and no linter/formatter configured in this repo yet.
Verify changes by building (it must compile cleanly with no new warnings)
and, where possible, running the app.

## Architecture

The code is split into two layers under `src/`:

- **`src/core/`** — UI-independent model and processing logic. Depends on Qt
  Core (for `QString`/JSON) and OpenCV, but nothing from Qt Widgets.
- **`src/ui/`** — Qt Widgets views, wired together by `MainWindow`.

### Core types

- `Photo` — one image file plus its current `EditParameters`.
- `PhotoLibrary` — the set of photos found in one opened directory.
- `EditParameters` — the adjustable values for a photo: brightness, contrast,
  a discrete rotation (`rotationQuarterTurns`, 0-3), and a crop rect
  normalized to `[0,1]` *relative to the rotated image* — resolution
  independent, so the same value crops the full-resolution photo and any
  downscaled preview identically. Serializes to/from JSON. Rotating a photo
  that already has a crop transforms the crop rect to match
  (`rotatedClockwise`/`rotatedCounterClockwise`) rather than resetting it, so
  the selected region keeps referring to the same content.
- `ImageProcessor::apply(source, params)` — pure function: original pixels +
  parameters → rendered pixels, applying rotate, then crop, then
  brightness/contrast, in that order. Stateless, so it can run on the UI
  thread for live preview or off-thread for export/thumbnailing.
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
  `EditParameters`, so they reflect rotate/crop/brightness edits too.
- `ImageViewer` (center) — `QGraphicsView`-based pan/zoom display of the
  current rendered image; also hosts `CropOverlayItem` while cropping.
- `AdjustmentsPanel` (right dock) — brightness/contrast sliders, rotate
  buttons, and the crop tool for the current photo.
- `CropOverlayItem` — a `QGraphicsItem` drawn on top of the image while
  cropping: darkens everything outside the selection and lets the user drag
  its body to move it or its edges/corners to resize it. `ImageViewer` only
  deals in normalized `[0,1]` crop fractions at its API boundary
  (`beginCropping`/`currentCropNormalizedRect`/`endCropping`) — callers never
  need to know the displayed image's pixel dimensions.

Two editing patterns coexist, depending on whether an operation has a
meaningful "in progress" state:

- **Brightness/contrast** (continuous, dragged): slider move ->
  `AdjustmentsPanel::previewParametersChanged` -> `MainWindow::updatePreview`
  -> `ImageProcessor::apply` -> `ImageViewer::setImage`. Slider release
  additionally fires `parametersCommitted`, which updates the `Photo` and
  writes its sidecar. `AdjustmentsPanel` tracks a full `m_baseParameters`
  (not just the slider values) so committing a brightness/contrast change
  carries the photo's existing rotation/crop forward instead of wiping it.
- **Rotate** (discrete, no dragging): clicking a rotate button commits
  immediately — skips straight to updating the `Photo`, writing its sidecar,
  and re-rendering.
- **Crop** (a mode, not a single action): "Crop" hands the full (uncropped)
  rotated frame to `ImageViewer::beginCropping`, seeded with any existing
  crop; the overlay is dragged freely with nothing persisted; "Apply" reads
  the overlay's rect back and commits it; "Cancel" discards it. Switching
  photos or opening a new directory mid-crop cancels it first
  (`MainWindow::cancelCropIfActive`).

Committing any edit invalidates that photo's cached thumbnail
(`ThumbnailModel::invalidateThumbnail`).

The "Export..." button lives in the status bar's permanent-widget area
(bottom-right corner) rather than a menu — that placement was a specific
ask, not a default, so preserve it rather than "cleaning it up" into a menu
action. It renders `m_currentSource` through `ImageProcessor::apply` with
the photo's last *committed* `EditParameters` and writes the result via
`cv::imwrite` to a file the user picks; it never touches the sidecar, so
exporting doesn't affect how revisable the edit stays. Acts on the single
active photo only — the thumbnail grid is single-selection, so there's no
multi-photo selection to drive a batch export from yet.

### Preview vs. full-resolution rendering

`MainWindow` keeps two decoded copies of the selected photo: `m_currentSource`
(full resolution) and `m_previewSource` (downscaled to a ~1600px long edge).
Live slider dragging renders from `m_previewSource` so every mouse-move stays
cheap regardless of the photo's actual resolution; slider release, rotate,
and crop-apply all re-render from `m_currentSource` for a crisp result.
`ImageViewer` only re-fits/re-centers when the displayed pixmap's *dimensions*
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
  decode, so *every* photo in a directory would start decoding the instant
  it's opened instead of just the visible ones.
