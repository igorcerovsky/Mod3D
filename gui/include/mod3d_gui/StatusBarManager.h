#pragma once

#include <QObject>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>

namespace mod3d_gui {

class StatusBarManager : public QObject {
    Q_OBJECT

public:
    explicit StatusBarManager(QStatusBar *statusBar, QObject *parent = nullptr);

    void setMessage(const QString &msg);
    void setProgress(int percentage); // 0..100, -1 hides/clears
    void setRow(int row);
    void setCol(int col);
    void setX(double x);
    void setY(double y);
    void setZ(double z);
    void setFieldValue(const QString &fieldCompName, double value, const QString &unit = "mGal");
    void clearCoordinates();

private:
    QStatusBar *m_statusBar;
    QLabel *m_lblMessage;
    QProgressBar *m_progressBar;
    QLabel *m_lblRow;
    QLabel *m_lblCol;
    QLabel *m_lblX;
    QLabel *m_lblY;
    QLabel *m_lblZ;
    QLabel *m_lblField;
};

} // namespace mod3d_gui
