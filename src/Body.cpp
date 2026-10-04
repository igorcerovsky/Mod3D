#include "mod3d/Body.h"
#include "mod3d/PotField.h"
#include <ostream>
#include <utility>

namespace mod3d {

Body::Body() = default;

Body::Body(int id, std::string name, double density)
    : m_nID(id), m_nIndex(0), m_strName(std::move(name)), m_dDensity(density)
{
}

void Body::swap(Body &other) noexcept {
    using std::swap;
    swap(m_nID, other.m_nID);
    swap(m_nIndex, other.m_nIndex);
    swap(m_strName, other.m_strName);
    swap(m_strDescription, other.m_strDescription);
    swap(m_bShow, other.m_bShow);
    swap(m_bLocked, other.m_bLocked);
    swap(m_bFill, other.m_bFill);
    swap(m_bActive, other.m_bActive);
    swap(m_dDensity, other.m_dDensity);
    swap(m_vDensGrad, other.m_vDensGrad);
    swap(m_vDensOrg, other.m_vDensOrg);
    swap(m_dSusc, other.m_dSusc);
    swap(m_vMagVector, other.m_vMagVector);
    swap(m_vMagRem, other.m_vMagRem);
    swap(m_color, other.m_color);
    swap(m_fAlpha, other.m_fAlpha);
    swap(m_bTransparent, other.m_bTransparent);
}

void Body::compute_magnetization_vector(const Point3D &vIndFld) {
    m_vMagVector = MagnetizationVector(m_dSusc, vIndFld.x, vIndFld.y, vIndFld.z,
                                       m_vMagRem.x, m_vMagRem.y, m_vMagRem.z);
}

bool Body::operator==(const Body &other) const noexcept {
    return m_nID == other.m_nID &&
           m_nIndex == other.m_nIndex &&
           m_strName == other.m_strName &&
           m_strDescription == other.m_strDescription &&
           m_bShow == other.m_bShow &&
           m_bLocked == other.m_bLocked &&
           m_bFill == other.m_bFill &&
           m_bActive == other.m_bActive &&
           m_dDensity == other.m_dDensity &&
           m_vDensGrad == other.m_vDensGrad &&
           m_vDensOrg == other.m_vDensOrg &&
           m_dSusc == other.m_dSusc &&
           m_vMagVector == other.m_vMagVector &&
           m_vMagRem == other.m_vMagRem &&
           m_color == other.m_color &&
           m_fAlpha == other.m_fAlpha &&
           m_bTransparent == other.m_bTransparent;
}

std::ostream &operator<<(std::ostream &os, const BodyColor &col) {
    os << "rgba(" << static_cast<int>(col.r) << ", "
       << static_cast<int>(col.g) << ", "
       << static_cast<int>(col.b) << ", "
       << static_cast<int>(col.a) << ")";
    return os;
}

std::ostream &operator<<(std::ostream &os, const Body &b) {
    os << "Body(id=" << b.m_nID
       << ", name=\"" << b.m_strName << "\""
       << ", density=" << b.m_dDensity
       << ", susc=" << b.m_dSusc
       << ", active=" << (b.m_bActive ? "true" : "false")
       << ")";
    return os;
}

} // namespace mod3d
