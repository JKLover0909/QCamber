/**
 * @file   exportdialog.cpp
 * @author Extended for QCamber export functionality
 *
 * Export dialog for PNG export configuration
 */

#include "exportdialog.h"
#include "logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>

ExportDialog::ExportDialog(QWidget *parent)
    : QDialog(parent)
    , m_backgroundColor(Qt::black)
    , m_defaultOutputDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))
{
    setWindowTitle("Export Panel to PNG");
    setModal(true);
    resize(500, 600);

    // Initialize resolution presets
    m_resolutionPresets = {
        {"Ultra HD 4K (3840x2160)", 3840, 2160},
        {"Full HD (1920x1080)", 1920, 1080},
        {"HD (1280x720)", 1280, 720},
        {"High Resolution (10000x10000)", 10000, 10000},
        {"Ultra High (20000x20000)", 20000, 20000},
        {"Custom", 0, 0}
    };

    setupUI();
    setupConnections();
    updateUI();
}

ExportDialog::~ExportDialog()
{
}

void ExportDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Output settings group
    QGroupBox* outputGroup = new QGroupBox("Output Settings");
    QGridLayout* outputLayout = new QGridLayout(outputGroup);
    
    outputLayout->addWidget(new QLabel("Output Path:"), 0, 0);
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText("Select output PNG file path...");
    outputLayout->addWidget(m_outputPathEdit, 0, 1);
    
    m_browseButton = new QPushButton("Browse...");
    outputLayout->addWidget(m_browseButton, 0, 2);

    // Resolution settings group
    QGroupBox* resolutionGroup = new QGroupBox("Resolution Settings");
    QGridLayout* resolutionLayout = new QGridLayout(resolutionGroup);
    
    resolutionLayout->addWidget(new QLabel("Preset:"), 0, 0);
    m_resolutionPresetCombo = new QComboBox();
    for (const auto& preset : m_resolutionPresets) {
        m_resolutionPresetCombo->addItem(preset.name);
    }
    m_resolutionPresetCombo->setCurrentIndex(4); // Default to Ultra High (20000x20000)
    resolutionLayout->addWidget(m_resolutionPresetCombo, 0, 1, 1, 2);
    
    m_customResolutionCheck = new QCheckBox("Custom Resolution");
    resolutionLayout->addWidget(m_customResolutionCheck, 1, 0, 1, 3);
    
    resolutionLayout->addWidget(new QLabel("Width:"), 2, 0);
    m_widthSpinBox = new QSpinBox();
    m_widthSpinBox->setRange(100, 50000);
    m_widthSpinBox->setValue(20000);
    m_widthSpinBox->setSuffix(" px");
    m_widthSpinBox->setEnabled(false);
    resolutionLayout->addWidget(m_widthSpinBox, 2, 1);
    
    resolutionLayout->addWidget(new QLabel("Height:"), 2, 2);
    m_heightSpinBox = new QSpinBox();
    m_heightSpinBox->setRange(100, 50000);
    m_heightSpinBox->setValue(20000);
    m_heightSpinBox->setSuffix(" px");
    m_heightSpinBox->setEnabled(false);
    resolutionLayout->addWidget(m_heightSpinBox, 2, 3);
    
    resolutionLayout->addWidget(new QLabel("DPI:"), 3, 0);
    m_dpiSpinBox = new QSpinBox();
    m_dpiSpinBox->setRange(72, 2400);
    m_dpiSpinBox->setValue(300);
    resolutionLayout->addWidget(m_dpiSpinBox, 3, 1);

    // Layer settings group
    QGroupBox* layerGroup = new QGroupBox("Layer Settings");
    QGridLayout* layerLayout = new QGridLayout(layerGroup);
    
    layerLayout->addWidget(new QLabel("Target Layer:"), 0, 0);
    m_layerCombo = new QComboBox();
    m_layerCombo->addItem("All Layers", "");
    m_layerCombo->addItem("L1 (Top)", "L1");
    m_layerCombo->addItem("L2 (Inner)", "L2");
    m_layerCombo->addItem("L3 (Inner)", "L3");
    m_layerCombo->addItem("L4 (Bottom)", "L4");
    m_layerCombo->setCurrentIndex(2); // Default to L2
    layerLayout->addWidget(m_layerCombo, 0, 1);

    m_includeStepRepeatCheck = new QCheckBox("Include Step & Repeat (Panel)");
    m_includeStepRepeatCheck->setChecked(true);
    layerLayout->addWidget(m_includeStepRepeatCheck, 1, 0, 1, 2);

    m_cropToContentCheck = new QCheckBox("Crop to Content");
    m_cropToContentCheck->setChecked(true);
    layerLayout->addWidget(m_cropToContentCheck, 2, 0, 1, 2);

    // Appearance settings group
    QGroupBox* appearanceGroup = new QGroupBox("Appearance Settings");
    QGridLayout* appearanceLayout = new QGridLayout(appearanceGroup);
    
    appearanceLayout->addWidget(new QLabel("Background Color:"), 0, 0);
    m_backgroundColorButton = new QPushButton();
    m_backgroundColorButton->setFixedSize(100, 30);
    updateBackgroundColorButton();
    appearanceLayout->addWidget(m_backgroundColorButton, 0, 1);
    
    m_backgroundColorLabel = new QLabel("Black");
    appearanceLayout->addWidget(m_backgroundColorLabel, 0, 2);

    // File size estimate
    QGroupBox* infoGroup = new QGroupBox("Information");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
    
    m_fileSizeLabel = new QLabel("Estimated file size: ~400 MB");
    m_fileSizeLabel->setStyleSheet("color: #666666;");
    infoLayout->addWidget(m_fileSizeLabel);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);

    // Dialog buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* exportButton = new QPushButton("Export");
    QPushButton* cancelButton = new QPushButton("Cancel");
    
    exportButton->setDefault(true);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(cancelButton);

    // Add all groups to main layout
    mainLayout->addWidget(outputGroup);
    mainLayout->addWidget(resolutionGroup);
    mainLayout->addWidget(layerGroup);
    mainLayout->addWidget(appearanceGroup);
    mainLayout->addWidget(infoGroup);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addLayout(buttonLayout);

    // Connect dialog buttons
    connect(exportButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void ExportDialog::setupConnections()
{
    connect(m_browseButton, &QPushButton::clicked, this, &ExportDialog::onBrowseOutputPath);
    connect(m_backgroundColorButton, &QPushButton::clicked, this, &ExportDialog::onBackgroundColorChange);
    connect(m_resolutionPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::onResolutionPresetChanged);
    connect(m_customResolutionCheck, &QCheckBox::toggled, this, &ExportDialog::onCustomResolutionToggled);
    
    // Update file size estimate when parameters change
    connect(m_widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExportDialog::updateEstimatedFileSize);
    connect(m_heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExportDialog::updateEstimatedFileSize);
}

void ExportDialog::updateUI()
{
    // Set default output path
    QString defaultFileName = QString("PCB_Panel_L2_%1x%2.png")
                                .arg(m_widthSpinBox->value())
                                .arg(m_heightSpinBox->value());
    m_outputPathEdit->setText(QDir(m_defaultOutputDir).filePath(defaultFileName));
    
    updateEstimatedFileSize();
}

void ExportDialog::onBrowseOutputPath()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save PNG Export",
        m_outputPathEdit->text(),
        "PNG Images (*.png);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        m_outputPathEdit->setText(fileName);
    }
}

