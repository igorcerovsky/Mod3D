#include "mod3d/ColumnPoint.h"
#include <ostream>

namespace mod3d {

std::ostream &operator<<(std::ostream &os, const ColumnPoint &cp) {
    os << "ColumnPoint(pt=(" << cp.m_pt.x << ", " << cp.m_pt.y << ", " << cp.m_pt.z
       << "), z=" << cp.m_z << ", bodyId=" << cp.m_bodyId << ", modified=" << (cp.m_modified ? "true" : "false") << ")";
    return os;
}

} // namespace mod3d
