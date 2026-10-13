#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>

namespace mod3d_gui {

class DefineObservationDialog : public QDialog {
    Q_OBJECT

public:
    explicit DefineObservationDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~DefineObservationDialog() override = default;

private:
    ModelDocument *m_doc;

    QSpinBox *m_spinRows;
    QSpinBox *m_spinCols;
    QDoubleSpinBox *m_spinX0;
    QDoubleSpinBox *m_spinY0;
    QDoubleSpinBox *m_spinDX;
    QDoubleSpinBox *m_spinDY;
    QDoubleSpinBox *m_spinZMin;
    QDoubleSpinBox *m_spinZMax;
};

} // namespace mod3d_gui
