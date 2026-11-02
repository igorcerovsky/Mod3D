#pragma once

#include "pfld/point.hpp"

namespace inv {

/**
 * @brief Re-use high-performance, unit-aware 3D point and vector primitive from pfld (DRY).
 * Provides standard layout, trivial copyability, constexpr vector math, and zero overhead.
 */
template <typename T = double>
using Point3D = pfld::Point3D<T>;

using Point3Dd = pfld::Point3Dd;
using Point3Df = pfld::Point3Df;
using point = pfld::point;

} // namespace inv
