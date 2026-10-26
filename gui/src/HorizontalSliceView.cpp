#include "mod3d_gui/HorizontalSliceView.h"
#include "mod3d_gui/ProfileView.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QApplication>
#include <algorithm>
#include <cmath>

namespace mod3d_gui {

HorizontalSliceView::HorizontalSliceView(ModelDocument *doc, double initialZ, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    if (m_doc) {
        if (!std::isnan(initialZ)) {
            m_sliceZ = initialZ;
            double zMin = m_doc->model().z_min_val();
            double zMax = m_doc->model().z_max_val();
            m_sliceZ = std::clamp(m_sliceZ, zMin, zMax);
            m_doc->setActiveSliceElevation(m_sliceZ);
        } else {
            m_sliceZ = m_doc->activeSliceElevation();
            double zMin = m_doc->model().z_min_val();
            double zMax = m_doc->model().z_max_val();
            m_sliceZ = std::clamp(m_sliceZ, zMin, zMax);
        }
    }

    setupUi();

    if (m_doc) {
        connect(m_doc, &ModelDocument::modelGeometryChanged, this, [this]() {
            m_rasterDirty = true;
            refreshBodyList();
            update();
        });
        connect(m_doc, &ModelDocument::computationFinished, this, [this]() {
            m_rasterDirty = true;
            update();
        });
        connect(m_doc, &ModelDocument::activeFieldChanged, this, [this](mod3d::FieldComponent comp) {
            setFieldComponent(comp);
        });
        connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) { update(); });
        connect(m_doc, &ModelDocument::activeProfileCursorChanged, this, [this](const ModelDocument::ProfileCursorPoint &) { update(); });
        connect(m_doc, &ModelDocument::documentReset, this, [this]() {
            m_rasterDirty = true;
            fitToWindow();
            if (m_doc) {
                m_sliceZ = m_doc->activeSliceElevation();
                updateHeaderUi();
                refreshBodyList();
            }
            update();
        });
        connect(m_doc, &ModelDocument::verticalRangeChanged, this, [this](double zMin, double zMax) {
            m_sliceZ = std::clamp(m_sliceZ, zMin, zMax);
            updateHeaderUi();
            update();
        });
        connect(m_doc, &ModelDocument::activeSliceElevationChanged, this, [this](double z) {
            if (std::abs(m_sliceZ - z) > 1e-4) {
                m_sliceZ = z;
                updateHeaderUi();
                updateTitle();
                update();
            }
        });
    }

    refreshBodyList();
    fitToWindow();
    updateHeaderUi();
    updateTitle();
}

void HorizontalSliceView::setupUi() {
    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName("sliceHeaderWidget");
    m_headerWidget->setStyleSheet(
        "#sliceHeaderWidget {"
        "  background: #f0f1f4;"
        "  border-bottom: 1px solid #d0d2d8;"
        "}"
        "QLabel { font-size: 11px; font-weight: 600; color: #333338; }"
        "QDoubleSpinBox {"
        "  background: #ffffff;"
        "  border: 1px solid #c0c2c8;"
        "  border-radius: 4px;"
        "  padding: 2px 4px;"
        "  font-size: 11px;"
        "}"
        "QPushButton {"
        "  background: #ffffff;"
        "  border: 1px solid #c0c2c8;"
        "  border-radius: 4px;"
        "  padding: 2px 8px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background: #e6e8ee; }"
        "QPushButton:pressed { background: #dcdfe6; }"
    );

    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(8);

    auto *lblElev = new QLabel(tr("Elevation Z:"), m_headerWidget);
    headerLayout->addWidget(lblElev);

    m_spinElevation = new QDoubleSpinBox(m_headerWidget);
    m_spinElevation->setDecimals(1);
    m_spinElevation->setSingleStep(50.0);
    m_spinElevation->setSuffix(" m");
    m_spinElevation->setMinimumWidth(90);
    headerLayout->addWidget(m_spinElevation);

    m_btnStepDown = new QPushButton(tr("▼ -50m"), m_headerWidget);
    m_btnStepDown->setToolTip(tr("Step slice down by 50m (Page Down)"));
    headerLayout->addWidget(m_btnStepDown);

    m_btnStepUp = new QPushButton(tr("▲ +50m"), m_headerWidget);
    m_btnStepUp->setToolTip(tr("Step slice up by 50m (Page Up)"));
    headerLayout->addWidget(m_btnStepUp);

    m_sliderElevation = new QSlider(Qt::Horizontal, m_headerWidget);
    m_sliderElevation->setRange(0, 1000);
    m_sliderElevation->setMinimumWidth(120);
    headerLayout->addWidget(m_sliderElevation);

    m_labelDepth = new QLabel(m_headerWidget);
    m_labelDepth->setStyleSheet("color: #60626a; font-weight: normal;");
    headerLayout->addWidget(m_labelDepth);

    headerLayout->addSpacing(8);
    auto *lblBody = new QLabel(tr("Draw Body:"), m_headerWidget);
    headerLayout->addWidget(lblBody);

    m_comboActiveBody = new QComboBox(m_headerWidget);
    m_comboActiveBody->setMinimumWidth(110);
    m_comboActiveBody->setToolTip(tr("Select active body for click-drag drawing mode"));
    m_comboActiveBody->setStyleSheet(
        "QComboBox {"
        "  background: #ffffff;"
        "  border: 1px solid #c0c2c8;"
        "  border-radius: 4px;"
        "  padding: 2px 6px;"
        "  font-size: 11px;"
        "}"
    );
    headerLayout->addWidget(m_comboActiveBody);

    headerLayout->addStretch(1);

    m_btnFit = new QPushButton(tr("Fit"), m_headerWidget);
    m_btnFit->setToolTip(tr("Fit model grid to view window"));
    headerLayout->addWidget(m_btnFit);

    connect(m_spinElevation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (std::abs(m_sliceZ - val) > 1e-4) {
            setSliceElevation(val);
        }
    });

    connect(m_sliderElevation, &QSlider::valueChanged, this, [this](int sliderVal) {
        if (!m_doc) return;
        double zMin = m_doc->model().z_min_val();
        double zMax = m_doc->model().z_max_val();
        double fraction = static_cast<double>(sliderVal) / 1000.0;
        double newZ = zMin + fraction * (zMax - zMin);
        if (std::abs(m_sliceZ - newZ) > 1.0) {
            setSliceElevation(newZ);
        }
    });

    connect(m_btnStepUp, &QPushButton::clicked, this, [this]() { stepElevation(1); });
    connect(m_btnStepDown, &QPushButton::clicked, this, [this]() { stepElevation(-1); });
    connect(m_btnFit, &QPushButton::clicked, this, &HorizontalSliceView::fitToWindow);
}

void HorizontalSliceView::updateHeaderUi() {
    if (!m_doc || !m_spinElevation || !m_sliderElevation) return;
    double zMin = m_doc->model().z_min_val();
    double zMax = m_doc->model().z_max_val();

    m_spinElevation->blockSignals(true);
    m_sliderElevation->blockSignals(true);

    m_spinElevation->setRange(zMin, zMax);
    m_spinElevation->setValue(m_sliceZ);

    double span = zMax - zMin;
    if (span > 0.0) {
        int sVal = std::clamp(static_cast<int>(std::round(((m_sliceZ - zMin) / span) * 1000.0)), 0, 1000);
        m_sliderElevation->setValue(sVal);
    }

    m_spinElevation->blockSignals(false);
    m_sliderElevation->blockSignals(false);

    // Compute approximate depth below surface relief
    double avgTopo = 0.0;
    int count = 0;
    const auto &model = m_doc->model();
    for (int r = 1; r < model.rows() - 1; ++r) {
        for (int c = 1; c < model.cols() - 1; ++c) {
            avgTopo += model.z(r, c, 0);
            count++;
        }
    }
    if (count > 0) {
        avgTopo /= count;
        double depth = avgTopo - m_sliceZ;
        m_labelDepth->setText(tr("Avg Depth: ~%1 m").arg(std::round(depth)));
    } else {
        m_labelDepth->clear();
    }
}

void HorizontalSliceView::refreshBodyList() {
    if (!m_comboActiveBody || !m_doc) return;
    int currentId = activeBodyId();
    m_comboActiveBody->blockSignals(true);
    m_comboActiveBody->clear();

    const auto &bodies = m_doc->model().bodies();
    int selectIdx = 0;
    for (size_t i = 0; i < bodies.size(); ++i) {
        const auto &b = bodies[i];
        QString name = QString::fromStdString(b->name());
        if (name.isEmpty()) name = tr("Body %1").arg(b->id());
        m_comboActiveBody->addItem(name, b->id());
        if (b->id() == currentId) {
            selectIdx = static_cast<int>(i);
        }
    }
    if (m_comboActiveBody->count() > 0) {
        m_comboActiveBody->setCurrentIndex(selectIdx);
    }
    m_comboActiveBody->blockSignals(false);
}

