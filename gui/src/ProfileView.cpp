#include "mod3d_gui/ProfileView.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace mod3d_gui {

ProfileView::ProfileView(ModelDocument *doc, QWidget *parent)
    : ProfileView(
          doc, doc ? doc->profileOrientation() : ProfileOrientation::EastWest,
          doc ? doc->profileIndex() : -1, parent) {}

ProfileView::ProfileView(ModelDocument *doc, ProfileOrientation orient,
                         int profileIndex, QWidget *parent)
    : QWidget(parent), m_doc(doc), m_profileOrient(orient),
      m_profileIndex(profileIndex) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  if (m_profileIndex < 0) {
    m_profileIndex = maxProfileIndex() / 2;
  } else {
    m_profileIndex = std::clamp(m_profileIndex, 0, maxProfileIndex());
  }

  connect(m_doc, &ModelDocument::modelGeometryChanged, this,
          QOverload<>::of(&QWidget::update));
  connect(m_doc, &ModelDocument::computationFinished, this,
          QOverload<>::of(&QWidget::update));
  connect(m_doc, &ModelDocument::fieldDisplayToggled, this,
          QOverload<>::of(&QWidget::update));
  connect(m_doc, &ModelDocument::activeFieldChanged, this,
          [this](mod3d::FieldComponent) { update(); });
  connect(m_doc, &ModelDocument::documentReset, this, [this]() {
    m_profileIndex = std::clamp(m_profileIndex, 0, maxProfileIndex());
    fitToWindow();
    updateTitle();
    update();
  });

  if (m_doc) {
    m_mainField = m_doc->activeField();
    m_showModeledCurve = m_doc->showModeledField();
    m_showMeasuredCurve = m_doc->showMeasuredField();
    m_showDifferenceCurve = m_doc->showDifferenceField();
  }

  fitToWindow();
  updateTitle();
}

