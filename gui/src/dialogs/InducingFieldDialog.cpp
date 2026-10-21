#include "mod3d_gui/dialogs/InducingFieldDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace mod3d_gui {

InducingFieldDialog::InducingFieldDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Inducing Geomagnetic Field"));
    setMinimumWidth(320);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_spinT0 = new QDoubleSpinBox(this);
    m_spinT0->setRange(0.0, 100000.0);
    m_spinT0->setSuffix(" nT");
    m_spinT0->setValue(m_doc->observation().magnetic_intensity().value());
    form->addRow(tr("Field Intensity (T0):"), m_spinT0);

    m_spinInc = new QDoubleSpinBox(this);
    m_spinInc->setRange(-90.0, 90.0);
    m_spinInc->setSuffix(" °");
    m_spinInc->setValue(m_doc->observation().inclination().value());
    form->addRow(tr("Inclination (I0):"), m_spinInc);

    m_spinDec = new QDoubleSpinBox(this);
    m_spinDec->setRange(0.0, 360.0);
    m_spinDec->setSuffix(" °");
    m_spinDec->setValue(m_doc->observation().declination().value());
    form->addRow(tr("Declination (D0):"), m_spinDec);

    mainLayout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        m_doc->observation().set_ambient_field_params(m_spinInc->value(), m_spinDec->value(), m_spinT0->value());
        m_doc->setModified(true);
        m_doc->computeAll();
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

} // namespace mod3d_gui
