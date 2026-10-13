#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QTextEdit>

namespace mod3d_gui {

class ModelPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModelPropertiesDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~ModelPropertiesDialog() override = default;

private:
    ModelDocument *m_doc;
    QDoubleSpinBox *m_spinZMin;
    QDoubleSpinBox *m_spinZMax;
    QTextEdit *m_editDescription;
};

} // namespace mod3d_gui
