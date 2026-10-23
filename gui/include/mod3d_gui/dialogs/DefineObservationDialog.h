#pragma once

#include "mod3d_gui/ModelDocument.h"
#include "mod3d/Grid.h"
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>

namespace mod3d_gui {

class DefineObservationDialog : public QDialog {
    Q_OBJECT

public:
    enum class CreationMode {
        DefineObservation = 0, // Legacy CDlgStart option 0: Custom dimensions or geometry from grid file
        ImportReliefGrid = 1   // Legacy CDlgStart option 1: Import observation / surface relief grid (.grd)
    };

    explicit DefineObservationDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~DefineObservationDialog() override = default;

    CreationMode creationMode() const noexcept { return m_mode; }
    void setCreationMode(CreationMode mode);

    bool loadFromGridFile(const QString &filePath);
    void clearGridFile();
    bool isFromGridFile() const noexcept { return m_fromGrid; }
    QString gridFilePath() const;

    bool applyToDocument();

    // Widget accessors for testing
    QRadioButton *defineObsRadioButton() const noexcept { return m_radioDefineObs; }
    QRadioButton *importReliefRadioButton() const noexcept { return m_radioImportRelief; }
    QDoubleSpinBox *minXSpinBox() const noexcept { return m_spinX0; }
    QDoubleSpinBox *maxXSpinBox() const noexcept { return m_spinXMax; }
    QDoubleSpinBox *dxSpinBox() const noexcept { return m_spinDX; }
    QSpinBox *colsSpinBox() const noexcept { return m_spinCols; }
    QDoubleSpinBox *minYSpinBox() const noexcept { return m_spinY0; }
    QDoubleSpinBox *maxYSpinBox() const noexcept { return m_spinYMax; }
    QDoubleSpinBox *dySpinBox() const noexcept { return m_spinDY; }
    QSpinBox *rowsSpinBox() const noexcept { return m_spinRows; }
    QDoubleSpinBox *minZSpinBox() const noexcept { return m_spinZMin; }
    QDoubleSpinBox *maxZSpinBox() const noexcept { return m_spinZMax; }

private:
    void syncColsFromXBounds();
    void syncXMaxFromCols();
    void syncRowsFromYBounds();
    void syncYMaxFromRows();
    void updateControlStates();
    void onBrowseGridFile();

    ModelDocument *m_doc;
    CreationMode m_mode{CreationMode::DefineObservation};
    bool m_fromGrid{false};
    mod3d::Grid m_loadedGrid;

    QRadioButton *m_radioDefineObs{nullptr};
    QRadioButton *m_radioImportRelief{nullptr};

    QPushButton *m_btnGetFromGrid{nullptr};
    QPushButton *m_btnClearGrid{nullptr};
    QLineEdit *m_editGridFile{nullptr};

    QDoubleSpinBox *m_spinX0{nullptr};
    QDoubleSpinBox *m_spinXMax{nullptr};
    QDoubleSpinBox *m_spinDX{nullptr};
    QSpinBox *m_spinCols{nullptr};

    QDoubleSpinBox *m_spinY0{nullptr};
    QDoubleSpinBox *m_spinYMax{nullptr};
    QDoubleSpinBox *m_spinDY{nullptr};
    QSpinBox *m_spinRows{nullptr};

    QDoubleSpinBox *m_spinZMin{nullptr};
    QDoubleSpinBox *m_spinZMax{nullptr};
};

} // namespace mod3d_gui
