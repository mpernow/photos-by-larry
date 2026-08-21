#pragma once

#include <QWidget>

#include "core/EditParameters.h"

class QSlider;
class QLabel;
class QPushButton;
class QCheckBox;

// Right-hand dock panel with the editing controls for the currently selected
// photo: brightness/contrast sliders, rotate buttons, and the crop tool.
//
// Brightness/contrast follow a live-preview/commit split: previewParametersChanged
// fires on every slider move (fast, not persisted), parametersCommitted fires
// once the slider is released (persisted). Rotate and crop are discrete
// actions instead, each "committed" the instant it happens, so they get their
// own request signals rather than reusing the slider pattern. Crop is further
// split into entering crop mode, then either applying or cancelling it -
// MainWindow drives the actual crop rectangle (via ImageViewer), and calls
// setCropModeActive() here just to swap which buttons are visible/enabled.
class AdjustmentsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AdjustmentsPanel(QWidget *parent = nullptr);

    void setParameters(const EditParameters &params);
    void setCropModeActive(bool active);

signals:
    void previewParametersChanged(const EditParameters &params);
    void parametersCommitted(const EditParameters &params);

    void rotateCounterClockwiseRequested();
    void rotateClockwiseRequested();

    void cropRequested();
    void cropApplyRequested();
    void cropCancelRequested();
    void keepAspectRatioToggled(bool keep);

private:
    EditParameters currentParameters() const;
    void emitPreview();

    EditParameters m_baseParameters; // last full params set via setParameters(); carries rotation/crop
                                      // forward since only brightness/contrast have sliders
    QSlider *m_brightnessSlider;
    QSlider *m_contrastSlider;
    QLabel *m_brightnessValueLabel;
    QLabel *m_contrastValueLabel;

    QPushButton *m_rotateLeftButton;
    QPushButton *m_rotateRightButton;
    QPushButton *m_cropButton;
    QPushButton *m_cropApplyButton;
    QPushButton *m_cropCancelButton;
    QCheckBox *m_keepAspectRatioCheckBox;
};