int HorizontalSliceView::activeBodyId() const {
    if (m_comboActiveBody && m_comboActiveBody->currentIndex() >= 0) {
        return m_comboActiveBody->currentData().toInt();
    }
    if (m_doc && !m_doc->model().bodies().empty()) {
        return m_doc->model().bodies().front()->id();
    }
    return -1;
}

void HorizontalSliceView::setActiveBodyId(int bodyId) {
    if (!m_comboActiveBody) return;
    int idx = m_comboActiveBody->findData(bodyId);
    if (idx >= 0 && idx != m_comboActiveBody->currentIndex()) {
        m_comboActiveBody->blockSignals(true);
        m_comboActiveBody->setCurrentIndex(idx);
        m_comboActiveBody->blockSignals(false);
    }
}

void HorizontalSliceView::setSliceElevation(double z) {
    if (!m_doc) return;
    double zMin = m_doc->model().z_min_val();
    double zMax = m_doc->model().z_max_val();
    z = std::clamp(z, zMin, zMax);
    if (std::abs(m_sliceZ - z) > 1e-4) {
        m_sliceZ = z;
        updateHeaderUi();
        updateTitle();
        m_doc->setActiveSliceElevation(m_sliceZ);
        emit sliceElevationChanged(m_sliceZ);
        update();
    }
}

void HorizontalSliceView::stepElevation(int numSteps) {
    if (!m_doc) return;
    double stepSize = 50.0;
    double zMin = m_doc->model().z_min_val();
    double zMax = m_doc->model().z_max_val();
    if ((zMax - zMin) < 200.0) {
        stepSize = 10.0;
    } else if ((zMax - zMin) >= 5000.0) {
        stepSize = 100.0;
    }
    setSliceElevation(m_sliceZ + numSteps * stepSize);
}

void HorizontalSliceView::updateTitle() {
    QString title = QString("Horizontal Planar View — Z = %1 m").arg(std::round(m_sliceZ));
    if (auto *sub = qobject_cast<QMdiSubWindow *>(parentWidget())) {
        sub->setWindowTitle(title);
    } else {
        setWindowTitle(title);
    }
}

void HorizontalSliceView::fitToWindow() {
    if (!m_doc) return;
    const auto &m = m_doc->model();
    double dx = m.dx_val();
    double dy = m.dy_val();
    m_worldXMin = m.x_min_val() - dx * 0.5;
    m_worldXMax = m.x_max_val() + dx * 0.5;
    m_worldYMin = m.y_min_val() - dy * 0.5;
    m_worldYMax = m.y_max_val() + dy * 0.5;
    update();
}

void HorizontalSliceView::zoomIn() {
    double spanX = (m_worldXMax - m_worldXMin) * 0.8;
    double midX = (m_worldXMax + m_worldXMin) * 0.5;
    m_worldXMin = midX - spanX * 0.5;
    m_worldXMax = midX + spanX * 0.5;

    double spanY = (m_worldYMax - m_worldYMin) * 0.8;
    double midY = (m_worldYMax + m_worldYMin) * 0.5;
    m_worldYMin = midY - spanY * 0.5;
    m_worldYMax = midY + spanY * 0.5;
    update();
}

void HorizontalSliceView::zoomOut() {
    double spanX = (m_worldXMax - m_worldXMin) * 1.25;
    double midX = (m_worldXMax + m_worldXMin) * 0.5;
    m_worldXMin = midX - spanX * 0.5;
    m_worldXMax = midX + spanX * 0.5;

    double spanY = (m_worldYMax - m_worldYMin) * 1.25;
    double midY = (m_worldYMax + m_worldYMin) * 0.5;
    m_worldYMin = midY - spanY * 0.5;
    m_worldYMax = midY + spanY * 0.5;
    update();
}

QRect HorizontalSliceView::canvasRect() const {
    int availW = width() - m_marginLeft - m_marginRight;
    int availH = height() - m_marginTop - m_marginBottom;
    if (availW <= 10 || availH <= 10) {
        return QRect(m_marginLeft, m_marginTop, std::max(10, availW), std::max(10, availH));
    }

    double spanX = m_worldXMax - m_worldXMin;
    double spanY = m_worldYMax - m_worldYMin;
    if (spanX <= 0.0 || spanY <= 0.0) {
        return QRect(m_marginLeft, m_marginTop, availW, availH);
    }

    // Equal scale: pixels per meter identical on both axes
    double scaleX = static_cast<double>(availW) / spanX;
    double scaleY = static_cast<double>(availH) / spanY;
    double scale = std::min(scaleX, scaleY);

    int actualW = std::max(1, static_cast<int>(std::round(scale * spanX)));
    int actualH = std::max(1, static_cast<int>(std::round(scale * spanY)));

    int offX = m_marginLeft + (availW - actualW) / 2;
    int offY = m_marginTop + (availH - actualH) / 2;

    return QRect(offX, offY, actualW, actualH);
}

double HorizontalSliceView::scalePixelsPerMeter() const {
    QRect canvas = canvasRect();
    double spanX = m_worldXMax - m_worldXMin;
    if (spanX <= 0.0) return 1.0;
    return static_cast<double>(canvas.width()) / spanX;
}

double HorizontalSliceView::screenToWorldX(int sx) const {
    QRect canvas = canvasRect();
    if (canvas.width() <= 0) return m_worldXMin;
    double t = static_cast<double>(sx - canvas.left()) / canvas.width();
    return m_worldXMin + t * (m_worldXMax - m_worldXMin);
}

double HorizontalSliceView::screenToWorldY(int sy) const {
    QRect canvas = canvasRect();
    if (canvas.height() <= 0) return m_worldYMin;
    double t = static_cast<double>(sy - canvas.top()) / canvas.height();
    return m_worldYMax - t * (m_worldYMax - m_worldYMin);
}

int HorizontalSliceView::worldToScreenX(double wx) const {
    QRect canvas = canvasRect();
    double spanX = m_worldXMax - m_worldXMin;
    if (spanX <= 0.0) return canvas.left();
    double t = (wx - m_worldXMin) / spanX;
    return canvas.left() + static_cast<int>(std::round(t * canvas.width()));
}

int HorizontalSliceView::worldToScreenY(double wy) const {
    QRect canvas = canvasRect();
    double spanY = m_worldYMax - m_worldYMin;
    if (spanY <= 0.0) return canvas.top();
    double t = (m_worldYMax - wy) / spanY;
    return canvas.top() + static_cast<int>(std::round(t * canvas.height()));
}

bool HorizontalSliceView::hasMeasuredField() const {
    if (!m_doc) return false;
    return m_doc->hasObservedGrid(m_fieldComponent);
}

void HorizontalSliceView::setRasterMode(SliceRasterMode mode) {
    if ((mode == SliceRasterMode::Observed || mode == SliceRasterMode::Difference) && !hasMeasuredField()) {
        mode = SliceRasterMode::Modeled;
    }
    if (m_rasterMode != mode) {
        m_rasterMode = mode;
        m_rasterDirty = true;
        m_marginRight = (mode != SliceRasterMode::None) ? 75 : 35;
        update();
        emit displaySettingsChanged();
    }
}

void HorizontalSliceView::setFieldComponent(mod3d::FieldComponent comp) {
    if (m_fieldComponent != comp) {
        m_fieldComponent = comp;
        m_rasterDirty = true;
        if ((m_rasterMode == SliceRasterMode::Observed || m_rasterMode == SliceRasterMode::Difference) && !hasMeasuredField()) {
            m_rasterMode = SliceRasterMode::Modeled;
        }
        update();
        emit displaySettingsChanged();
    }
}

void HorizontalSliceView::setShowStationPoints(bool show) {
    if (m_showStationPoints != show) {
        m_showStationPoints = show;
        update();
        emit displaySettingsChanged();
    }
}

void HorizontalSliceView::setShowProfiles(bool show) {
    if (m_showProfiles != show) {
        m_showProfiles = show;
        update();
        emit displaySettingsChanged();
    }
}

void HorizontalSliceView::setShowCellBorders(bool show) {
    if (m_showCellBorders != show) {
        m_showCellBorders = show;
        update();
        emit displaySettingsChanged();
    }
}

void HorizontalSliceView::setShowBodyOutlines(bool show) {
    if (m_showBodyOutlines != show) {
        m_showBodyOutlines = show;
        update();
        emit displaySettingsChanged();
    }
}

QMdiArea *HorizontalSliceView::findMdiArea() const {
    QWidget *p = parentWidget();
    while (p) {
        if (auto *m = qobject_cast<QMdiArea*>(p)) {
            return m;
        }
        p = p->parentWidget();
    }
    return nullptr;
}

