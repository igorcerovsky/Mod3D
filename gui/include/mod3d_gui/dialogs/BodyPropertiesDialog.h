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

    mod3d::Body *m_body{nullptr};

    // Gravity
    QDoubleSpinBox *m_spinDensity{nullptr};
    QDoubleSpinBox *m_spinGradX{nullptr};
    QDoubleSpinBox *m_spinGradY{nullptr};
    QDoubleSpinBox *m_spinGradZ{nullptr};
    QDoubleSpinBox *m_spinGradOriginX{nullptr};
    QDoubleSpinBox *m_spinGradOriginY{nullptr};
    QDoubleSpinBox *m_spinGradOriginZ{nullptr};

    // Magnetics
    QDoubleSpinBox *m_spinKappa{nullptr};
    QDoubleSpinBox *m_spinRemJ{nullptr};
    QDoubleSpinBox *m_spinRemInc{nullptr};
    QDoubleSpinBox *m_spinRemDec{nullptr};

    // Drawing
    QPushButton *m_btnColor{nullptr};
    QColor m_selectedColor;
    QCheckBox *m_chkFill{nullptr};
    QSlider *m_sliderAlpha{nullptr};

    // Computation
    QCheckBox *m_chkActive{nullptr};
    QCheckBox *m_chkLocked{nullptr};

    // Description
    QLineEdit *m_editName{nullptr};
    QTextEdit *m_editDescription{nullptr};
};

} // namespace mod3d_gui
