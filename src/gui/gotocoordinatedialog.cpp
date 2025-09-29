/**
 * @file   gotocoordinatedialog.cpp
 * @author [Your Name]
 *
 * Copyright (C) 2024
 * All Rights reserved.
 */

#include "gotocoordinatedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGroupBox>

GoToCoordinateDialog::GoToCoordinateDialog(QWidget *parent)
    : QDialog(parent)
    , m_xSpinBox(nullptr)
    , m_ySpinBox(nullptr)
    , m_unitComboBox(nullptr)
    , m_goToButton(nullptr)
    , m_cancelButton(nullptr)
    , m_xLabel(nullptr)
    , m_yLabel(nullptr)
    , m_unitLabel(nullptr)
    , m_currentUnit(0) // Default to inches
    , m_coordinate(0.0, 0.0)
{
    setupUI();
    setWindowTitle(tr("Go To Coordinate"));
    setModal(true);
    resize(300, 200);
}

GoToCoordinateDialog::~GoToCoordinateDialog()
{
}

void GoToCoordinateDialog::setupUI()
{
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create coordinate input group
    QGroupBox* coordGroup = new QGroupBox(tr("Coordinate Input"), this);
    QGridLayout* coordLayout = new QGridLayout(coordGroup);
    
    // X coordinate
    m_xLabel = new QLabel(tr("X:"), this);
    m_xSpinBox = new QDoubleSpinBox(this);
    m_xSpinBox->setDecimals(6);
    m_xSpinBox->setRange(-999999.0, 999999.0);
    m_xSpinBox->setValue(0.0);
    m_xSpinBox->setSuffix(" inch");
    
    // Y coordinate  
    m_yLabel = new QLabel(tr("Y:"), this);
    m_ySpinBox = new QDoubleSpinBox(this);
    m_ySpinBox->setDecimals(6);
    m_ySpinBox->setRange(-999999.0, 999999.0);
    m_ySpinBox->setValue(0.0);
    m_ySpinBox->setSuffix(" inch");
    
    // Unit selection
    m_unitLabel = new QLabel(tr("Unit:"), this);
    m_unitComboBox = new QComboBox(this);
    m_unitComboBox->addItem(tr("Inch"));
    m_unitComboBox->addItem(tr("MM"));
    m_unitComboBox->setCurrentIndex(0);
    
    // Add to grid layout
    coordLayout->addWidget(m_xLabel, 0, 0);
    coordLayout->addWidget(m_xSpinBox, 0, 1);
    coordLayout->addWidget(m_yLabel, 1, 0);
    coordLayout->addWidget(m_ySpinBox, 1, 1);
    coordLayout->addWidget(m_unitLabel, 2, 0);
    coordLayout->addWidget(m_unitComboBox, 2, 1);
    
    // Create button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_goToButton = new QPushButton(tr("Go To"), this);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    
    m_goToButton->setDefault(true);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_goToButton);
    buttonLayout->addWidget(m_cancelButton);
    
    // Add to main layout
    mainLayout->addWidget(coordGroup);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_unitComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GoToCoordinateDialog::onUnitChanged);
    connect(m_goToButton, &QPushButton::clicked,
            this, &GoToCoordinateDialog::onGoToClicked);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &GoToCoordinateDialog::onCancelClicked);
}

void GoToCoordinateDialog::onUnitChanged(int index)
{
    if (index == m_currentUnit) {
        return;
    }
    
    // Convert current values to the new unit
    convertCoordinates(m_currentUnit, index);
    m_currentUnit = index;
    
    // Update suffixes
    QString suffix = (index == 0) ? " inch" : " mm";
    m_xSpinBox->setSuffix(suffix);
    m_ySpinBox->setSuffix(suffix);
}

void GoToCoordinateDialog::convertCoordinates(int fromUnit, int toUnit)
{
    if (fromUnit == toUnit) {
        return;
    }
    
    double currentX = m_xSpinBox->value();
    double currentY = m_ySpinBox->value();
    
    if (fromUnit == 0 && toUnit == 1) {
        // Convert from inches to mm
        currentX *= 25.4;
        currentY *= 25.4;
    } else if (fromUnit == 1 && toUnit == 0) {
        // Convert from mm to inches
        currentX /= 25.4;
        currentY /= 25.4;
    }
    
    m_xSpinBox->setValue(currentX);
    m_ySpinBox->setValue(currentY);
}

void GoToCoordinateDialog::onGoToClicked()
{
    // Get current values and convert to inches if necessary
    double x = m_xSpinBox->value();
    double y = m_ySpinBox->value();
    
    if (m_currentUnit == 1) {
        // Convert from mm to inches
        x /= 25.4;
        y /= 25.4;
    }
    
    m_coordinate = QPointF(x, y);
    accept();
}

void GoToCoordinateDialog::onCancelClicked()
{
    reject();
}

QPointF GoToCoordinateDialog::getCoordinate() const
{
    return m_coordinate;
}

void GoToCoordinateDialog::setDisplayUnit(int unit)
{
    if (unit >= 0 && unit <= 1) {
        m_unitComboBox->setCurrentIndex(unit);
        onUnitChanged(unit);
    }
}