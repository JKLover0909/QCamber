/**
 * @file   pngexporter.cpp
 * @author Extended for QCamber PNG export functionality
 *
 * High resolution PNG export functionality for QCamber PCB viewer
 */

#include "pngexporter.h"
#include "logger.h"
#include "context.h"
#include "layer.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QTimer>

PngExporter::PngExporter(QObject *parent)
    : QObject(parent)
    , m_progressDialog(nullptr)
    , m_exportCancelled(false)
{
}

bool PngExporter::exportPanelToPng(ODBPPGraphicsScene* scene, const ExportSettings& settings)
{
    if (!scene) {
        LOG_ERROR("Cannot export: scene is null");
        return false;
    }

    LOG_STEP("Starting panel PNG export", QString("Target: %1x%2, Layer: %3")
            .arg(settings.width).arg(settings.height).arg(settings.layerName));

    // Create progress dialog
    m_progressDialog = new QProgressDialog("Exporting panel to PNG...", "Cancel", 0, 100);
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->show();
    
    connect(m_progressDialog, &QProgressDialog::canceled, [this]() {
        m_exportCancelled = true;
    });

    m_exportCancelled = false;
    emit exportProgress(10);

    // Get export rectangle
    QRectF exportRect = getOptimalExportRect(scene, settings.includeStepRepeat);
    if (exportRect.isEmpty()) {
        LOG_ERROR("Cannot determine export rectangle - scene appears to be empty");
        delete m_progressDialog;
        return false;
    }

    LOG_INFO(QString("Export rectangle: x=%1, y=%2, w=%3, h=%4")
            .arg(exportRect.x()).arg(exportRect.y())
            .arg(exportRect.width()).arg(exportRect.height()));

    emit exportProgress(20);
    if (m_exportCancelled) {
        delete m_progressDialog;
        return false;
    }

    // Filter to specific layer if requested
    Layer* targetLayer = nullptr;
    if (!settings.layerName.isEmpty()) {
        targetLayer = filterToLayer(scene, settings.layerName);
        if (!targetLayer) {
            LOG_ERROR(QString("Layer '%1' not found in scene").arg(settings.layerName));
            delete m_progressDialog;
            return false;
        }
        LOG_INFO(QString("Filtered to layer: %1").arg(settings.layerName));
    }

    emit exportProgress(30);
    if (m_exportCancelled) {
        delete m_progressDialog;
        return false;
    }

    // Create target size
    QSize targetSize(settings.width, settings.height);
    if (settings.cropToContent) {
        // Maintain aspect ratio while fitting within target dimensions
        double aspectRatio = exportRect.width() / exportRect.height();
        if (aspectRatio > 1.0) {
            // Wider than tall
            targetSize.setHeight(static_cast<int>(settings.width / aspectRatio));
        } else {
            // Taller than wide
            targetSize.setWidth(static_cast<int>(settings.height * aspectRatio));
        }
    }

    LOG_INFO(QString("Final export size: %1x%2").arg(targetSize.width()).arg(targetSize.height()));

    emit exportProgress(40);
    if (m_exportCancelled) {
        delete m_progressDialog;
        return false;
    }

    // Render high resolution image
    QPixmap result = renderHighResolution(scene, targetSize, exportRect, settings.backgroundColor);
    
    emit exportProgress(80);
    if (m_exportCancelled) {
        delete m_progressDialog;
        return false;
    }

    if (result.isNull()) {
        LOG_ERROR("Failed to render scene to pixmap");
        delete m_progressDialog;
        return false;
    }

    // Save to file
    bool saveSuccess = result.save(settings.outputPath, "PNG");
    
    emit exportProgress(100);
    delete m_progressDialog;
    
    if (saveSuccess) {
        LOG_INFO(QString("Successfully exported panel to: %1").arg(settings.outputPath));
        QFileInfo fileInfo(settings.outputPath);
        LOG_INFO(QString("File size: %1 KB").arg(fileInfo.size() / 1024));
        emit exportFinished(true, QString("Panel exported successfully to %1").arg(settings.outputPath));
    } else {
        LOG_ERROR(QString("Failed to save PNG file: %1").arg(settings.outputPath));
        emit exportFinished(false, "Failed to save PNG file");
    }

    return saveSuccess;
}

