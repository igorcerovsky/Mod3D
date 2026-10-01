#include "mod3d/ColumnPoint.h"

namespace mod3d {

ColumnPoint::ColumnPoint()
    : m_modified(true), m_bodyId(-1), m_body(nullptr), m_pt(0.0, 0.0, 0.0), m_z(0.0)
{
}

ColumnPoint::ColumnPoint(double z)
    : m_modified(true), m_bodyId(-1), m_body(nullptr), m_pt(0.0, 0.0, z), m_z(z)
{
}

ColumnPoint::ColumnPoint(int bodyId, double z, Body *body)
    : m_modified(true), m_bodyId(bodyId), m_body(body), m_pt(0.0, 0.0, z), m_z(z)
{
}

ColumnPoint::ColumnPoint(const Point3D &pt, int bodyId, Body *body)
    : m_modified(true), m_bodyId(bodyId), m_body(body), m_pt(pt), m_z(pt.z)
{
}

} // namespace mod3d
