#pragma once

#include "mod3d/Point3D.h"

namespace mod3d {

class Body;

/**
 * @brief Stratigraphic contact point in a vertical geological column.
 * 
 * Each vertical column in the Mod3D grid contains an ordered sequence of
 * ColumnPoints from surface relief down to the base depth (hell), where pairs
 * of points define the upper and lower boundaries of geological bodies.
 */
class ColumnPoint {
public:
    ColumnPoint();
    explicit ColumnPoint(double z);
    ColumnPoint(int bodyId, double z, Body *body = nullptr);
    ColumnPoint(const Point3D &pt, int bodyId = -1, Body *body = nullptr);

    bool isModified() const { return m_modified; }
    void setModified(bool modified = true) { m_modified = modified; }

    int bodyId() const { return m_bodyId; }
    void setBodyId(int id) { m_bodyId = id; }

    Body *body() const { return m_body; }
    void setBody(Body *b) { m_body = b; }

    double z() const { return m_z; }
    void setZ(double zVal) {
        m_z = zVal;
        m_pt.z = zVal;
        m_modified = true;
    }

    const Point3D &point() const { return m_pt; }
    Point3D &point() { return m_pt; }
    void setPoint(const Point3D &pt) {
        m_pt = pt;
        m_z = pt.z;
        m_modified = true;
    }

private:
    bool m_modified = true;
    int m_bodyId = -1;
    Body *m_body = nullptr;
    Point3D m_pt{0.0, 0.0, 0.0};
    double m_z = 0.0;
};

} // namespace mod3d
