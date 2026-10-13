#include "mod3d_gui/dialogs/BodyPropertiesDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QLabel>
#include <cmath>

namespace mod3d_gui {

BodyPropertiesDialog::BodyPropertiesDialog(mod3d::Body *body, QWidget *parent)
    : QDialog(parent)
    , m_body(body)
{
    setWindowTitle(tr("Body Properties - Body %1").arg(m_body ? m_body->id() : 0));
    setMinimumSize(480, 420);

    auto *mainLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    setupGravityTab(tabs);
    setupMagneticsTab(tabs);
    setupDrawingTab(tabs);
    setupComputationTab(tabs);
    setupDescriptionTab(tabs);

    mainLayout->addWidget(tabs);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        applyChanges();
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(btnBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &BodyPropertiesDialog::applyChanges);

    mainLayout->addWidget(btnBox);
}

void BodyPropertiesDialog::setupGravityTab(QTabWidget *tabs) {
    auto *page = new QWidget(tabs);
    auto *layout = new QFormLayout(page);

    m_spinDensity = new QDoubleSpinBox(page);
    m_spinDensity->setRange(-10000.0, 10000.0);
    m_spinDensity->setSingleStep(50.0);
    m_spinDensity->setDecimals(1);
    m_spinDensity->setSuffix(" kg/m³");
    m_spinDensity->setValue(m_body ? m_body->raw_density() : 2670.0);
    layout->addRow(tr("Density:"), m_spinDensity);

    m_spinGradX = new QDoubleSpinBox(page);
    m_spinGradX->setRange(-100.0, 100.0);
    m_spinGradX->setDecimals(5);
    m_spinGradX->setValue(m_body ? m_body->density_gradient().x : 0.0);
    layout->addRow(tr("Density Grad X (kg/m⁴):"), m_spinGradX);

    m_spinGradY = new QDoubleSpinBox(page);
    m_spinGradY->setRange(-100.0, 100.0);
    m_spinGradY->setDecimals(5);
    m_spinGradY->setValue(m_body ? m_body->density_gradient().y : 0.0);
    layout->addRow(tr("Density Grad Y (kg/m⁴):"), m_spinGradY);

    m_spinGradZ = new QDoubleSpinBox(page);
    m_spinGradZ->setRange(-100.0, 100.0);
    m_spinGradZ->setDecimals(5);
    m_spinGradZ->setValue(m_body ? m_body->density_gradient().z : 0.0);
    layout->addRow(tr("Density Grad Z (kg/m⁴):"), m_spinGradZ);

    m_spinGradOriginX = new QDoubleSpinBox(page);
    m_spinGradOriginX->setRange(-1e7, 1e7);
    m_spinGradOriginX->setValue(m_body ? m_body->density_origin().x : 0.0);
    layout->addRow(tr("Grad Origin X (m):"), m_spinGradOriginX);

    m_spinGradOriginY = new QDoubleSpinBox(page);
    m_spinGradOriginY->setRange(-1e7, 1e7);
    m_spinGradOriginY->setValue(m_body ? m_body->density_origin().y : 0.0);
    layout->addRow(tr("Grad Origin Y (m):"), m_spinGradOriginY);

    m_spinGradOriginZ = new QDoubleSpinBox(page);
    m_spinGradOriginZ->setRange(-1e7, 1e7);
    m_spinGradOriginZ->setValue(m_body ? m_body->density_origin().z : 0.0);
    layout->addRow(tr("Grad Origin Z (m):"), m_spinGradOriginZ);

    tabs->addTab(page, tr("Gravity"));
}

void BodyPropertiesDialog::setupMagneticsTab(QTabWidget *tabs) {
    auto *page = new QWidget(tabs);
    auto *layout = new QFormLayout(page);

    m_spinKappa = new QDoubleSpinBox(page);
    m_spinKappa->setRange(-10.0, 10.0);
    m_spinKappa->setSingleStep(0.001);
    m_spinKappa->setDecimals(5);
    m_spinKappa->setSuffix(" SI");
    m_spinKappa->setValue(m_body ? m_body->susceptibility() : 0.0);
    layout->addRow(tr("Susceptibility κ (SI):"), m_spinKappa);

    const auto &rem = m_body ? m_body->remanent_magnetization() : mod3d::Point3D(0, 0, 0);
    double jRem = std::sqrt(rem.x * rem.x + rem.y * rem.y + rem.z * rem.z);

    m_spinRemJ = new QDoubleSpinBox(page);
    m_spinRemJ->setRange(0.0, 1e6);
    m_spinRemJ->setSuffix(" nT");
    m_spinRemJ->setValue(jRem);
    layout->addRow(tr("Remanent Magnetization J:"), m_spinRemJ);

    double inc = (jRem > 1e-9) ? (std::asin(-rem.z / jRem) * 180.0 / 3.14159265) : 0.0;
    double dec = (std::abs(rem.x) > 1e-9 || std::abs(rem.y) > 1e-9) ? (std::atan2(rem.x, rem.y) * 180.0 / 3.14159265) : 0.0;
    if (dec < 0.0) dec += 360.0;

    m_spinRemInc = new QDoubleSpinBox(page);
    m_spinRemInc->setRange(-90.0, 90.0);
    m_spinRemInc->setSuffix(" °");
    m_spinRemInc->setValue(inc);
    layout->addRow(tr("Remanent Inclination:"), m_spinRemInc);

    m_spinRemDec = new QDoubleSpinBox(page);
    m_spinRemDec->setRange(0.0, 360.0);
    m_spinRemDec->setSuffix(" °");
    m_spinRemDec->setValue(dec);
    layout->addRow(tr("Remanent Declination:"), m_spinRemDec);

    tabs->addTab(page, tr("Magnetics"));
}

void BodyPropertiesDialog::setupDrawingTab(QTabWidget *tabs) {
    auto *page = new QWidget(tabs);
    auto *layout = new QFormLayout(page);

    if (m_body) {
        auto c = m_body->color();
        m_selectedColor = QColor(c.r, c.g, c.b, c.a);
    } else {
        m_selectedColor = QColor(180, 120, 80);
    }
    m_btnColor = new QPushButton(tr("Select Color..."), page);
    auto updateBtnStyle = [this]() {
        QString textCol = (m_selectedColor.lightness() > 140) ? "#000000" : "#ffffff";
        m_btnColor->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold; padding: 4px;")
                                      .arg(m_selectedColor.name(), textCol));
    };
    updateBtnStyle();
    connect(m_btnColor, &QPushButton::clicked, this, [this, updateBtnStyle]() {
        QColor col = QColorDialog::getColor(m_selectedColor, this, tr("Select Body Color"));
        if (col.isValid()) {
            m_selectedColor = col;
            updateBtnStyle();
        }
    });
    layout->addRow(tr("Body Color:"), m_btnColor);

