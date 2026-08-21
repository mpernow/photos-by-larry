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
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>

#include <opencv2/imgcodecs.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_library(new PhotoLibrary(this)),
      m_thumbnailModel(new ThumbnailModel(m_library, this)),
      m_thumbnailPanel(new ThumbnailPanel(this)),
      m_imageViewer(new ImageViewer(this)),
      m_adjustmentsPanel(new AdjustmentsPanel(this))
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

    statusBar();

    connect(m_thumbnailPanel, &ThumbnailPanel::photoSelected, this, &MainWindow::onPhotoSelected);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::previewParametersChanged, this,
            &MainWindow::onPreviewParametersChanged);
    connect(m_adjustmentsPanel, &AdjustmentsPanel::parametersCommitted, this,
            &MainWindow::onParametersCommitted);
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

    if (m_currentPhoto)
        m_currentPhoto->saveSidecar();
    m_currentPhoto = nullptr;
    m_currentSource.release();
    m_imageViewer->clear();
    m_adjustmentsPanel->setEnabled(false);

    m_library->openDirectory(dir);
    statusBar()->showMessage(tr("%1 photo(s) found in %2").arg(m_library->count()).arg(dir), 5000);
}

void MainWindow::onPhotoSelected(int row)
{
    loadPhoto(row);
}

void MainWindow::loadPhoto(int row)
{
    if (m_currentPhoto)
        m_currentPhoto->saveSidecar();

    Photo *photo = m_library->photoAt(row);
    if (!photo) {
        m_currentPhoto = nullptr;
        m_currentSource.release();
        m_imageViewer->clear();
        m_adjustmentsPanel->setEnabled(false);
        return;
    }

    m_currentPhoto = photo;
    m_currentSource = cv::imread(photo->filePath().toStdString(), cv::IMREAD_COLOR);

    m_adjustmentsPanel->setEnabled(!m_currentSource.empty());
    m_adjustmentsPanel->setParameters(photo->editParameters());

    updatePreview(photo->editParameters());
}

void MainWindow::onPreviewParametersChanged(const EditParameters &params)
{
    updatePreview(params);
}

void MainWindow::onParametersCommitted(const EditParameters &params)
{
    if (!m_currentPhoto)
        return;
    m_currentPhoto->setEditParameters(params);
    m_currentPhoto->saveSidecar();
}

void MainWindow::updatePreview(const EditParameters &params)
{
    if (m_currentSource.empty())
        return;
    const cv::Mat rendered = ImageProcessor::apply(m_currentSource, params);
    m_imageViewer->setImage(ImageConversion::matToQImage(rendered));
}