std::vector<HorizontalSliceView::OpenProfileInfo> HorizontalSliceView::collectOpenProfiles() const {
    if (!m_customProfiles.empty()) {
        return m_customProfiles;
    }
    std::vector<OpenProfileInfo> list;
    if (auto *mdi = findMdiArea()) {
        ProfileView *activePv = nullptr;
        if (auto *activeSub = mdi->activeSubWindow()) {
            activePv = qobject_cast<ProfileView*>(activeSub->widget());
        }
        for (auto *sub : mdi->subWindowList()) {
            if (!sub || !sub->widget()) continue;
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
                bool isActive = (pv == activePv) || (activePv == nullptr && m_doc &&
                                                     pv->profileIndex() == m_doc->profileIndex() &&
                                                     pv->profileOrientation() == m_doc->profileOrientation());
                list.push_back({
                    .orientation = pv->profileOrientation(),
                    .index = pv->profileIndex(),
                    .coordinate = pv->currentProfileCoordinate(),
                    .isActive = isActive
                });
            }
        }
    }
    if (list.empty() && m_doc) {
        list.push_back({
            .orientation = m_doc->profileOrientation(),
            .index = m_doc->profileIndex(),
            .coordinate = m_doc->currentProfileCoordinate(),
            .isActive = true
        });
    }
    return list;
}

std::vector<HorizontalSliceView::PlaneIntersection> HorizontalSliceView::planeIntersections() const {
    std::vector<PlaneIntersection> result;
    auto profiles = collectOpenProfiles();
    std::vector<OpenProfileInfo> ewProfiles;
    std::vector<OpenProfileInfo> snProfiles;

    for (const auto &p : profiles) {
        if (p.orientation == ProfileOrientation::EastWest) {
            ewProfiles.push_back(p);
        } else {
            snProfiles.push_back(p);
        }
    }

    for (const auto &ew : ewProfiles) {
        for (const auto &sn : snProfiles) {
            result.push_back({
                .x = sn.coordinate,
                .y = ew.coordinate,
                .z = m_sliceZ,
                .ewIndex = ew.index,
                .snIndex = sn.index,
                .isActive = (ew.isActive || sn.isActive)
            });
        }
    }

    return result;
}

void HorizontalSliceView::setCustomOpenProfiles(const std::vector<OpenProfileInfo> &profiles) {
    m_customProfiles = profiles;
    update();
}

void HorizontalSliceView::clearCustomOpenProfiles() {
    m_customProfiles.clear();
    update();
}

std::vector<HorizontalSliceView::BoundaryEdge> HorizontalSliceView::activeBoundaryEdges() const {
    std::vector<BoundaryEdge> edges;
    if (!m_doc) return edges;
    const auto &model = m_doc->model();
    int nRows = model.rows();
    int nCols = model.cols();
    double dx = model.dx_val();
    double dy = model.dy_val();

    for (const auto &bPtr : model.bodies()) {
        int bId = bPtr->id();
        for (int r = 1; r < nRows - 1; ++r) {
            for (int c = 1; c < nCols - 1; ++c) {
                int bIdx = model.body_index(bId, r, c);
                if (bIdx < 0) continue;
                double topZ = model.z(r, c, bIdx);
                double botZ = model.z(r, c, bIdx + 1);
                if (topZ < botZ) continue;
                if (m_sliceZ < botZ - 1e-4 || m_sliceZ > topZ + 1e-4) continue;

                double xc = model.xd_val(c);
                double yr = model.yd_val(r);
                double xLeft = xc - dx * 0.5;
                double xRight = xc + dx * 0.5;
                double yBot = yr - dy * 0.5;
                double yTop = yr + dy * 0.5;

                auto intersectsAt = [&](int checkR, int checkC) {
                    if (checkR < 1 || checkR >= nRows - 1 || checkC < 1 || checkC >= nCols - 1) return false;
                    int idx = model.body_index(bId, checkR, checkC);
                    if (idx < 0) return false;
                    double tz = model.z(checkR, checkC, idx);
                    double bz = model.z(checkR, checkC, idx + 1);
                    return (tz >= bz && m_sliceZ >= bz - 1e-4 && m_sliceZ <= tz + 1e-4);
                };

                // East edge: between c and c+1
                if (!intersectsAt(r, c + 1)) {
                    edges.push_back({bId, r, c, r, c + 1, CardinalDirection::East, xRight, yBot, xRight, yTop});
                }
                // West edge: between c and c-1
                if (!intersectsAt(r, c - 1)) {
                    edges.push_back({bId, r, c, r, c - 1, CardinalDirection::West, xLeft, yBot, xLeft, yTop});
                }
                // North edge: between r and r+1
                if (!intersectsAt(r + 1, c)) {
                    edges.push_back({bId, r, c, r + 1, c, CardinalDirection::North, xLeft, yTop, xRight, yTop});
                }
                // South edge: between r and r-1
                if (!intersectsAt(r - 1, c)) {
                    edges.push_back({bId, r, c, r - 1, c, CardinalDirection::South, xLeft, yBot, xRight, yBot});
                }
            }
        }
    }
    return edges;
}

HorizontalSliceView::HitType HorizontalSliceView::performHitTest(
    const QPoint &pos, BoundaryEdge &outEdge, int &outRow, int &outCol, int &outBodyId) const
{
    outRow = -1;
    outCol = -1;
    outBodyId = -1;

    if (!m_doc) return HitType::None;
    const auto &model = m_doc->model();
    double wx = screenToWorldX(pos.x());
    double wy = screenToWorldY(pos.y());
    double x0 = model.x0_val();
    double y0 = model.y0_val();
    double dx = model.dx_val();
    double dy = model.dy_val();
    if (dx <= 0.0 || dy <= 0.0) return HitType::None;

    int col = static_cast<int>(std::round((wx - x0) / dx)) + 1;
    int row = static_cast<int>(std::round((wy - y0) / dy)) + 1;

    double distCenter = 999999.0;
    if (row >= 1 && row < model.rows() - 1 && col >= 1 && col < model.cols() - 1) {
        outRow = row;
        outCol = col;
        double xc = model.xd_val(col);
        double yr = model.yd_val(row);
        int scx = worldToScreenX(xc);
        int scy = worldToScreenY(yr);
        distCenter = std::hypot(pos.x() - scx, pos.y() - scy);

        for (const auto &bPtr : model.bodies()) {
            int bIdx = model.body_index(bPtr->id(), row, col);
            if (bIdx >= 0) {
                double tz = model.z(row, col, bIdx);
                double bz = model.z(row, col, bIdx + 1);
                if (tz >= bz && m_sliceZ >= bz - 1e-4 && m_sliceZ <= tz + 1e-4) {
                    outBodyId = bPtr->id();
                    break;
                }
            }
        }

        // Check if directly on the middle point of the cell / station (<= 8 px)
        if (distCenter <= 8.0) {
            return HitType::CellCenterPoint;
        }

        if (col < model.cols() - 1 && row < model.rows() - 1) {
            double mx = (model.xd_val(col) + model.xd_val(col + 1)) * 0.5;
            double my = (model.yd_val(row) + model.yd_val(row + 1)) * 0.5;
            int smx = worldToScreenX(mx);
            int smy = worldToScreenY(my);
            if (std::hypot(pos.x() - smx, pos.y() - smy) <= 8.0) {
                return HitType::CellCenterPoint;
            }
        }
    }

    // Check boundary edges (distance <= 6 px)
    auto edges = activeBoundaryEdges();
    for (const auto &e : edges) {
        int sx1 = worldToScreenX(e.x1);
        int sy1 = worldToScreenY(e.y1);
        int sx2 = worldToScreenX(e.x2);
        int sy2 = worldToScreenY(e.y2);

        double dxSeg = sx2 - sx1;
        double dySeg = sy2 - sy1;
        double lenSq = dxSeg * dxSeg + dySeg * dySeg;
        if (lenSq < 1e-4) continue;

        double t = std::clamp(((pos.x() - sx1) * dxSeg + (pos.y() - sy1) * dySeg) / lenSq, 0.0, 1.0);
        double px = sx1 + t * dxSeg;
        double py = sy1 + t * dySeg;
        double dist = std::hypot(pos.x() - px, pos.y() - py);

        if (dist <= 6.0) {
            outEdge = e;
            outRow = e.srcRow;
            outCol = e.srcCol;
            outBodyId = e.bodyId;
            return HitType::BoundaryEdge;
        }
    }

    // Inside cell bounds
    if (outRow >= 1 && outCol >= 1) {
        if (outBodyId >= 0) {
            return HitType::BodyCell;
        }
        return HitType::EmptyCell;
    }

    return HitType::None;
}