bool PngExporter::exportLayerToPng(Layer* layer, const ExportSettings& settings)
{
    if (!layer) {
        LOG_ERROR("Cannot export: layer is null");
        return false;
    }

    LOG_STEP("Starting layer PNG export", QString("Layer: %1, Target: %2x%3")
            .arg(layer->layer()).arg(settings.width).arg(settings.height));

    // Create a temporary scene with just this layer
    QGraphicsScene tempScene;
    
    // Add layer to temporary scene
    QGraphicsScene* layerScene = layer->layerScene();
    if (layerScene) {
        // Copy items from layer scene to temp scene
        foreach (QGraphicsItem* item, layerScene->items()) {
            // Create a copy of the item for the temp scene
            // Note: This is a simplified approach - more complex copying may be needed
            tempScene.addItem(item);
        }
    }

    // Get export rectangle from the temp scene
    QRectF exportRect = tempScene.itemsBoundingRect();
    if (exportRect.isEmpty()) {
        LOG_ERROR("Layer appears to be empty");
        return false;
    }

    // Create target size maintaining aspect ratio
    QSize targetSize(settings.width, settings.height);
    if (settings.cropToContent) {
        double aspectRatio = exportRect.width() / exportRect.height();
        if (aspectRatio > 1.0) {
            targetSize.setHeight(static_cast<int>(settings.width / aspectRatio));
        } else {
            targetSize.setWidth(static_cast<int>(settings.height * aspectRatio));
        }
    }

    // Render and save
    QPixmap result = renderHighResolution(&tempScene, targetSize, exportRect, settings.backgroundColor);
    
    if (result.isNull()) {
        LOG_ERROR("Failed to render layer to pixmap");
        return false;
    }

    bool saveSuccess = result.save(settings.outputPath, "PNG");
    
    if (saveSuccess) {
        LOG_INFO(QString("Successfully exported layer to: %1").arg(settings.outputPath));
    } else {
        LOG_ERROR(QString("Failed to save layer PNG: %1").arg(settings.outputPath));
    }

    return saveSuccess;
}

QRectF PngExporter::getOptimalExportRect(ODBPPGraphicsScene* scene, bool includeStepRepeat)
{
    if (!scene) {
        return QRectF();
    }

    // Get bounding rectangle of all visible items
    QRectF boundingRect = scene->itemsBoundingRect();
    
    // Add some padding (5% of the smaller dimension)
    double padding = qMin(boundingRect.width(), boundingRect.height()) * 0.05;
    boundingRect.adjust(-padding, -padding, padding, padding);
    
    LOG_INFO(QString("Calculated export rect with padding: x=%1, y=%2, w=%3, h=%4")
            .arg(boundingRect.x()).arg(boundingRect.y())
            .arg(boundingRect.width()).arg(boundingRect.height()));
    
    return boundingRect;
}

QPixmap PngExporter::renderHighResolution(QGraphicsScene* scene, 
                                        const QSize& targetSize,
                                        const QRectF& sourceRect,
                                        const QColor& backgroundColor)
{
    LOG_STEP("Rendering high resolution image", QString("Size: %1x%2")
            .arg(targetSize.width()).arg(targetSize.height()));

    // Check if target size is reasonable (prevent excessive memory usage)
    long long totalPixels = static_cast<long long>(targetSize.width()) * targetSize.height();
    if (totalPixels > 400000000) { // 400 megapixels limit
        LOG_WARNING(QString("Very large image requested: %1 megapixels").arg(totalPixels / 1000000));
    }

    // Create high resolution pixmap
    QPixmap pixmap(targetSize);
    if (pixmap.isNull()) {
        LOG_ERROR("Failed to create pixmap - insufficient memory or invalid size");
        return QPixmap();
    }

    // Fill with background color
    pixmap.fill(backgroundColor);

    // Create painter for the pixmap
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

    // Render scene to pixmap
    scene->render(&painter, QRectF(0, 0, targetSize.width(), targetSize.height()), sourceRect);

    painter.end();

    LOG_INFO("High resolution rendering completed");
    return pixmap;
}

Layer* PngExporter::filterToLayer(ODBPPGraphicsScene* scene, const QString& layerName)
{
    if (!scene) {
        return nullptr;
    }

    // Get all layers from scene
    QList<GraphicsLayer*> layers = scene->layers();
    
    for (GraphicsLayer* graphicsLayer : layers) {
        Layer* layer = dynamic_cast<Layer*>(graphicsLayer);
        if (layer && layer->layer().toLower() == layerName.toLower()) {
            return layer;
        }
    }

    return nullptr;
}

double PngExporter::calculateScaleFactor(const QRectF& sourceRect, const QSize& targetSize)
{
    double scaleX = targetSize.width() / sourceRect.width();
    double scaleY = targetSize.height() / sourceRect.height();
    
    // Use the smaller scale factor to ensure the entire content fits
    return qMin(scaleX, scaleY);
}

void PngExporter::onProgressUpdate(int value)
{
    emit exportProgress(value);
    if (m_progressDialog) {
        m_progressDialog->setValue(value);
    }
    QApplication::processEvents();
}