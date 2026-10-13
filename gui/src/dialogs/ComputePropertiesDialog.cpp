#include "mod3d_gui/dialogs/ComputePropertiesDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace mod3d_gui {

ComputePropertiesDialog::ComputePropertiesDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Computation Properties"));
    setMinimumWidth(360);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_spinGrvHeight = new QDoubleSpinBox(this);
    m_spinGrvHeight->setRange(0.0, 50000.0);
    m_spinGrvHeight->setSuffix(" m");
    m_spinGrvHeight->setValue(m_doc->observation().gravity_height());
    form->addRow(tr("Gravity Sensor Height:"), m_spinGrvHeight);

    m_spinRefDensity = new QDoubleSpinBox(this);
    m_spinRefDensity->setRange(0.0, 10000.0);
    m_spinRefDensity->setSingleStep(50.0);
    m_spinRefDensity->setDecimals(1);
    m_spinRefDensity->setSuffix(" kg/m³");
    m_spinRefDensity->setValue(m_doc->observation().reference_density());
    form->addRow(tr("Reference Reduction Density:"), m_spinRefDensity);

    m_spinMagHeight = new QDoubleSpinBox(this);
    m_spinMagHeight->setRange(0.0, 50000.0);
    m_spinMagHeight->setSuffix(" m");
    m_spinMagHeight->setValue(m_doc->observation().magnetic_height());
    form->addRow(tr("Magnetics Sensor Height:"), m_spinMagHeight);

    m_chkRealTime = new QCheckBox(tr("Real-Time Computation during Vertex Drag"), this);
    m_chkRealTime->setChecked(m_doc->isRealTimeCompute());
    form->addRow(m_chkRealTime);

    mainLayout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        m_doc->observation().set_gravity_observation(mod3d::ObservationMode::SensorHeight, m_spinGrvHeight->value());
        m_doc->observation().set_reference_density(m_spinRefDensity->value());
        m_doc->observation().set_magnetic_observation(mod3d::ObservationMode::SensorHeight, m_spinMagHeight->value());
        m_doc->setRealTimeCompute(m_chkRealTime->isChecked());
        m_doc->computeAll();
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

} // namespace mod3d_gui