namespace {

enum class SlicePaletteType {
    Gravity = 0,
    Magnetics = 1,
    Difference = 2,
    Field = 3
};

QColor getSlicePaletteColor(double t, SlicePaletteType palette) {
    t = std::clamp(t, 0.0, 1.0);

    struct ColorPeg {
        double pos;
        int r, g, b;
    };

    // CTBL_FLD: Dark Blue -> Cyan -> Green -> Yellow -> Red -> Magenta -> Light Pink
    static const std::vector<ColorPeg> gravPegs = {
        {0.0 / 6.0, 0, 0, 153},
        {1.0 / 6.0, 0, 204, 255},
        {2.0 / 6.0, 0, 255, 0},
        {3.0 / 6.0, 255, 255, 0},
        {4.0 / 6.0, 255, 0, 0},
        {5.0 / 6.0, 255, 0, 255},
        {6.0 / 6.0, 255, 200, 255}
    };

    // Magnetics / Difference: Diverging Blue -> White -> Red
    static const std::vector<ColorPeg> diffPegs = {
        {0.0, 0, 0, 160},
        {0.5, 255, 255, 255},
        {1.0, 160, 0, 0}
    };

    const auto &pegs = (palette == SlicePaletteType::Difference || palette == SlicePaletteType::Magnetics)
        ? diffPegs : gravPegs;

    if (t <= pegs.front().pos) return QColor(pegs.front().r, pegs.front().g, pegs.front().b);
    if (t >= pegs.back().pos) return QColor(pegs.back().r, pegs.back().g, pegs.back().b);

    for (size_t i = 0; i < pegs.size() - 1; ++i) {
        if (t >= pegs[i].pos && t <= pegs[i + 1].pos) {
            double span = pegs[i + 1].pos - pegs[i].pos;
            double f = (span > 1e-9) ? (t - pegs[i].pos) / span : 0.0;
            int r = static_cast<int>(std::round(pegs[i].r + f * (pegs[i + 1].r - pegs[i].r)));
            int g = static_cast<int>(std::round(pegs[i].g + f * (pegs[i + 1].g - pegs[i].g)));
            int b = static_cast<int>(std::round(pegs[i].b + f * (pegs[i + 1].b - pegs[i].b)));
            return QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
        }
    }
    return QColor(pegs.back().r, pegs.back().g, pegs.back().b);
}

} // namespace

void HorizontalSliceView::drawBackgroundRaster(QPainter &painter, const QRect &canvas) {
    if (m_rasterMode == SliceRasterMode::None || !m_doc) return;

    if (m_rasterDirty) {
        const mod3d::Grid *grid = nullptr;
        bool isMag = mod3d::is_magnetic_component(m_fieldComponent);
        SlicePaletteType palType = isMag ? SlicePaletteType::Magnetics : SlicePaletteType::Gravity;

        if (m_rasterMode == SliceRasterMode::Difference) {
            grid = m_doc->differenceGrid(m_fieldComponent);
            if (!isMag) {
                palType = SlicePaletteType::Difference;
            }
        } else if (m_rasterMode == SliceRasterMode::Observed) {
            grid = m_doc->observedGrid(m_fieldComponent);
            if ((!grid || grid->empty()) && m_fieldComponent == mod3d::FieldComponent::GZ) {
                grid = m_doc->observedGrid(mod3d::FieldComponent::G_TOT);
            }
        } else { // Modeled
            grid = m_doc->modeledGrid(m_fieldComponent);
            if (!grid || grid->empty()) {
                grid = m_doc->observedGrid(m_fieldComponent);
            }
        }

        if (!grid || grid->empty()) {
            m_cachedRasterImage = QImage();
            m_rasterDirty = false;
            return;
        }

        size_t rows = grid->rows();
        size_t cols = grid->cols();
        if (rows < 2 || cols < 2) {
            m_cachedRasterImage = QImage();
            m_rasterDirty = false;
            return;
        }

        double minV = grid->min();
        double maxV = grid->max();
        bool flat = (minV >= maxV);

        m_cachedRasterImage = QImage(static_cast<int>(cols), static_cast<int>(rows), QImage::Format_RGB32);

        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < cols; ++c) {
                double v = (*grid)(r, c);
                double t = flat ? 0.5 : std::clamp((v - minV) / (maxV - minV), 0.0, 1.0);
                QColor col = getSlicePaletteColor(t, palType);

                // Invert row for screen Y (Northing is up)
                m_cachedRasterImage.setPixel(static_cast<int>(c), static_cast<int>(rows - 1 - r), col.rgb());
            }
        }

        double dx = grid->x_size().value();
        double dy = grid->y_size().value();

        // Node-registered grid to cell-registered bitmap: each field cell of size dx x dy
        // is centered directly on the station grid point (x_c, y_r).
        m_cachedRasterX0 = grid->x0().value() - 0.5 * dx;
        m_cachedRasterY0 = grid->y0().value() - 0.5 * dy;
        m_cachedRasterXMax = (grid->x0() + static_cast<double>(cols - 1) * grid->x_size()).value() + 0.5 * dx;
        m_cachedRasterYMax = (grid->y0() + static_cast<double>(rows - 1) * grid->y_size()).value() + 0.5 * dy;
        m_rasterDirty = false;
    }

    if (m_cachedRasterImage.isNull()) return;

    int left = worldToScreenX(m_cachedRasterX0);
    int right = worldToScreenX(m_cachedRasterXMax);
    int top = worldToScreenY(m_cachedRasterYMax);
    int bottom = worldToScreenY(m_cachedRasterY0);

    QRect targetRect(left, top, right - left, bottom - top);
    painter.save();
    painter.setClipRect(canvas);
    painter.setOpacity(0.65);
    painter.drawImage(targetRect, m_cachedRasterImage);
    painter.restore();
}

void HorizontalSliceView::drawColorScaleLegend(QPainter &painter, const QRect &canvas) {
    if (m_rasterMode == SliceRasterMode::None || !m_doc) return;

    const mod3d::Grid *grid = nullptr;
    bool isMag = mod3d::is_magnetic_component(m_fieldComponent);
    SlicePaletteType palType = isMag ? SlicePaletteType::Magnetics : SlicePaletteType::Gravity;

    if (m_rasterMode == SliceRasterMode::Difference) {
        grid = m_doc->differenceGrid(m_fieldComponent);
        if (!isMag) palType = SlicePaletteType::Difference;
    } else if (m_rasterMode == SliceRasterMode::Observed) {
        grid = m_doc->observedGrid(m_fieldComponent);
        if ((!grid || grid->empty()) && m_fieldComponent == mod3d::FieldComponent::GZ) {
            grid = m_doc->observedGrid(mod3d::FieldComponent::G_TOT);
        }
    } else { // Modeled
        grid = m_doc->modeledGrid(m_fieldComponent);
        if (!grid || grid->empty()) {
            grid = m_doc->observedGrid(m_fieldComponent);
        }
    }
    if (!grid || grid->empty()) return;

    double minV = grid->min();
    double maxV = grid->max();
    if (minV >= maxV) return;

    int barW = 12;
    int barH = std::clamp(canvas.height() - 40, 60, 240);
    int barLeft = canvas.right() + 10;
    int barTop = canvas.top() + (canvas.height() - barH) / 2 - 10;
    int barBottom = barTop + barH;

    painter.save();

    // 1. Draw gradient vertical bar
    for (int y = barTop; y <= barBottom; ++y) {
        double t = (barH > 0) ? static_cast<double>(barBottom - y) / barH : 0.5;
        QColor col = getSlicePaletteColor(t, palType);
        painter.setPen(col);
        painter.drawLine(barLeft, y, barLeft + barW - 1, y);
    }

    // 2. Bar Border
    painter.setPen(QColor(60, 60, 70));
    painter.drawRect(barLeft, barTop, barW, barH);

    // 3. Ticks and Numerical Labels
    painter.setFont(QFont("Arial", 8));
    QFontMetrics fm = painter.fontMetrics();
    int numTicks = 5;
    double span = maxV - minV;
    int decimals = (span < 2.0) ? 2 : ((span < 20.0) ? 1 : 0);

    for (int i = 0; i <= numTicks; ++i) {
        double frac = static_cast<double>(i) / numTicks;
        double val = minV + frac * span;
        int ty = barBottom - static_cast<int>(std::round(frac * barH));

        painter.setPen(QColor(80, 80, 90));
        painter.drawLine(barLeft + barW, ty, barLeft + barW + 4, ty);

        QString valStr = QString::number(val, 'f', decimals);
        int textY = ty + fm.ascent() / 2 - 1;
        painter.drawText(barLeft + barW + 7, textY, valStr);
    }

    // 4. Component Label & Unit below bar
    QString compStr = ModelDocument::componentName(m_fieldComponent);
    if (m_rasterMode == SliceRasterMode::Difference) {
        compStr = "d" + compStr;
    }
    QString unitStr = ModelDocument::componentUnit(m_fieldComponent);
    QString titleStr = unitStr.isEmpty() ? compStr : QString("%1\n(%2)").arg(compStr, unitStr);

    painter.setPen(QColor(40, 40, 50));
    QFont boldFont("Arial", 8, QFont::Bold);
    painter.setFont(boldFont);
    QRect titleRect(barLeft - 6, barBottom + 8, 65, 32);
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, titleStr);

    painter.restore();
}

