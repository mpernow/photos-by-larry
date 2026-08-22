# RAW support & selective color tools — implementation plan

Written 2026-08-22. Covers RAW file support (LibRaw integration, the
bit-depth pipeline rework it depends on, and the photo-library extension
list) plus three creative editing features that build on top of it: a
channel-mixer/weighted-mono mode, an HSL color mixer, and a tone curve.

Motivating context: the user shoots a Fujifilm X-T5 (40.2MP X-Trans CMOS 5
HR sensor, `.RAF` files - X-Trans rather than a standard Bayer array, which
affects the demosaic step specifically). The color-mixer/tone-curve features
were scoped in because they're most of what's needed to approximate Fuji
Film Simulation looks (Provia, Velvia, Classic Chrome, Acros, etc.) from
RAW data - those simulations are metadata/rendering choices applied to the
sensor data, not something baked irrecoverably into a JPEG, so a RAW file
can be converted with any simulation's *approximate* look after the fact.
None of this repo's tools can produce Fuji's exact proprietary rendering,
but the phases below get within reach of it.

This is substantial scope - six distinct pieces of engineering, not a quick
add-on. Phases are ordered so foundational, high-risk work (bit depth) lands
before the thing that depends on it (RAW decode), and the three creative
features are built once against the finished float pipeline rather than
needing to be redone afterward.

No test suite exists in this repo (see CLAUDE.md). Every phase below should
still end in a full clean rebuild (zero warnings) and a hands-on run, same
discipline the rest of this codebase's history follows.

## Phase 1 — Bit-depth pipeline rework

Do this first, using only the existing 8-bit JPEG sources to verify - so
this refactor is de-risked on its own before a new decoder is layered on
top of it.

**Goal:** make `ImageProcessor::apply` depth-agnostic and
precision-preserving, so that when Phase 2 later feeds it 16-bit RAW data,
none of that extra headroom gets thrown away by an intermediate saturating
cast to 8-bit.

**Why this matters:** today, `cv::imread(..., cv::IMREAD_COLOR)` forces
8-bit output, `ImageConversion` only handles `CV_8UC1/3/4`, and every
existing operation inside `ImageProcessor::apply` (white balance, tonal
range, brightness/contrast, vibrance/saturation) ends with a
`convertTo(..., CV_8U)` that saturates back to 8 bits. Decoding a RAW file
at 16-bit and feeding it into this pipeline as-is would still lose all the
extra bit depth at the very first step - RAW would be technically openable
but not actually deliver the headroom that's the whole point of shooting
it.

**Changes:**

- Change `ImageProcessor::apply`'s internal working representation to a
  normalized `[0,1]` float Mat (`CV_32FC3`): convert in once at the top
  (`source.convertTo(working, CV_32FC3, 1.0/255.0)` for 8-bit sources,
  `1.0/65535.0` for the 16-bit sources Phase 2 will introduce), run every
  existing operation against that shared float domain, and convert back to
  `CV_8UC3` once at the very end. The per-step saturating casts throughout
  the current code all go away.
- `ImageConversion` doesn't need to change - it still only ever sees
  `ImageProcessor::apply`'s final `CV_8UC3` output.
- `MainWindow` and `ThumbnailModel` are untouched; this is entirely
  internal to `ImageProcessor`.

**Verification:** full rebuild, then run and compare a few JPEGs across the
existing sliders against pre-refactor output - this phase changes precision
internally, not the behavior of 8-bit sources, so results should match.
Worth an actual pixel diff (export the same settings before/after the
refactor and compare) rather than relying on eyeballing, since a silent
regression here would be easy to miss visually.

**Optional stretch, not required for the core goal:** 16-bit-capable export
(`cv::imwrite` to `.png`/`.tiff` with `CV_16U`) for users who want to
deliver high-bit-depth files. Skip unless specifically requested - the
benefit RAW support is chasing is editing headroom, not necessarily 16-bit
*delivery* files (most exports are still 8-bit JPEG for sharing).

## Phase 2 — LibRaw integration + extension-list change

Bundled together deliberately: landing the extension-list change before the
decoder exists would let RAW files show up in the thumbnail grid before
anything can open them.

**Goal:** RAW files decode into the now-float-ready pipeline at real bit
depth.

**Dependency:** LibRaw, via `PkgConfig`'s `pkg_check_modules` rather than
`find_package` - LibRaw doesn't reliably ship a CMake config package, so
`PkgConfig` is the more portable route across Linux/macOS. Ubuntu:
`libraw-dev`. macOS Homebrew: `libraw`. Update README's dependency install
lines for both platforms. LibRaw is dual-licensed (LGPL 2.1 / CDDL) - not a
blocker, just worth recording alongside the existing Qt/OpenCV dependency
list for future reference.

