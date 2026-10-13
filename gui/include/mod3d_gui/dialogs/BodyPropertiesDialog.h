#pragma once

#include "mod3d/Body.h"
#include <QDialog>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QSlider>
#include <QPushButton>

namespace mod3d_gui {

class BodyPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit BodyPropertiesDialog(mod3d::Body *body, QWidget *parent = nullptr);
    ~BodyPropertiesDialog() override = default;

    QDoubleSpinBox *densitySpinBox() const { return m_spinDensity; }
    QDoubleSpinBox *susceptibilitySpinBox() const { return m_spinKappa; }

private:
    void setupGravityTab(QTabWidget *tabs);
    void setupMagneticsTab(QTabWidget *tabs);
    void setupDrawingTab(QTabWidget *tabs);
    void setupComputationTab(QTabWidget *tabs);
    void setupDescriptionTab(QTabWidget *tabs);
    void applyChanges();

    mod3d::Body *m_body;

    // Gravity
    QDoubleSpinBox *m_spinDensity;
    QDoubleSpinBox *m_spinGradX;
    QDoubleSpinBox *m_spinGradY;
    QDoubleSpinBox *m_spinGradZ;
    QDoubleSpinBox *m_spinGradOriginX;
    QDoubleSpinBox *m_spinGradOriginY;
    QDoubleSpinBox *m_spinGradOriginZ;

    // Magnetics
    QDoubleSpinBox *m_spinKappa;
    QDoubleSpinBox *m_spinRemJ;
    QDoubleSpinBox *m_spinRemInc;
    QDoubleSpinBox *m_spinRemDec;

    // Drawing
    QPushButton *m_btnColor;
    QColor m_selectedColor;
    QCheckBox *m_chkFill;
    QSlider *m_sliderAlpha;

    // Computation
    QCheckBox *m_chkActive;
    QCheckBox *m_chkLocked;

    // Description
    QLineEdit *m_editName;
    QTextEdit *m_editDescription;
};

} // namespace mod3d_gui