void HorizontalSliceView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Canvas background
    painter.fillRect(rect(), QColor(248, 248, 250));

    QRect canvas = canvasRect();
    painter.fillRect(canvas, Qt::white);

    // Draw main components
    drawBackgroundRaster(painter, canvas);
    drawGridAndCells(painter, canvas);
    drawBodyOutlines(painter, canvas);
    drawProfilesAndMarkers(painter, canvas);
    drawBoundaryEdges(painter, canvas);
    drawAxes(painter, canvas);
    drawColorScaleLegend(painter, canvas);
}

void HorizontalSliceView::drawGridAndCells(QPainter &painter, const QRect &canvas) {
    if (!m_doc) return;
    const auto &model = m_doc->model();
    int nRows = model.rows();
    int nCols = model.cols();

    painter.save();
    painter.setClipRect(canvas);

    auto intersectsAt = [&](int checkR, int checkC, int bId) {
        if (checkR < 1 || checkR >= nRows || checkC < 1 || checkC >= nCols) return false;
        int bIdx = model.body_index(bId, checkR, checkC);
        if (bIdx < 0) return false;
        double tz = model.z(checkR, checkC, bIdx);
        double bz = model.z(checkR, checkC, bIdx + 1);
        return (tz >= bz && m_sliceZ >= bz - 1e-4 && m_sliceZ <= tz + 1e-4);
    };

    int actId = activeBodyId();
    const auto *actBody = (actId >= 0) ? model.body(actId) : nullptr;

    // 1. Draw cell fills and borders matching the real grid geometry [xd(c)..xd(c+1)] x [yd(r)..yd(r+1)]
    for (int r = 1; r < nRows - 1; ++r) {
        for (int c = 1; c < nCols - 1; ++c) {
            double x0 = model.xd_val(c);
            double x1 = model.xd_val(c + 1);
            double y0 = model.yd_val(r);
            double y1 = model.yd_val(r + 1);

            int sx1 = worldToScreenX(x0);
            int sx2 = worldToScreenX(x1);
            int sy1 = worldToScreenY(y1); // top in screen
            int sy2 = worldToScreenY(y0); // bottom in screen

            QRect cellRect(sx1, sy1, sx2 - sx1, sy2 - sy1);

            auto checkBodyInCell = [&](const mod3d::Body *b) -> bool {
                if (!b) return false;
                int bId = b->id();
                return (intersectsAt(r, c, bId) ||
                        intersectsAt(r, c + 1, bId) ||
                        intersectsAt(r + 1, c + 1, bId) ||
                        intersectsAt(r + 1, c, bId));
            };

            const mod3d::Body *fillBody = nullptr;
            if (actBody && checkBodyInCell(actBody)) {
                fillBody = actBody;
            } else {
                for (const auto &bPtr : model.bodies()) {
                    if (checkBodyInCell(bPtr.get())) {
                        fillBody = bPtr.get();
                        break;
                    }
                }
            }

            if (fillBody) {
                auto bc = fillBody->color();
                QColor fillCol(bc.r, bc.g, bc.b, 160);
                painter.fillRect(cellRect, fillCol);
            }

            // Cell border
            if (m_showCellBorders) {
                painter.setPen(QColor(230, 232, 236));
                painter.drawRect(cellRect);
            }
        }
    }

    // 2. Station node markers and drawing handles
    for (int r = 1; r < nRows - 1; ++r) {
        for (int c = 1; c < nCols - 1; ++c) {
            double xc = model.xd_val(c);
            double yr = model.yd_val(r);
            int scx = worldToScreenX(xc);
            int scy = worldToScreenY(yr);

            // Check intersecting body at this station
            int stationBodyId = -1;
            for (const auto &bPtr : model.bodies()) {
                if (intersectsAt(r, c, bPtr->id())) {
                    stationBodyId = bPtr->id();
                    break;
                }
            }

            if (m_showStationPoints) {
                painter.setPen(Qt::NoPen);
                if (stationBodyId >= 0) {
                    if (auto *b = model.body(stationBodyId)) {
                        auto bc = b->color();
                        painter.setBrush(QColor(bc.r, bc.g, bc.b, 220));
                    } else {
                        painter.setBrush(QColor(160, 165, 175, 160));
                    }
                } else {
                    painter.setBrush(QColor(160, 165, 175, 160));
                }
                painter.drawEllipse(QPoint(scx, scy), 2, 2);
            }

            // Cell center / station drawing handle (hovered or currently drawing)
            bool isCenterHovered = (m_isHoveringCenter && m_hoverCenterRow == r && m_hoverCenterCol == c);
            bool isDrawingHere = (m_dragOp == DragOperation::DrawBody && m_drawLastRow == r && m_drawLastCol == c);
            if (isCenterHovered || isDrawingHere) {
                painter.setPen(QPen(QColor(0, 160, 255), 2.0));
                painter.setBrush(QColor(255, 255, 255, 220));
                painter.drawEllipse(QPoint(scx, scy), 5, 5);
                painter.setBrush(QColor(0, 160, 255));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPoint(scx, scy), 2, 2);
            }
        }
    }

    painter.restore();
}

