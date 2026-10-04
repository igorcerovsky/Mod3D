#include "mod3d/ColumnPoint.h"
#include <ostream>

namespace mod3d {

std::ostream &operator<<(std::ostream &os, const ColumnPoint &cp) {
    os << "ColumnPoint(pt=(" << cp.pt_.x << ", " << cp.pt_.y << ", " << cp.pt_.z
       << "), z=" << cp.z_ << ", bodyId=" << cp.body_id_ << ", modified=" << (cp.modified_ ? "true" : "false") << ")";
    return os;
}

} // namespace mod3d