void ProfileView::setMainFieldComponent(mod3d::FieldComponent comp) {
  if (m_mainField != comp) {
    m_mainField = comp;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::setShowModeledCurve(bool show) {
  if (m_showModeledCurve != show) {
    m_showModeledCurve = show;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::setShowMeasuredCurve(bool show) {
  if (m_showMeasuredCurve != show) {
    m_showMeasuredCurve = show;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::setShowDifferenceCurve(bool show) {
  if (m_showDifferenceCurve != show) {
    m_showDifferenceCurve = show;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::setShowSecondaryCurve(bool show) {
  if (m_showSecondaryCurve != show) {
    m_showSecondaryCurve = show;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::setSecondaryFieldComponent(std::optional<mod3d::FieldComponent> comp) {
  m_secondaryField = comp;
  emit displaySettingsChanged();
  update();
}

void ProfileView::setContinuousDrawingMode(bool enabled) {
  m_continuousDrawingMode = enabled;
  emit displaySettingsChanged();
  update();
}

void ProfileView::setProfileOrientation(ProfileOrientation orient) {
  if (m_profileOrient != orient) {
    m_profileOrient = orient;
    int maxIdx = maxProfileIndex();
    if (m_profileIndex > maxIdx) {
      m_profileIndex = maxIdx;
    }
    fitToWindow();
    updateTitle();
    emit profileSettingsChanged(m_profileIndex, m_profileOrient);
    updateStationPointOnProfileChange();
    update();
  }
}

void ProfileView::setProfileIndex(int index) {
  int clamped = std::clamp(index, 0, maxProfileIndex());
  if (m_profileIndex != clamped) {
    m_profileIndex = clamped;
    updateTitle();
    emit profileSettingsChanged(m_profileIndex, m_profileOrient);
    updateStationPointOnProfileChange();
    update();
  }
}

void ProfileView::moveProfile(int delta) {
  setProfileIndex(m_profileIndex + delta);
}

int ProfileView::maxProfileIndex() const noexcept {
  if (!m_doc) return 0;
  if (m_profileOrient == ProfileOrientation::EastWest) {
    return std::max(0, m_doc->model().getRows() - 3);
  } else {
    return std::max(0, m_doc->model().getCols() - 3);
  }
}

double ProfileView::currentProfileCoordinate() const noexcept {
  if (!m_doc)
    return 0.0;
  if (m_profileOrient == ProfileOrientation::EastWest) {
    return m_doc->model().getYd(m_profileIndex + 1);
  } else {
    return m_doc->model().getXd(m_profileIndex + 1);
  }
}

int ProfileView::profileColumnLineCount() const noexcept {
  if (!m_doc)
    return 0;
  if (m_profileOrient == ProfileOrientation::EastWest) {
    return std::max(0, m_doc->model().getCols() - 2);
  } else {
    return std::max(0, m_doc->model().getRows() - 2);
  }
}

double ProfileView::columnCoordinate(int colIdx) const noexcept {
  if (!m_doc)
    return 0.0;
  if (m_profileOrient == ProfileOrientation::EastWest) {
    return m_doc->model().getXd(colIdx + 1);
  } else {
    return m_doc->model().getYd(colIdx + 1);
  }
}

void ProfileView::getRowCol(int lineIdx, int &row, int &col) const {
  if (m_profileOrient == ProfileOrientation::EastWest) {
    row = m_profileIndex + 1; // 1-based in Model
    col = lineIdx + 1;
  } else {
    row = lineIdx + 1;
    col = m_profileIndex + 1;
  }
}

void ProfileView::updateStationPointOnProfileChange() {
  if (!m_doc) return;
  int nLines = profileColumnLineCount();
  if (nLines <= 0) return;

  int targetLineIdx = m_hoverLineIdx;
  if (targetLineIdx < 0) {
    const auto &cur = m_doc->activeProfileCursor();
    if (cur.active) {
      if (m_profileOrient == ProfileOrientation::EastWest && cur.col >= 1) {
        targetLineIdx = cur.col - 1;
      } else if (m_profileOrient == ProfileOrientation::SouthNorth && cur.row >= 1) {
        targetLineIdx = cur.row - 1;
      }
    }
  }
  if (targetLineIdx < 0 || targetLineIdx >= nLines) {
    targetLineIdx = nLines / 2;
  }
  m_hoverLineIdx = targetLineIdx;

  int rStation = 0, cStation = 0;
  getRowCol(targetLineIdx, rStation, cStation);

  double wy = currentProfileCoordinate();
  double wx = columnCoordinate(targetLineIdx);
  if (m_profileOrient == ProfileOrientation::SouthNorth) {
    std::swap(wx, wy);
  }

  int bId = (m_hoverBodyId >= 0) ? m_hoverBodyId : m_doc->activeProfileCursor().bodyId;
  std::string bName;
  double wz = 0.0;
  bool foundBody = false;

  if (bId >= 0) {
    int bIdx = m_doc->model().getBodyIndex(bId, rStation, cStation);
    if (bIdx >= 0) {
      wz = m_doc->model().getZ(rStation, cStation, bIdx);
      if (const auto *b = m_doc->model().getBody(bId)) {
        bName = b->name();
      }
      foundBody = true;
    } else {
      bId = -1;
    }
  }

  if (!foundBody) {
    for (const auto &b : m_doc->model().getBodies()) {
      int bIdx = m_doc->model().getBodyIndex(b->id(), rStation, cStation);
      if (bIdx >= 0) {
        bId = b->id();
        bName = b->name();
        wz = m_doc->model().getZ(rStation, cStation, bIdx);
        foundBody = true;
        break;
      }
    }
  }

  if (!foundBody) {
    wz = m_doc->model().getZ(rStation, cStation, 0);
  }

  emit cursorStationChanged(rStation, cStation, wx, wy, wz, 0.0);
  m_doc->setActiveProfileCursor({
      true,
      rStation,
      cStation,
      wx,
      wy,
      wz,
      bId,
      bName,
      false
  });
}

void ProfileView::setShowPreviousProfile(bool show) {
  if (m_showPreviousProfile != show) {
    m_showPreviousProfile = show;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::setShowNextProfile(bool show) {
  if (m_showNextProfile != show) {
    m_showNextProfile = show;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::setShowGridLines(bool show) {
  if (m_showGridLines != show) {
    m_showGridLines = show;
    emit displaySettingsChanged();
    update();
  }
}

void ProfileView::updateTitle() {
  QString dirStr =
      (m_profileOrient == ProfileOrientation::EastWest) ? "E-W" : "S-N";
  QString coordStr =
      (m_profileOrient == ProfileOrientation::EastWest) ? "Y" : "X";
  double coord = currentProfileCoordinate();
  QString title = QString("Profile %1 [%2] (%3 = %4 m)")
                      .arg(m_profileIndex + 1)
                      .arg(dirStr)
                      .arg(coordStr)
                      .arg(coord, 0, 'f', 1);
  setWindowTitle(title);
  if (parentWidget()) {
    parentWidget()->setWindowTitle(title);
  }
}

void ProfileView::jumpToExtreme() {
  const auto *grid = m_doc->activeDifferenceGrid();
  if (!grid)
    grid = m_doc->activeModeledGrid();
  if (!grid)
    return;

  int bestIdx = m_profileIndex;
  double maxVal = -1.0;
  if (m_profileOrient == ProfileOrientation::EastWest) {
    for (size_t r = 0; r < grid->rows(); ++r) {
      for (size_t c = 0; c < grid->cols(); ++c) {
        double v = std::abs((*grid)(r, c));
        if (v > maxVal) {
          maxVal = v;
          bestIdx = static_cast<int>(r);
        }
      }
    }
  } else {
    for (size_t c = 0; c < grid->cols(); ++c) {
      for (size_t r = 0; r < grid->rows(); ++r) {
        double v = std::abs((*grid)(r, c));
        if (v > maxVal) {
          maxVal = v;
          bestIdx = static_cast<int>(c);
        }
      }
    }
  }
  setProfileIndex(bestIdx);
}

void ProfileView::jumpToMin() {
  const auto *grid = m_doc->activeDifferenceGrid();
  if (!grid)
    grid = m_doc->activeModeledGrid();
  if (!grid)
    return;

  int bestIdx = m_profileIndex;
  double minVal = 1e30;
  if (m_profileOrient == ProfileOrientation::EastWest) {
    for (size_t r = 0; r < grid->rows(); ++r) {
      for (size_t c = 0; c < grid->cols(); ++c) {
        double v = (*grid)(r, c);
        if (v < minVal) {
          minVal = v;
          bestIdx = static_cast<int>(r);
        }
      }
    }
  } else {
    for (size_t c = 0; c < grid->cols(); ++c) {
      for (size_t r = 0; r < grid->rows(); ++r) {
        double v = (*grid)(r, c);
        if (v < minVal) {
          minVal = v;
          bestIdx = static_cast<int>(c);
        }
      }
    }
  }
  setProfileIndex(bestIdx);
}

void ProfileView::jumpToMax() {
  const auto *grid = m_doc->activeDifferenceGrid();
  if (!grid)
    grid = m_doc->activeModeledGrid();
  if (!grid)
    return;

  int bestIdx = m_profileIndex;
  double maxVal = -1e30;
  if (m_profileOrient == ProfileOrientation::EastWest) {
    for (size_t r = 0; r < grid->rows(); ++r) {
      for (size_t c = 0; c < grid->cols(); ++c) {
        double v = (*grid)(r, c);
        if (v > maxVal) {
          maxVal = v;
          bestIdx = static_cast<int>(r);
        }
      }
    }
  } else {
    for (size_t c = 0; c < grid->cols(); ++c) {
      for (size_t r = 0; r < grid->rows(); ++r) {
        double v = (*grid)(r, c);
        if (v > maxVal) {
          maxVal = v;
          bestIdx = static_cast<int>(c);
        }
      }
    }
  }
  setProfileIndex(bestIdx);
}

void ProfileView::fitToWindow() {
  if (!m_doc)
    return;
  const auto &m = m_doc->model();
  if (m_profileOrient == ProfileOrientation::EastWest) {
    m_worldXMin = m.getXMin();
    m_worldXMax = m.getXMax();
  } else {
    m_worldXMin = m.getYMin();
    m_worldXMax = m.getYMax();
  }
  m_worldZMin = m.getZMin();
  m_worldZMax = m.getZMax();
  update();
}

void ProfileView::zoomIn() {
  double spanX = (m_worldXMax - m_worldXMin) * 0.8;
  double midX = (m_worldXMax + m_worldXMin) * 0.5;
  m_worldXMin = midX - spanX * 0.5;
  m_worldXMax = midX + spanX * 0.5;

  double spanZ = (m_worldZMax - m_worldZMin) * 0.8;
  double midZ = (m_worldZMax + m_worldZMin) * 0.5;
  m_worldZMin = midZ - spanZ * 0.5;
  m_worldZMax = midZ + spanZ * 0.5;
  update();
}

void ProfileView::zoomOut() {
  double spanX = (m_worldXMax - m_worldXMin) * 1.25;
  double midX = (m_worldXMax + m_worldXMin) * 0.5;
  m_worldXMin = midX - spanX * 0.5;
  m_worldXMax = midX + spanX * 0.5;

  double spanZ = (m_worldZMax - m_worldZMin) * 1.25;
  double midZ = (m_worldZMax + m_worldZMin) * 0.5;
  m_worldZMin = midZ - spanZ * 0.5;
  m_worldZMax = midZ + spanZ * 0.5;
  update();
}

void ProfileView::setEqualScale(bool equal) {
  m_equalScale = equal;
  update();
}

void ProfileView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updatePaneRects();
}

void ProfileView::updatePaneRects() {
  int w = width();
  int h = height();

  int divH = 6;
  int fldH = static_cast<int>(h * m_dividerRatio);
  if (fldH < 80)
    fldH = 80;
  if (fldH > h - 120)
    fldH = h - 120;

  m_rectFieldPane = QRect(0, 0, w, fldH);
  m_rectDivider = QRect(0, fldH, w, divH);
  m_rectSectionPane = QRect(0, fldH + divH, w, h - (fldH + divH));
}

double ProfileView::screenToWorldX(int screenX) const {
  int drawW = m_rectSectionPane.width() - m_marginLeft - m_marginRight;
  if (drawW <= 0)
    return m_worldXMin;
  double t = static_cast<double>(screenX - m_marginLeft) / drawW;
  return m_worldXMin + t * (m_worldXMax - m_worldXMin);
}

int ProfileView::worldToScreenX(double worldX) const {
  int drawW = m_rectSectionPane.width() - m_marginLeft - m_marginRight;
  if (m_worldXMax <= m_worldXMin)
    return m_marginLeft;
  double t = (worldX - m_worldXMin) / (m_worldXMax - m_worldXMin);
  return m_marginLeft + static_cast<int>(t * drawW);
}

double ProfileView::screenToWorldZ(int screenY) const {
  int drawTop = m_rectSectionPane.top() + m_marginTop;
  int drawH = m_rectSectionPane.height() - m_marginTop - m_marginBottom;
  if (drawH <= 0)
    return m_worldZMin;
  double t = static_cast<double>(screenY - drawTop) / drawH;
  return m_worldZMax - t * (m_worldZMax - m_worldZMin);
}

int ProfileView::worldToScreenZ(double worldZ) const {
  int drawTop = m_rectSectionPane.top() + m_marginTop;
  int drawH = m_rectSectionPane.height() - m_marginTop - m_marginBottom;
  if (m_worldZMax <= m_worldZMin)
    return drawTop;
  double t = (m_worldZMax - worldZ) / (m_worldZMax - m_worldZMin);
  return drawTop + static_cast<int>(t * drawH);
}

int ProfileView::worldToScreenField(double fldVal, double minFld,
                                    double maxFld) const {
  int top = m_rectFieldPane.top() + 25;
  int bottom = m_rectFieldPane.bottom() - 25;
  int h = bottom - top;
  if (maxFld <= minFld || h <= 0)
    return (top + bottom) / 2;
  double t = (maxFld - fldVal) / (maxFld - minFld);
  return top + static_cast<int>(t * h);
}

int ProfileView::findClosestLineIndex(int screenX) const {
  int nLines = profileColumnLineCount();
  if (nLines <= 0)
    return -1;
  int bestLine = 0;
  int bestDist = 1000000;
  for (int c = 0; c < nLines; ++c) {
    int sx = worldToScreenX(columnCoordinate(c));
    int dist = std::abs(screenX - sx);
    if (dist < bestDist) {
      bestDist = dist;
      bestLine = c;
    }
  }
  return bestLine;
}

void ProfileView::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // Background
  painter.fillRect(rect(), QColor(245, 245, 248));

  // 1. Draw Field Curves Pane (Upper)
  drawFieldCurves(painter);

  // 2. Draw Splitter Divider
  drawDivider(painter);

  // 3. Draw Geological Cross-Section Pane (Lower)
  drawGeologicalSection(painter);
}

void ProfileView::drawDivider(QPainter &painter) {
  painter.fillRect(m_rectDivider, QColor(215, 215, 220));
  painter.setPen(QColor(160, 160, 165));
  painter.drawLine(m_rectDivider.left(), m_rectDivider.top(),
                   m_rectDivider.right(), m_rectDivider.top());
  painter.drawLine(m_rectDivider.left(), m_rectDivider.bottom(),
                   m_rectDivider.right(), m_rectDivider.bottom());

  int cx = m_rectDivider.center().x();
  int cy = m_rectDivider.center().y();
  painter.fillRect(cx - 24, cy - 1, 48, 2, QColor(100, 100, 105));
}

void ProfileView::drawFieldCurves(QPainter &painter) {
  painter.save();
  painter.setClipRect(m_rectFieldPane);

  QRect canvas(m_marginLeft, m_rectFieldPane.top() + 10,
               m_rectFieldPane.width() - m_marginLeft - m_marginRight,
               m_rectFieldPane.height() - 20);
  painter.fillRect(canvas, Qt::white);
  painter.setPen(QColor(180, 180, 190));
  painter.drawRect(canvas);

  int nLines = profileColumnLineCount();
  if (nLines <= 0) {
    painter.restore();
    return;
  }

  std::vector<double> modeledVals(nLines, 0.0);
  std::vector<double> observedVals(nLines, 0.0);
  std::vector<double> diffVals(nLines, 0.0);
  std::vector<double> xCoords(nLines, 0.0);
  std::vector<bool> hasModeled(nLines, false);
  std::vector<bool> hasObserved(nLines, false);
  std::vector<bool> hasDiff(nLines, false);

  const auto *modGrd = m_doc->modeledGrid(m_mainField);
  const auto *obsGrd = m_doc->observedGrid(m_mainField);
  if ((!obsGrd || obsGrd->empty()) && m_mainField == mod3d::FieldComponent::GZ) {
    obsGrd = m_doc->observedGrid(mod3d::FieldComponent::G_TOT);
  }
  const auto *difGrd = m_doc->differenceGrid(m_mainField);

  int prfIdx = m_profileIndex;
  bool isEW = (m_profileOrient == ProfileOrientation::EastWest);

  for (int i = 0; i < nLines; ++i) {
    xCoords[i] = columnCoordinate(i);
    size_t r = isEW ? static_cast<size_t>(prfIdx) : static_cast<size_t>(i);
    size_t c = isEW ? static_cast<size_t>(i) : static_cast<size_t>(prfIdx);

    if (modGrd && r < modGrd->rows() && c < modGrd->cols()) {
      double v = (*modGrd)(r, c);
      if (!mod3d::Grid::is_dummy_value(v)) {
        modeledVals[i] = v;
        hasModeled[i] = true;
      }
    }

    if (obsGrd && r < obsGrd->rows() && c < obsGrd->cols()) {
      double v = (*obsGrd)(r, c);
      if (!mod3d::Grid::is_dummy_value(v)) {
        observedVals[i] = v;
        hasObserved[i] = true;
      }
    }

    if (difGrd && r < difGrd->rows() && c < difGrd->cols()) {
      double v = (*difGrd)(r, c);
      if (!mod3d::Grid::is_dummy_value(v)) {
        diffVals[i] = v;
        hasDiff[i] = true;
      }
    }
  }

  double minVal = 1e30;
  double maxVal = -1e30;

  // Use full 2D grid min/max of the main field
  if (m_showModeledCurve && modGrd && !modGrd->empty()) {
    auto stats = modGrd->compute_stats();
    if (stats.valid_count > 0 && stats.min <= stats.max) {
      minVal = std::min(minVal, stats.min);
      maxVal = std::max(maxVal, stats.max);
    }
  }
  if (m_showMeasuredCurve && obsGrd && !obsGrd->empty()) {
    auto stats = obsGrd->compute_stats();
    if (stats.valid_count > 0 && stats.min <= stats.max) {
      minVal = std::min(minVal, stats.min);
      maxVal = std::max(maxVal, stats.max);
    }
  }
  // Only include difference field if neither modeled nor measured field is displayed
  if (minVal >= maxVal && m_showDifferenceCurve && difGrd && !difGrd->empty()) {
    auto stats = difGrd->compute_stats();
    if (stats.valid_count > 0 && stats.min <= stats.max) {
      minVal = std::min(minVal, stats.min);
      maxVal = std::max(maxVal, stats.max);
    }
  }

  // Fallback if grids were empty or not yet computed
  if (minVal >= maxVal) {
    for (int i = 0; i < nLines; ++i) {
      if (m_showModeledCurve && hasModeled[i]) {
        minVal = std::min(minVal, modeledVals[i]);
        maxVal = std::max(maxVal, modeledVals[i]);
      }
      if (m_showMeasuredCurve && hasObserved[i]) {
        minVal = std::min(minVal, observedVals[i]);
        maxVal = std::max(maxVal, observedVals[i]);
      }
      if (m_showDifferenceCurve && hasDiff[i]) {
        minVal = std::min(minVal, diffVals[i]);
        maxVal = std::max(maxVal, diffVals[i]);
      }
    }
  }

  if (minVal >= maxVal || std::abs(maxVal - minVal) < 1e-9) {
    double center = (minVal < 1e20) ? (minVal + maxVal) / 2.0 : 0.0;
    double span = (std::abs(center) > 1e-3) ? std::abs(center) * 0.1 : 1.0;
    minVal = center - span;
    maxVal = center + span;
  } else {
    double pad = (maxVal - minVal) * 0.05;
    minVal -= pad;
    maxVal += pad;
  }

  drawFieldAxes(painter, minVal, maxVal);

  if (minVal <= 0.0 && maxVal >= 0.0) {
    int zeroY = worldToScreenField(0.0, minVal, maxVal);
    painter.setPen(QPen(QColor(220, 220, 225), 1, Qt::DashLine));
    painter.drawLine(canvas.left(), zeroY, canvas.right(), zeroY);
  }

  // Difference curve (magenta dashed line)
  if (m_showDifferenceCurve) {
    painter.setPen(QPen(QColor(180, 0, 180), 1.5, Qt::DashLine));
    QPolygonF poly;
    for (int i = 0; i < nLines; ++i) {
      if (hasDiff[i]) {
        int sx = worldToScreenX(xCoords[i]);
        int sy = worldToScreenField(diffVals[i], minVal, maxVal);
        poly << QPointF(sx, sy);
      } else if (!poly.isEmpty()) {
        painter.drawPolyline(poly);
        poly.clear();
      }
    }
    if (!poly.isEmpty()) {
      painter.drawPolyline(poly);
    }
  }

  // Measured Field (connecting line + upright '+' crosses)
  if (m_showMeasuredCurve) {
    painter.setPen(QPen(QColor(200, 20, 20, 180), 1.0, Qt::SolidLine));
    QPolygonF poly;
    for (int i = 0; i < nLines; ++i) {
      if (hasObserved[i]) {
        int sx = worldToScreenX(xCoords[i]);
        int sy = worldToScreenField(observedVals[i], minVal, maxVal);
        poly << QPointF(sx, sy);
      } else if (!poly.isEmpty()) {
        painter.drawPolyline(poly);
        poly.clear();
      }
    }
    if (!poly.isEmpty()) {
      painter.drawPolyline(poly);
    }

    painter.setPen(QPen(QColor(200, 20, 20), 1.5));
    for (int i = 0; i < nLines; ++i) {
      if (hasObserved[i]) {
        int sx = worldToScreenX(xCoords[i]);
        int sy = worldToScreenField(observedVals[i], minVal, maxVal);
        painter.drawLine(sx - 4, sy, sx + 4, sy);
        painter.drawLine(sx, sy - 4, sx, sy + 4);
      }
    }
  }

  // Modeled Field (blue solid curve)
  if (m_showModeledCurve) {
    painter.setPen(QPen(QColor(20, 80, 220), 2.0, Qt::SolidLine));
    QPolygonF poly;
    for (int i = 0; i < nLines; ++i) {
      if (hasModeled[i]) {
        int sx = worldToScreenX(xCoords[i]);
        int sy = worldToScreenField(modeledVals[i], minVal, maxVal);
        poly << QPointF(sx, sy);
      } else if (!poly.isEmpty()) {
        painter.drawPolyline(poly);
        poly.clear();
      }
    }
    if (!poly.isEmpty()) {
      painter.drawPolyline(poly);
    }
  }

  // Secondary curve overlay (e.g. Magnetics overlay on Gravity)
  if (m_showSecondaryCurve && m_secondaryField.has_value()) {
    const auto *secGrd = m_doc->modeledGrid(*m_secondaryField);
    if (secGrd && !secGrd->empty()) {
      std::vector<double> secVals(nLines, 0.0);
      std::vector<bool> hasSec(nLines, false);
      double secMin = 1e30, secMax = -1e30;
      for (int i = 0; i < nLines; ++i) {
        size_t r = isEW ? static_cast<size_t>(prfIdx) : static_cast<size_t>(i);
        size_t c = isEW ? static_cast<size_t>(i) : static_cast<size_t>(prfIdx);
        if (r < secGrd->rows() && c < secGrd->cols()) {
          double v = (*secGrd)(r, c);
          if (!mod3d::Grid::is_dummy_value(v)) {
            secVals[i] = v;
            hasSec[i] = true;
            secMin = std::min(secMin, v);
            secMax = std::max(secMax, v);
          }
        }
      }
      if (secMin < secMax) {
        double secPad = (secMax - secMin) * 0.05;
        secMin -= secPad;
        secMax += secPad;
        painter.setPen(QPen(QColor(13, 148, 136), 1.8, Qt::DashDotLine)); // Teal dash-dot
        QPolygonF poly;
        for (int i = 0; i < nLines; ++i) {
          if (hasSec[i]) {
            int sx = worldToScreenX(xCoords[i]);
            int sy = worldToScreenField(secVals[i], secMin, secMax);
            poly << QPointF(sx, sy);
          } else if (!poly.isEmpty()) {
            painter.drawPolyline(poly);
            poly.clear();
          }
        }
        if (!poly.isEmpty()) {
          painter.drawPolyline(poly);
        }
      }
    }
  }

  drawRmsDrvBadges(painter);

  painter.restore();
}

void ProfileView::drawFieldAxes(QPainter &painter, double minFld,
                                double maxFld) {
  painter.setPen(QColor(80, 80, 80));
  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);

  double range = std::abs(maxFld - minFld);
  int decimals = 1;
  if (range < 0.005)
    decimals = 4;
  else if (range < 0.05)
    decimals = 3;
  else if (range < 0.5)
    decimals = 2;
  else if (range >= 100.0)
    decimals = 0;

  int nTicks = 4;
  for (int t = 0; t <= nTicks; ++t) {
    double f = minFld + t * (maxFld - minFld) / nTicks;
    int y = worldToScreenField(f, minFld, maxFld);
    painter.drawLine(m_marginLeft - 5, y, m_marginLeft, y);
    QString label = QString::number(f, 'f', decimals);
    painter.drawText(2, y + 4, m_marginLeft - 8, 16, Qt::AlignRight, label);
  }

  // Vertical axis title strictly for main field
  QString compName = ModelDocument::componentName(m_mainField);
  QString compUnit = ModelDocument::componentUnit(m_mainField);
  QString axisTitle = compUnit.isEmpty()
                          ? compName
                          : QString("%1 (%2)").arg(compName, compUnit);

  painter.save();
  painter.translate(15, m_rectFieldPane.center().y());
  painter.rotate(-90);
  painter.drawText(-70, -8, 140, 16, Qt::AlignCenter, axisTitle);
  painter.restore();
}

void ProfileView::drawRmsDrvBadges(QPainter &painter) {
  double rms = m_doc->currentRms();
  double drv = m_doc->currentDerivative();

  int right = m_rectFieldPane.right() - m_marginRight - 10;
  int top = m_rectFieldPane.top() + 15;

  QRect badgeRect(right - 170, top, 170, 22);
  painter.fillRect(badgeRect, QColor(255, 255, 255, 220));
  painter.setPen(QColor(180, 180, 190));
  painter.drawRect(badgeRect);

  painter.setPen(QColor(30, 30, 30));
  QFont f = painter.font();
  f.setPointSize(8);
  f.setBold(true);
  painter.setFont(f);

  QString text =
      QString("RMS: %1  DRV: %2").arg(rms, 0, 'f', 2).arg(drv, 0, 'f', 2);
  painter.drawText(badgeRect, Qt::AlignCenter, text);
}

void ProfileView::drawGeologicalSection(QPainter &painter) {
  painter.save();
  painter.setClipRect(m_rectSectionPane);

  QRect canvas(m_marginLeft, m_rectSectionPane.top() + m_marginTop,
               m_rectSectionPane.width() - m_marginLeft - m_marginRight,
               m_rectSectionPane.height() - m_marginTop - m_marginBottom);
  painter.fillRect(canvas, QColor(250, 250, 252));
  painter.setPen(QColor(170, 170, 180));
  painter.drawRect(canvas);

  drawSectionAxes(painter);

  if (m_showGridLines) {
    drawGridLines(painter);
  }

  drawProfileIntersections(painter);

  drawReliefAndBottom(painter);

  if (m_showPreviousProfile && m_profileIndex > 0) {
    drawGhostBodies(painter, m_profileIndex - 1,
                    QPen(QColor(120, 160, 200), 1.5, Qt::DashLine));
  }
  if (m_showNextProfile && m_profileIndex < maxProfileIndex()) {
    drawGhostBodies(painter, m_profileIndex + 1,
                    QPen(QColor(220, 140, 100), 1.5, Qt::DotLine));
  }

  drawBodies(painter);

  painter.restore();
}

void ProfileView::drawSectionAxes(QPainter &painter) {
  painter.setPen(QColor(60, 60, 60));
  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);

  int nZTicks = 5;
  for (int i = 0; i <= nZTicks; ++i) {
    double z = m_worldZMin + i * (m_worldZMax - m_worldZMin) / nZTicks;
    int y = worldToScreenZ(z);
    painter.drawLine(m_marginLeft - 5, y, m_marginLeft, y);
    QString label = QString::number(z, 'f', 0);
    painter.drawText(5, y + 4, m_marginLeft - 10, 16, Qt::AlignRight, label);
  }

  painter.save();
  painter.translate(15, m_rectSectionPane.center().y());
  painter.rotate(-90);
  painter.drawText(-40, 0, "Z (m)");
  painter.restore();

  int nXTicks = 5;
  int axisY = m_rectSectionPane.bottom() - m_marginBottom;
  for (int i = 0; i <= nXTicks; ++i) {
    double x = m_worldXMin + i * (m_worldXMax - m_worldXMin) / nXTicks;
    int sx = worldToScreenX(x);
    painter.drawLine(sx, axisY, sx, axisY + 5);
    QString label = QString::number(x, 'f', 0);
    painter.drawText(sx - 35, axisY + 8, 70, 16, Qt::AlignCenter, label);
  }

  QString hTitle = (m_profileOrient == ProfileOrientation::EastWest)
                       ? "X (m) [East]"
                       : "Y (m) [North]";
  painter.drawText(m_rectSectionPane.center().x() - 50, axisY + 24, 100, 16,
                   Qt::AlignCenter, hTitle);
}

void ProfileView::drawGridLines(QPainter &painter) {
  int nLines = profileColumnLineCount();
  painter.setPen(QPen(QColor(220, 220, 230), 1, Qt::DotLine));
  int topY = worldToScreenZ(m_worldZMax);
  int botY = worldToScreenZ(m_worldZMin);

  for (int i = 0; i < nLines; ++i) {
    double xw = columnCoordinate(i);
    int sx = worldToScreenX(xw);
    painter.drawLine(sx, topY, sx, botY);
  }
}

void ProfileView::drawProfileIntersections(QPainter &painter) {
  QMdiArea *mdi = nullptr;
  QWidget *p = parentWidget();
  while (p) {
    if (auto *m = qobject_cast<QMdiArea *>(p)) {
      mdi = m;
      break;
    }
    p = p->parentWidget();
  }
  if (!mdi)
    return;

  for (auto *sub : mdi->subWindowList()) {
    auto *otherPv = qobject_cast<ProfileView *>(sub->widget());
    if (otherPv && otherPv != this &&
        otherPv->profileOrientation() != m_profileOrient) {
      int otherIdx = otherPv->profileIndex();
      if (otherIdx >= 0 && otherIdx < profileColumnLineCount()) {
        double xw = columnCoordinate(otherIdx);
        int sx = worldToScreenX(xw);
        int topY = worldToScreenZ(m_worldZMax);
        int botY = worldToScreenZ(m_worldZMin);

        // Perpendicular profile vertical indicator line
        painter.setPen(QPen(QColor(220, 110, 20, 200), 2.0, Qt::DashLine));
        painter.drawLine(sx, topY, sx, botY);

        // Badge marker at the top of geological section
        QString label = QString("Prf %1 [%2]")
                            .arg(otherIdx + 1)
                            .arg(otherPv->profileOrientation() ==
                                         ProfileOrientation::EastWest
                                     ? "E-W"
                                     : "S-N");
        QRect badge(sx - 34, topY + 4, 68, 16);
        painter.fillRect(badge, QColor(255, 245, 230, 230));
        painter.setPen(QColor(200, 90, 10));
        painter.drawRect(badge);
        QFont f = painter.font();
        f.setPointSize(7);
        f.setBold(true);
        painter.setFont(f);
        painter.drawText(badge, Qt::AlignCenter, label);
      }
    }
  }
}

void ProfileView::drawReliefAndBottom(QPainter &painter) {
  int nLines = profileColumnLineCount();
  if (nLines <= 0)
    return;

  QPolygonF reliefPoly;
  for (int i = 0; i < nLines; ++i) {
    double xw = columnCoordinate(i);
    int sx = worldToScreenX(xw);
    int r = 0, c = 0;
    getRowCol(i, r, c);
    double topoZ = m_doc->model().getZ(r, c, 0);
    int sy = worldToScreenZ(topoZ);
    reliefPoly << QPointF(sx, sy);
  }
  painter.setPen(QPen(QColor(40, 140, 40), 2.0));
  painter.drawPolyline(reliefPoly);

  int botY = worldToScreenZ(m_doc->model().getZMin());
  painter.setPen(QPen(QColor(100, 100, 110), 2.0, Qt::DashLine));
  int leftX = worldToScreenX(m_worldXMin);
  int rightX = worldToScreenX(m_worldXMax);
  painter.drawLine(leftX, botY, rightX, botY);
}

void ProfileView::drawBodies(QPainter &painter) {
  const auto &model = m_doc->model();
  int nLines = profileColumnLineCount();
  int prfIdx = m_profileIndex;
  bool isEW = (m_profileOrient == ProfileOrientation::EastWest);

  for (const auto &bodyPtr : model.getBodies()) {
    int bId = bodyPtr->id();
    auto bc = bodyPtr->color();
    QColor bColor(bc.r, bc.g, bc.b, 190);
    bool filled = bodyPtr->is_filled();

    for (int c = 0; c < nLines - 1; ++c) {
      int r0 = isEW ? (prfIdx + 1) : (c + 1);
      int col0 = isEW ? (c + 1) : (prfIdx + 1);

      int r1 = isEW ? (prfIdx + 1) : (c + 2);
      int col1 = isEW ? (c + 2) : (prfIdx + 1);

      int idx0 = model.getBodyIndex(bId, r0, col0);
      int idx1 = model.getBodyIndex(bId, r1, col1);

      if (idx0 >= 0 && idx1 >= 0) {
        double x0w = columnCoordinate(c);
        double x1w = columnCoordinate(c + 1);

        int sx0 = worldToScreenX(x0w);
        int sx1 = worldToScreenX(x1w);

        int topY0 = worldToScreenZ(model.getZ(r0, col0, idx0));
        int botY0 = worldToScreenZ(model.getZ(r0, col0, idx0 + 1));

        int topY1 = worldToScreenZ(model.getZ(r1, col1, idx1));
        int botY1 = worldToScreenZ(model.getZ(r1, col1, idx1 + 1));

        QPolygonF slicePoly;
        slicePoly << QPointF(sx0, topY0) << QPointF(sx1, topY1)
                  << QPointF(sx1, botY1) << QPointF(sx0, botY0);

        painter.setPen(QPen(bColor.darker(140), 1.5));
        if (filled) {
          painter.setBrush(bColor);
        } else {
          painter.setBrush(Qt::NoBrush);
        }
        painter.drawPolygon(slicePoly);
      }
    }

    for (int c = 0; c < nLines; ++c) {
      int r = isEW ? (prfIdx + 1) : (c + 1);
      int col = isEW ? (c + 1) : (prfIdx + 1);
      int idx = model.getBodyIndex(bId, r, col);
      if (idx >= 0) {
        double xw = columnCoordinate(c);
        int sx = worldToScreenX(xw);
        int topY = worldToScreenZ(model.getZ(r, col, idx));
        int botY = worldToScreenZ(model.getZ(r, col, idx + 1));

        // Vertical column segment connecting top and bottom vertices
        painter.setPen(QPen(bColor.darker(140), 3.0));
        painter.drawLine(sx, topY, sx, botY);

        painter.setPen(Qt::black);
        painter.setBrush(Qt::white);
        painter.drawRect(sx - 3, topY - 3, 6, 6);
        painter.drawRect(sx - 3, botY - 3, 6, 6);
      }
    }
  }
}

void ProfileView::drawGhostBodies(QPainter &painter, int ghostProfileIdx,
                                  const QPen &pen) {
  const auto &model = m_doc->model();
  int nLines = profileColumnLineCount();
  bool isEW = (m_profileOrient == ProfileOrientation::EastWest);

  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  for (const auto &bodyPtr : model.getBodies()) {
    int bId = bodyPtr->id();
    for (int c = 0; c < nLines - 1; ++c) {
      int r0 = isEW ? (ghostProfileIdx + 1) : (c + 1);
      int col0 = isEW ? (c + 1) : (ghostProfileIdx + 1);

      int r1 = isEW ? (ghostProfileIdx + 1) : (c + 2);
      int col1 = isEW ? (c + 2) : (ghostProfileIdx + 1);

      int idx0 = model.getBodyIndex(bId, r0, col0);
      int idx1 = model.getBodyIndex(bId, r1, col1);

      if (idx0 >= 0 && idx1 >= 0) {
        double x0w = columnCoordinate(c);
        double x1w = columnCoordinate(c + 1);

        int sx0 = worldToScreenX(x0w);
        int sx1 = worldToScreenX(x1w);

        int topY0 = worldToScreenZ(model.getZ(r0, col0, idx0));
        int botY0 = worldToScreenZ(model.getZ(r0, col0, idx0 + 1));

        int topY1 = worldToScreenZ(model.getZ(r1, col1, idx1));
        int botY1 = worldToScreenZ(model.getZ(r1, col1, idx1 + 1));

        QPolygonF slicePoly;
        slicePoly << QPointF(sx0, topY0) << QPointF(sx1, topY1)
                  << QPointF(sx1, botY1) << QPointF(sx0, botY0);

        painter.drawPolygon(slicePoly);
      }
    }
  }
}

ProfileView::CursorMode ProfileView::performHitTest(const QPoint &pos,
                                                    int &hitLineIdx,
                                                    int &hitVertexIdx,
                                                    int &hitBodyId) {
  hitLineIdx = -1;
  hitVertexIdx = -1;
  hitBodyId = -1;

  if (m_rectDivider.adjusted(0, -2, 0, 2).contains(pos)) {
    return CursorMode::OverDivider;
  }

  if (!m_rectSectionPane.contains(pos)) {
    return CursorMode::Normal;
  }

  const auto &model = m_doc->model();
  int nLines = profileColumnLineCount();
  int prfIdx = m_profileIndex;
  bool isEW = (m_profileOrient == ProfileOrientation::EastWest);

  for (const auto &bodyPtr : model.getBodies()) {
    int bId = bodyPtr->id();
    for (int c = 0; c < nLines; ++c) {
      int r = isEW ? (prfIdx + 1) : (c + 1);
      int col = isEW ? (c + 1) : (prfIdx + 1);
      int idx = model.getBodyIndex(bId, r, col);
      if (idx >= 0) {
        double xw = columnCoordinate(c);
        int sx = worldToScreenX(xw);
        int topY = worldToScreenZ(model.getZ(r, col, idx));
        int botY = worldToScreenZ(model.getZ(r, col, idx + 1));

        if (std::abs(pos.x() - sx) <= 8) {
          if (std::abs(pos.y() - topY) <= 8) {
            hitLineIdx = c;
            hitVertexIdx = idx;
            hitBodyId = bId;
            return CursorMode::OverVertex;
          }
          if (std::abs(pos.y() - botY) <= 8) {
            hitLineIdx = c;
            hitVertexIdx = idx + 1;
            hitBodyId = bId;
            return CursorMode::OverVertex;
          }
        }
      }
    }
  }

  for (const auto &bodyPtr : model.getBodies()) {
    int bId = bodyPtr->id();
    for (int c = 0; c < nLines; ++c) {
      int r = isEW ? (prfIdx + 1) : (c + 1);
      int col = isEW ? (c + 1) : (prfIdx + 1);
      int idx = model.getBodyIndex(bId, r, col);
      if (idx >= 0) {
        bool hasLeft = (c > 0 && model.getBodyIndex(bId, isEW ? r : c,
                                                    isEW ? c : col) >= 0);
        bool hasRight =
            (c < nLines - 1 && model.getBodyIndex(bId, isEW ? r : (c + 2),
                                                  isEW ? (c + 2) : col) >= 0);

        if (!hasLeft || !hasRight) {
          double xw = columnCoordinate(c);
          int sx = worldToScreenX(xw);
          int topY = worldToScreenZ(model.getZ(r, col, idx));
          int botY = worldToScreenZ(model.getZ(r, col, idx + 1));
          if (std::abs(pos.x() - sx) <= 6 && pos.y() >= topY &&
              pos.y() <= botY) {
            hitLineIdx = c;
            hitVertexIdx = idx;
            hitBodyId = bId;
            return CursorMode::OverBodyLine;
          }
        }
      }
    }
  }

  for (const auto &bodyPtr : model.getBodies()) {
    int bId = bodyPtr->id();
    for (int c = 0; c < nLines - 1; ++c) {
      int r0 = isEW ? (prfIdx + 1) : (c + 1);
      int col0 = isEW ? (c + 1) : (prfIdx + 1);
      int r1 = isEW ? (prfIdx + 1) : (c + 2);
      int col1 = isEW ? (c + 2) : (prfIdx + 1);

      int idx0 = model.getBodyIndex(bId, r0, col0);
      int idx1 = model.getBodyIndex(bId, r1, col1);

      if (idx0 >= 0 && idx1 >= 0) {
        int sx0 = worldToScreenX(columnCoordinate(c));
        int sx1 = worldToScreenX(columnCoordinate(c + 1));
        int topY0 = worldToScreenZ(model.getZ(r0, col0, idx0));
        int botY0 = worldToScreenZ(model.getZ(r0, col0, idx0 + 1));
        int topY1 = worldToScreenZ(model.getZ(r1, col1, idx1));
        int botY1 = worldToScreenZ(model.getZ(r1, col1, idx1 + 1));

        QPolygonF poly;
        poly << QPointF(sx0, topY0) << QPointF(sx1, topY1)
             << QPointF(sx1, botY1) << QPointF(sx0, botY0);

        if (poly.containsPoint(pos, Qt::OddEvenFill)) {
          hitLineIdx = c;
          hitVertexIdx = idx0;
          hitBodyId = bId;
          return CursorMode::OverBody;
        }
      }
    }
  }

  for (int c = 0; c < nLines; ++c) {
    double xw = columnCoordinate(c);
    int sx = worldToScreenX(xw);
    if (std::abs(pos.x() - sx) <= 6) {
      hitLineIdx = c;
      return CursorMode::OverGridLine;
    }
  }

  return CursorMode::Normal;
}

void ProfileView::updateCursorShape(CursorMode mode) {
  switch (mode) {
  case CursorMode::OverDivider:
    setCursor(Qt::SplitVCursor);
    break;
  case CursorMode::OverVertex:
    setCursor(Qt::SizeVerCursor);
    break;
  case CursorMode::OverBodyLine:
    setCursor(Qt::SizeHorCursor);
    break;
  case CursorMode::OverBody:
    setCursor((QApplication::keyboardModifiers() & Qt::ShiftModifier)
                  ? Qt::ClosedHandCursor
                  : Qt::OpenHandCursor);
    break;
  case CursorMode::OverGridLine:
    setCursor(Qt::CrossCursor);
    break;
  default:
    setCursor(Qt::ArrowCursor);
    break;
  }
}

void ProfileView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int lineIdx = -1, vIdx = -1, bId = -1;
    CursorMode mode = performHitTest(event->pos(), lineIdx, vIdx, bId);

    if (mode == CursorMode::OverDivider) {
      m_dragOp = DragOperation::ResizeDivider;
      m_dragStartPos = event->pos();
    } else if (mode == CursorMode::OverVertex) {
      m_dragOp = DragOperation::MoveVertex;
      m_dragLineIdx = lineIdx;
      m_dragVertexIdx = vIdx;
      m_dragBodyId = bId;
      m_dragStartPos = event->pos();
      m_dragLastMouseZ = screenToWorldZ(event->pos().y());
    } else if (mode == CursorMode::OverBodyLine) {
      m_dragOp = DragOperation::ExtendBodyLateral;
      m_dragLineIdx = lineIdx;
      m_dragVertexIdx = vIdx;
      m_dragBodyId = bId;
      m_dragStartPos = event->pos();
    } else if (mode == CursorMode::OverBody &&
               (event->modifiers() & Qt::ShiftModifier)) {
      m_dragOp = DragOperation::MoveBodyVertical;
      m_dragBodyId = bId;
      m_dragStartPos = event->pos();
      m_dragLastMouseZ = screenToWorldZ(event->pos().y());
    }
  }
  QWidget::mousePressEvent(event);
}

void ProfileView::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragOp == DragOperation::ResizeDivider) {
    float newRatio = static_cast<float>(event->pos().y()) / height();
    if (newRatio >= 0.15f && newRatio <= 0.85f) {
      m_dividerRatio = newRatio;
      updatePaneRects();
      update();
    }
    return;
  }

  if (m_dragOp == DragOperation::MoveVertex) {
    double newZ = screenToWorldZ(event->pos().y());
    double zMin = m_doc->model().getZMin();
    double zMax = m_doc->model().getZMax();
    newZ = std::clamp(newZ, zMin, zMax);

    bool ctrlPressed = (event->modifiers() & Qt::ControlModifier);
    mod3d::BodyMoveType moveType =
        ctrlPressed ? mod3d::BodyMoveType::Split : mod3d::BodyMoveType::Normal;

    double xw = screenToWorldX(event->pos().x());
    double startCoord = (m_profileOrient == ProfileOrientation::EastWest)
                            ? m_doc->model().getX0()
                            : m_doc->model().getY0();
    double stepCoord = (m_profileOrient == ProfileOrientation::EastWest)
                           ? m_doc->model().getXSize()
                           : m_doc->model().getYSize();
    int nLines = profileColumnLineCount();
    int targetLineIdx = std::clamp(
        static_cast<int>(std::round((xw - startCoord) / stepCoord)), 0,
        nLines - 1);

    if (m_dragBodyId < 0) {
      int r = 0, c = 0;
      getRowCol(m_dragLineIdx, r, c);
      m_dragBodyId = m_doc->model().getID(m_dragVertexIdx, r, c);
    }

    if (targetLineIdx != m_dragLineIdx && m_dragBodyId >= 0) {
      int step = (targetLineIdx > m_dragLineIdx) ? 1 : -1;
      int currLine = m_dragLineIdx;
      while (currLine != targetLineIdx) {
        int nextLine = currLine + step;
        int nextR = 0, nextC = 0;
        getRowCol(nextLine, nextR, nextC);
        int nextVIdx = m_doc->model().updateMoveIndex(
            m_dragVertexIdx, nextR, nextC, m_dragBodyId);
        if (nextVIdx < 0) {
          break;
        }
        currLine = nextLine;
        m_dragLineIdx = nextLine;
        m_dragVertexIdx = nextVIdx;
        m_doc->moveVertexAtRowCol(nextR, nextC, m_dragVertexIdx, newZ, moveType,
                                  &m_dragVertexIdx);
      }
    } else {
      int r = 0, c = 0;
      getRowCol(m_dragLineIdx, r, c);
      m_doc->moveVertexAtRowCol(r, c, m_dragVertexIdx, newZ, moveType,
                                &m_dragVertexIdx);
    }
    m_dragLastMouseZ = newZ;
    m_hoverLineIdx = m_dragLineIdx;
    m_hoverVertexIdx = m_dragVertexIdx;
    m_hoverBodyId = m_dragBodyId;

    int rStation = 0, cStation = 0;
    getRowCol(m_dragLineIdx, rStation, cStation);
    double wy = currentProfileCoordinate();
    double wxCoord = xw;
    if (m_profileOrient == ProfileOrientation::SouthNorth) {
      std::swap(wxCoord, wy);
    }
    emit cursorStationChanged(rStation, cStation, wxCoord, wy, newZ, 0.0);
    std::string bName;
    if (auto *b = m_doc->model().getBody(m_dragBodyId)) {
      bName = b->name();
    }
    m_doc->setActiveProfileCursor({
        true,
        rStation,
        cStation,
        wxCoord,
        wy,
        newZ,
        m_dragBodyId,
        bName,
        true
    });
    return;
  }

  if (m_dragOp == DragOperation::MoveBodyVertical) {
    double currentZ = screenToWorldZ(event->pos().y());
    double deltaZ = currentZ - m_dragLastMouseZ;
    if (std::abs(deltaZ) > 1.0) {
      m_doc->translateBodyVerticalOnProfile(m_dragBodyId, m_profileOrient,
                                            m_profileIndex, deltaZ);
      m_dragLastMouseZ = currentZ;
      int rStation = 0, cStation = 0;
      getRowCol(m_dragLineIdx, rStation, cStation);
      double wy = currentProfileCoordinate();
      double wxCoord = columnCoordinate(m_dragLineIdx);
      if (m_profileOrient == ProfileOrientation::SouthNorth) {
        std::swap(wxCoord, wy);
      }
      std::string bName;
      if (auto *b = m_doc->model().getBody(m_dragBodyId)) {
        bName = b->name();
      }
      m_doc->setActiveProfileCursor({
          true,
          rStation,
          cStation,
          wxCoord,
          wy,
          currentZ,
          m_dragBodyId,
          bName,
          true
      });
    }
    return;
  }

  if (m_dragOp == DragOperation::ExtendBodyLateral) {
    double xw = screenToWorldX(event->pos().x());
    double startCoord = (m_profileOrient == ProfileOrientation::EastWest)
                            ? m_doc->model().getX0()
                            : m_doc->model().getY0();
    double stepCoord = (m_profileOrient == ProfileOrientation::EastWest)
                            ? m_doc->model().getXSize()
                            : m_doc->model().getYSize();
    int nLines = profileColumnLineCount();
    int currentLine = std::clamp(
        static_cast<int>(std::round((xw - startCoord) / stepCoord)), 0,
        nLines - 1);

    if (currentLine >= 0 && currentLine != m_dragLineIdx) {
      int step = (currentLine > m_dragLineIdx) ? 1 : -1;
      while (m_dragLineIdx != currentLine) {
        int nextLine = m_dragLineIdx + step;
        int srcR = 0, srcC = 0;
        int tgtR = 0, tgtC = 0;
        getRowCol(m_dragLineIdx, srcR, srcC);
        getRowCol(nextLine, tgtR, tgtC);

        double currentMouseZ = screenToWorldZ(event->pos().y());
        double startMouseZ = screenToWorldZ(m_dragStartPos.y());
        int bIdx = m_doc->model().getBodyIndex(m_dragBodyId, srcR, srcC);
        double targetZ = std::numeric_limits<double>::quiet_NaN();
        if (bIdx >= 0) {
          double topZ = m_doc->model().getZ(srcR, srcC, bIdx);
          double botZ = m_doc->model().getZ(srcR, srcC, bIdx + 1);
          targetZ = (topZ + botZ) / 2.0 + (currentMouseZ - startMouseZ);
        }

        bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, tgtR, tgtC,
                                                m_dragBodyId, targetZ);
        if (!ok) {
          break;
        }
        m_dragLineIdx = nextLine;
      }
      int rStation = 0, cStation = 0;
      getRowCol(m_dragLineIdx, rStation, cStation);
      double wy = currentProfileCoordinate();
      double wxCoord = columnCoordinate(m_dragLineIdx);
      if (m_profileOrient == ProfileOrientation::SouthNorth) {
        std::swap(wxCoord, wy);
      }
      std::string bName;
      if (auto *b = m_doc->model().getBody(m_dragBodyId)) {
        bName = b->name();
      }
      m_doc->setActiveProfileCursor({
          true,
          rStation,
          cStation,
          wxCoord,
          wy,
          screenToWorldZ(event->pos().y()),
          m_dragBodyId,
          bName,
          true
      });
    }
    return;
  }

  int lineIdx = -1, vIdx = -1, bId = -1;
  CursorMode mode = performHitTest(event->pos(), lineIdx, vIdx, bId);
  m_currentCursorMode = mode;
  m_hoverLineIdx = lineIdx;
  m_hoverVertexIdx = vIdx;
  m_hoverBodyId = bId;
  updateCursorShape(mode);

  if (mode == CursorMode::OverVertex && lineIdx >= 0 && vIdx > 0) {
    int r = 0, c = 0;
    getRowCol(lineIdx, r, c);
    double curZ = m_doc->model().getZ(r, c, vIdx);
    bool isSnapped =
        (vIdx > 1 && m_doc->model().getZ(r, c, vIdx - 1) == curZ) ||
        (vIdx < static_cast<int>(m_doc->model().getCount(r, c)) - 1 &&
         m_doc->model().getZ(r, c, vIdx + 1) == curZ);
    if (isSnapped) {
      setToolTip(tr("Snapped Contact (Hold Ctrl+drag to unsnap / split)"));
    } else {
      setToolTip(QString());
    }
  } else {
    setToolTip(QString());
  }

  int rStation = 0, cStation = 0;
  if (lineIdx >= 0) {
    getRowCol(lineIdx, rStation, cStation);
  } else if (m_rectSectionPane.contains(event->pos())) {
    lineIdx = findClosestLineIndex(event->pos().x());
    if (lineIdx >= 0) {
      getRowCol(lineIdx, rStation, cStation);
    }
  }
  double wx = (lineIdx >= 0) ? columnCoordinate(lineIdx) : screenToWorldX(event->pos().x());
  double wz = screenToWorldZ(event->pos().y());
  if (vIdx >= 0 && lineIdx >= 0) {
    wz = m_doc->model().getZ(rStation, cStation, vIdx);
  }
  double wy = currentProfileCoordinate();
  if (m_profileOrient == ProfileOrientation::SouthNorth) {
    std::swap(wx, wy);
  }
  emit cursorStationChanged(rStation, cStation, wx, wy, wz, 0.0);

  if (m_rectSectionPane.contains(event->pos()) || lineIdx >= 0) {
    std::string bName;
    if (bId >= 0) {
      if (auto *b = m_doc->model().getBody(bId)) {
        bName = b->name();
      }
    }
    m_doc->setActiveProfileCursor({
        true,
        rStation,
        cStation,
        wx,
        wy,
        wz,
        bId,
        bName,
        false
    });
  } else {
    m_doc->clearActiveProfileCursor();
  }

  QWidget::mouseMoveEvent(event);
}

void ProfileView::mouseReleaseEvent(QMouseEvent *event) {
  if (m_dragOp != DragOperation::None) {
    bool wasModelDrag = (m_dragOp == DragOperation::MoveVertex ||
                         m_dragOp == DragOperation::MoveBodyVertical ||
                         m_dragOp == DragOperation::ExtendBodyLateral);
    m_dragOp = DragOperation::None;
    m_dragBodyId = -1;
    m_dragLineIdx = -1;
    m_dragVertexIdx = -1;
    if (wasModelDrag) {
      m_doc->computeAll();
    }
    auto cur = m_doc->activeProfileCursor();
    cur.isDragging = false;
    m_doc->setActiveProfileCursor(cur);
    update();
  }
  QWidget::mouseReleaseEvent(event);
}

void ProfileView::leaveEvent(QEvent *event) {
  if (m_doc) {
    auto cur = m_doc->activeProfileCursor();
    cur.isDragging = false;
    m_doc->setActiveProfileCursor(cur);
  }
  QWidget::leaveEvent(event);
}

void ProfileView::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->angleDelta().y() > 0) {
      zoomIn();
    } else if (event->angleDelta().y() < 0) {
      zoomOut();
    }
  } else {
    int lineUnderCursor = findClosestLineIndex(event->position().toPoint().x());
    if (lineUnderCursor >= 0) {
      m_hoverLineIdx = lineUnderCursor;
    }

    int delta = event->angleDelta().y();
    if (delta == 0) {
      delta = event->angleDelta().x();
    }
    if ((m_wheelDeltaAccumulator > 0 && delta < 0) || (m_wheelDeltaAccumulator < 0 && delta > 0)) {
      m_wheelDeltaAccumulator = 0;
    }
    m_wheelDeltaAccumulator += delta;
    while (m_wheelDeltaAccumulator >= 120) {
      moveProfile(1);
      m_wheelDeltaAccumulator -= 120;
    }
    while (m_wheelDeltaAccumulator <= -120) {
      moveProfile(-1);
      m_wheelDeltaAccumulator += 120;
    }
  }
  event->accept();
}

void ProfileView::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_N:
    if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
      if (m_hoverBodyId >= 0)
        m_doc->copyBodyBetweenProfiles(m_hoverBodyId, m_profileOrient,
                                       m_profileIndex, m_profileIndex + 1);
    } else {
      moveProfile(1);
    }
    break;
  case Qt::Key_P:
    if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
      if (m_hoverBodyId >= 0)
        m_doc->copyBodyBetweenProfiles(m_hoverBodyId, m_profileOrient,
                                       m_profileIndex, m_profileIndex - 1);
    } else {
      moveProfile(-1);
    }
    break;
  case Qt::Key_H:
    setProfileOrientation(ProfileOrientation::EastWest);
    break;
  case Qt::Key_V:
    setProfileOrientation(ProfileOrientation::SouthNorth);
    break;
  case Qt::Key_E:
    jumpToExtreme();
    break;
  case Qt::Key_I:
    jumpToMin();
    break;
  case Qt::Key_A:
    jumpToMax();
    break;
  default:
    QWidget::keyPressEvent(event);
    break;
  }
}

