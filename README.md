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

- `Photo` — one image file plus its current `EditParameters`.
- `PhotoLibrary` — the set of photos found in one opened directory.
- `EditParameters` — the adjustable values for a photo (currently
  brightness/contrast). Serializes to/from JSON.
- `ImageProcessor::apply(source, params)` — pure function: original pixels +
  parameters → rendered pixels. Stateless, so it can run on the UI thread for
  live preview or off-thread for export/thumbnailing.
- `ImageConversion` — `cv::Mat` <-> `QImage` conversions (the seam between
  OpenCV and Qt).

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
  lazily on a background thread (`QtConcurrent`) and caches it.
- `ImageViewer` (center) — `QGraphicsView`-based pan/zoom display of the
  current rendered image.
- `AdjustmentsPanel` (right dock) — sliders for the current photo's edit
  parameters. Slider movement emits a live-preview signal (fast, not
  persisted); releasing the slider emits a "committed" signal that
  `MainWindow` saves to the sidecar.

Data flow for editing: slider move -> `AdjustmentsPanel::previewParametersChanged`
-> `MainWindow::updatePreview` -> `ImageProcessor::apply` on the cached
decoded source -> `ImageViewer::setImage`. Slider release additionally fires
`parametersCommitted`, which updates the `Photo` and writes its sidecar.

### What's deliberately not built yet

This is infrastructure, not a feature-complete editor. Only brightness and
contrast are wired up end-to-end, as a proof that the whole pipeline works.
Adding a new adjustment means: a field on `EditParameters` (+ JSON
read/write), a case in `ImageProcessor::apply`, and a control in
`AdjustmentsPanel` — no changes needed to `Photo`, `PhotoLibrary`, or the
persistence mechanism.

Not yet implemented: crop/rotate, undo/redo history, export, RAW support,
multi-select/batch editing, EXIF-based sorting.
