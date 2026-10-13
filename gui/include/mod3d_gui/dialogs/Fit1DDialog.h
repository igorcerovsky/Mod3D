#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QSpinBox>

namespace mod3d_gui {

class Fit1DDialog : public QDialog {
    Q_OBJECT

public:
    explicit Fit1DDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~Fit1DDialog() override = default;

private:
    ModelDocument *m_doc;
    QRadioButton *m_rbBrent;
    QRadioButton *m_rbGolden;
    QRadioButton *m_rbRms;
    QRadioButton *m_rbDrv;
    QDoubleSpinBox *m_spinTolerance;
    QSpinBox *m_spinMaxIter;
};

} // namespace mod3d_gui
