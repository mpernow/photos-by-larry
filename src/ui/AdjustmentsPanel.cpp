#include "AdjustmentsPanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace
{
constexpr int kContrastScale = 100; // slider value 100 <=> contrast 1.0
}

AdjustmentsPanel::AdjustmentsPanel(QWidget *parent)
    : QWidget(parent),
      m_brightnessSlider(new QSlider(Qt::Horizontal, this)),
      m_contrastSlider(new QSlider(Qt::Horizontal, this)),
      m_highlightsSlider(new QSlider(Qt::Horizontal, this)),
      m_shadowsSlider(new QSlider(Qt::Horizontal, this)),
      m_whitesSlider(new QSlider(Qt::Horizontal, this)),
      m_blacksSlider(new QSlider(Qt::Horizontal, this)),
      m_temperatureSlider(new QSlider(Qt::Horizontal, this)),
      m_tintSlider(new QSlider(Qt::Horizontal, this)),
      m_vibranceSlider(new QSlider(Qt::Horizontal, this)),
      m_saturationSlider(new QSlider(Qt::Horizontal, this)),
      m_brightnessValueLabel(new QLabel(this)),
      m_contrastValueLabel(new QLabel(this)),
      m_highlightsValueLabel(new QLabel(this)),
      m_shadowsValueLabel(new QLabel(this)),
      m_whitesValueLabel(new QLabel(this)),
      m_blacksValueLabel(new QLabel(this)),
      m_temperatureValueLabel(new QLabel(this)),
      m_tintValueLabel(new QLabel(this)),
      m_vibranceValueLabel(new QLabel(this)),
      m_saturationValueLabel(new QLabel(this)),
      m_rotateLeftButton(new QPushButton(tr("Rotate Left"), this)),
      m_rotateRightButton(new QPushButton(tr("Rotate Right"), this)),
      m_cropButton(new QPushButton(tr("Crop"), this)),
      m_cropApplyButton(new QPushButton(tr("Apply"), this)),
      m_cropCancelButton(new QPushButton(tr("Cancel"), this)),
      m_keepAspectRatioCheckBox(new QCheckBox(tr("Keep aspect ratio"), this)),
      m_copySettingsButton(new QPushButton(tr("Copy Settings"), this)),
      m_pasteSettingsButton(new QPushButton(tr("Paste Settings"), this))
{
    m_brightnessSlider->setRange(-100, 100);
    m_contrastSlider->setRange(0, 300);
    m_highlightsSlider->setRange(-100, 100);
    m_shadowsSlider->setRange(-100, 100);
    m_whitesSlider->setRange(-100, 100);
    m_blacksSlider->setRange(-100, 100);
    m_temperatureSlider->setRange(-100, 100);
    m_tintSlider->setRange(-100, 100);
    m_vibranceSlider->setRange(-100, 100);
    m_saturationSlider->setRange(-100, 100);

    auto *sliderLayout = new QFormLayout;
    sliderLayout->addRow(tr("Brightness"), m_brightnessSlider);
    sliderLayout->addRow(QString(), m_brightnessValueLabel);
    sliderLayout->addRow(tr("Contrast"), m_contrastSlider);
    sliderLayout->addRow(QString(), m_contrastValueLabel);
    sliderLayout->addRow(tr("Highlights"), m_highlightsSlider);
    sliderLayout->addRow(QString(), m_highlightsValueLabel);
    sliderLayout->addRow(tr("Shadows"), m_shadowsSlider);
    sliderLayout->addRow(QString(), m_shadowsValueLabel);
    sliderLayout->addRow(tr("Whites"), m_whitesSlider);
    sliderLayout->addRow(QString(), m_whitesValueLabel);
    sliderLayout->addRow(tr("Blacks"), m_blacksSlider);
    sliderLayout->addRow(QString(), m_blacksValueLabel);
    sliderLayout->addRow(tr("Temperature"), m_temperatureSlider);
    sliderLayout->addRow(QString(), m_temperatureValueLabel);
    sliderLayout->addRow(tr("Tint"), m_tintSlider);
    sliderLayout->addRow(QString(), m_tintValueLabel);
    sliderLayout->addRow(tr("Vibrance"), m_vibranceSlider);
    sliderLayout->addRow(QString(), m_vibranceValueLabel);
    sliderLayout->addRow(tr("Saturation"), m_saturationSlider);
    sliderLayout->addRow(QString(), m_saturationValueLabel);

    auto *rotateLayout = new QHBoxLayout;
    rotateLayout->addWidget(m_rotateLeftButton);
    rotateLayout->addWidget(m_rotateRightButton);

    m_cropApplyButton->setVisible(false);
    m_cropCancelButton->setVisible(false);
    m_keepAspectRatioCheckBox->setVisible(false);
    auto *cropLayout = new QHBoxLayout;
    cropLayout->addWidget(m_cropButton);
    cropLayout->addWidget(m_cropApplyButton);
    cropLayout->addWidget(m_cropCancelButton);

    m_pasteSettingsButton->setEnabled(false); // nothing copied yet
    auto *settingsLayout = new QHBoxLayout;
    settingsLayout->addWidget(m_copySettingsButton);
    settingsLayout->addWidget(m_pasteSettingsButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(sliderLayout);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(new QLabel(tr("Rotate"), this));
    mainLayout->addLayout(rotateLayout);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(new QLabel(tr("Crop"), this));
    mainLayout->addLayout(cropLayout);
    mainLayout->addWidget(m_keepAspectRatioCheckBox);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(new QLabel(tr("Settings"), this));
    mainLayout->addLayout(settingsLayout);
    mainLayout->addStretch(1);

    setParameters(EditParameters{});
    setEnabled(false); // no photo selected yet; QWidget::setEnabled disables the whole tree

    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int value) {
        m_brightnessValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_contrastSlider, &QSlider::valueChanged, this, [this](int value) {
        m_contrastValueLabel->setText(QString::number(value / double(kContrastScale), 'f', 2));
        emitPreview();
    });
    connect(m_highlightsSlider, &QSlider::valueChanged, this, [this](int value) {
        m_highlightsValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_shadowsSlider, &QSlider::valueChanged, this, [this](int value) {
        m_shadowsValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_whitesSlider, &QSlider::valueChanged, this, [this](int value) {
        m_whitesValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_blacksSlider, &QSlider::valueChanged, this, [this](int value) {
        m_blacksValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_temperatureSlider, &QSlider::valueChanged, this, [this](int value) {
        m_temperatureValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_tintSlider, &QSlider::valueChanged, this, [this](int value) {
        m_tintValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_vibranceSlider, &QSlider::valueChanged, this, [this](int value) {
        m_vibranceValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_saturationSlider, &QSlider::valueChanged, this, [this](int value) {
        m_saturationValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_brightnessSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_contrastSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_highlightsSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_shadowsSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_whitesSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_blacksSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_temperatureSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_tintSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_vibranceSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_saturationSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });

    connect(m_rotateLeftButton, &QPushButton::clicked, this,
            &AdjustmentsPanel::rotateCounterClockwiseRequested);
    connect(m_rotateRightButton, &QPushButton::clicked, this, &AdjustmentsPanel::rotateClockwiseRequested);

    connect(m_cropButton, &QPushButton::clicked, this, &AdjustmentsPanel::cropRequested);
    connect(m_cropApplyButton, &QPushButton::clicked, this, &AdjustmentsPanel::cropApplyRequested);
    connect(m_cropCancelButton, &QPushButton::clicked, this, &AdjustmentsPanel::cropCancelRequested);

    connect(m_keepAspectRatioCheckBox, &QCheckBox::toggled, this, &AdjustmentsPanel::keepAspectRatioToggled);

    connect(m_copySettingsButton, &QPushButton::clicked, this, &AdjustmentsPanel::copySettingsRequested);
    connect(m_pasteSettingsButton, &QPushButton::clicked, this, &AdjustmentsPanel::pasteSettingsRequested);
}

void AdjustmentsPanel::setParameters(const EditParameters &params)
{
    // currentParameters() below needs to carry rotation/crop forward even
    // though those don't have sliders - otherwise committing a slider change
    // would silently reset any existing rotate/crop.
    m_baseParameters = params;

    const QSignalBlocker blockBrightness(m_brightnessSlider);
    const QSignalBlocker blockContrast(m_contrastSlider);
    const QSignalBlocker blockHighlights(m_highlightsSlider);
    const QSignalBlocker blockShadows(m_shadowsSlider);
    const QSignalBlocker blockWhites(m_whitesSlider);
    const QSignalBlocker blockBlacks(m_blacksSlider);
    const QSignalBlocker blockTemperature(m_temperatureSlider);
    const QSignalBlocker blockTint(m_tintSlider);
    const QSignalBlocker blockVibrance(m_vibranceSlider);
    const QSignalBlocker blockSaturation(m_saturationSlider);

    m_brightnessSlider->setValue(static_cast<int>(params.brightness));
    m_contrastSlider->setValue(static_cast<int>(params.contrast * kContrastScale));
    m_highlightsSlider->setValue(static_cast<int>(params.highlights));
    m_shadowsSlider->setValue(static_cast<int>(params.shadows));
    m_whitesSlider->setValue(static_cast<int>(params.whites));
    m_blacksSlider->setValue(static_cast<int>(params.blacks));
    m_temperatureSlider->setValue(static_cast<int>(params.temperature));
    m_tintSlider->setValue(static_cast<int>(params.tint));
    m_vibranceSlider->setValue(static_cast<int>(params.vibrance));
    m_saturationSlider->setValue(static_cast<int>(params.saturation));

    m_brightnessValueLabel->setText(QString::number(m_brightnessSlider->value()));
    m_contrastValueLabel->setText(QString::number(params.contrast, 'f', 2));
    m_highlightsValueLabel->setText(QString::number(m_highlightsSlider->value()));
    m_shadowsValueLabel->setText(QString::number(m_shadowsSlider->value()));
    m_whitesValueLabel->setText(QString::number(m_whitesSlider->value()));
    m_blacksValueLabel->setText(QString::number(m_blacksSlider->value()));
    m_temperatureValueLabel->setText(QString::number(m_temperatureSlider->value()));
    m_tintValueLabel->setText(QString::number(m_tintSlider->value()));
    m_vibranceValueLabel->setText(QString::number(m_vibranceSlider->value()));
    m_saturationValueLabel->setText(QString::number(m_saturationSlider->value()));
}

void AdjustmentsPanel::setCropModeActive(bool active)
{
    m_cropButton->setVisible(!active);
    m_cropApplyButton->setVisible(active);
    m_cropCancelButton->setVisible(active);
    m_keepAspectRatioCheckBox->setVisible(active);

    // Adjusting any slider, rotating, or pasting mid-crop would re-render
    // the background from under the overlay the user is actively
    // positioning (or, for rotate, change the image's dimensions entirely),
    // so lock those out until they're done. Copying is read-only - it
    // doesn't touch the photo or the overlay - so it stays available.
    m_brightnessSlider->setEnabled(!active);
    m_contrastSlider->setEnabled(!active);
    m_highlightsSlider->setEnabled(!active);
    m_shadowsSlider->setEnabled(!active);
    m_whitesSlider->setEnabled(!active);
    m_blacksSlider->setEnabled(!active);
    m_temperatureSlider->setEnabled(!active);
    m_tintSlider->setEnabled(!active);
    m_vibranceSlider->setEnabled(!active);
    m_saturationSlider->setEnabled(!active);
    m_rotateLeftButton->setEnabled(!active);
    m_rotateRightButton->setEnabled(!active);

    m_cropModeActive = active;
    updatePasteButtonEnabled();
}

void AdjustmentsPanel::setPasteSettingsEnabled(bool enabled)
{
    m_pasteAvailable = enabled;
    updatePasteButtonEnabled();
}

void AdjustmentsPanel::updatePasteButtonEnabled()
{
    m_pasteSettingsButton->setEnabled(m_pasteAvailable && !m_cropModeActive);
}

EditParameters AdjustmentsPanel::currentParameters() const
{
    EditParameters params = m_baseParameters;
    params.brightness = m_brightnessSlider->value();
    params.contrast = m_contrastSlider->value() / double(kContrastScale);
    params.highlights = m_highlightsSlider->value();
    params.shadows = m_shadowsSlider->value();
    params.whites = m_whitesSlider->value();
    params.blacks = m_blacksSlider->value();
    params.temperature = m_temperatureSlider->value();
    params.tint = m_tintSlider->value();
    params.vibrance = m_vibranceSlider->value();
    params.saturation = m_saturationSlider->value();
    return params;
}

void AdjustmentsPanel::emitPreview()
{
    emit previewParametersChanged(currentParameters());
}
