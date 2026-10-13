#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QDoubleSpinBox>

namespace mod3d_gui {

class InducingFieldDialog : public QDialog {
    Q_OBJECT

public:
    explicit InducingFieldDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~InducingFieldDialog() override = default;

private:
    ModelDocument *m_doc;
    QDoubleSpinBox *m_spinT0;
    QDoubleSpinBox *m_spinInc;
    QDoubleSpinBox *m_spinDec;
};

} // namespace mod3d_gui
