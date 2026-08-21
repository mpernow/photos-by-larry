#pragma once

#include <QWidget>

#include "core/EditParameters.h"

class QSlider;
class QLabel;
class QPushButton;
class QCheckBox;

// Right-hand dock panel with the editing controls for the currently selected
// photo: brightness/contrast/temperature/tint sliders, rotate buttons, and
// the crop tool.
//
// The sliders follow a live-preview/commit split: previewParametersChanged
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

    // Whether a previous "Copy Settings" has left something to paste. Paste
    // is further gated on not being mid-crop (see setCropModeActive).
    void setPasteSettingsEnabled(bool enabled);

signals:
    void previewParametersChanged(const EditParameters &params);
    void parametersCommitted(const EditParameters &params);

    void rotateCounterClockwiseRequested();
    void rotateClockwiseRequested();

    void cropRequested();
    void cropApplyRequested();
    void cropCancelRequested();
    void keepAspectRatioToggled(bool keep);

    void copySettingsRequested();
    void pasteSettingsRequested();

private:
    EditParameters currentParameters() const;
    void emitPreview();
    void updatePasteButtonEnabled();

    EditParameters m_baseParameters; // last full params set via setParameters(); carries rotation/crop
                                      // forward since only the sliders below have direct controls
    QSlider *m_brightnessSlider;
    QSlider *m_contrastSlider;
    QSlider *m_temperatureSlider;
    QSlider *m_tintSlider;
    QLabel *m_brightnessValueLabel;
    QLabel *m_contrastValueLabel;
    QLabel *m_temperatureValueLabel;
    QLabel *m_tintValueLabel;

    QPushButton *m_rotateLeftButton;
    QPushButton *m_rotateRightButton;
    QPushButton *m_cropButton;
    QPushButton *m_cropApplyButton;
    QPushButton *m_cropCancelButton;
    QCheckBox *m_keepAspectRatioCheckBox;

    QPushButton *m_copySettingsButton;
    QPushButton *m_pasteSettingsButton;
    bool m_pasteAvailable = false; // something has been copied
    bool m_cropModeActive = false;
};