void ProfileView::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }

  int lineIdx = -1, vIdx = -1, bId = -1;
  CursorMode mode = performHitTest(event->pos(), lineIdx, vIdx, bId);

  if (mode == CursorMode::OverVertex) {
    emit requestBodyProperties(bId);
    return;
  }

  if (mode == CursorMode::OverBodyLine &&
      (event->modifiers() & Qt::ControlModifier)) {
    int r = 0, c = 0;
    getRowCol(lineIdx, r, c);
    m_doc->removeBodyAtRowCol(r, c, vIdx);
    return;
  }

  if (m_rectSectionPane.contains(event->pos())) {
    if (lineIdx < 0) {
      lineIdx = findClosestLineIndex(event->pos().x());
    }
    if (lineIdx >= 0) {
      double targetZ = screenToWorldZ(event->pos().y());
      int row = 0, col = 0;
      getRowCol(lineIdx, row, col);
      double zMin = m_doc->model().getZMin();
      double zSurface = m_doc->model().getZ(row, col, 0);
      double defThickness = m_doc->model().getDefaultThickness();
      double halfT = defThickness / 2.0;
      if (halfT <= 0.0 || halfT >= (zSurface - zMin) / 2.0) {
        halfT = (zSurface - zMin) * 0.1;
      }
      targetZ = std::clamp(targetZ, zMin + halfT, zSurface - halfT);

      if (event->modifiers() & Qt::ShiftModifier) {
        int existingId = (m_hoverBodyId >= 0) ? m_hoverBodyId : -1;
        if (existingId < 0 && !m_doc->model().getBodies().empty()) {
          existingId = m_doc->model().getBodies().front()->id();
        }
        m_doc->insertBodyAtRowCol(row, col, targetZ, -1.0, existingId);
      } else {
        m_doc->insertBodyAtRowCol(row, col, targetZ);
      }
      update();
      return;
    }
  }

  QWidget::mouseDoubleClickEvent(event);
}

