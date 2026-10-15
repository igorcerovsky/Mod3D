#include "mod3d_gui/SpreadsheetView.h"
#include <QVBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>

namespace mod3d_gui {

SpreadsheetModel::SpreadsheetModel(ModelDocument *doc, QObject *parent)
    : QAbstractTableModel(parent)
    , m_doc(doc)
{
    connect(m_doc, &ModelDocument::computationFinished, this, &SpreadsheetModel::refresh);
    connect(m_doc, &ModelDocument::profileChanged, this, &SpreadsheetModel::refresh);
    connect(m_doc, &ModelDocument::activeFieldChanged, this, &SpreadsheetModel::refresh);
    connect(m_doc, &ModelDocument::documentReset, this, &SpreadsheetModel::refresh);
}

int SpreadsheetModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_doc) return 0;
    return m_doc->profileColumnLineCount();
}

int SpreadsheetModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_doc) return 0;
    // Index, LineCoord, ProfileCoord, Modeled, Measured, Difference, BodyCount
    return 7;
}

QVariant SpreadsheetModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !m_doc) return {};

    int row = index.row();
    int col = index.column();

    if (role == Qt::DisplayRole) {
        int prfIdx = m_doc->profileIndex();
        bool isEW = (m_doc->profileOrientation() == ProfileOrientation::EastWest);

        size_t r = isEW ? static_cast<size_t>(prfIdx) : static_cast<size_t>(row);
        size_t c = isEW ? static_cast<size_t>(row) : static_cast<size_t>(prfIdx);

        const auto *modGrd = m_doc->activeModeledGrid();
        const auto *obsGrd = m_doc->activeObservedGrid();
        const auto *difGrd = m_doc->activeDifferenceGrid();

        switch (col) {
        case 0: // Column / Point Index
            return row + 1;
        case 1: // Coordinate along profile
            return QString::number(m_doc->columnCoordinate(row), 'f', 1);
        case 2: // Profile slice coordinate
            return QString::number(m_doc->currentProfileCoordinate(), 'f', 1);
        case 3: // Modeled
            if (modGrd && r < modGrd->rows() && c < modGrd->cols()) {
                return QString::number((*modGrd)(r, c), 'f', 3);
            }
            return "-";
        case 4: // Measured
            if (obsGrd && r < obsGrd->rows() && c < obsGrd->cols()) {
                return QString::number((*obsGrd)(r, c), 'f', 3);
            }
            return "-";
        case 5: // Difference
            if (difGrd && r < difGrd->rows() && c < difGrd->cols()) {
                return QString::number((*difGrd)(r, c), 'f', 3);
            }
            return "-";
        case 6: { // Number of bodies at this column
            int modelR = static_cast<int>(r + 1);
            int modelC = static_cast<int>(c + 1);
            return static_cast<int>(m_doc->model().count(modelR, modelC));
        }
        default:
            break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    return {};
}

QVariant SpreadsheetModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};

    if (orientation == Qt::Horizontal) {
        bool isEW = m_doc && (m_doc->profileOrientation() == ProfileOrientation::EastWest);
        switch (section) {
        case 0: return tr("Point");
        case 1: return isEW ? tr("X (m)") : tr("Y (m)");
        case 2: return isEW ? tr("Y (m)") : tr("X (m)");
        case 3: return tr("Modeled");
        case 4: return tr("Measured");
        case 5: return tr("Difference");
        case 6: return tr("Bodies");
        default: break;
        }
    } else {
        return section + 1;
    }
    return {};
}

void SpreadsheetModel::refresh() {
    beginResetModel();
    endResetModel();
}

SpreadsheetView::SpreadsheetView(ModelDocument *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tableView = new QTableView(this);
    m_tableModel = new SpreadsheetModel(m_doc, this);
    m_tableView->setModel(m_tableModel);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout->addWidget(m_tableView);
}

void SpreadsheetView::copyToClipboard() {
    QString text;
    int rows = m_tableModel->rowCount();
    int cols = m_tableModel->columnCount();

    // Headers
    for (int c = 0; c < cols; ++c) {
        text += m_tableModel->headerData(c, Qt::Horizontal).toString();
        if (c < cols - 1) text += "\t";
    }
    text += "\n";

    // Data
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            text += m_tableModel->data(m_tableModel->index(r, c)).toString();
            if (c < cols - 1) text += "\t";
        }
        text += "\n";
    }

    QApplication::clipboard()->setText(text);
}

void SpreadsheetView::exportCsv(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    int rows = m_tableModel->rowCount();
    int cols = m_tableModel->columnCount();

    // Headers
    for (int c = 0; c < cols; ++c) {
        out << "\"" << m_tableModel->headerData(c, Qt::Horizontal).toString() << "\"";
        if (c < cols - 1) out << ",";
    }
    out << "\n";

    // Data
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            out << m_tableModel->data(m_tableModel->index(r, c)).toString();
            if (c < cols - 1) out << ",";
        }
        out << "\n";
    }
}

} // namespace mod3d_gui
