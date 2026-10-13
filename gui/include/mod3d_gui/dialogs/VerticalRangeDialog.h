#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QDoubleSpinBox>

namespace mod3d_gui {

class VerticalRangeDialog : public QDialog {
    Q_OBJECT

public:
    explicit VerticalRangeDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~VerticalRangeDialog() override = default;

private:
    ModelDocument *m_doc;
    QDoubleSpinBox *m_spinZMin;
    QDoubleSpinBox *m_spinZMax;
};

} // namespace mod3d_gui
