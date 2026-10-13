#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <vector>

namespace mod3d_gui {

class SpreadsheetModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit SpreadsheetModel(ModelDocument *doc, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void refresh();

private:
    ModelDocument *m_doc;
};

class SpreadsheetView : public QWidget {
    Q_OBJECT

public:
    explicit SpreadsheetView(ModelDocument *doc, QWidget *parent = nullptr);
    ~SpreadsheetView() override = default;

    void copyToClipboard();
    void exportCsv(const QString &filePath);

private:
    ModelDocument *m_doc;
    QTableView *m_tableView;
    SpreadsheetModel *m_tableModel;
};

} // namespace mod3d_gui
