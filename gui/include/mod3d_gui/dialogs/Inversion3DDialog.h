#pragma once

#include "mod3d_gui/ModelDocument.h"
#include "inv/inversion_engine.hpp"

#include <QDialog>
#include <QTabWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>

#include <thread>
#include <stop_token>
#include <optional>
#include <memory>

namespace mod3d_gui {

/**
 * @brief 3D Potential Field Focusing Inversion Dialog.
 *
 * Provides full control over:
 *  - 3D voxel discretization with exact horizontal profile plane centering (x_i +- dx/2, y_i +- dy/2)
 *  - Portniaguine-Zhdanov focusing stabilizers (S_MS volume + S_MGS interface area)
 *  - Multi-scale coarse-to-fine hierarchy (1 to 3 levels)
 *  - pfld::InterfaceMesh DEM terrain-conforming forward operator (primary) and Toeplitz FFT benchmark
 *  - Asynchronous background execution using C++20 std::jthread and cooperative std::stop_token
 *  - Real-time convergence monitoring and direct document application
 */
class Inversion3DDialog : public QDialog {
    Q_OBJECT

public:
    explicit Inversion3DDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~Inversion3DDialog() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onRunClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onExportVtkClicked();
    void onExportJsonClicked();
    void onBrowsePriorClicked();

private:
    void setupUi();
    void setupDomainTab(QWidget *parent);
    void setupParametersTab(QWidget *parent);
    void setupExecutionTab(QWidget *parent);

    void updateDomainFromDocument();
    void setRunningState(bool running);
    void appendLog(const QString &text);

    ModelDocument *m_doc{nullptr};

    // Tabs
    QTabWidget *m_tabWidget{nullptr};

    // Domain & Grid Widgets
    QLabel *m_lblGridInfo{nullptr};
    QLabel *m_lblCenteringNotice{nullptr};
    QSpinBox *m_spinNz{nullptr};
    QDoubleSpinBox *m_spinDz{nullptr};
    QDoubleSpinBox *m_spinObsHeight{nullptr};
    QLabel *m_lblZRange{nullptr};

    // Parameters Widgets
    QCheckBox *m_chkFocusing{nullptr};
    QComboBox *m_comboLevels{nullptr};
    QCheckBox *m_chkAdaptiveRefine{nullptr};
    QDoubleSpinBox *m_spinMsWeight{nullptr};
    QDoubleSpinBox *m_spinMgsWeight{nullptr};
    QDoubleSpinBox *m_spinDepthExp{nullptr};
    QDoubleSpinBox *m_spinMinDensity{nullptr};
    QDoubleSpinBox *m_spinMaxDensity{nullptr};
    QComboBox *m_comboOperator{nullptr};
    QDoubleSpinBox *m_spinNoiseRel{nullptr};
    QSpinBox *m_spinIrlsIters{nullptr};
    QSpinBox *m_spinCgMaxIters{nullptr};
    QLineEdit *m_txtPriorPath{nullptr};
    QPushButton *m_btnBrowsePrior{nullptr};

    // Execution & Monitoring Widgets
    QLabel *m_lblStatus{nullptr};
    QProgressBar *m_progressBar{nullptr};
    QLabel *m_lblCurrentStats{nullptr};
    QTextEdit *m_txtLog{nullptr};
    QPushButton *m_btnRun{nullptr};
    QPushButton *m_btnCancel{nullptr};
    QPushButton *m_btnApply{nullptr};
    QPushButton *m_btnExportVtk{nullptr};
    QPushButton *m_btnExportJson{nullptr};
    QPushButton *m_btnClose{nullptr};

    // Background worker thread using C++20 std::jthread
    std::jthread m_workerThread;

    // Last completed inversion result
    std::optional<inv::InversionResult> m_currentResult;
    inv::InversionSetup m_lastSetup;
};

} // namespace mod3d_gui
