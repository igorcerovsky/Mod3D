#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QDoubleSpinBox>
#include <QCheckBox>

namespace mod3d_gui {

class ComputePropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit ComputePropertiesDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~ComputePropertiesDialog() override = default;

private:
    ModelDocument *m_doc;
    QDoubleSpinBox *m_spinGrvHeight;
    QDoubleSpinBox *m_spinRefDensity;
    QDoubleSpinBox *m_spinMagHeight;
    QCheckBox *m_chkRealTime;
};

} // namespace mod3d_gui
