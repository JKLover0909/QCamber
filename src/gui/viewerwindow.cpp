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

#include "context.h"
#include "gotocoordinatedialog.h"
#include "layerinfobox.h"
#include "logger.h"
#include "settingsdialog.h"
#include "settings.h"

ViewerWindow::ViewerWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::ViewerWindow), m_displayUnit(U_INCH),
  m_activeInfoBox(NULL), m_transition(false)
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

void ViewerWindow::exportPNGAtCoordinate(const QPointF& coordinate)
{
  LOG_STEP("Auto PNG export after coordinate navigation triggered");
  
  // Create export directory if it doesn't exist
  QString exportDir = "C:/Users/sonng/OneDrive/Desktop/EExxport";
  QDir dir;
  if (!dir.exists(exportDir)) {
    if (!dir.mkpath(exportDir)) {
      LOG_ERROR("Failed to create export directory: " + exportDir);
      QMessageBox::critical(this, tr("Export Failed"),
                           tr("Failed to create export directory: %1").arg(exportDir));
      return;
    }
  }
  
  // Create filename with coordinate information
  QString coordStr = QString("_at_%1_%2")
                     .arg(coordinate.x(), 0, 'f', 3)
                     .arg(coordinate.y(), 0, 'f', 3);
  
  QString filename = QString("%1_%2%3").arg(m_job).arg(m_step).arg(coordStr);
  
  // Add layer names to the filename if there are visible layers
  if (!m_visibles.isEmpty()) {
    filename += "_";
    for (int i = 0; i < qMin(m_visibles.size(), 3); ++i) {
      if (i > 0) {
        filename += "+";
      }
      filename += m_visibles[i]->name();
    }
    if (m_visibles.size() > 3) {
      filename += QString("+%1more").arg(m_visibles.size() - 3);
    }
  }
  
  filename += ".png";
  QString filePath = exportDir + "/" + filename;
  
  LOG_INFO(QString("Auto-exporting to PNG file: %1").arg(filePath));
  
  // Show progress message
  QMessageBox msg(QMessageBox::Information, "Progress", "Auto-rendering coordinate image...");
  msg.setStandardButtons(QMessageBox::NoButton);
  msg.show();
  QApplication::processEvents();
  
  try {
    // Use current view resolution with 3x scale for automatic export
    QRect viewRect = ui->viewWidget->viewport()->rect();
    int scale = 3;
    int imgWidth = viewRect.width() * scale;
    int imgHeight = viewRect.height() * scale;
    
    LOG_INFO(QString("Creating auto-export image with dimensions: %1x%2").arg(imgWidth).arg(imgHeight));
    QImage image(imgWidth, imgHeight, QImage::Format_ARGB32);
    
    if (image.isNull()) {
      throw std::runtime_error("Failed to allocate memory for auto-export image");
    }
    
    image.fill(ctx.bg_color);
    
    // Set up a painter for the image
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // Get the current scene rect
    QRectF sceneRect = ui->viewWidget->mapToScene(viewRect).boundingRect();
    QRectF targetRect = QRectF(0, 0, imgWidth, imgHeight);
    
    // Update message
    msg.setText(tr("Auto-rendering image (%1x%2)...").arg(imgWidth).arg(imgHeight));
    QApplication::processEvents();
    
    // Render the scene onto the image
    LOG_INFO("Auto-rendering scene to image");
    ui->viewWidget->scene()->render(&painter, targetRect, sceneRect);
    
    // Save the image
    msg.setText(tr("Saving auto-export PNG file..."));
    QApplication::processEvents();
    
    LOG_INFO("Auto-saving image to file");
    bool success = image.save(filePath, "PNG");
    
    msg.hide();
    
    if (success) {
      LOG_INFO(QString("Auto-export PNG file saved successfully: %1").arg(filePath));
      QMessageBox::information(this, tr("Auto-Export Successful"),
                              tr("Coordinate view has been automatically exported to:\n%1\n\nResolution: %2x%3 pixels\nCoordinate: (%4, %5) inches")
                              .arg(filePath)
                              .arg(imgWidth)
                              .arg(imgHeight)
                              .arg(coordinate.x(), 0, 'f', 3)
                              .arg(coordinate.y(), 0, 'f', 3));
    } else {
      LOG_ERROR(QString("Failed to save auto-export PNG file: %1").arg(filePath));
      QMessageBox::critical(this, tr("Auto-Export Failed"),
                           tr("Failed to auto-save the coordinate view as PNG file. Please check file permissions."));
    }
  }
  catch (const std::exception& e) {
    LOG_ERROR(QString("Exception during auto-export PNG: %1").arg(e.what()));
    msg.hide();
    QMessageBox::critical(this, tr("Auto-Export Failed"),
                         tr("Failed to create the auto-export PNG image: %1").arg(e.what()));
  }
  catch (...) {
    LOG_ERROR("Unknown exception during auto-export PNG");
    msg.hide();
    QMessageBox::critical(this, tr("Auto-Export Failed"),
                         tr("Failed to create the auto-export PNG image due to insufficient memory."));
  }
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
    
    // Convert to scene coordinates (Y axis needs to be flipped to match display coordinate system)
    QPointF sceneCoord(targetCoord.x(), -targetCoord.y());
    
    // Center the view on the target coordinate
    ui->viewWidget->centerOn(sceneCoord);
    
    // Zoom to a reasonable level (64x zoom)
    ui->viewWidget->scaleView(64.0);
    
    // Update focus
    ui->viewWidget->setFocus(Qt::MouseFocusReason);
    
    // Auto-export PNG after navigation
    exportPNGAtCoordinate(targetCoord);
  }
}