void HorizontalSliceView::drawBodyOutlines(QPainter &painter, const QRect &canvas) {
    if (!m_showBodyOutlines || !m_doc) return;
    const auto &model = m_doc->model();
    auto boundaries = model.all_body_boundaries();
    if (boundaries.empty()) return;

    painter.save();
    painter.setClipRect(canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const auto &seg : boundaries) {
        const auto *body = model.body(seg.bodyId);
        if (body && !body->is_visible()) continue;
        QColor col = body ? QColor(body->color().r, body->color().g, body->color().b) : QColor(0, 0, 180);
        painter.setPen(QPen(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        int sx0 = worldToScreenX(seg.pt0.x);
        int sy0 = worldToScreenY(seg.pt0.y);
        int sx1 = worldToScreenX(seg.pt1.x);
        int sy1 = worldToScreenY(seg.pt1.y);
        painter.drawLine(sx0, sy0, sx1, sy1);
    }
    painter.restore();
}

void HorizontalSliceView::drawBoundaryEdges(QPainter &painter, const QRect &canvas) {
    if (!m_doc) return;
    painter.save();
    painter.setClipRect(canvas);

    auto edges = activeBoundaryEdges();

    for (const auto &e : edges) {
        int sx1 = worldToScreenX(e.x1);
        int sy1 = worldToScreenY(e.y1);
        int sx2 = worldToScreenX(e.x2);
        int sy2 = worldToScreenY(e.y2);

        bool isHover = (m_isHoveringEdge &&
                        m_hoverEdge.bodyId == e.bodyId &&
                        m_hoverEdge.srcRow == e.srcRow &&
                        m_hoverEdge.srcCol == e.srcCol &&
                        m_hoverEdge.dir == e.dir);

        bool isDrag = (m_dragOp == DragOperation::ExtendOrCollapseEdge &&
                       m_activeDragEdge.bodyId == e.bodyId &&
                       m_activeDragEdge.srcRow == e.srcRow &&
                       m_activeDragEdge.srcCol == e.srcCol &&
                       m_activeDragEdge.dir == e.dir);

        if (isDrag || isHover) {
            // Highlighted active edge
            painter.setPen(QPen(QColor(255, 130, 0), 3.5, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(sx1, sy1, sx2, sy2);

            // Handle circle at midpoint
            int mx = (sx1 + sx2) / 2;
            int my = (sy1 + sy2) / 2;
            painter.setBrush(QColor(255, 150, 0));
            painter.setPen(QPen(Qt::white, 1.5));
            painter.drawEllipse(QPoint(mx, my), 5, 5);
        }
    }

    painter.restore();
}

void HorizontalSliceView::drawProfilesAndMarkers(QPainter &painter, const QRect &canvas) {
    if (!m_doc) return;
    painter.save();
    painter.setClipRect(canvas);

    if (m_showProfiles) {
        auto profiles = collectOpenProfiles();
        std::vector<OpenProfileInfo> ewProfiles;
        std::vector<OpenProfileInfo> snProfiles;
        for (const auto &p : profiles) {
            if (p.orientation == ProfileOrientation::EastWest) {
                ewProfiles.push_back(p);
            } else {
                snProfiles.push_back(p);
            }
        }

        // 1. Draw East-West profile planes (horizontal lines at Y = ew.coordinate)
        for (const auto &ew : ewProfiles) {
            int sy = worldToScreenY(ew.coordinate);
            int sx1 = worldToScreenX(m_worldXMin);
            int sx2 = worldToScreenX(m_worldXMax);

            if (ew.isActive) {
                painter.setPen(QPen(QColor(0, 122, 237, 240), 2.2, Qt::SolidLine, Qt::RoundCap));
                painter.drawLine(sx1, sy, sx2, sy);
                // Direction arrow pointing East
                painter.drawLine(sx2 - 9, sy - 4, sx2, sy);
                painter.drawLine(sx2 - 9, sy + 4, sx2, sy);

                // Badge / label
                painter.setPen(QColor(0, 75, 160));
                painter.setFont(QFont("Arial", 8, QFont::Bold));
                painter.drawText(sx1 + 8, sy - 5, QString("Profile %1 [E-W] (Y = %2 m)").arg(ew.index + 1).arg(ew.coordinate, 0, 'f', 0));
            } else {
                painter.setPen(QPen(QColor(0, 145, 235, 180), 1.6, Qt::DashLine, Qt::RoundCap));
                painter.drawLine(sx1, sy, sx2, sy);
                painter.setPen(QColor(30, 110, 190));
                painter.setFont(QFont("Arial", 8));
                painter.drawText(sx1 + 8, sy - 4, QString("Prf %1 [E-W]").arg(ew.index + 1));
            }
        }

        // 2. Draw South-North profile planes (vertical lines at X = sn.coordinate)
        for (const auto &sn : snProfiles) {
            int sx = worldToScreenX(sn.coordinate);
            int sy1 = worldToScreenY(m_worldYMin);
            int sy2 = worldToScreenY(m_worldYMax);

            if (sn.isActive) {
                painter.setPen(QPen(QColor(235, 110, 0, 240), 2.2, Qt::SolidLine, Qt::RoundCap));
                painter.drawLine(sx, sy1, sx, sy2);
                // Direction arrow pointing North
                painter.drawLine(sx - 4, sy2 + 9, sx, sy2);
                painter.drawLine(sx + 4, sy2 + 9, sx, sy2);

                // Badge / label
                painter.setPen(QColor(180, 70, 0));
                painter.setFont(QFont("Arial", 8, QFont::Bold));
                painter.drawText(sx + 6, sy1 - 8, QString("Profile %1 [S-N] (X = %2 m)").arg(sn.index + 1).arg(sn.coordinate, 0, 'f', 0));
            } else {
                painter.setPen(QPen(QColor(220, 120, 20, 180), 1.6, Qt::DashLine, Qt::RoundCap));
                painter.drawLine(sx, sy1, sx, sy2);
                painter.setPen(QColor(170, 80, 10));
                painter.setFont(QFont("Arial", 8));
                painter.drawText(sx + 5, sy1 - 6, QString("Prf %1 [S-N]").arg(sn.index + 1));
            }
        }

        // 3. Draw Plane Intersections
        auto intersections = planeIntersections();
        for (size_t i = 0; i < intersections.size(); ++i) {
            const auto &pt = intersections[i];
            int ix = worldToScreenX(pt.x);
            int iy = worldToScreenY(pt.y);

            if (!canvas.adjusted(-20, -20, 20, 20).contains(ix, iy)) continue;

            bool isHovered = (static_cast<int>(i) == m_hoverIntersectionIdx);

            // Halo / glow ring
            painter.setPen(QPen(pt.isActive ? QColor(220, 30, 30, 80) : QColor(140, 60, 200, 70), isHovered ? 4.0 : 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QPoint(ix, iy), isHovered ? 13 : 10, isHovered ? 13 : 10);

            // Middle target ring
            painter.setPen(QPen(pt.isActive ? QColor(220, 20, 20) : QColor(130, 40, 170), isHovered ? 2.5 : 2.0));
            painter.setBrush(QColor(255, 255, 255, 230));
            painter.drawEllipse(QPoint(ix, iy), 6, 6);

            // Crosshair tick marks
            painter.drawLine(ix - 11, iy, ix - 6, iy);
            painter.drawLine(ix + 6, iy, ix + 11, iy);
            painter.drawLine(ix, iy - 11, ix, iy - 6);
            painter.drawLine(ix, iy + 6, ix, iy + 11);

            // Center bullseye
            painter.setPen(Qt::NoPen);
            painter.setBrush(pt.isActive ? QColor(220, 20, 20) : QColor(130, 40, 170));
            painter.drawEllipse(QPoint(ix, iy), 3, 3);

            // Plane intersection pill badge
            QString badgeText = QString("EW #%1 ∩ SN #%2").arg(pt.ewIndex + 1).arg(pt.snIndex + 1);
            QFont badgeFont("Arial", 8, QFont::Bold);
            QFontMetrics bfm(badgeFont);
            int textW = bfm.horizontalAdvance(badgeText);
            int badgeW = textW + 12;
            int badgeH = 18;

            int badgeX = ix + 10;
            int badgeY = iy - 22;
            if (badgeX + badgeW > canvas.right() - 4) {
                badgeX = ix - 10 - badgeW;
            }
            if (badgeY < canvas.top() + 4) {
                badgeY = iy + 10;
            }

            QRect badgeRect(badgeX, badgeY, badgeW, badgeH);
            painter.setPen(QPen(pt.isActive ? QColor(180, 20, 20) : QColor(100, 30, 140), 1.0));
            painter.setBrush(pt.isActive ? QColor(250, 235, 240, 235) : QColor(245, 240, 252, 235));
            painter.drawRoundedRect(badgeRect, 4, 4);

            painter.setPen(pt.isActive ? QColor(160, 10, 10) : QColor(90, 20, 130));
            painter.setFont(badgeFont);
            painter.drawText(badgeRect, Qt::AlignCenter, badgeText);
        }
    }

    // 4. Draw Active Cursor Point Marker
    const auto &pt = m_doc->activeProfileCursor();
    if (pt.active && pt.row >= 1 && pt.col >= 1) {
        int sx = worldToScreenX(pt.x);
        int sy = worldToScreenY(pt.y);

        painter.setPen(QPen(QColor(220, 30, 30), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPoint(sx, sy), 7, 7);

        painter.drawLine(sx - 10, sy, sx + 10, sy);
        painter.drawLine(sx, sy - 10, sx, sy + 10);
    }

    painter.restore();
}

void HorizontalSliceView::drawAxes(QPainter &painter, const QRect &canvas) {
    painter.save();
    painter.setPen(QColor(160, 162, 170));
    painter.drawRect(canvas);

    QFont f = painter.font();
    f.setPointSize(8);
    painter.setFont(f);

    // Bottom X-Axis (East coordinate in meters)
    int nTicksX = 5;
    for (int i = 0; i <= nTicksX; ++i) {
        double t = static_cast<double>(i) / nTicksX;
        double wx = m_worldXMin + t * (m_worldXMax - m_worldXMin);
        int sx = canvas.left() + static_cast<int>(std::round(t * canvas.width()));
        painter.drawLine(sx, canvas.bottom(), sx, canvas.bottom() + 4);
        QString label = QString::number(std::round(wx));
        QRect textRect(sx - 30, canvas.bottom() + 6, 60, 15);
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
    painter.drawText(QRect(canvas.left(), canvas.bottom() + 22, canvas.width(), 16),
                     Qt::AlignCenter, tr("East Coordinate X (m)"));

    // Left Y-Axis (North coordinate in meters)
    int nTicksY = 5;
    for (int i = 0; i <= nTicksY; ++i) {
        double t = static_cast<double>(i) / nTicksY;
        double wy = m_worldYMin + t * (m_worldYMax - m_worldYMin);
        int sy = canvas.bottom() - static_cast<int>(std::round(t * canvas.height()));
        painter.drawLine(canvas.left() - 4, sy, canvas.left(), sy);
        QString label = QString::number(std::round(wy));
        QRect textRect(canvas.left() - 60, sy - 8, 54, 16);
        painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
    }

    painter.restore();
}

void HorizontalSliceView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        BoundaryEdge hitEdge;
        int hitRow = -1, hitCol = -1, hitBodyId = -1;
        HitType ht = performHitTest(event->pos(), hitEdge, hitRow, hitCol, hitBodyId);

        if (ht == HitType::CellCenterPoint) {
            // Drawing Mode initiated from cell center point!
            m_dragOp = DragOperation::DrawBody;
            m_dragStartPos = event->pos();
            m_drawStartRow = hitRow;
            m_drawStartCol = hitCol;
            m_drawLastRow = hitRow;
            m_drawLastCol = hitCol;
            m_drawVisitedCells.clear();
            m_drawVisitedCells.insert({hitRow, hitCol});

            // Determine body to draw
            if (hitBodyId >= 0) {
                m_drawBodyId = hitBodyId;
                setActiveBodyId(hitBodyId);
                m_drawPendingStartCell = false;
            } else {
                int actId = activeBodyId();
                m_drawBodyId = actId;
                m_drawPendingStartCell = true;
            }

            setCursor(Qt::PointingHandCursor);
            if (m_doc && hitRow >= 1 && hitCol >= 1) {
                double wx = m_doc->model().xd_val(hitCol);
                double wy = m_doc->model().yd_val(hitRow);
                std::string bName;
                if (auto *b = m_doc->model().body(m_drawBodyId)) bName = b->name();
                m_doc->setActiveProfileCursor({
                    true,
                    hitRow,
                    hitCol,
                    wx,
                    wy,
                    m_sliceZ,
                    m_drawBodyId,
                    bName,
                    true
                });
                emit cursorStationChanged(hitRow, hitCol, wx, wy, m_sliceZ, 0.0);
            }
            update();
            return;
        } else if (ht == HitType::BoundaryEdge) {
            m_dragOp = DragOperation::ExtendOrCollapseEdge;
            m_dragStartPos = event->pos();
            m_activeDragEdge = hitEdge;
            m_lastDragRow = hitEdge.srcRow;
            m_lastDragCol = hitEdge.srcCol;
            update();
        } else if (ht == HitType::BodyCell || ht == HitType::EmptyCell) {
            if (m_doc && hitRow >= 1 && hitCol >= 1) {
                double wx = m_doc->model().xd_val(hitCol);
                double wy = m_doc->model().yd_val(hitRow);
                std::string bName;
                if (auto *b = m_doc->model().body(hitBodyId)) {
                    bName = b->name();
                }
                m_doc->setActiveProfileCursor({
                    true,
                    hitRow,
                    hitCol,
                    wx,
                    wy,
                    m_sliceZ,
                    hitBodyId,
                    bName,
                    false
                });
                emit cursorStationChanged(hitRow, hitCol, wx, wy, m_sliceZ, 0.0);
                update();
            }
        }
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_dragOp = DragOperation::Pan;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }

    QWidget::mousePressEvent(event);
}

void HorizontalSliceView::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragOp == DragOperation::Pan) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();

        double scale = scalePixelsPerMeter();
        if (scale > 0.0) {
            double dwx = -delta.x() / scale;
            double dwy = delta.y() / scale;
            m_worldXMin += dwx;
            m_worldXMax += dwx;
            m_worldYMin += dwy;
            m_worldYMax += dwy;
            update();
        }
        return;
    }

    if (m_dragOp == DragOperation::DrawBody && m_doc && m_drawBodyId >= 0) {
        const auto &model = m_doc->model();
        double wx = screenToWorldX(event->pos().x());
        double wy = screenToWorldY(event->pos().y());
        double x0 = model.x0_val();
        double y0 = model.y0_val();
        double dx = model.dx_val();
        double dy = model.dy_val();
        if (dx > 0.0 && dy > 0.0) {
            int currC = std::clamp(static_cast<int>(std::round((wx - x0) / dx)) + 1, 1, model.cols() - 2);
            int currR = std::clamp(static_cast<int>(std::round((wy - y0) / dy)) + 1, 1, model.rows() - 2);

            if (m_drawPendingStartCell) {
                if (m_drawBodyId < 0) {
                    m_drawBodyId = activeBodyId();
                }
                if (m_drawBodyId < 0) {
                    m_drawBodyId = m_doc->insertBodyAtRowCol(m_drawStartRow, m_drawStartCol, m_sliceZ, m_doc->model().default_thickness_val() * 0.5, -1);
                    refreshBodyList();
                    setActiveBodyId(m_drawBodyId);
                } else if (model.body_index(m_drawBodyId, m_drawStartRow, m_drawStartCol) < 0) {
                    m_doc->insertBodyAtRowCol(m_drawStartRow, m_drawStartCol, m_sliceZ, m_doc->model().default_thickness_val() * 0.5, m_drawBodyId);
                }
                m_drawPendingStartCell = false;
            }

            int r0 = m_drawLastRow;
            int c0 = m_drawLastCol;
            int r1 = currR;
            int c1 = currC;

            int dr = std::abs(r1 - r0);
            int dc = std::abs(c1 - c0);
            int steps = std::max(dr, dc);

            for (int s = 1; s <= steps; ++s) {
                int r = r0 + static_cast<int>(std::round(static_cast<double>(r1 - r0) * s / steps));
                int c = c0 + static_cast<int>(std::round(static_cast<double>(c1 - c0) * s / steps));

                if (r < 1 || r >= model.rows() - 1 || c < 1 || c >= model.cols() - 1) continue;

                if (!m_drawVisitedCells.count({r, c})) {
                    m_drawVisitedCells.insert({r, c});

                    if (model.body_index(m_drawBodyId, r, c) < 0) {
                        // Cell does not have this body yet: insert it!
                        double halfT = model.default_thickness_val() * 0.25;
                        double midZ = m_sliceZ;
                        int lastBIdx = model.body_index(m_drawBodyId, m_drawLastRow, m_drawLastCol);
                        if (lastBIdx >= 0) {
                            double topZ = model.z(m_drawLastRow, m_drawLastCol, lastBIdx);
                            double botZ = model.z(m_drawLastRow, m_drawLastCol, lastBIdx + 1);
                            halfT = std::abs(topZ - botZ) / 2.0;
                            midZ = (topZ + botZ) / 2.0;
                        }
                        int res = m_doc->insertBodyAtRowCol(r, c, midZ, halfT * 2.0, m_drawBodyId);
                        if (res < 0 && std::abs(midZ - m_sliceZ) > 1e-3) {
                            m_doc->insertBodyAtRowCol(r, c, m_sliceZ, halfT * 2.0, m_drawBodyId);
                        }
                    }
                    m_drawLastRow = r;
                    m_drawLastCol = c;
                }
            }

            double wxCoord = model.xd_val(currC);
            double wyCoord = model.yd_val(currR);
            std::string bName;
            if (auto *b = model.body(m_drawBodyId)) bName = b->name();
            m_doc->setActiveProfileCursor({
                true,
                currR,
                currC,
                wxCoord,
                wyCoord,
                m_sliceZ,
                m_drawBodyId,
                bName,
                true
            });
            emit cursorStationChanged(currR, currC, wxCoord, wyCoord, m_sliceZ, 0.0);
            update();
        }
        return;
    }

    if (m_dragOp == DragOperation::ExtendOrCollapseEdge && m_doc) {
        const auto &model = m_doc->model();
        double wx = screenToWorldX(event->pos().x());
        double wy = screenToWorldY(event->pos().y());
        double x0 = model.x0_val();
        double y0 = model.y0_val();
        double dx = model.dx_val();
        double dy = model.dy_val();

        int currC = std::clamp(static_cast<int>(std::round((wx - x0) / dx)) + 1, 1, model.cols() - 2);
        int currR = std::clamp(static_cast<int>(std::round((wy - y0) / dy)) + 1, 1, model.rows() - 2);

        int srcR = m_activeDragEdge.srcRow;
        int srcC = m_activeDragEdge.srcCol;
        int bId = m_activeDragEdge.bodyId;

        // Process step based on initial drag direction
        switch (m_activeDragEdge.dir) {
        case CardinalDirection::East:
            if (currC > srcC) {
                // Outward extension into column currC
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR, srcC + 1, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC + 1;
                    m_activeDragEdge.tgtCol = srcC + 2;
                    update();
                }
            } else if (currC < srcC) {
                // Inward collapse of srcCol
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC - 1;
                    m_activeDragEdge.tgtCol = srcC;
                    update();
                }
            }
            break;

        case CardinalDirection::West:
            if (currC < srcC) {
                // Outward extension into column currC
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR, srcC - 1, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC - 1;
                    m_activeDragEdge.tgtCol = srcC - 2;
                    update();
                }
            } else if (currC > srcC) {
                // Inward collapse
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC + 1;
                    m_activeDragEdge.tgtCol = srcC;
                    update();
                }
            }
            break;

        case CardinalDirection::North:
            if (currR > srcR) {
                // Outward extension into row currR
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR + 1, srcC, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR + 1;
                    m_activeDragEdge.tgtRow = srcR + 2;
                    update();
                }
            } else if (currR < srcR) {
                // Inward collapse
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR - 1;
                    m_activeDragEdge.tgtRow = srcR;
                    update();
                }
            }
            break;

        case CardinalDirection::South:
            if (currR < srcR) {
                // Outward extension into row currR
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR - 1, srcC, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR - 1;
                    m_activeDragEdge.tgtRow = srcR - 2;
                    update();
                }
            } else if (currR > srcR) {
                // Inward collapse
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR + 1;
                    m_activeDragEdge.tgtRow = srcR;
                    update();
                }
            }
            break;
        }

        double wxCoord = model.xd_val(m_activeDragEdge.srcCol);
        double wyCoord = model.yd_val(m_activeDragEdge.srcRow);
        std::string bName;
        if (auto *b = model.body(bId)) bName = b->name();
        m_doc->setActiveProfileCursor({
            true,
            m_activeDragEdge.srcRow,
            m_activeDragEdge.srcCol,
            wxCoord,
            wyCoord,
            m_sliceZ,
            bId,
            bName,
            true
        });
        emit cursorStationChanged(m_activeDragEdge.srcRow, m_activeDragEdge.srcCol, wxCoord, wyCoord, m_sliceZ, 0.0);
        return;
    }

    // Check Plane Intersection Hover
    int hoverInterIdx = -1;
    if (m_showProfiles) {
        auto intersections = planeIntersections();
        for (size_t i = 0; i < intersections.size(); ++i) {
            int ix = worldToScreenX(intersections[i].x);
            int iy = worldToScreenY(intersections[i].y);
            if (std::hypot(event->pos().x() - ix, event->pos().y() - iy) <= 9.0) {
                hoverInterIdx = static_cast<int>(i);
                break;
            }
        }
    }
    if (hoverInterIdx != m_hoverIntersectionIdx) {
        m_hoverIntersectionIdx = hoverInterIdx;
        update();
    }
    if (m_hoverIntersectionIdx >= 0) {
        if (m_isHoveringEdge || m_isHoveringCenter) {
            m_isHoveringEdge = false;
            m_isHoveringCenter = false;
            update();
        }
        auto intersections = planeIntersections();
        if (m_hoverIntersectionIdx < static_cast<int>(intersections.size())) {
            const auto &pt = intersections[m_hoverIntersectionIdx];
            QString tip = tr("Plane Intersection:\nE-W Profile %1 (Y = %2 m)\nS-N Profile %3 (X = %4 m)\nSlice Z = %5 m")
                              .arg(pt.ewIndex + 1).arg(pt.y, 0, 'f', 1)
                              .arg(pt.snIndex + 1).arg(pt.x, 0, 'f', 1)
                              .arg(pt.z, 0, 'f', 1);
            if (toolTip() != tip) {
                setToolTip(tip);
            }
            emit cursorStationChanged(-1, -1, pt.x, pt.y, pt.z, 0.0);
        }
        setCursor(Qt::CrossCursor);
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!toolTip().isEmpty()) {
        setToolTip(QString());
    }

    // Normal Hover detection
    BoundaryEdge hitEdge;
    int hitRow = -1, hitCol = -1, hitBodyId = -1;
    HitType ht = performHitTest(event->pos(), hitEdge, hitRow, hitCol, hitBodyId);

    if (ht == HitType::CellCenterPoint) {
        if (!m_isHoveringCenter || m_hoverCenterRow != hitRow || m_hoverCenterCol != hitCol) {
            m_isHoveringCenter = true;
            m_hoverCenterRow = hitRow;
            m_hoverCenterCol = hitCol;
            m_isHoveringEdge = false;
            update();
        }
        setCursor(Qt::PointingHandCursor);
    } else if (ht == HitType::BoundaryEdge) {
        if (m_isHoveringCenter) {
            m_isHoveringCenter = false;
            update();
        }
        if (!m_isHoveringEdge || m_hoverEdge.bodyId != hitEdge.bodyId ||
            m_hoverEdge.srcRow != hitEdge.srcRow || m_hoverEdge.srcCol != hitEdge.srcCol ||
            m_hoverEdge.dir != hitEdge.dir) {
            m_isHoveringEdge = true;
            m_hoverEdge = hitEdge;
            m_hoverRow = hitRow;
            m_hoverCol = hitCol;
            update();
        }
        if (hitEdge.dir == CardinalDirection::East || hitEdge.dir == CardinalDirection::West) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::SizeVerCursor);
        }
    } else {
        if (m_isHoveringEdge || m_isHoveringCenter) {
            m_isHoveringEdge = false;
            m_isHoveringCenter = false;
            update();
        }
        if (ht == HitType::BodyCell) {
            setCursor(Qt::ArrowCursor);
        } else if (ht == HitType::EmptyCell) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }

    if (m_doc && hitRow >= 1 && hitCol >= 1) {
        double wx = m_doc->model().xd_val(hitCol);
        double wy = m_doc->model().yd_val(hitRow);
        double fldVal = 0.0;
        if (m_rasterMode != SliceRasterMode::None) {
            const mod3d::Grid *g = nullptr;
            if (m_rasterMode == SliceRasterMode::Difference) {
                g = m_doc->differenceGrid(m_fieldComponent);
            } else if (m_rasterMode == SliceRasterMode::Observed) {
                g = m_doc->observedGrid(m_fieldComponent);
            } else {
                g = m_doc->modeledGrid(m_fieldComponent);
            }
            if (g && !g->empty() && hitRow < static_cast<int>(g->rows()) && hitCol < static_cast<int>(g->cols())) {
                fldVal = (*g)(hitRow, hitCol);
            }
        }
        emit cursorStationChanged(hitRow, hitCol, wx, wy, m_sliceZ, fldVal);
    }

    QWidget::mouseMoveEvent(event);
}

