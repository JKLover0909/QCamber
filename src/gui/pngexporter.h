/**
 * @file   pngexporter.h
 * @author Extended for QCamber PNG export functionality
 *
 * High resolution PNG export functionality for QCamber PCB viewer
 */

#ifndef __PNGEXPORTER_H__
#define __PNGEXPORTER_H__

#include <QObject>
#include <QPixmap>
#include <QGraphicsScene>
#include <QString>
#include <QRectF>
#include <QProgressDialog>
#include <QSize>
#include <QColor>

#include "odbppgraphicsscene.h"

// Forward declarations
class Layer;
class ODBPPGraphicsScene;

class PngExporter : public QObject
{
    Q_OBJECT

public:
    explicit PngExporter(QObject *parent = nullptr);
    
    struct ExportSettings {
        int width = 20000;          // Target width in pixels
        int height = 20000;         // Target height in pixels
        QString outputPath;         // Output file path
        QString layerName = "L2";   // Target layer name (e.g., "L2", "L1", etc.)
        bool includeStepRepeat = true;  // Include panel step repeat
        QColor backgroundColor = Qt::black;  // Background color
        double dpi = 300.0;         // DPI for export
        bool cropToContent = true;  // Crop to actual PCB content
    };

    /**
     * Export entire panel with step repeat to PNG
     * @param scene The graphics scene containing the PCB data
     * @param settings Export configuration
     * @return true if export successful
     */
    bool exportPanelToPng(ODBPPGraphicsScene* scene, const ExportSettings& settings);
    
    /**
     * Export specific layer to PNG
     * @param layer The layer to export
     * @param settings Export configuration
     * @return true if export successful
     */
    bool exportLayerToPng(Layer* layer, const ExportSettings& settings);
    
    /**
     * Get optimal export rectangle for panel
     * @param scene The graphics scene
     * @param includeStepRepeat Include step repeat elements
     * @return Optimal bounding rectangle
     */
    QRectF getOptimalExportRect(ODBPPGraphicsScene* scene, bool includeStepRepeat = true);

signals:
    void exportProgress(int percentage);
    void exportFinished(bool success, const QString& message);

private slots:
    void onProgressUpdate(int value);

private:
    /**
     * Render scene to high resolution QPixmap
     * @param scene Scene to render
     * @param targetSize Target pixel dimensions
     * @param sourceRect Source rectangle to render
     * @param backgroundColor Background color
     * @return Rendered pixmap
     */
    QPixmap renderHighResolution(QGraphicsScene* scene, 
                                const QSize& targetSize,
                                const QRectF& sourceRect,
                                const QColor& backgroundColor);
    
    /**
     * Filter and show only specified layer
     * @param scene Scene containing layers
     * @param layerName Target layer name
     * @return Layer pointer if found
     */
    Layer* filterToLayer(ODBPPGraphicsScene* scene, const QString& layerName);
    
    /**
     * Calculate scaling factor for target resolution
     * @param sourceRect Source rectangle in scene coordinates
     * @param targetSize Target pixel dimensions
     * @return Scaling factor
     */
    double calculateScaleFactor(const QRectF& sourceRect, const QSize& targetSize);
    
    QProgressDialog* m_progressDialog;
    bool m_exportCancelled;
};

#endif /* __PNGEXPORTER_H__ */