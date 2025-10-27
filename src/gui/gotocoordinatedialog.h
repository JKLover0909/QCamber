/**
 * @file   gotocoordinatedialog.h
 * @author [Your Name]
 *
 * Copyright (C) 2024
 * All Rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GOTOCOORDINATEDIALOG_H
#define GOTOCOORDINATEDIALOG_H

#include <QDialog>
#include <QPointF>

class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

class GoToCoordinateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoToCoordinateDialog(QWidget *parent = nullptr);
    ~GoToCoordinateDialog();

    // Get the coordinate in the selected unit
    QPointF getCoordinate() const;
    
    // Get the selected zoom level
    double getZoomLevel() const;
    
    // Set the current display unit (0=Inch, 1=MM)
    void setDisplayUnit(int unit);

private slots:
    void onUnitChanged(int index);
    void onGoToClicked();
    void onCancelClicked();

private:
    void setupUI();
    void convertCoordinates(int fromUnit, int toUnit);

private:
    QDoubleSpinBox* m_xSpinBox;
    QDoubleSpinBox* m_ySpinBox;
    QComboBox* m_unitComboBox;
    QComboBox* m_zoomComboBox;
    QPushButton* m_goToButton;
    QPushButton* m_cancelButton;
    QLabel* m_xLabel;
    QLabel* m_yLabel;
    QLabel* m_unitLabel;
    QLabel* m_zoomLabel;
    
    int m_currentUnit; // 0=Inch, 1=MM
    QPointF m_coordinate; // Always stored in inches internally
    double m_zoomLevel;
};

#endif // GOTOCOORDINATEDIALOG_H