void HorizontalSliceView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragOp == DragOperation::Pan) {
        m_dragOp = DragOperation::None;
        setCursor(Qt::ArrowCursor);
    } else if (m_dragOp == DragOperation::ExtendOrCollapseEdge) {
        m_dragOp = DragOperation::None;
        if (m_doc) {
            auto pt = m_doc->activeProfileCursor();
            pt.isDragging = false;
            m_doc->setActiveProfileCursor(pt);
        }
        update();
    } else if (m_dragOp == DragOperation::DrawBody) {
        bool wasPending = m_drawPendingStartCell;
        m_dragOp = DragOperation::None;
        m_drawPendingStartCell = false;
        m_drawVisitedCells.clear();
        if (m_doc) {
            auto pt = m_doc->activeProfileCursor();
            pt.isDragging = false;
            m_doc->setActiveProfileCursor(pt);
            if (!wasPending) {
                m_doc->computeAll();
                m_doc->emitModelGeometryChanged();
            }
        }
        setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void HorizontalSliceView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    BoundaryEdge hitEdge;
    int hitRow = -1, hitCol = -1, hitBodyId = -1;
    HitType ht = performHitTest(event->pos(), hitEdge, hitRow, hitCol, hitBodyId);

    if (ht == HitType::None || !m_doc || hitRow < 1 || hitCol < 1) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (hitBodyId >= 0) {
        // Double-click on an existing body in this cell
        if (event->modifiers() & Qt::ControlModifier) {
            // Ctrl + double-click: remove body at this cell (same as ProfileView)
            int bIdx = m_doc->model().body_index(hitBodyId, hitRow, hitCol);
            if (bIdx >= 0) {
                m_doc->removeBodyAtRowCol(hitRow, hitCol, bIdx);
                m_doc->computeAll();
                refreshBodyList();
                update();
                return;
            }
        } else {
            // Plain double-click on body: open body properties (same as ProfileView)
            emit requestBodyProperties(hitBodyId);
            return;
        }
    } else {
        // Double-click on empty cell
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift + double-click: insert existing active body (same as ProfileView)
            int existingId = activeBodyId();
            if (existingId < 0 && !m_doc->model().bodies().empty()) {
                existingId = m_doc->model().bodies().front()->id();
            }
            if (existingId >= 0) {
                int res = m_doc->insertBodyAtRowCol(hitRow, hitCol, m_sliceZ, -1.0, existingId);
                if (res >= 0) {
                    m_doc->setModified(true);
                    m_doc->computeAll();
                    m_doc->emitModelGeometryChanged();
                    update();
                    return;
                }
            }
        } else {
            // Plain double-click: create a brand NEW body! (same as ProfileView)
            int newBodyId = m_doc->insertBodyAtRowCol(hitRow, hitCol, m_sliceZ, -1.0, -1);
            if (newBodyId >= 0) {
                m_doc->setModified(true);
                m_doc->computeAll();
                m_doc->emitModelGeometryChanged();
                refreshBodyList();
                setActiveBodyId(newBodyId);
                emit requestBodyProperties(newBodyId);
                update();
                return;
            }
        }
    }

    QWidget::mouseDoubleClickEvent(event);
}