void ProfileView::contextMenuEvent(QContextMenuEvent *event) {
  int lineIdx = -1, vIdx = -1, bId = -1;
  CursorMode mode = performHitTest(event->pos(), lineIdx, vIdx, bId);

  if (mode == CursorMode::OverVertex) {
    showVertexContextMenu(event->globalPos(), lineIdx, vIdx);
  } else if (mode == CursorMode::OverBody || mode == CursorMode::OverBodyLine) {
    showBodyContextMenu(event->globalPos(), bId, lineIdx, vIdx);
  } else if (mode == CursorMode::OverGridLine) {
    double targetZ = screenToWorldZ(event->pos().y());
    showGridLineContextMenu(event->globalPos(), lineIdx, targetZ);
  } else {
    if (m_rectSectionPane.contains(event->pos())) {
      int closestLine = findClosestLineIndex(event->pos().x());
      if (closestLine >= 0) {
        double targetZ = screenToWorldZ(event->pos().y());
        showGridLineContextMenu(event->globalPos(), closestLine, targetZ);
        return;
      }
    }
    showBackgroundContextMenu(event->globalPos());
  }
}

void ProfileView::showBackgroundContextMenu(const QPoint &globalPos) {
  QMenu menu(this);
  menu.addAction(tr("Next\tN"), [this]() { moveProfile(1); });
  menu.addAction(tr("Previous\tP"), [this]() { moveProfile(-1); });
  menu.addSeparator();
  menu.addAction(tr("E-W Profile\tH"), [this]() {
    setProfileOrientation(ProfileOrientation::EastWest);
  });
  menu.addAction(tr("S-N Profile\tV"), [this]() {
    setProfileOrientation(ProfileOrientation::SouthNorth);
  });
  menu.addSeparator();
  auto *actPrev = menu.addAction(tr("Show Previous"), [this]() {
    setShowPreviousProfile(!m_showPreviousProfile);
  });
  actPrev->setCheckable(true);
  actPrev->setChecked(m_showPreviousProfile);

  auto *actNext = menu.addAction(
      tr("Show Next"), [this]() { setShowNextProfile(!m_showNextProfile); });
  actNext->setCheckable(true);
  actNext->setChecked(m_showNextProfile);
  menu.addSeparator();
  menu.addAction(tr("Jump to Extreme\tE"), [this]() { jumpToExtreme(); });
  menu.addAction(tr("Jump to Min\tI"), [this]() { jumpToMin(); });
  menu.addAction(tr("Jump to Max\tA"), [this]() { jumpToMax(); });
  menu.exec(globalPos);
}

