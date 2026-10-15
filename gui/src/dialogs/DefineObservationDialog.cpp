#include "mod3d_gui/dialogs/DefineObservationDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace mod3d_gui {

DefineObservationDialog::DefineObservationDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Define Observation Area"));
    setMinimumWidth(360);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    const auto &m = m_doc->model();

    m_spinRows = new QSpinBox(this);
    m_spinRows->setRange(3, 500);
    m_spinRows->setValue(m.rows() > 0 ? m.rows() : 21);
    form->addRow(tr("Rows:"), m_spinRows);

    m_spinCols = new QSpinBox(this);
    m_spinCols->setRange(3, 500);
    m_spinCols->setValue(m.cols() > 0 ? m.cols() : 21);
    form->addRow(tr("Columns:"), m_spinCols);

    m_spinX0 = new QDoubleSpinBox(this);
    m_spinX0->setRange(-1e7, 1e7);
    m_spinX0->setSuffix(" m");
    m_spinX0->setValue(m.x0());
    form->addRow(tr("X origin (X0):"), m_spinX0);

    m_spinY0 = new QDoubleSpinBox(this);
    m_spinY0->setRange(-1e7, 1e7);
    m_spinY0->setSuffix(" m");
    m_spinY0->setValue(m.y0());
    form->addRow(tr("Y origin (Y0):"), m_spinY0);

    m_spinDX = new QDoubleSpinBox(this);
    m_spinDX->setRange(1.0, 1e6);
    m_spinDX->setSuffix(" m");
    m_spinDX->setValue(m.dx() > 0 ? m.dx() : 500.0);
    form->addRow(tr("Step dX:"), m_spinDX);

    m_spinDY = new QDoubleSpinBox(this);
    m_spinDY->setRange(1.0, 1e6);
    m_spinDY->setSuffix(" m");
    m_spinDY->setValue(m.dy() > 0 ? m.dy() : 500.0);
    form->addRow(tr("Step dY:"), m_spinDY);

    m_spinZMin = new QDoubleSpinBox(this);
    m_spinZMin->setRange(-1e6, 1e6);
    m_spinZMin->setSuffix(" m");
    m_spinZMin->setValue(m.z_min() != 0 ? m.z_min() : -4000.0);
    form->addRow(tr("Vertical Range Z min:"), m_spinZMin);

    m_spinZMax = new QDoubleSpinBox(this);
    m_spinZMax->setRange(-1e6, 1e6);
    m_spinZMax->setSuffix(" m");
    m_spinZMax->setValue(m.z_max() != 0 ? m.z_max() : 1000.0);
    form->addRow(tr("Vertical Range Z max:"), m_spinZMax);

    mainLayout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        m_doc->newProject(m_spinRows->value(), m_spinCols->value(),
                          m_spinX0->value(), m_spinY0->value(),
                          m_spinDX->value(), m_spinDY->value(),
                          m_spinZMin->value(), m_spinZMax->value());
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

} // namespace mod3d_gui
