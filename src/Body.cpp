#include "mod3d/Body.h"
#include "mod3d/PotField.h"

namespace mod3d {

Body::Body()
    : m_nID(0), m_nIndex(0), m_dDensity(2700.0), m_dSusc(0.01)
{
}

Body::Body(int id, std::string name, double density)
    : m_nID(id), m_nIndex(0), m_strName(std::move(name)), m_dDensity(density), m_dSusc(0.01)
{
}

void Body::ComputeMagnetizationVector(const Point3D& vIndFld)
{
    m_vMagVector = MagnetizationVector(m_dSusc, vIndFld.x, vIndFld.y, vIndFld.z,
                                       m_vMagRem.x, m_vMagRem.y, m_vMagRem.z);
}

} // namespace mod3d