void ProfileView::showGridLineContextMenu(const QPoint &globalPos, int lineIdx,
                                          double targetZ) {
  QPoint localPos = mapFromGlobal(globalPos);
  if (m_rectSectionPane.contains(localPos)) {
    targetZ = screenToWorldZ(localPos.y());
  }
  int row = 0, col = 0;
  getRowCol(lineIdx, row, col);
  double zMin = m_doc->model().getZMin();
  double zSurface = m_doc->model().getZ(row, col, 0);
  double defThickness = m_doc->model().getDefaultThickness();
  double halfT = defThickness / 2.0;
  if (halfT <= 0.0 || halfT >= (zSurface - zMin) / 2.0) {
    halfT = (zSurface - zMin) * 0.1;
  }
  targetZ = std::clamp(targetZ, zMin + halfT, zSurface - halfT);

  QMenu menu(this);
  menu.addAction(tr("Insert New Body"), [this, row, col, targetZ]() {
    m_doc->insertBodyAtRowCol(row, col, targetZ);
  });

  if (!m_doc->model().getBodies().empty()) {
    auto *menuExist = menu.addMenu(tr("Insert Existing Body"));
    for (const auto &b : m_doc->model().getBodies()) {
      int bId = b->id();
      QString bName = QString::fromStdString(b->name());
      menuExist->addAction(QString("Body %1 (%2)").arg(bId).arg(bName),
                           [this, row, col, targetZ, bId]() {
                             m_doc->insertBodyAtRowCol(row, col, targetZ, -1.0,
                                                       bId);
                           });
    }
  }

  menu.addSeparator();
  ProfileOrientation perpOrient =
      (m_profileOrient == ProfileOrientation::EastWest)
          ? ProfileOrientation::SouthNorth
          : ProfileOrientation::EastWest;
  QString perpStr =
      (perpOrient == ProfileOrientation::EastWest) ? "E-W" : "S-N";
  menu.addAction(tr("View on Perpendicular Profile (%1) at Line %2")
                     .arg(perpStr)
                     .arg(lineIdx + 1),
                 [this, perpOrient, lineIdx]() {
                   emit requestOpenPerpendicularProfile(perpOrient, lineIdx);
                 });

  menu.addSeparator();
  menu.addAction(tr("Body Creation Properties..."), []() {});
  menu.exec(globalPos);
}

