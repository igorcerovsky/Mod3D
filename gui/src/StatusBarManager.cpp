#include "mod3d_gui/StatusBarManager.h"

namespace mod3d_gui {

StatusBarManager::StatusBarManager(QStatusBar *statusBar, QObject *parent)
    : QObject(parent)
    , m_statusBar(statusBar)
{
    // Pane 0: Message / Help (expands)
    m_lblMessage = new QLabel(tr("Ready"), m_statusBar);
    m_lblMessage->setMinimumWidth(180);
    m_statusBar->addWidget(m_lblMessage, 1);

    // Pane 1: Progress
    m_progressBar = new QProgressBar(m_statusBar);
    m_progressBar->setMaximumWidth(120);
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_statusBar->addPermanentWidget(m_progressBar);

    // Pane 2: Row
    m_lblRow = new QLabel("row: --", m_statusBar);
    m_lblRow->setMinimumWidth(65);
    m_lblRow->setAlignment(Qt::AlignCenter);
    m_lblRow->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_statusBar->addPermanentWidget(m_lblRow);

    // Pane 3: Col
    m_lblCol = new QLabel("col: --", m_statusBar);
    m_lblCol->setMinimumWidth(65);
    m_lblCol->setAlignment(Qt::AlignCenter);
    m_lblCol->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_statusBar->addPermanentWidget(m_lblCol);

    // Pane 4: X
    m_lblX = new QLabel("x: --", m_statusBar);
    m_lblX->setMinimumWidth(110);
    m_lblX->setAlignment(Qt::AlignCenter);
    m_lblX->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_statusBar->addPermanentWidget(m_lblX);

    // Pane 5: Y
    m_lblY = new QLabel("y: --", m_statusBar);
    m_lblY->setMinimumWidth(110);
    m_lblY->setAlignment(Qt::AlignCenter);
    m_lblY->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_statusBar->addPermanentWidget(m_lblY);

    // Pane 6: Z
    m_lblZ = new QLabel("z: --", m_statusBar);
    m_lblZ->setMinimumWidth(100);
    m_lblZ->setAlignment(Qt::AlignCenter);
    m_lblZ->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_statusBar->addPermanentWidget(m_lblZ);

    // Pane 7: Field
    m_lblField = new QLabel("Fld: --", m_statusBar);
    m_lblField->setMinimumWidth(140);
    m_lblField->setAlignment(Qt::AlignCenter);
    m_lblField->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_statusBar->addPermanentWidget(m_lblField);
}

void StatusBarManager::setMessage(const QString &msg) {
    m_lblMessage->setText(msg);
}

void StatusBarManager::setProgress(int percentage) {
    if (percentage < 0) {
        m_progressBar->setVisible(false);
    } else {
        m_progressBar->setVisible(true);
        m_progressBar->setValue(percentage);
    }
}

void StatusBarManager::setRow(int row) {
    m_lblRow->setText(QString("row: %1").arg(row));
}

void StatusBarManager::setCol(int col) {
    m_lblCol->setText(QString("col: %1").arg(col));
}

void StatusBarManager::setX(double x) {
    m_lblX->setText(QString("x: %1 m").arg(x, 0, 'f', 1));
}

void StatusBarManager::setY(double y) {
    m_lblY->setText(QString("y: %1 m").arg(y, 0, 'f', 1));
}

void StatusBarManager::setZ(double z) {
    m_lblZ->setText(QString("z: %1 m").arg(z, 0, 'f', 1));
}

void StatusBarManager::setFieldValue(const QString &fieldCompName, double value, const QString &unit) {
    m_lblField->setText(QString("%1: %2 %3").arg(fieldCompName).arg(value, 0, 'f', 2).arg(unit));
}

void StatusBarManager::clearCoordinates() {
    m_lblRow->setText("row: --");
    m_lblCol->setText("col: --");
    m_lblX->setText("x: --");
    m_lblY->setText("y: --");
    m_lblZ->setText("z: --");
    m_lblField->setText("Fld: --");
}

} // namespace mod3d_gui