void ExportDialog::onBackgroundColorChange()
{
    QColor color = QColorDialog::getColor(m_backgroundColor, this, "Select Background Color");
    if (color.isValid()) {
        m_backgroundColor = color;
        updateBackgroundColorButton();
        m_backgroundColorLabel->setText(color.name());
    }
}

void ExportDialog::onResolutionPresetChanged()
{
    int index = m_resolutionPresetCombo->currentIndex();
    if (index >= 0 && index < m_resolutionPresets.size()) {
        const ResolutionPreset& preset = m_resolutionPresets[index];
        
        if (preset.width > 0 && preset.height > 0) {
            m_widthSpinBox->setValue(preset.width);
            m_heightSpinBox->setValue(preset.height);
            m_customResolutionCheck->setChecked(false);
        } else {
            // Custom preset selected
            m_customResolutionCheck->setChecked(true);
        }
    }
    updateEstimatedFileSize();
}

void ExportDialog::onCustomResolutionToggled(bool enabled)
{
    m_widthSpinBox->setEnabled(enabled);
    m_heightSpinBox->setEnabled(enabled);
    
    if (enabled) {
        m_resolutionPresetCombo->setCurrentIndex(m_resolutionPresets.size() - 1); // Custom
    }
}

void ExportDialog::updateEstimatedFileSize()
{
    // Estimate PNG file size (rough calculation)
    // Assume ~3 bytes per pixel (RGB) with PNG compression (~30% reduction)
    long long pixels = static_cast<long long>(m_widthSpinBox->value()) * m_heightSpinBox->value();
    long long estimatedBytes = pixels * 3 * 0.7; // 30% compression
    
    m_fileSizeLabel->setText(QString("Estimated file size: %1").arg(formatFileSize(estimatedBytes)));
    
    // Update default filename
    QString currentPath = m_outputPathEdit->text();
    QFileInfo fileInfo(currentPath);
    QString newFileName = QString("PCB_Panel_L2_%1x%2.png")
                            .arg(m_widthSpinBox->value())
                            .arg(m_heightSpinBox->value());
    QString newPath = QDir(fileInfo.absolutePath()).filePath(newFileName);
    m_outputPathEdit->setText(newPath);
}

