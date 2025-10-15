/**
 * @file   viewerwindow.cpp
 * @author Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 *
 * Copyright (C) 2012 - 2014 Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 * All Rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "viewerwindow.h"
#include "ui_viewerwindow.h"

#include <QtWidgets>
#include <QDebug>
#include <QDir>
#include <QBuffer>
#include <QPixmap>
#include <QTimer>
#include <QDateTime>
#include <QEventLoop>

#include "context.h"
#include "gotocoordinatedialog.h"
#include "layerinfobox.h"
#include "logger.h"
#include "settingsdialog.h"
#include "settings.h"
#include "restapi/restapiserver.h"


ViewerWindow::ViewerWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::ViewerWindow), m_displayUnit(U_INCH),
  m_activeInfoBox(NULL), m_transition(false), m_restApiServer(nullptr)
{
  ui->setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);

  loadColorConfig();

  m_cursorCoordLabel = new QLabel();
  m_featureDetailLabel = new QLabel();
  m_featureDetailLabel->setAlignment(Qt::AlignVCenter);
  m_cursorCoordLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
  statusBar()->addPermanentWidget(m_featureDetailLabel);
  statusBar()->addPermanentWidget(m_cursorCoordLabel, 1);

  QComboBox* unitCombo = new QComboBox;
  unitCombo->addItem("Inch");
  unitCombo->addItem("MM");
  statusBar()->addPermanentWidget(unitCombo);

  m_featurePropertiesDialog = new FeaturePropertiesDialog(this);
  m_goToCoordinateDialog = new GoToCoordinateDialog(this);

  connect(unitCombo, SIGNAL(currentIndexChanged(int)), this,
      SLOT(unitChanged(int)));

  connect(ui->viewWidget->scene(), SIGNAL(mouseMove(QPointF)), this,
      SLOT(updateCursorCoord(QPointF)));
  connect(ui->viewWidget->scene(), SIGNAL(measureRectSelected(QRectF)), this,
      SLOT(updateMeasureResult(QRectF)));
  connect(ui->viewWidget->scene(), SIGNAL(featureSelected(Symbol*)), this,
      SLOT(updateFeatureDetail(Symbol*)));
  connect(ui->viewWidget->scene(), SIGNAL(featureSelected(Symbol*)),
      m_featurePropertiesDialog, SLOT(update(Symbol*)));

  connect(ui->miniMapView, SIGNAL(minimapRectSelected(QRectF)), ui->viewWidget,
      SLOT(zoomToRect(QRectF)));
  connect(ui->viewWidget, SIGNAL(sceneRectChanged(QRectF)), ui->miniMapView,
      SLOT(redrawSceneRect(QRectF)));

  // bgColorChanged signal
  connect(this, SIGNAL(bgColorChanged(QColor)), ui->viewWidget,
      SLOT(setBackgroundColor(QColor)));
  connect(this, SIGNAL(bgColorChanged(QColor)), ui->miniMapView,
      SLOT(setBackgroundColor(QColor)));

  ui->viewWidget->setFocus(Qt::MouseFocusReason);
  ui->actionAreaZoom->setChecked(true);
  startRestApiServer(8686);
}

ViewerWindow::~ViewerWindow()
{
  delete ui;
  delete m_featurePropertiesDialog;
  delete m_goToCoordinateDialog;
}

void ViewerWindow::setJob(QString job)
{
  m_job = job;
}

void ViewerWindow::setStep(QString step)
{
  m_step = step;
  setWindowTitle(QString("CAMViewer::%1::%2").arg(m_job).arg(m_step));
}

void ViewerWindow::setLayers(const QStringList& layers,
    const QStringList& types)
{
  ui->viewWidget->clearScene();
  ui->viewWidget->loadProfile(m_step);
  ui->miniMapView->loadProfile(m_step);

  QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->scrollWidget->layout());
  clearLayout(layout, true);
  QString pathTmpl = "steps/%1/layers/%2";

  for (int i = 0; i < layers.count(); ++i) {
    LayerInfoBox *l = new LayerInfoBox(layers[i], m_step, types[i]);

    connect(l, SIGNAL(toggled(bool)), this, SLOT(toggleShowLayer(bool)));
    connect(l, SIGNAL(activated(bool)), this, SLOT(layerActivated(bool)));

    m_SelectorMap[layers[i]] = l;
    layout->addWidget(l);
  }
  layout->addStretch();
}

void ViewerWindow::clearLayout(QLayout* layout, bool deleteWidgets)
{
  while (QLayoutItem* item = layout->takeAt(0))
  {
    if (deleteWidgets)
    {
      if (QWidget* widget = item->widget())
        delete widget;
    }
    else if (QLayout* childLayout = item->layout())
      clearLayout(childLayout, deleteWidgets);
    delete item;
  }
}

void ViewerWindow::showLayer(QString name)
{
  LayerInfoBox* infobox = m_SelectorMap[name];
  infobox->toggle();
}

void ViewerWindow::show(void)
{
  QMainWindow::show();
  ui->viewWidget->initialZoom();
  ui->miniMapView->zoomToAll();
}

void ViewerWindow::toggleShowLayer(bool selected)
{
  LayerInfoBox* infobox = dynamic_cast<LayerInfoBox*>(sender());
  if (!selected) {
    ui->viewWidget->addLayer(infobox->layer());
    infobox->setColor(nextColor());
    infobox->layer()->setShowOutline(ui->actionShowOutline->isChecked());
    infobox->layer()->setShowStepRepeat(ui->actionShowStepRepeat->isChecked());

    m_visibles.append(infobox);
    if (m_visibles.size() == 1) {
      infobox->setActive(true);
    }
  } else {
    int index = m_colors.indexOf(infobox->color());
    m_colorsMap[index] = false;
    ui->viewWidget->removeLayer(infobox->layer());
    m_visibles.removeOne(infobox);

    if (infobox->isActive()) {
      if (m_visibles.size()) {
        m_visibles.last()->setActive(true);
      }
    }
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::layerActivated(bool status)
{
  LayerInfoBox* infobox = dynamic_cast<LayerInfoBox*>(sender());
  if (status) {
    if (m_activeInfoBox && m_activeInfoBox != infobox) {
      m_activeInfoBox->setActive(false);
    }
    m_activeInfoBox = infobox;
    if (ui->actionHighlight->isChecked()) {
      m_activeInfoBox->layer()->setHighlightEnabled(true);
    }
  } else {
    infobox->layer()->setHighlightEnabled(false);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

QColor ViewerWindow::nextColor(void)
{
  for (int i = 0; i < m_colors.size(); ++i) {
    if (!m_colorsMap[i]) {
      m_colorsMap[i] = true;
      return m_colors[i];
    }
  }
  return Qt::red;
}

void ViewerWindow::loadColorConfig()
{
  ctx.bg_color = QColor(SETTINGS->get("color", "BG").toString());

  m_colors.clear();
  for(int i = 0; i < 6; ++i) {
    m_colors << QColor(SETTINGS->get("Color",
          QString("C%1").arg(i + 1)).toString());
  }

  for (int i = 0; i < m_colors.size(); ++i) {
    m_colorsMap[i] = false;
  }

  for (int i = 0; i < m_visibles.size(); ++i) {
    m_visibles[i]->setColor(nextColor());
    m_visibles[i]->layer()->forceUpdate();
  }

  emit bgColorChanged(ctx.bg_color);
}

void ViewerWindow::unitChanged(int index)
{
  m_displayUnit = (DisplayUnit)index;
}

void ViewerWindow::updateCursorCoord(QPointF pos)
{
  QString text;
  if (m_displayUnit == U_INCH) {
    text = QString::asprintf("(%f, %f)", pos.x(), -pos.y());
  } else {
    text = QString::asprintf("(%f, %f)", pos.x() * 25.4, -pos.y() * 25.4);
  }
  m_cursorCoordLabel->setText(text);
}

void ViewerWindow::updateFeatureDetail(Symbol* symbol)
{
  m_featureDetailLabel->setText(symbol->infoText());
}

void ViewerWindow::updateMeasureResult(QRectF rect)
{
  QString result("DX=%1, DY=%2, D=%3");
  m_featureDetailLabel->setText(result.arg(rect.width()).arg(rect.height())
    .arg(qSqrt(rect.width() * rect.width() + rect.height() * rect.height())));
}

void ViewerWindow::on_actionSetColor_triggered(void)
{
  SettingsDialog dialog;
  dialog.exec();
  loadColorConfig();
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionZoomIn_triggered(void)
{
  ui->viewWidget->scaleView(2);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionZoomOut_triggered(void)
{
  ui->viewWidget->scaleView(0.5);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionHome_triggered(void)
{
  ui->viewWidget->zoomToAll();
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionMousePan_toggled(bool checked)
{
  Q_UNUSED(checked);
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionAreaZoom->setChecked(false);
  ui->actionHighlight->setChecked(false);
  ui->actionMeasure->setChecked(false);
  m_transition = false;
  ui->viewWidget->setZoomMode(ODBPPGraphicsView::MousePan);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionAreaZoom_toggled(bool checked)
{
  Q_UNUSED(checked);
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionMousePan->setChecked(false);
  ui->actionHighlight->setChecked(false);
  ui->actionMeasure->setChecked(false);
  m_transition = false;
  ui->viewWidget->setZoomMode(ODBPPGraphicsView::AreaZoom);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanLeft_triggered(void)
{
  ui->viewWidget->scrollView(-500, 0);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanRight_triggered(void)
{
  ui->viewWidget->scrollView(500, 0);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanUp_triggered(void)
{
  ui->viewWidget->scrollView(0, -500);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanDown_triggered(void)
{
  ui->viewWidget->scrollView(0, 500);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionHighlight_toggled(bool checked)
{
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionAreaZoom->setChecked(false);
  ui->actionMousePan->setChecked(false);
  ui->actionMeasure->setChecked(false);
  m_transition = false;
  ui->viewWidget->setHighlightEnabled(checked);
  if (m_activeInfoBox) {
    m_activeInfoBox->layer()->setHighlightEnabled(checked);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionClearHighlight_triggered(void)
{
  ui->viewWidget->clearHighlight();
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionFeatureProperties_triggered(void)
{
  m_featurePropertiesDialog->show();
}

void ViewerWindow::on_actionMeasure_toggled(bool checked)
{
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionAreaZoom->setChecked(false);
  ui->actionMousePan->setChecked(false);
  ui->actionHighlight->setChecked(false);
  m_transition = false;

  ui->viewWidget->setMeasureEnabled(checked);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionShowOutline_toggled(bool checked)
{
  for (int i = 0; i < m_visibles.size(); ++i) {
    m_visibles[i]->layer()->setShowOutline(checked);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionShowStepRepeat_toggled(bool checked)
{
  for (int i = 0; i < m_visibles.size(); ++i) {
    m_visibles[i]->layer()->setShowStepRepeat(checked);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionShowNotes_toggled(bool checked)
{
  for (int i = 0; i < m_visibles.size(); ++i) {
    if (checked) {
      ui->viewWidget->addItem(m_visibles[i]->layer()->notes());
    } else {
      ui->viewWidget->removeItem(m_visibles[i]->layer()->notes());
    }
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionExportPNG_triggered(void)
{
  LOG_STEP("Export to PNG triggered");
  
  // Create a default file name using job and step names
  QString defaultFileName = QString("%1_%2").arg(m_job).arg(m_step);
  
  // Add layer names to the filename if there are visible layers
  if (!m_visibles.isEmpty()) {
    defaultFileName += "_";
    for (int i = 0; i < qMin(m_visibles.size(), 3); ++i) {  // Limit to first 3 layers to avoid too long filenames
      if (i > 0) {
        defaultFileName += "+";
      }
      defaultFileName += m_visibles[i]->name();
    }
    if (m_visibles.size() > 3) {
      defaultFileName += QString("+%1more").arg(m_visibles.size() - 3);
    }
  }
  
  defaultFileName += ".png";
  
  // Show file dialog to select save location
  QString filePath = QFileDialog::getSaveFileName(this, tr("Export to PNG"),
      defaultFileName, tr("PNG Files (*.png)"));
  
  if (filePath.isEmpty()) {
    LOG_INFO("PNG export cancelled by user");
    return;
  }
  
  LOG_INFO(QString("Exporting to PNG file: %1").arg(filePath));
  
  // Make sure the file has .png extension
  if (!filePath.endsWith(".png", Qt::CaseInsensitive)) {
    filePath += ".png";
  }
  
  // Create resolution options dialog
  QDialog resDialog(this);
  resDialog.setWindowTitle(tr("Export Resolution"));
  
  QVBoxLayout* layout = new QVBoxLayout(&resDialog);
  
  // Resolution options
  QLabel* label = new QLabel(tr("Choose PNG resolution:"), &resDialog);
  layout->addWidget(label);
  
  QRadioButton* screenRes = new QRadioButton(tr("Current view size (3x scale)"), &resDialog);
  QRadioButton* fixedRes = new QRadioButton(tr("Fixed size: 20000 x 20000 pixels"), &resDialog);
  QRadioButton* customRes = new QRadioButton(tr("Custom size:"), &resDialog);
  
  screenRes->setChecked(true);
  
  layout->addWidget(screenRes);
  layout->addWidget(fixedRes);
  layout->addWidget(customRes);
  
  // Custom resolution input
  QHBoxLayout* customLayout = new QHBoxLayout();
  QSpinBox* widthBox = new QSpinBox(&resDialog);
  QSpinBox* heightBox = new QSpinBox(&resDialog);
  
  widthBox->setRange(100, 32767);
  heightBox->setRange(100, 32767);
  widthBox->setValue(10000);
  heightBox->setValue(10000);
  widthBox->setSuffix(" px");
  heightBox->setSuffix(" px");
  widthBox->setEnabled(false);
  heightBox->setEnabled(false);
  
  customLayout->addWidget(new QLabel(tr("Width:"), &resDialog));
  customLayout->addWidget(widthBox);
  customLayout->addWidget(new QLabel(tr("Height:"), &resDialog));
  customLayout->addWidget(heightBox);
  
  layout->addLayout(customLayout);
  
  // Connect signals
  QObject::connect(customRes, &QRadioButton::toggled, [widthBox, heightBox](bool checked) {
    widthBox->setEnabled(checked);
    heightBox->setEnabled(checked);
  });
  
  // Warning for large images
  QLabel* warningLabel = new QLabel(tr("Note: Very large images may take significant time to render and require substantial memory."), &resDialog);
  warningLabel->setWordWrap(true);
  warningLabel->setStyleSheet("color: #FF6600;");
  layout->addWidget(warningLabel);
  
  // Buttons
  QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &resDialog);
  layout->addWidget(buttonBox);
  
  QObject::connect(buttonBox, &QDialogButtonBox::accepted, &resDialog, &QDialog::accept);
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, &resDialog, &QDialog::reject);
  
  resDialog.setLayout(layout);
  
  // Show the dialog
  if (resDialog.exec() != QDialog::Accepted) {
    LOG_INFO("PNG export resolution dialog cancelled");
    return;
  }
  
  // Create a pixmap to render the scene
  QMessageBox msg(QMessageBox::Information, "Progress", "Rendering image...");
  msg.setStandardButtons(QMessageBox::NoButton);
  msg.show();
  QApplication::processEvents();
  
  // Get the current view rect
  QRect viewRect = ui->viewWidget->viewport()->rect();
  QRectF sceneRect = ui->viewWidget->mapToScene(viewRect).boundingRect();
  
  // Determine output image size based on user selection
  int imgWidth, imgHeight;
  QRectF targetRect;
  
  if (fixedRes->isChecked()) {
    // Fixed 20k x 20k resolution
    imgWidth = 20000;
    imgHeight = 20000;
    
    LOG_INFO("Using fixed 20000x20000 resolution");
    
    // Adjust scene rect to maintain aspect ratio
    QRectF adjustedSceneRect = sceneRect;
    double sceneAspect = sceneRect.width() / sceneRect.height();
    double imgAspect = static_cast<double>(imgWidth) / imgHeight;
    
    if (sceneAspect > imgAspect) {
      // Scene is wider than image aspect ratio
      double newHeight = sceneRect.width() / imgAspect;
      double heightDiff = newHeight - sceneRect.height();
      adjustedSceneRect.adjust(0, -heightDiff/2, 0, heightDiff/2);
    } else {
      // Scene is taller than image aspect ratio
      double newWidth = sceneRect.height() * imgAspect;
      double widthDiff = newWidth - sceneRect.width();
      adjustedSceneRect.adjust(-widthDiff/2, 0, widthDiff/2, 0);
    }
    
    sceneRect = adjustedSceneRect;
    targetRect = QRectF(0, 0, imgWidth, imgHeight);
    
  } else if (customRes->isChecked()) {
    // Custom resolution
    imgWidth = widthBox->value();
    imgHeight = heightBox->value();
    
    LOG_INFO(QString("Using custom resolution: %1x%2").arg(imgWidth).arg(imgHeight));
    
    // Adjust scene rect to maintain aspect ratio
    QRectF adjustedSceneRect = sceneRect;
    double sceneAspect = sceneRect.width() / sceneRect.height();
    double imgAspect = static_cast<double>(imgWidth) / imgHeight;
    
    if (sceneAspect > imgAspect) {
      // Scene is wider than image aspect ratio
      double newHeight = sceneRect.width() / imgAspect;
      double heightDiff = newHeight - sceneRect.height();
      adjustedSceneRect.adjust(0, -heightDiff/2, 0, heightDiff/2);
    } else {
      // Scene is taller than image aspect ratio
      double newWidth = sceneRect.height() * imgAspect;
      double widthDiff = newWidth - sceneRect.width();
      adjustedSceneRect.adjust(-widthDiff/2, 0, widthDiff/2, 0);
    }
    
    sceneRect = adjustedSceneRect;
    targetRect = QRectF(0, 0, imgWidth, imgHeight);
    
  } else {
    // Screen resolution (3x scale)
    int scale = 3;
    imgWidth = viewRect.width() * scale;
    imgHeight = viewRect.height() * scale;
    
    LOG_INFO(QString("Using screen resolution with 3x scale: %1x%2").arg(imgWidth).arg(imgHeight));
    targetRect = QRectF(0, 0, imgWidth, imgHeight);
  }
  
  try {
    // Create image with the selected dimensions
    LOG_INFO(QString("Creating image with dimensions: %1x%2").arg(imgWidth).arg(imgHeight));
    QImage image(imgWidth, imgHeight, QImage::Format_ARGB32);
    
    if (image.isNull()) {
      throw std::runtime_error("Failed to allocate memory for image");
    }
    
    image.fill(ctx.bg_color);
    
    // Set up a painter for the image
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // Update message
    msg.setText(tr("Rendering image (%1x%2)...").arg(imgWidth).arg(imgHeight));
    QApplication::processEvents();
    
    // Render the scene onto the image
    LOG_INFO("Rendering scene to image");
    ui->viewWidget->scene()->render(&painter, targetRect, sceneRect);
    
    // Save the image
    msg.setText(tr("Saving PNG file..."));
    QApplication::processEvents();
    
    LOG_INFO("Saving image to file");
    bool success = image.save(filePath, "PNG");
    
    msg.hide();
    
    if (success) {
      LOG_INFO(QString("PNG file saved successfully: %1").arg(filePath));
      QMessageBox::information(this, tr("Export Successful"),
                              tr("Design has been successfully exported to:\n%1\n\nResolution: %2x%3 pixels")
                              .arg(filePath)
                              .arg(imgWidth)
                              .arg(imgHeight));
    } else {
      LOG_ERROR(QString("Failed to save PNG file: %1").arg(filePath));
      QMessageBox::critical(this, tr("Export Failed"),
                           tr("Failed to save the design as PNG file. Please check file permissions."));
    }
  }
  catch (const std::exception& e) {
    LOG_ERROR(QString("Exception during PNG export: %1").arg(e.what()));
    msg.hide();
    QMessageBox::critical(this, tr("Export Failed"),
                         tr("Failed to create the PNG image: %1").arg(e.what()));
  }
  catch (...) {
    LOG_ERROR("Unknown exception during PNG export");
    msg.hide();
    QMessageBox::critical(this, tr("Export Failed"),
                         tr("Failed to create the PNG image due to insufficient memory. Try a smaller resolution."));
  }
  
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionGoToCoordinate_triggered(void)
{
  // Set the current display unit in the dialog
  m_goToCoordinateDialog->setDisplayUnit(m_displayUnit);
  
  // Show the dialog
  if (m_goToCoordinateDialog->exec() == QDialog::Accepted) {
    // Get the coordinate (always in inches)
    QPointF targetCoord = m_goToCoordinateDialog->getCoordinate();
    
    LOG_INFO(QString("Going to coordinate: (%1, %2) inches")
             .arg(targetCoord.x())
             .arg(targetCoord.y()));
    
    // Use unified navigate and capture method
    QString savedFilePath;
    bool success = navigateAndCapture("", targetCoord.x(), targetCoord.y(), 64.0, &savedFilePath, nullptr);
    
    if (success) {
      // Show success message
      QMessageBox::information(this, tr("Auto-Export Successful"),
                              tr("Coordinate view has been automatically exported to:\n%1\n\nCoordinate: (%2, %3) inches")
                              .arg(savedFilePath)
                              .arg(targetCoord.x(), 0, 'f', 3)
                              .arg(targetCoord.y(), 0, 'f', 3));
    } else {
      QMessageBox::critical(this, tr("Auto-Export Failed"),
                           tr("Failed to navigate and capture the coordinate view."));
    }
  }
}

// ✅ Unified method: Navigate to coordinate and capture image
bool ViewerWindow::navigateAndCapture(const QString &layerName, double x, double y, double zoom,
                                     QString *outputPath, QByteArray *imageData)
{
    LOG_INFO(QString("navigateAndCapture: layer=%1, x=%2, y=%3, zoom=%4")
             .arg(layerName).arg(x).arg(y).arg(zoom));
    
    // Step 1: Select the layer by name using m_SelectorMap
    if (!layerName.isEmpty()) {
        if (m_SelectorMap.contains(layerName)) {
            LayerInfoBox* box = m_SelectorMap[layerName];
            if (box) {
                LOG_INFO(QString("Found layer: %1").arg(layerName));
                // Make sure layer is visible (toggle if not already)
                if (!box->isActive()) {
                    box->toggle();
                }
                // Set as active layer
                box->setActive(true);
            }
        } else {
            LOG_ERROR(QString("Layer not found: %1").arg(layerName));
            return false;
        }
    }
    
    // Step 2: Convert to scene coordinates and navigate
    QPointF targetCoord(x, y);  // Input is in inches
    QPointF sceneCoord(x, -y);  // Y axis needs to be flipped to match display coordinate system
    
    LOG_INFO(QString("Centering view on coordinate: (%1, %2) inches -> scene(%3, %4)")
             .arg(x).arg(y).arg(sceneCoord.x()).arg(sceneCoord.y()));
    ui->viewWidget->centerOn(sceneCoord);
    
    // Step 3: Apply zoom
    LOG_INFO(QString("Applying zoom: %1x").arg(zoom));
    ui->viewWidget->scaleView(zoom);
    ui->viewWidget->setFocus(Qt::MouseFocusReason);
    
    // Step 4: Create export directory
    QString exportDir = "C:/Users/sonng/OneDrive/Desktop/EExxport";
    QDir dir;
    if (!dir.exists(exportDir)) {
        if (!dir.mkpath(exportDir)) {
            LOG_ERROR("Failed to create export directory: " + exportDir);
            return false;
        }
    }
    
    // Step 5: Generate filename
    QString coordStr = QString("_at_%1_%2")
                       .arg(targetCoord.x(), 0, 'f', 3)
                       .arg(targetCoord.y(), 0, 'f', 3);
    
    QString filename = QString("%1_%2%3_%4").arg(m_job).arg(m_step).arg(layerName).arg(coordStr);
    filename += ".png";
    QString filePath = exportDir + "/" + filename;
    
    LOG_INFO(QString("Rendering image to: %1").arg(filePath));
    
    // Step 6: Capture image using current view resolution with 3x scale
    QRect viewRect = ui->viewWidget->viewport()->rect();
    int scale = 3;
    int imgWidth = viewRect.width() * scale;
    int imgHeight = viewRect.height() * scale;
    
    LOG_INFO(QString("Creating image with dimensions: %1x%2").arg(imgWidth).arg(imgHeight));
    QImage image(imgWidth, imgHeight, QImage::Format_ARGB32);
    
    if (image.isNull()) {
        LOG_ERROR("Failed to allocate memory for image");
        return false;
    }
    
    image.fill(ctx.bg_color);
    
    // Step 7: Render the scene
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    QRectF sceneRect = ui->viewWidget->mapToScene(viewRect).boundingRect();
    QRectF targetRect = QRectF(0, 0, imgWidth, imgHeight);
    
    LOG_INFO("Rendering scene to image");
    ui->viewWidget->scene()->render(&painter, targetRect, sceneRect);
    
    // Step 8: Save to file if requested
    if (outputPath) {
        LOG_INFO("Saving image to file");
        if (!image.save(filePath, "PNG")) {
            LOG_ERROR(QString("Failed to save PNG file: %1").arg(filePath));
            return false;
        }
        *outputPath = filePath;
        LOG_INFO(QString("PNG file saved successfully: %1").arg(filePath));
    }
    
    // Step 9: Convert to byte array if requested
    if (imageData) {
        QBuffer buffer(imageData);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            LOG_ERROR("Failed to convert image to byte array");
            return false;
        }
        LOG_INFO(QString("Image converted to byte array: %1 bytes").arg(imageData->size()));
    }
    
    return true;
}

void ViewerWindow::startRestApiServer(quint16 port)
{
    if (m_restApiServer) {
        delete m_restApiServer;
    }
    
    m_restApiServer = new RestApiServer(port, this);
    
    if (m_restApiServer->isListening()) {
        qDebug() << "REST API server started on port" << port;
        
        // Kết nối signals
        connect(m_restApiServer, &RestApiServer::captureRequest,
                this, &ViewerWindow::handleCaptureRequest);
    } else {
        qDebug() << "Failed to start REST API server";
    }
}

void ViewerWindow::handleCaptureRequest(const QJsonObject &request)
{
    LOG_INFO("=== REST API Capture Request Received ===");
    qDebug() << request;
    
    QString requestId = request["requestId"].toString();
    QString jobName = request["jobName"].toString();
    QString layerName = request["layerName"].toString();
    double x = request["x"].toDouble();
    double y = request["y"].toDouble();
    double zoom = request["zoom"].toDouble(64.0);  // Default to 64x zoom
    
    // Validation
    if (jobName.isEmpty()) {
        LOG_ERROR("jobName is empty");
        return;
    }
    
    // Load job if needed (currently not implemented - assuming job is already loaded)
    if (m_job != jobName) {
        LOG_WARNING(QString("Job name mismatch: current=%1, requested=%2 (job loading not implemented)")
                   .arg(m_job).arg(jobName));
        // Continue anyway - use current job
    }
    
    // Wait a bit to ensure UI is ready, then navigate and capture
    QTimer::singleShot(100, this, [=]() {
        QString savedFilePath;
        QByteArray imageData;
        
        // Use unified navigate and capture method
        bool success = navigateAndCapture(layerName, x, y, zoom, &savedFilePath, &imageData);
        
        if (!success) {
            LOG_ERROR("Failed to navigate and capture image");
            return;
        }
        
        LOG_INFO(QString("Capture successful: %1 bytes, saved to %2")
                .arg(imageData.size()).arg(savedFilePath));
        
        // Build metadata
        QJsonObject metadata;
        metadata["requestId"] = requestId;
        metadata["jobName"] = m_job;  // Use actual current job name
        metadata["layerName"] = layerName;
        metadata["x"] = x;
        metadata["y"] = y;
        metadata["zoom"] = zoom;
        metadata["imageSize"] = imageData.size();
        metadata["format"] = "PNG";
        metadata["savedPath"] = savedFilePath;
        metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        // Send response back to client
        if (m_restApiServer) {
            m_restApiServer->sendCaptureResponse(requestId, imageData, metadata);
            LOG_INFO("Capture response sent to client");
        } else {
            LOG_ERROR("REST API server is null");
        }
    });
}
