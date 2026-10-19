#include "mod3d_gui/dialogs/VerticalRangeDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace mod3d_gui {

VerticalRangeDialog::VerticalRangeDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Change Vertical Range"));
    setMinimumWidth(320);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_spinZMin = new QDoubleSpinBox(this);
    m_spinZMin->setRange(-1e6, 1e6);
    m_spinZMin->setSuffix(" m");
    m_spinZMin->setValue(m_doc->model().z_min_val());
    form->addRow(tr("Z min (Hell):"), m_spinZMin);

    m_spinZMax = new QDoubleSpinBox(this);
    m_spinZMax->setRange(-1e6, 1e6);
    m_spinZMax->setSuffix(" m");
    m_spinZMax->setValue(m_doc->model().z_max_val());
    form->addRow(tr("Z max (Heaven):"), m_spinZMax);

    mainLayout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        // Update model and observation vertical range
        m_doc->setModified(true);
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

} // namespace mod3d_gui