    m_chkFill = new QCheckBox(tr("Fill Body Polygons in Profile View"), page);
    m_chkFill->setChecked(m_body ? m_body->is_filled() : true);
    layout->addRow(m_chkFill);

    m_sliderAlpha = new QSlider(Qt::Horizontal, page);
    m_sliderAlpha->setRange(0, 100);
    m_sliderAlpha->setValue(static_cast<int>((m_body ? m_body->transparency() : 0.9f) * 100));
    layout->addRow(tr("3D Transparency:"), m_sliderAlpha);

    tabs->addTab(page, tr("Drawing"));
}

void BodyPropertiesDialog::setupComputationTab(QTabWidget *tabs) {
    auto *page = new QWidget(tabs);
    auto *layout = new QVBoxLayout(page);

    m_chkActive = new QCheckBox(tr("Active in Potential Field Computation"), page);
    m_chkActive->setChecked(m_body ? m_body->is_active() : true);
    layout->addWidget(m_chkActive);

    m_chkLocked = new QCheckBox(tr("Locked (Protected from Mouse Reshaping)"), page);
    m_chkLocked->setChecked(m_body ? m_body->is_locked() : false);
    layout->addWidget(m_chkLocked);

    layout->addStretch();
    tabs->addTab(page, tr("Computation"));
}

void BodyPropertiesDialog::setupDescriptionTab(QTabWidget *tabs) {
    auto *page = new QWidget(tabs);
    auto *layout = new QFormLayout(page);

    auto *lblId = new QLabel(QString::number(m_body ? m_body->id() : 0), page);
    layout->addRow(tr("Body ID:"), lblId);

    m_editName = new QLineEdit(page);
    m_editName->setText(m_body ? QString::fromStdString(m_body->name()) : "");
    layout->addRow(tr("Geological Name:"), m_editName);

    m_editDescription = new QTextEdit(page);
    m_editDescription->setPlainText(m_body ? QString::fromStdString(m_body->description()) : "");
    layout->addRow(tr("Description:"), m_editDescription);

    tabs->addTab(page, tr("Description"));
}

void BodyPropertiesDialog::applyChanges() {
    if (!m_body) return;

    m_body->set_density(m_spinDensity->value());
    m_body->set_density_gradient(mod3d::Point3D(m_spinGradX->value(), m_spinGradY->value(), m_spinGradZ->value()));
    m_body->set_density_origin(mod3d::Point3D(m_spinGradOriginX->value(), m_spinGradOriginY->value(), m_spinGradOriginZ->value()));

    m_body->set_susceptibility(m_spinKappa->value());

    double j = m_spinRemJ->value();
    double incRad = m_spinRemInc->value() * 3.14159265 / 180.0;
    double decRad = m_spinRemDec->value() * 3.14159265 / 180.0;
    double rx = j * std::cos(incRad) * std::sin(decRad);
    double ry = j * std::cos(incRad) * std::cos(decRad);
    double rz = -j * std::sin(incRad);
    m_body->set_remanent_magnetization(mod3d::Point3D(rx, ry, rz));

    m_body->set_color(mod3d::BodyColor(
        static_cast<uint8_t>(m_selectedColor.red()),
        static_cast<uint8_t>(m_selectedColor.green()),
        static_cast<uint8_t>(m_selectedColor.blue()),
        static_cast<uint8_t>(m_selectedColor.alpha())
    ));

    m_body->set_filled(m_chkFill->isChecked());
    m_body->set_transparency(m_sliderAlpha->value() / 100.0f);

    m_body->set_active(m_chkActive->isChecked());
    m_body->set_locked(m_chkLocked->isChecked());

    m_body->set_name(m_editName->text().toStdString());
    m_body->set_description(m_editDescription->toPlainText().toStdString());
}

} // namespace mod3d_gui
