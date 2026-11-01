#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDialog>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

namespace mod3d_gui {

class FitNeighborhoodDialog : public QDialog {
    Q_OBJECT

public:
    explicit FitNeighborhoodDialog(ModelDocument *doc, int lineIdx = -1,
                                   int vertexIdx = -1, QWidget *parent = nullptr);
    ~FitNeighborhoodDialog() override = default;

private slots:
    void onFitNowClicked();
    void onRadiusModeChanged();

private:
    void loadSettingsToUI();
    void applySettingsFromUI();

    ModelDocument *m_doc{nullptr};
    int m_lineIdx{-1};
    int m_vertexIdx{-1};

    QRadioButton *m_rbAutoRadius{nullptr};
    QRadioButton *m_rbCustomRadius{nullptr};
    QDoubleSpinBox *m_spinCustomRadius{nullptr};
    QDoubleSpinBox *m_spinCutoff{nullptr};
    QDoubleSpinBox *m_spinSmoothness{nullptr};
    QDoubleSpinBox *m_spinTolerance{nullptr};
    QSpinBox *m_spinMaxIter{nullptr};
    QPushButton *m_btnFitNow{nullptr};
    QLabel *m_lblStatus{nullptr};
};

} // namespace mod3d_gui
