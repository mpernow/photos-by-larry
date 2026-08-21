#include "MainWindow.h"

#include "AdjustmentsPanel.h"
#include "ImageViewer.h"
#include "ThumbnailModel.h"
#include "ThumbnailPanel.h"

#include "core/ImageConversion.h"
#include "core/ImageProcessor.h"
#include "core/Photo.h"
#include "core/PhotoLibrary.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace
{
// Cap for the downscaled copy used while a slider is actively being dragged.
// The viewer fits the image to the window anyway, so anything past what the
// window can show is wasted work - this just needs to comfortably cover any
// reasonable window size and zoom level while staying cheap to re-process on
// every mouse-move.
constexpr int kPreviewMaxDimension = 1600;

cv::Mat makePreviewSource(const cv::Mat &fullResolution)
{
    if (fullResolution.empty())
        return fullResolution;
    const int longEdge = std::max(fullResolution.cols, fullResolution.rows);
    if (longEdge <= kPreviewMaxDimension)
        return fullResolution;

    const double scale = static_cast<double>(kPreviewMaxDimension) / longEdge;
    cv::Mat preview;
    cv::resize(fullResolution, preview, {}, scale, scale, cv::INTER_AREA);
    return preview;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_library(new PhotoLibrary(this)),
      m_thumbnailModel(new ThumbnailModel(m_library, this)),
      m_thumbnailPanel(new ThumbnailPanel(this)),
      m_imageViewer(new ImageViewer(this)),
      m_adjustmentsPanel(new AdjustmentsPanel(this)),
      m_exportButton(new QPushButton(tr("Export..."), this))
{
    setWindowTitle(tr("Photos by Larry"));
    resize(1280, 800);

    m_thumbnailPanel->setModel(m_thumbnailModel);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_thumbnailPanel);
    splitter->addWidget(m_imageViewer);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 1000});
    setCentralWidget(splitter);

    auto *dock = new QDockWidget(tr("Adjustments"), this);
    dock->setWidget(m_adjustmentsPanel);
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *openAction = fileMenu->addAction(tr("&Open Directory..."), this, &MainWindow::openDirectory);
    openAction->setShortcut(QKeySequence::Open);

    // Requested to live as a corner button rather than a menu action - the
    // status bar's permanent-widget area is the idiomatic Qt way to anchor a
    // widget to the window's bottom-right, and stays there across resizes.
    statusBar()->addPermanentWidget(m_exportButton);
    updateExportEnabled();

    connect(m_exportButton, &QPushButton::clicked, this, &MainWindow::onExportRequested);
    connect(m_thumbnailPanel, &ThumbnailPanel::photoSelected, this, &MainWindow::onPhotoSelected);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::previewParametersChanged, this,
            &MainWindow::onPreviewParametersChanged);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::parametersCommitted, this,
            &MainWindow::onParametersCommitted);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::rotateClockwiseRequested, this,
            &MainWindow::onRotateClockwiseRequested);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::rotateCounterClockwiseRequested, this,
            &MainWindow::onRotateCounterClockwiseRequested);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::cropRequested, this, &MainWindow::onCropRequested);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::cropApplyRequested, this, &MainWindow::onCropApplyRequested);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::cropCancelRequested, this,
            &MainWindow::onCropCancelRequested);
    // Purely a crop-tool interaction setting - AdjustmentsPanel tells
    // ImageViewer how to behave directly, no Photo/persistence involved.
    connect(m_adjustmentsPanel, &AdjustmentsPanel::keepAspectRatioToggled, m_imageViewer,
            &ImageViewer::setCropAspectRatioLocked);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_currentPhoto)
        m_currentPhoto->saveSidecar();
    QMainWindow::closeEvent(event);
}

void MainWindow::openDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Photo Directory"));
    if (dir.isEmpty())
        return;

    cancelCropIfActive();
    if (m_currentPhoto)
        m_currentPhoto->saveSidecar();
    m_currentPhoto = nullptr;
    m_currentSource.release();
    m_previewSource.release();
    m_imageViewer->clear();
    m_adjustmentsPanel->setEnabled(false);
    updateExportEnabled();

    m_library->openDirectory(dir);
    statusBar()->showMessage(tr("%1 photo(s) found in %2").arg(m_library->count()).arg(dir), 5000);
}

void MainWindow::onPhotoSelected(int row)
{
    loadPhoto(row);
}

void MainWindow::loadPhoto(int row)
{
    cancelCropIfActive();
    if (m_currentPhoto)
        m_currentPhoto->saveSidecar();

    Photo *photo = m_library->photoAt(row);
    if (!photo) {
        m_currentPhoto = nullptr;
        m_currentRow = -1;
        m_currentSource.release();
        m_previewSource.release();
        m_imageViewer->clear();
        m_adjustmentsPanel->setEnabled(false);
        updateExportEnabled();
        return;
    }

    m_currentPhoto = photo;
    m_currentRow = row;
    m_currentSource = cv::imread(photo->filePath().toStdString(), cv::IMREAD_COLOR);
    m_previewSource = makePreviewSource(m_currentSource);

    m_adjustmentsPanel->setEnabled(!m_currentSource.empty());
    m_adjustmentsPanel->setParameters(photo->editParameters());
    updateExportEnabled();

    updatePreview(photo->editParameters(), /*fullResolution=*/true);
}

