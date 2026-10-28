#include "mod3d_gui/dialogs/VerticalRangeDialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QVBoxLayout>

namespace mod3d_gui {

VerticalRangeDialog::VerticalRangeDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent), m_doc(doc) {
  setWindowTitle(tr("Change Vertical Range"));
  setMinimumWidth(320);

  auto *mainLayout = new QVBoxLayout(this);
  auto *form = new QFormLayout();

  m_spinZMin = new QDoubleSpinBox(this);
  m_spinZMin->setRange(-1e6, 1e6);
  m_spinZMin->setSuffix(" m");
  m_spinZMin->setValue(m_doc->model().z_min_val());
  form->addRow(tr("Z min:"), m_spinZMin);

  m_spinZMax = new QDoubleSpinBox(this);
  m_spinZMax->setRange(-1e6, 1e6);
  m_spinZMax->setSuffix(" m");
  m_spinZMax->setValue(m_doc->model().z_max_val());
  form->addRow(tr("Z max:"), m_spinZMax);

  mainLayout->addLayout(form);

  auto *btnBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
    if (applyToDocument()) {
      accept();
    }
  });
  connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  mainLayout->addWidget(btnBox);
}

bool VerticalRangeDialog::applyToDocument() {
  if (!m_doc)
    return false;
  const double zMin = m_spinZMin->value();
  const double zMax = m_spinZMax->value();
  if (zMin >= zMax) {
    QMessageBox::warning(this, tr("Invalid Vertical Range"),
                         tr("Minimum Z must be strictly less than Maximum Z."));
    return false;
  }
  if (m_doc->model().would_vertical_range_adjust_bodies(zMin, zMax)) {
    const auto res = QMessageBox::warning(
        this, tr("Confirm Vertical Range Adjustment"),
        tr("The new vertical range [%1, %2] m intersects existing model geometry.\n\n"
           "Applying this operation will adjust model vertices to fit within the new range. "
           "Are you sure you want to proceed?")
            .arg(zMin)
            .arg(zMax),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res != QMessageBox::Yes) {
      return false;
    }
  }
  return m_doc->setVerticalRange(zMin, zMax);
}

} // namespace mod3d_gui