void ProfileView::showBodyContextMenu(const QPoint &globalPos, int bodyId, int,
                                      int vertexIdx) {
  QMenu menu(this);
  menu.addAction(tr("Properties..."),
                 [this, bodyId]() { emit requestBodyProperties(bodyId); });
  auto *b = m_doc->model().getBody(bodyId);
  auto *actFill = menu.addAction(tr("Fill"), [this, b]() {
    if (b) {
      const_cast<mod3d::Body *>(b)->set_filled(!b->is_filled());
      m_doc->setModified(true);
      emit m_doc->modelGeometryChanged();
    }
  });
  if (b) {
    actFill->setCheckable(true);
    actFill->setChecked(b->is_filled());
  }
  menu.addSeparator();
  menu.addAction(
      tr("Copy to Previous Profile\tCtrl+Shift+P"), [this, bodyId]() {
        m_doc->copyBodyBetweenProfiles(bodyId, m_profileOrient, m_profileIndex,
                                       m_profileIndex - 1);
      });
  menu.addAction(tr("Copy to Next Profile\tCtrl+Shift+N"), [this, bodyId]() {
    m_doc->copyBodyBetweenProfiles(bodyId, m_profileOrient, m_profileIndex,
                                   m_profileIndex + 1);
  });
  menu.addSeparator();
  menu.addAction(tr("Remove"), [this, lineIdx = m_hoverLineIdx, vertexIdx]() {
    int r = 0, c = 0;
    getRowCol(lineIdx, r, c);
    m_doc->removeBodyAtRowCol(r, c, vertexIdx);
  });
  menu.addAction(tr("Remove from Profile"), [this, bodyId]() {
    m_doc->removeBodyFromProfile(bodyId, m_profileOrient, m_profileIndex);
  });
  menu.addSeparator();
  menu.addAction(tr("Invert Density"), [this, bodyId]() {
    auto *b = m_doc->model().getBody(bodyId);
    if (b) {
      std::vector<mod3d::Facet3Pt> facets;
      m_doc->model().getFacetsComputation(facets);
      m_doc->observation().fit_density(b, facets, m_doc->activeField());
      m_doc->computeAll();
    }
  });
  menu.exec(globalPos);
}

