#include "mod3d_gui/dialogs/ActiveGridsDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace mod3d_gui {

ActiveGridsDialog::ActiveGridsDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Select Active Field Component"));
    setMinimumWidth(320);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_comboComponents = new QComboBox(this);

    auto addComp = [this](const QString &text, mod3d::FieldComponent comp) {
        m_comboComponents->addItem(text, static_cast<int>(comp));
    };

    addComp("Gx (Gravity East)", mod3d::FieldComponent::GX);
    addComp("Gy (Gravity North)", mod3d::FieldComponent::GY);
    addComp("Gz (Gravity Vertical Bouguer)", mod3d::FieldComponent::GZ);
    addComp("G tot (Total Gravity Magnitude)", mod3d::FieldComponent::G_TOT);

    addComp("Mx (Magnetics East)", mod3d::FieldComponent::MX);
    addComp("My (Magnetics North)", mod3d::FieldComponent::MY);
    addComp("Mz (Magnetics Vertical)", mod3d::FieldComponent::MZ);
    addComp("ΔT (Total Magnetic Anomaly)", mod3d::FieldComponent::DELTA_T);

    addComp("Txx (Gravity Gradient XX)", mod3d::FieldComponent::GXX);
    addComp("Tyy (Gravity Gradient YY)", mod3d::FieldComponent::GYY);
    addComp("Tzz (Gravity Gradient ZZ)", mod3d::FieldComponent::GZZ);
    addComp("Txy (Gravity Gradient XY)", mod3d::FieldComponent::GXY);
    addComp("Txz (Gravity Gradient XZ)", mod3d::FieldComponent::GXZ);
    addComp("Tyz (Gravity Gradient YZ)", mod3d::FieldComponent::GYZ);

    int curIdx = m_comboComponents->findData(static_cast<int>(m_doc->activeField()));
    if (curIdx >= 0) m_comboComponents->setCurrentIndex(curIdx);

    form->addRow(tr("Active Field Component:"), m_comboComponents);
    mainLayout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        auto comp = static_cast<mod3d::FieldComponent>(m_comboComponents->currentData().toInt());
        m_doc->setActiveField(comp);
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

} // namespace mod3d_gui
