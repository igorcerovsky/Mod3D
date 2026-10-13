#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QComboBox>

namespace mod3d_gui {

class ActiveGridsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ActiveGridsDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~ActiveGridsDialog() override = default;

private:
    ModelDocument *m_doc;
    QComboBox *m_comboComponents;
};

} // namespace mod3d_gui
