/**
 * @file   exportdialog.h
 * @author Extended for QCamber export functionality
 *
 * Export dialog for PNG export configuration
 */

#ifndef __EXPORTDIALOG_H__
#define __EXPORTDIALOG_H__

#include <QDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QColorDialog>

#include "pngexporter.h"

namespace Ui {
class ExportDialog;
}

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(QWidget *parent = nullptr);
    ~ExportDialog();

    /**
     * Get export settings from dialog
     * @return Configured export settings
     */
    PngExporter::ExportSettings getExportSettings() const;

    /**
     * Set available layers for selection
     * @param layers List of available layer names
     */
    void setAvailableLayers(const QStringList& layers);

    /**
     * Set default output directory
     * @param dir Default directory path
     */
    void setDefaultOutputDir(const QString& dir);

public slots:
    void onBrowseOutputPath();
    void onBackgroundColorChange();
    void onExportProgress(int percentage);
    void onExportFinished(bool success, const QString& message);

private slots:
    void onResolutionPresetChanged();
    void onCustomResolutionToggled(bool enabled);
    void updateEstimatedFileSize();

private:
    void setupUI();
    void setupConnections();
    void updateUI();
    QString formatFileSize(long long bytes) const;

    // UI Components
    QSpinBox* m_widthSpinBox;
    QSpinBox* m_heightSpinBox;
    QComboBox* m_resolutionPresetCombo;
    QComboBox* m_layerCombo;
    QCheckBox* m_includeStepRepeatCheck;
    QCheckBox* m_cropToContentCheck;
    QCheckBox* m_customResolutionCheck;
    QLineEdit* m_outputPathEdit;
    QPushButton* m_browseButton;
    QPushButton* m_backgroundColorButton;
    QSpinBox* m_dpiSpinBox;
    QProgressBar* m_progressBar;
    QLabel* m_fileSizeLabel;
    QLabel* m_backgroundColorLabel;
    
    QColor m_backgroundColor;
    QString m_defaultOutputDir;
    
    // Predefined resolution presets
    struct ResolutionPreset {
        QString name;
        int width;
        int height;
    };
    QList<ResolutionPreset> m_resolutionPresets;
};

#endif /* __EXPORTDIALOG_H__ */