void HorizontalSliceView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl + Wheel: Zoom in / zoom out
        int numDegrees = event->angleDelta().y() / 8;
        if (numDegrees > 0) {
            zoomIn();
        } else if (numDegrees < 0) {
            zoomOut();
        }
        event->accept();
    } else {
        // Wheel: Step slice elevation Z
        m_wheelDeltaAccumulator += event->angleDelta().y();
        int steps = m_wheelDeltaAccumulator / 120;
        if (steps != 0) {
            stepElevation(steps);
            m_wheelDeltaAccumulator -= steps * 120;
        }
        event->accept();
    }
}

void HorizontalSliceView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_PageUp) {
        stepElevation(1);
        event->accept();
    } else if (event->key() == Qt::Key_PageDown) {
        stepElevation(-1);
        event->accept();
    } else if (event->key() == Qt::Key_Home) {
        if (m_doc) setSliceElevation(m_doc->model().z_max_val());
        event->accept();
    } else if (event->key() == Qt::Key_End) {
        if (m_doc) setSliceElevation(m_doc->model().z_min_val());
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void HorizontalSliceView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_headerWidget) {
        m_headerWidget->setGeometry(0, 0, width(), 38);
    }
}

void HorizontalSliceView::leaveEvent(QEvent *event) {
    if (m_isHoveringEdge || m_isHoveringCenter || m_hoverIntersectionIdx >= 0) {
        m_isHoveringEdge = false;
        m_isHoveringCenter = false;
        m_hoverIntersectionIdx = -1;
        update();
    }
    if (!toolTip().isEmpty()) {
        setToolTip(QString());
    }
    QWidget::leaveEvent(event);
}

} // namespace mod3d_gui
