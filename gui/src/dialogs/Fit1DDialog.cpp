#include "mod3d_gui/dialogs/Fit1DDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>

namespace mod3d_gui {

Fit1DDialog::Fit1DDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("1D Automated Fitting Properties"));
    setMinimumWidth(380);

    auto *mainLayout = new QVBoxLayout(this);

    // Method box
    auto *grpMethod = new QGroupBox(tr("Optimization Algorithm"), this);
    auto *vboxMethod = new QVBoxLayout(grpMethod);
    m_rbBrent = new QRadioButton(tr("Brent's Method (Parabolic Interpolation + Golden Section Fallback)"), grpMethod);
    m_rbGolden = new QRadioButton(tr("Golden Section Search (Robust Linear Bracketing)"), grpMethod);
    m_rbBrent->setChecked(true);
    vboxMethod->addWidget(m_rbBrent);
    vboxMethod->addWidget(m_rbGolden);
    mainLayout->addWidget(grpMethod);

    // Objective function box
    auto *grpObj = new QGroupBox(tr("Objective Target Function"), this);
    auto *vboxObj = new QVBoxLayout(grpObj);
    m_rbRms = new QRadioButton(tr("RMS (Minimize Global Root-Mean-Square Anomaly Error)"), grpObj);
    m_rbDrv = new QRadioButton(tr("Derivative (Minimize Global Error Roughness)"), grpObj);
    m_rbRms->setChecked(true);
    vboxObj->addWidget(m_rbRms);
    vboxObj->addWidget(m_rbDrv);
    mainLayout->addWidget(grpObj);

    // Numerical limits
    auto *form = new QFormLayout();
    m_spinTolerance = new QDoubleSpinBox(this);
    m_spinTolerance->setRange(1e-9, 1e-2);
    m_spinTolerance->setDecimals(8);
    m_spinTolerance->setValue(1e-5);
    form->addRow(tr("Convergence Tolerance:"), m_spinTolerance);

    m_spinMaxIter = new QSpinBox(this);
    m_spinMaxIter->setRange(10, 1000);
    m_spinMaxIter->setValue(100);
    form->addRow(tr("Maximum Iterations:"), m_spinMaxIter);

    mainLayout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

} // namespace mod3d_gui
