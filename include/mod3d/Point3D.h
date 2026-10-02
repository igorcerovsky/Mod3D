#pragma once

#include "pfld/point.hpp"
#include <vector>

namespace mod3d {

/**
 * @brief 3D Point and Vector alias to modern header-only pfld::Point3D<double>.
 * Trivially copyable, standard layout, 24 bytes, and constexpr-ready.
 */
using Point3D = pfld::Point3D<double>;
using Vector3D = Point3D;
using Pt3DArray = std::vector<Point3D>;
using Pt3DArray2D = std::vector<Pt3DArray>;

} // namespace mod3d