void MainWindow::onPreviewParametersChanged(const EditParameters &params)
{
    // Fast path while a slider is actively being dragged: render from the
    // downscaled preview copy so every mouse-move stays cheap regardless of
    // the original photo's resolution.
    updatePreview(params, /*fullResolution=*/false);
}

void MainWindow::onParametersCommitted(const EditParameters &params)
{
    if (!m_currentPhoto)
        return;
    m_currentPhoto->setEditParameters(params);
    m_currentPhoto->saveSidecar();
    m_thumbnailModel->invalidateThumbnail(m_currentRow);

    // The slider has settled, so it's worth paying for one full-resolution
    // render to replace the (possibly softer, downscaled) live preview.
    updatePreview(params, /*fullResolution=*/true);
}

void MainWindow::updatePreview(const EditParameters &params, bool fullResolution)
{
    const cv::Mat &source = fullResolution ? m_currentSource : m_previewSource;
    if (source.empty())
        return;
    const cv::Mat rendered = ImageProcessor::apply(source, params);
    m_imageViewer->setImage(ImageConversion::matToQImage(rendered));
}

void MainWindow::onRotateClockwiseRequested()
{
    rotate(true);
}

void MainWindow::onRotateCounterClockwiseRequested()
{
    rotate(false);
}

void MainWindow::rotate(bool clockwise)
{
    if (!m_currentPhoto)
        return;

    const EditParameters current = m_currentPhoto->editParameters();
    const EditParameters rotated = clockwise ? current.rotatedClockwise() : current.rotatedCounterClockwise();

    m_currentPhoto->setEditParameters(rotated);
    m_currentPhoto->saveSidecar();
    m_thumbnailModel->invalidateThumbnail(m_currentRow);

    m_adjustmentsPanel->setParameters(rotated); // brightness/contrast unaffected, but keeps its base params in sync
    updatePreview(rotated, /*fullResolution=*/true);
}

void MainWindow::onCropRequested()
{
    if (!m_currentPhoto || m_currentSource.empty() || m_imageViewer->isCropping())
        return;

    // Show the full (uncropped) rotated frame to crop from - keep rotation
    // and brightness/contrast for reference, but ignore any existing crop
    // while the user picks a new one.
    EditParameters uncropped = m_currentPhoto->editParameters();
    const QRectF previousCrop = uncropped.cropRect;
    uncropped.cropRect = QRectF(0.0, 0.0, 1.0, 1.0);
    const cv::Mat rendered = ImageProcessor::apply(m_currentSource, uncropped);
    m_imageViewer->setImage(ImageConversion::matToQImage(rendered));

    m_imageViewer->beginCropping(previousCrop);
    m_adjustmentsPanel->setCropModeActive(true);
    updateExportEnabled(); // don't export the in-progress, not-yet-applied crop
}

void MainWindow::onCropApplyRequested()
{
    if (!m_currentPhoto)
        return;

    EditParameters params = m_currentPhoto->editParameters();
    params.cropRect = m_imageViewer->currentCropNormalizedRect();

    m_currentPhoto->setEditParameters(params);
    m_currentPhoto->saveSidecar();
    m_thumbnailModel->invalidateThumbnail(m_currentRow);

    m_imageViewer->endCropping();
    m_adjustmentsPanel->setCropModeActive(false);
    updateExportEnabled();
    updatePreview(params, /*fullResolution=*/true);
}

void MainWindow::onCropCancelRequested()
{
    m_imageViewer->endCropping();
    m_adjustmentsPanel->setCropModeActive(false);
    updateExportEnabled();
    if (m_currentPhoto)
        updatePreview(m_currentPhoto->editParameters(), /*fullResolution=*/true);
}

void MainWindow::cancelCropIfActive()
{
    if (m_imageViewer->isCropping())
        onCropCancelRequested();
}

void MainWindow::updateExportEnabled()
{
    // Disabled with no photo loaded, and while an in-progress crop hasn't
    // been applied yet - exporting mid-crop would silently ignore it, since
    // it isn't part of the Photo's EditParameters until "Apply" commits it.
    m_exportButton->setEnabled(m_currentPhoto && !m_currentSource.empty() && !m_imageViewer->isCropping());
}

void MainWindow::onExportRequested()
{
    if (!m_currentPhoto || m_currentSource.empty())
        return;

    const QFileInfo sourceInfo(m_currentPhoto->filePath());
    // Default to a sibling file rather than the original name, so the (never
    // touched otherwise) original isn't one dialog-confirm away from being
    // silently overwritten by the flattened export.
    const QString suggestedPath =
        sourceInfo.absolutePath() + "/" + sourceInfo.completeBaseName() + "_edited." + sourceInfo.suffix();

    const QString path =
        QFileDialog::getSaveFileName(this, tr("Export Photo"), suggestedPath,
                                      tr("JPEG (*.jpg *.jpeg);;PNG (*.png);;BMP (*.bmp);;TIFF (*.tif *.tiff)"));
    if (path.isEmpty())
        return;

    // Always renders from the full-resolution source and the Photo's last
    // *committed* EditParameters - never touches the sidecar, so edits stay
    // just as revisable after exporting as before.
    const cv::Mat rendered = ImageProcessor::apply(m_currentSource, m_currentPhoto->editParameters());
    if (rendered.empty() || !cv::imwrite(path.toStdString(), rendered)) {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not write the exported photo to:\n%1").arg(path));
        return;
    }

    statusBar()->showMessage(tr("Exported to %1").arg(path), 5000);
}