void ExportDialog::updateBackgroundColorButton()
{
    QString styleSheet = QString("background-color: %1; border: 1px solid #888888;")
                          .arg(m_backgroundColor.name());
    m_backgroundColorButton->setStyleSheet(styleSheet);
}

QString ExportDialog::formatFileSize(long long bytes) const
{
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }
    
    return QString("%1 %2").arg(QString::number(size, 'f', 1)).arg(units[unitIndex]);
}

PngExporter::ExportSettings ExportDialog::getExportSettings() const
{
    PngExporter::ExportSettings settings;
    
    settings.width = m_widthSpinBox->value();
    settings.height = m_heightSpinBox->value();
    settings.outputPath = m_outputPathEdit->text();
    settings.layerName = m_layerCombo->currentData().toString();
    settings.includeStepRepeat = m_includeStepRepeatCheck->isChecked();
    settings.backgroundColor = m_backgroundColor;
    settings.dpi = m_dpiSpinBox->value();
    settings.cropToContent = m_cropToContentCheck->isChecked();
    
    return settings;
}

void ExportDialog::setAvailableLayers(const QStringList& layers)
{
    m_layerCombo->clear();
    m_layerCombo->addItem("All Layers", "");
    
    for (const QString& layer : layers) {
        m_layerCombo->addItem(layer, layer);
    }
    
    // Try to select L2 if available
    int l2Index = m_layerCombo->findData("L2");
    if (l2Index >= 0) {
        m_layerCombo->setCurrentIndex(l2Index);
    }
}

void ExportDialog::setDefaultOutputDir(const QString& dir)
{
    m_defaultOutputDir = dir;
    updateUI();
}

void ExportDialog::onExportProgress(int percentage)
{
    m_progressBar->setVisible(true);
    m_progressBar->setValue(percentage);
}

void ExportDialog::onExportFinished(bool success, const QString& message)
{
    m_progressBar->setVisible(false);
    
    if (success) {
        QMessageBox::information(this, "Export Complete", message);
        accept();
    } else {
        QMessageBox::warning(this, "Export Failed", message);
    }
}