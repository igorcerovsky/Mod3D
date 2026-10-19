#include "mod3d_gui/dialogs/ModelPropertiesDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace mod3d_gui {

ModelPropertiesDialog::ModelPropertiesDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Model Properties"));
    setMinimumWidth(380);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    const auto &m = m_doc->model();

    form->addRow(tr("Rows:"), new QLabel(QString::number(m.rows()), this));
    form->addRow(tr("Columns:"), new QLabel(QString::number(m.cols()), this));
    form->addRow(tr("X min:"), new QLabel(QString("%1 m").arg(m.x_min_val(), 0, 'f', 1), this));
    form->addRow(tr("X max:"), new QLabel(QString("%1 m").arg(m.x_max_val(), 0, 'f', 1), this));
    form->addRow(tr("Y min:"), new QLabel(QString("%1 m").arg(m.y_min_val(), 0, 'f', 1), this));
    form->addRow(tr("Y max:"), new QLabel(QString("%1 m").arg(m.y_max_val(), 0, 'f', 1), this));
    form->addRow(tr("Cell dX:"), new QLabel(QString("%1 m").arg(m.dx_val(), 0, 'f', 1), this));
    form->addRow(tr("Cell dY:"), new QLabel(QString("%1 m").arg(m.dy_val(), 0, 'f', 1), this));

    m_spinZMin = new QDoubleSpinBox(this);
    m_spinZMin->setRange(-1e6, 1e6);
    m_spinZMin->setSuffix(" m");
    m_spinZMin->setValue(m.z_min_val());
    form->addRow(tr("Model Bottom (Z min):"), m_spinZMin);

    m_spinZMax = new QDoubleSpinBox(this);
    m_spinZMax->setRange(-1e6, 1e6);
    m_spinZMax->setSuffix(" m");
    m_spinZMax->setValue(m.z_max_val());
    form->addRow(tr("Model Top (Z max):"), m_spinZMax);

    m_editDescription = new QTextEdit(this);
    m_editDescription->setMaximumHeight(80);
    m_editDescription->setPlainText(QString::fromStdString(m_doc->project().metadata().description));
    form->addRow(tr("Description:"), m_editDescription);

    mainLayout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        m_doc->project().metadata().description = m_editDescription->toPlainText().toStdString();
        m_doc->setModified(true);
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

} // namespace mod3d_gui
