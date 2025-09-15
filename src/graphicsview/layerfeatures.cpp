/**
 * @file   layerfeatures.cpp
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

#include "layerfeatures.h"

#include <QDebug>

#include "cachedparser.h"
#include "context.h"
#include "logger.h"

LayerFeatures::LayerFeatures(QString step, QString path, bool stepRepeat):
  Symbol("features"), m_step(step), m_path(path), m_scene(NULL),
  m_stepRepeatLoaded(false), m_showStepRepeat(stepRepeat),
  m_reportModel(NULL)
{
  LOG_STEP(QString("LayerFeatures constructor"), QString("Step: %1, Path: %2").arg(step, path));
  setHandlesChildEvents(true);

  QString fullPath = ctx.loader->absPath(path.arg(step));
  LOG_INFO(QString("Parsing features file: %1").arg(fullPath));
  
  m_ds = CachedFeaturesParser::parse(fullPath);

  if (!m_ds) {
    LOG_ERROR(QString("Failed to parse features file: %1").arg(fullPath));
    return;
  }

  LOG_INFO(QString("Features file parsed successfully, records count: %1").arg(m_ds->records().size()));

  int symbolCount = 0;
  for (QList<Record*>::const_iterator it = m_ds->records().begin();
      it != m_ds->records().end(); ++it) {
    try {
      Symbol* symbol = (*it)->createSymbol();
      if (symbol) {
        m_symbols.append(symbol);
        symbolCount++;
      } else {
        LOG_WARNING("Failed to create symbol from record");
      }
    } catch (const std::exception& e) {
      LOG_ERROR(QString("Exception creating symbol: %1").arg(e.what()));
    } catch (...) {
      LOG_ERROR("Unknown exception creating symbol");
    }
  }

  LOG_INFO(QString("Created %1 symbols from %2 records").arg(symbolCount).arg(m_ds->records().size()));

  // Copy count maps
  m_posLineCount = m_ds->posLineCountMap();
  m_negLineCount = m_ds->negLineCountMap();
  m_posPadCount = m_ds->posPadCountMap();
  m_negPadCount = m_ds->negPadCountMap();
  m_posArcCount = m_ds->posArcCountMap();
  m_negArcCount = m_ds->negArcCountMap();
  m_posSurfaceCount = m_ds->posSurfaceCount();
  m_negSurfaceCount = m_ds->negSurfaceCount();
  m_posTextCount = m_ds->posTextCount();
  m_negTextCount = m_ds->negTextCount();
  m_posBarcodeCount = m_ds->posBarcodeCount();
  m_negBarcodeCount = m_ds->negBarcodeCount();

  LOG_INFO(QString("Feature counts - Lines: %1/%2, Pads: %3/%4, Arcs: %5/%6, Surfaces: %7/%8, Text: %9/%10, Barcodes: %11/%12")
          .arg(m_posLineCount.size()).arg(m_negLineCount.size())
          .arg(m_posPadCount.size()).arg(m_negPadCount.size())
          .arg(m_posArcCount.size()).arg(m_negArcCount.size())
          .arg(m_posSurfaceCount).arg(m_negSurfaceCount)
          .arg(m_posTextCount).arg(m_negTextCount)
          .arg(m_posBarcodeCount).arg(m_negBarcodeCount));

  if (m_showStepRepeat) {
    LOG_STEP("Loading step and repeat");
    loadStepAndRepeat();
  }
  
  LOG_INFO("LayerFeatures constructor completed");
}

LayerFeatures::~LayerFeatures()
{
  LOG_STEP("LayerFeatures destructor");
  for (int i = 0; i < m_repeats.size(); ++i) {
    delete m_repeats[i];
  }

  if (m_reportModel) {
    delete m_reportModel;
  }
}

void LayerFeatures::loadStepAndRepeat(void)
{
  LOG_STEP("Loading step and repeat data");
  QString path = ctx.loader->absPath(QString("steps/%1/stephdr").arg(m_step));
  LOG_INFO(QString("Parsing step header: %1").arg(path));
  
  StructuredTextDataStore* hds = CachedStructuredTextParser::parse(path);

  StructuredTextDataStore::BlockIterPair ip = hds->getBlocksByKey(
      "STEP-REPEAT");

  qreal top_active, bottom_active, left_active, right_active;

#define GET(key) (QString::fromStdString(hds->get(key)))
  try {
    m_x_datum = GET("X_DATUM").toDouble();
    m_y_datum = GET("Y_DATUM").toDouble();
    m_x_origin = GET("X_ORIGIN").toDouble();
    m_y_origin = GET("Y_ORIGIN").toDouble();

    top_active = GET("TOP_ACTIVE").toDouble();
    bottom_active = GET("BOTTOM_ACTIVE").toDouble();
    left_active = GET("LEFT_ACTIVE").toDouble();
    right_active = GET("RIGHT_ACTIVE").toDouble();

    LOG_INFO(QString("Step parameters - Datum: (%1,%2), Origin: (%3,%4)")
            .arg(m_x_datum).arg(m_y_datum).arg(m_x_origin).arg(m_y_origin));

    m_activeRect.setX(m_activeRect.x() + left_active);
    m_activeRect.setY(m_activeRect.y() + top_active);
    m_activeRect.setWidth(m_activeRect.width() - right_active);
    m_activeRect.setHeight(m_activeRect.height() - bottom_active);
  } catch(StructuredTextDataStore::InvalidKeyException&) {
    LOG_WARNING("Some step header parameters not found");
  }

  if (ip.first == ip.second) {
    m_activeRect = QRectF();
    LOG_INFO("No step repeat blocks found");
  } else {
    LOG_INFO("Processing step repeat blocks");
  }
#undef GET

#define GET(key) (QString::fromStdString(it->second->get(key)))
  int repeatCount = 0;
  for (StructuredTextDataStore::BlockIter it = ip.first; it != ip.second; ++it)
  {
    QString name = GET("NAME").toLower();
    qreal x = GET("X").toDouble();
    qreal y = GET("Y").toDouble();
    qreal dx = GET("DX").toDouble();
    qreal dy = GET("DY").toDouble();
    int nx = GET("NX").toInt();
    int ny = GET("NY").toInt();
    qreal angle = GET("ANGLE").toDouble();
    bool mirror = (GET("MIRROR") == "YES");

    LOG_INFO(QString("Step repeat: %1 at (%2,%3), delta (%4,%5), array %6x%7, angle %8, mirror %9")
            .arg(name).arg(x).arg(y).arg(dx).arg(dy).arg(nx).arg(ny).arg(angle).arg(mirror));

    FeaturesDataStore::CountMapType countMap;

    for (int i = 0; i < nx; ++i) {
      for (int j = 0; j < ny; ++j) {
        try {
          LayerFeatures* step = new LayerFeatures(name, m_path, true);
          step->m_virtualParent = this;
          step->setPos(QPointF(x + dx * i, -(y + dy * j)));

          QTransform trans;
          if (mirror) {
            trans.scale(-1, 1);
          }
          trans.rotate(angle);
          trans.translate(-step->x_datum(), step->y_datum());
          step->setTransform(trans);
          m_repeats.append(step);
          repeatCount++;

          // Aggregate count maps (existing code)
          countMap = step->m_posLineCount;
          for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
              it != countMap.end(); ++it) {
            m_posLineCount[it.key()] += it.value();
          }
          countMap = step->m_negLineCount;
          for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
              it != countMap.end(); ++it) {
            m_negLineCount[it.key()] += it.value();
          }
          countMap = step->m_posPadCount;
          for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
              it != countMap.end(); ++it) {
            m_posPadCount[it.key()] += it.value();
          }
          countMap = step->m_negPadCount;
          for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
              it != countMap.end(); ++it) {
            m_negPadCount[it.key()] += it.value();
          }
          countMap = step->m_posArcCount;
          for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
              it != countMap.end(); ++it) {
            m_posArcCount[it.key()] += it.value();
          }
          countMap = step->m_negArcCount;
          for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
              it != countMap.end(); ++it) {
            m_negArcCount[it.key()] += it.value();
          }

          m_posSurfaceCount += step->m_posSurfaceCount;
          m_negSurfaceCount += step->m_negSurfaceCount;

          m_posTextCount += step->m_posTextCount;
          m_negTextCount += step->m_negTextCount;

          m_posBarcodeCount += step->m_posBarcodeCount;
          m_negBarcodeCount += step->m_negBarcodeCount;
        } catch (const std::exception& e) {
          LOG_ERROR(QString("Exception creating step repeat [%1,%2]: %3").arg(i).arg(j).arg(e.what()));
        } catch (...) {
          LOG_ERROR(QString("Unknown exception creating step repeat [%1,%2]").arg(i).arg(j));
        }
      }
    }
  }
#undef GET

  LOG_INFO(QString("Created %1 step repeat instances").arg(repeatCount));

  if (m_scene) {
    LOG_STEP("Adding step repeats to scene");
    for (QList<LayerFeatures*>::iterator it = m_repeats.begin();
        it != m_repeats.end(); ++it) {
      (*it)->addToScene(m_scene);
    }
  }

  m_stepRepeatLoaded = true;
  LOG_INFO("Step and repeat loading completed");
}

QRectF LayerFeatures::boundingRect() const
{
  LOG_INFO(QString("LayerFeatures::boundingRect() called, symbols count: %1").arg(m_symbols.size()));
  
  if (m_symbols.isEmpty()) {
    LOG_WARNING("LayerFeatures has no symbols, returning empty rect");
    return QRectF();
  }

  QRectF bounds;
  bool firstSymbol = true;
  
  for (int i = 0; i < m_symbols.size(); ++i) {
    if (m_symbols[i]) {
      QRectF symbolBounds = m_symbols[i]->boundingRect();
      QPointF symbolPos = m_symbols[i]->pos();
      
      // Translate symbol bounds to its actual position
      symbolBounds.translate(symbolPos);
      
      if (firstSymbol) {
        bounds = symbolBounds;
        firstSymbol = false;
        LOG_INFO(QString("First symbol bounds: x=%1, y=%2, w=%3, h=%4")
                .arg(symbolBounds.x()).arg(symbolBounds.y())
                .arg(symbolBounds.width()).arg(symbolBounds.height()));
      } else {
        bounds = bounds.united(symbolBounds);
      }
    }
  }
  
  // Include repeats
  for (QList<LayerFeatures*>::const_iterator it = m_repeats.begin();
      it != m_repeats.end(); ++it) {
    QRectF repeatBounds = (*it)->boundingRect();
    if (!repeatBounds.isEmpty()) {
      bounds = bounds.united(repeatBounds);
    }
  }
  
  LOG_INFO(QString("LayerFeatures final bounds: x=%1, y=%2, w=%3, h=%4")
          .arg(bounds.x()).arg(bounds.y())
          .arg(bounds.width()).arg(bounds.height()));
  
  return bounds;
}

void LayerFeatures::addToScene(QGraphicsScene* scene)
{
  LOG_STEP(QString("Adding LayerFeatures to scene"), QString("Symbols: %1, Repeats: %2").arg(m_symbols.size()).arg(m_repeats.size()));
  m_scene = scene;

  int addedSymbols = 0;
  for (int i = 0; i < m_symbols.size(); ++i) {
    if (m_symbols[i]) {
      scene->addItem(m_symbols[i]);
      addedSymbols++;
    } else {
      LOG_WARNING(QString("Null symbol at index %1").arg(i));
    }
  }
  LOG_INFO(QString("Added %1 symbols to scene").arg(addedSymbols));

  for (QList<LayerFeatures*>::iterator it = m_repeats.begin();
      it != m_repeats.end(); ++it) {
    (*it)->addToScene(scene);
    (*it)->setVisible(m_showStepRepeat);
  }
  LOG_INFO(QString("Added %1 step repeats to scene").arg(m_repeats.size()));
}

void LayerFeatures::setTransform(const QTransform& matrix, bool combine)
{
  LOG_INFO("Setting transform on LayerFeatures");
  for (int i = 0; i < m_symbols.size(); ++i) {
    Symbol* symbol = m_symbols[i];
    QTransform trans;
    QPointF o = transform().inverted().map(symbol->pos());
    trans.translate(-o.x(), -o.y());
    trans = matrix * trans;
    trans.translate(o.x(), o.y());
    symbol->setTransform(symbol->transform() * trans, false);
  }

  for (QList<LayerFeatures*>::iterator it = m_repeats.begin();
      it != m_repeats.end(); ++it) {
    QTransform trans;
    QPointF o = transform().inverted().map(pos());
    trans.translate(-o.x(), -o.y());
    trans = matrix * trans;
    trans.translate(o.x(), o.y());
    (*it)->setTransform(trans, combine);
  }

  QGraphicsItem::setTransform(matrix, true);
}

void LayerFeatures::setPos(QPointF pos)
{
  setPos(pos.x(), pos.y());
}

void LayerFeatures::setPos(qreal x, qreal y)
{
  LOG_INFO(QString("Setting LayerFeatures position to (%1, %2)").arg(x).arg(y));
  QTransform trans = QTransform::fromTranslate(x, y);
  for (int i = 0; i < m_symbols.size(); ++i) {
    m_symbols[i]->setTransform(m_symbols[i]->transform() * trans, false);
  }

  for (QList<LayerFeatures*>::iterator it = m_repeats.begin();
      it != m_repeats.end(); ++it) {
    (*it)->setPos(x, y);
  }

  QGraphicsItem::setTransform(trans);
  QGraphicsItem::setPos(x, y);
}

void LayerFeatures::setVisible(bool status)
{
  LOG_INFO(QString("Setting LayerFeatures visibility to %1").arg(status ? "true" : "false"));
  for (int i = 0; i < m_symbols.size(); ++i) {
    m_symbols[i]->setVisible(status);
  }

  for (QList<LayerFeatures*>::iterator it = m_repeats.begin();
      it != m_repeats.end(); ++it) {
    (*it)->setVisible(status);
  }
}

void LayerFeatures::setShowStepRepeat(bool status)
{
  LOG_STEP(QString("Setting show step repeat to %1").arg(status ? "true" : "false"));
  m_showStepRepeat = status;

  if (m_reportModel) {
    delete m_reportModel;
    m_reportModel = NULL;
  }

  if (status && !m_stepRepeatLoaded) {
    loadStepAndRepeat();

    QList<QGraphicsItem*> items = m_scene->items();
    for (int i = 0; i < items.size(); ++i) {
      Symbol* symbol = dynamic_cast<Symbol*>(items[i]);
      if (symbol) {
        symbol->setPen(m_pen);
        symbol->setBrush(m_brush);
      }
    }
  }

  for (QList<LayerFeatures*>::iterator it = m_repeats.begin();
      it != m_repeats.end(); ++it) {
    (*it)->setVisible(status);
  }
}

QStandardItemModel* LayerFeatures::reportModel(void)
{
  if (m_reportModel) {
    return m_reportModel;
  }

  LOG_STEP("Creating report model");
  m_reportModel = new QStandardItemModel;
  m_reportModel->setColumnCount(2);
  m_reportModel->setHeaderData(0, Qt::Horizontal, "name");
  m_reportModel->setHeaderData(1, Qt::Horizontal, "count");

  if (!m_ds) {
    LOG_WARNING("No data store available for report model");
    return m_reportModel;
  }

  FeaturesDataStore::CountMapType countMap;
  QStandardItem* root = m_reportModel->invisibleRootItem();
  QStandardItem* node = NULL;

  unsigned total = 0, n_nodes = 0;

  // Lines
  node = APPEND_ROW(root, "Lines", "");
  total = 0;

  countMap = (m_showStepRepeat? m_posLineCount: m_ds->posLineCountMap());
  for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
      it != countMap.end(); ++it) {
    APPEND_ROW(node, it.key() + " POS", QString::number(it.value()));
    total += it.value();
  }
  countMap = (m_showStepRepeat? m_negLineCount: m_ds->negLineCountMap());
  for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
      it != countMap.end(); ++it) {
    APPEND_ROW(node, it.key() + " NEG", QString::number(it.value()));
    total += it.value();
  }
  root->child(n_nodes++, 1)->setText(QString::number(total));

  // Pad
  node = APPEND_ROW(root, "Pad", "");
  total = 0;

  countMap = (m_showStepRepeat? m_posPadCount: m_ds->posPadCountMap());
  for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
      it != countMap.end(); ++it) {
    APPEND_ROW(node, it.key() + " POS", QString::number(it.value()));
    total += it.value();
  }
  countMap = (m_showStepRepeat? m_negPadCount: m_ds->negPadCountMap());
  for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
      it != countMap.end(); ++it) {
    APPEND_ROW(node, it.key() + " NEG", QString::number(it.value()));
    total += it.value();
  }
  root->child(n_nodes++, 1)->setText(QString::number(total));

  // Arc
  node = APPEND_ROW(root, "Arc", "");
  total = 0;

  countMap = (m_showStepRepeat? m_posArcCount: m_ds->posArcCountMap());
  for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
      it != countMap.end(); ++it) {
    APPEND_ROW(node, it.key() + " POS", QString::number(it.value()));
    total += it.value();
  }
  countMap = (m_showStepRepeat? m_negArcCount: m_ds->negArcCountMap());
  for (FeaturesDataStore::CountMapType::iterator it = countMap.begin();
      it != countMap.end(); ++it) {
    APPEND_ROW(node, it.key() + " NEG", QString::number(it.value()));
    total += it.value();
  }
  root->child(n_nodes++, 1)->setText(QString::number(total));

  // Surface
  unsigned pos = 0, neg = 0;
  node = APPEND_ROW(root, "Surface", "");
  pos = (m_showStepRepeat? m_posSurfaceCount: m_ds->posSurfaceCount());
  APPEND_ROW(node, "POS", QString::number(m_ds->posSurfaceCount()));
  neg = (m_showStepRepeat? m_negSurfaceCount: m_ds->negSurfaceCount());
  APPEND_ROW(node, "NEG", QString::number(m_ds->negSurfaceCount()));
  root->child(n_nodes++, 1)->setText(QString::number(pos + neg));

  // Text
  node = APPEND_ROW(root, "Text", "");
  pos = (m_showStepRepeat? m_posTextCount: m_ds->posTextCount());
  APPEND_ROW(node, "POS", QString::number(m_ds->posTextCount()));
  neg = (m_showStepRepeat? m_negTextCount: m_ds->negTextCount());
  APPEND_ROW(node, "NEG", QString::number(m_ds->negTextCount()));
  root->child(n_nodes++, 1)->setText(QString::number(pos + neg));

  // Barcode
  node = APPEND_ROW(root, "Barcode", "");
  pos = (m_showStepRepeat? m_posBarcodeCount: m_ds->posBarcodeCount());
  APPEND_ROW(node, "POS", QString::number(m_ds->posBarcodeCount()));
  neg = (m_showStepRepeat? m_negBarcodeCount: m_ds->negBarcodeCount());
  APPEND_ROW(node, "NEG", QString::number(m_ds->negBarcodeCount()));
  root->child(n_nodes++, 1)->setText(QString::number(pos + neg));

  LOG_INFO("Report model created successfully");
  return m_reportModel;
}
