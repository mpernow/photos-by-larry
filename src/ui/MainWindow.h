#pragma once

#include <QMainWindow>
#include <opencv2/core.hpp>

#include "core/EditParameters.h"

class PhotoLibrary;
class ThumbnailModel;
class ThumbnailPanel;
class ImageViewer;
class AdjustmentsPanel;
class Photo;
class QPushButton;

// Top-level window. Owns the library/model layer and the three UI panels,
// and wires them together: thumbnail selection loads a photo, slider moves
// re-render the preview, and slider release persists the edit to disk.
// Rotate and crop are discrete, immediately-committed actions rather than
// live-preview/commit like the sliders - see AdjustmentsPanel's header.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openDirectory();
    void onPhotoSelected(int row);
    void onPreviewParametersChanged(const EditParameters &params);
    void onParametersCommitted(const EditParameters &params);

    void onRotateClockwiseRequested();
    void onRotateCounterClockwiseRequested();
    void onCropRequested();
    void onCropApplyRequested();
    void onCropCancelRequested();
    void onExportRequested();
    void onCopySettingsRequested();
    void onPasteSettingsRequested();

private:
    void loadPhoto(int row);
    void updatePreview(const EditParameters &params, bool fullResolution);
    void rotate(bool clockwise);
    void cancelCropIfActive();
    void updateExportEnabled();

    PhotoLibrary *m_library;
    ThumbnailModel *m_thumbnailModel;
    ThumbnailPanel *m_thumbnailPanel;
    ImageViewer *m_imageViewer;
    AdjustmentsPanel *m_adjustmentsPanel;
    QPushButton *m_exportButton;

    Photo *m_currentPhoto = nullptr;
    int m_currentRow = -1; // index of m_currentPhoto in the library, for invalidating its thumbnail
    cv::Mat m_currentSource; // decoded, never-modified, full-resolution pixels
    cv::Mat m_previewSource; // downscaled copy of m_currentSource, for fast interactive preview

    EditParameters m_copiedParameters; // last "Copy Settings" snapshot; meaningless unless m_hasCopiedParameters
    bool m_hasCopiedParameters = false;
};