void ProfileView::showVertexContextMenu(const QPoint &globalPos, int lineIdx,
                                        int vertexIdx) {
  QMenu menu(this);
  int row = 0, col = 0;
  getRowCol(lineIdx, row, col);
  double curZ = m_doc->model().getZ(row, col, vertexIdx);
  bool isSnapped =
      (vertexIdx > 1 && m_doc->model().getZ(row, col, vertexIdx - 1) == curZ) ||
      (vertexIdx < static_cast<int>(m_doc->model().getCount(row, col)) - 1 &&
       m_doc->model().getZ(row, col, vertexIdx + 1) == curZ);
  if (isSnapped) {
    menu.addAction(tr("Unsnap Contact (Split)\tCtrl+Drag"),
                   [this, row, col, vertexIdx]() {
                     m_doc->unsnapVertexAtRowCol(row, col, vertexIdx);
                   });
    menu.addSeparator();
  }
  menu.addAction(tr("Fit"), [this, vertexIdx, row, col]() {
    m_doc->observation().fit_vertex(m_doc->model(), row, col, vertexIdx,
                                    m_doc->activeField());
    m_doc->computeAll();
  });
  menu.addAction(tr("1D Fit Properties..."), [this, lineIdx, vertexIdx]() {
    emit requestFit1DDialog(lineIdx, vertexIdx);
  });
  menu.exec(globalPos);
}

} // namespace mod3d_gui
