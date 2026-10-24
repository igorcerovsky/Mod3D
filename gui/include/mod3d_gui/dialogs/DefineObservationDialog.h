#pragma once

#include "mod3d_gui/ModelDocument.h"
#include "mod3d/Grid.h"
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

namespace mod3d_gui {

class DefineObservationDialog : public QDialog {
    Q_OBJECT

public:
    enum class CreationMode {
        DefineObservation = 0, // Legacy CDlgStart option 0: Custom dimensions or geometry from grid file
        ImportReliefGrid = 1   // Legacy CDlgStart option 1: Import observation area grid (topography and/or field)
    };

    explicit DefineObservationDialog(ModelDocument *doc, QWidget *parent = nullptr);
    ~DefineObservationDialog() override = default;

    CreationMode creationMode() const noexcept { return m_mode; }
    void setCreationMode(CreationMode mode);

    // Topography Grid operations
    bool loadFromTopoGridFile(const QString &filePath);
    void clearTopoGrid();
    bool hasTopoGrid() const noexcept { return m_fromTopoGrid; }
    QString topoGridFilePath() const;

    // Measured Field Grid operations
    bool loadFromFieldGridFile(const QString &filePath);
    void clearFieldGrid();
    bool hasFieldGrid() const noexcept { return m_fromFieldGrid; }
    QString fieldGridFilePath() const;
    mod3d::FieldComponent fieldComponent() const noexcept;
    void setFieldComponent(mod3d::FieldComponent comp);

    // Backward-compatibility wrappers
    bool loadFromGridFile(const QString &filePath) { return loadFromTopoGridFile(filePath); }
    void clearGridFile();
    bool isFromGridFile() const noexcept { return m_fromTopoGrid || m_fromFieldGrid; }
    QString gridFilePath() const;

    bool applyToDocument();

    // Computed statistics accessors
    [[nodiscard]] double lengthX() const noexcept;
    [[nodiscard]] double lengthY() const noexcept;
    [[nodiscard]] double gridMinZ() const noexcept;
    [[nodiscard]] double gridMaxZ() const noexcept;

    // Widget accessors for testing
    QRadioButton *defineObsRadioButton() const noexcept { return m_radioDefineObs; }
    QRadioButton *importReliefRadioButton() const noexcept { return m_radioImportRelief; }
    QPushButton *getTopoGridButton() const noexcept { return m_btnGetTopoGrid; }
    QPushButton *getFieldGridButton() const noexcept { return m_btnGetFieldGrid; }
    QPushButton *clearTopoGridButton() const noexcept { return m_btnClearTopoGrid; }
    QPushButton *clearFieldGridButton() const noexcept { return m_btnClearFieldGrid; }
    QComboBox *fieldComponentComboBox() const noexcept { return m_comboFieldComp; }

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
    QLabel *lengthXLabel() const noexcept { return m_lblLengthX; }
    QLabel *lengthYLabel() const noexcept { return m_lblLengthY; }
    QLabel *gridMinZLabel() const noexcept { return m_lblGridMinZ; }
    QLabel *gridMaxZLabel() const noexcept { return m_lblGridMaxZ; }

private:
    void syncColsFromXBounds();
    void syncXMaxFromCols();
    void syncRowsFromYBounds();
    void syncYMaxFromRows();
    void syncDimensionsFromGrid(const mod3d::Grid &g);
    void updateControlStates();
    void updateStatisticsDisplay();
    void onBrowseTopoGridFile();
    void onBrowseFieldGridFile();

    ModelDocument *m_doc;
    CreationMode m_mode{CreationMode::DefineObservation};

    bool m_fromTopoGrid{false};
    mod3d::Grid m_loadedTopoGrid;

    bool m_fromFieldGrid{false};
    mod3d::Grid m_loadedFieldGrid;

    QRadioButton *m_radioDefineObs{nullptr};
    QRadioButton *m_radioImportRelief{nullptr};

    QPushButton *m_btnGetTopoGrid{nullptr};
    QLineEdit *m_editTopoGridFile{nullptr};
    QPushButton *m_btnClearTopoGrid{nullptr};

    QPushButton *m_btnGetFieldGrid{nullptr};
    QLineEdit *m_editFieldGridFile{nullptr};
    QComboBox *m_comboFieldComp{nullptr};
    QPushButton *m_btnClearFieldGrid{nullptr};

    QLabel *m_lblLengthX{nullptr};
    QLabel *m_lblLengthY{nullptr};
    QLabel *m_lblGridMinZTitle{nullptr};
    QLabel *m_lblGridMinZ{nullptr};
    QLabel *m_lblGridMaxZTitle{nullptr};
    QLabel *m_lblGridMaxZ{nullptr};

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