**New module** `src/core/RawDecoder.h`/`.cpp`: wraps the LibRaw C++ API -
`open_file()` -> `unpack()` -> `dcraw_process()` -> `dcraw_make_mem_image()`,
configured for `output_bps = 16`, camera-embedded white balance, sRGB
output. Returns a `CV_16UC3` `cv::Mat` in BGR order (LibRaw emits RGB, so
needs a channel swap to match this codebase's existing BGR convention, per
`ImageProcessor.cpp`'s "BGR order, matching cv::imread's default" comment).

For X-Trans sensors (the X-T5), LibRaw selects an appropriate non-Bayer
demosaic path automatically based on the sensor pattern read from the file
- this needs verifying empirically against real `.RAF` files during
implementation, not something to pin down on paper. Real sample `.RAF`
files from the X-T5 are needed to actually validate this phase; without
them, "compiles and runs" can't be distinguished from "actually decodes
correctly."

**Single decode chokepoint:** introduce one function (e.g.
`ImageConversion::loadImage(path) -> cv::Mat`) that dispatches by file
extension - known raster extensions go through the existing `cv::imread`,
RAW extensions go through `RawDecoder`. Replace the two direct
`cv::imread` call sites (`MainWindow.cpp` in `loadPhoto`, `ThumbnailModel.cpp`
in its background decode) with calls to this instead, so decode-format
branching lives in exactly one place rather than being duplicated.

**Thumbnail fast path:** a full 40MP X-Trans demosaic per thumbnail would
make `ThumbnailModel`'s background decode noticeably slower than today.
LibRaw exposes `unpack_thumb()` to pull the RAW file's embedded JPEG
preview directly - route `ThumbnailModel`'s decode through that for RAW
files specifically, reserving the full `RawDecoder` demosaic path for the
main viewer and export. This mirrors what Lightroom does with its own
preview caching, and is worth calling out explicitly since it's easy to
skip and only notice as a performance problem later, on a real 40MP body.

**Extension list:** add `.raf` plus the other common RAW extensions
(`.cr2`, `.cr3`, `.nef`, `.arw`, `.orf`, `.rw2`, `.dng` - LibRaw handles all
of them through roughly the same path once the decoder exists) to
`PhotoLibrary.cpp`'s `setNameFilters` call.

## Phase 3 — Channel-mixer / weighted-mono mode

**Goal:** a proper black-and-white conversion mode, replacing the
"set Saturation to -100" workaround discussed when this was first raised -
that approach desaturates using HSV's Value channel as brightness, so a red
flower and a blue sky at similar apparent brightness render nearly
identically in gray; a real weighted conversion (what film-emulation B&W
and colored-filter effects actually do) treats them very differently based
on their R/G/B mix.

**Data:** new `EditParameters` fields - `bool monochrome = false`, plus
`redWeight`/`greenWeight`/`blueWeight` (defaulting to standard luminance
weights, ~0.299/0.587/0.114, the first time monochrome is enabled). When
on, `ImageProcessor::apply` computes
`gray = r*redWeight + g*greenWeight + b*blueWeight` and writes it to all
three channels.

**UI:** new collapsible "Black & White" section in `AdjustmentsPanel` - a
checkbox plus three sliders, enabled only when checked - following the
same `CollapsibleSection` pattern already used for Geometry/Light/Color.

Simplest of the three creative features: same UI idiom as everything that
already exists, just a few new scalar parameters.

## Phase 4 — HSL color mixer

**Goal:** per-hue-band saturation/hue/luminance control - this is most of
what makes Film-Simulation-style selective-color looks possible at all
(e.g. Velvia's push into reds/greens while going easier on skin tones,
Classic Chrome's specific muting of greens and blues).

**Data:** fixed 8 hue bands (Lightroom's convention - Red/Orange/Yellow/
Green/Aqua/Blue/Purple/Magenta), each with Hue/Saturation/Luminance
offsets: `std::array<HslBand, 8> hslBands` on `EditParameters`,
JSON-serialized as an array of objects, with `isIdentity()` requiring all
eight bands' three fields to be zero.

**Processing:** per pixel, compute a soft weight for each band based on hue
distance (a smooth falloff so adjacent bands blend rather than producing
hard edges at band boundaries), apply each band's offsets scaled by its
weight, and sum across bands. Slots in alongside the existing
vibrance/saturation step, operating on the same HSV representation that
step already uses.

**UI - the real new piece:** a band selector (8 swatches, or a combo box)
with three contextual sliders that retarget to whichever band is currently
selected. This is a materially different pattern than "one slider per
parameter" - 24 total parameters (8 bands x 3 axes) as flat sliders would
recreate exactly the crowding problem the collapsible sections were built
to solve.

**Open scoping question:** 8 bands x 3 axes is the full Lightroom-equivalent
scope. Cutting to fewer bands (e.g. 6) or dropping Luminance for a first
pass (Hue+Saturation only) is a reasonable way to de-scope without changing
the architecture, if a smaller first version is preferred.

## Phase 5 — Tone curve

**Goal:** arbitrary-shape contrast control, replacing/augmenting the
fixed-shape Highlights/Shadows/Whites/Blacks sliders.

**Data:** a small ordered set of control points, e.g.
`QVector<QPointF> toneCurvePoints` normalized to `[0,1] x [0,1]`, defaulting
to the identity line `[(0,0),(1,1)]`.

**Processing:** build a 256-entry lookup table from the control points via
spline (or piecewise-linear, simpler for a first cut) interpolation,
applied as a per-pixel remap on luminance. Scope v1 to a single master
curve; per-RGB-channel curves (which enable color-grading-style looks) are
a reasonable later extension, not required for v1.

**UI - the hardest piece of the whole plan:** there's no built-in Qt
Widgets curve editor, so this needs a custom `QWidget` with its own
`paintEvent`/mouse-drag handling for control points. There's real precedent
for this kind of custom-interaction widget already in the codebase
(`CropOverlayItem`), so it isn't unprecedented complexity here, but it is a
genuinely new widget, not a variation on the slider pattern used
everywhere else so far.

**Why this is last:** it's worth having the channel-mixer and HSL mixer's
simpler UI patterns done first, both because they're more likely to matter
sooner day-to-day, and to avoid opening the hardest, most novel piece of UI
work first.

## Cross-cutting, applies to every phase

- Every new field defaults to a no-op value (0/false/identity), so old
  sidecars without the new keys keep parsing exactly as before - the same
  pattern already used for `favorite` and the tonal-range/vibrance/
  saturation fields.
- README.md and CLAUDE.md get updated per phase, matching this repo's
  established practice of keeping architecture docs in sync with the code
  in the same commit.
- Verification stays "clean full rebuild with zero warnings, then a
  hands-on run" per phase, since there's no automated test suite.
