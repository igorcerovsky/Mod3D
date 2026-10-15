#include "mod3d/Body.h"
#include "mod3d/Magnetics.h"
#include <ostream>
#include <utility>

namespace mod3d {

Body::Body() = default;

Body::Body(int id, std::string name, double density)
    : id_(id), index_(0), name_(std::move(name))
{
    density_.value = density;
    if (name_.empty()) {
        name_ = "Body " + std::to_string(id);
    }
}

void BodyMagnetics::compute_vector(const Point3D &vIndFld) {
    total_vector = mod3d::magnetization_vector(susceptibility, vIndFld.x, vIndFld.y, vIndFld.z,
                                              remanence.x, remanence.y, remanence.z);
}

void Body::swap(Body &other) noexcept {
    using std::swap;
    swap(id_, other.id_);
    swap(index_, other.index_);
    swap(name_, other.name_);
    swap(description_, other.description_);
    swap(active_, other.active_);
    swap(locked_, other.locked_);
    swap(density_, other.density_);
    swap(magnetics_, other.magnetics_);
    swap(drawing_, other.drawing_);
}

void Body::compute_magnetization_vector(const Point3D &vIndFld) {
    magnetics_.compute_vector(vIndFld);
}

bool Body::operator==(const Body &other) const noexcept {
    return id_ == other.id_ &&
           index_ == other.index_ &&
           name_ == other.name_ &&
           description_ == other.description_ &&
           active_ == other.active_ &&
           locked_ == other.locked_ &&
           density_ == other.density_ &&
           magnetics_ == other.magnetics_ &&
           drawing_ == other.drawing_;
}

std::ostream &operator<<(std::ostream &os, const BodyColor &col) {
    os << "rgba(" << static_cast<int>(col.r) << ", "
       << static_cast<int>(col.g) << ", "
       << static_cast<int>(col.b) << ", "
       << static_cast<int>(col.a) << ")";
    return os;
}

std::ostream &operator<<(std::ostream &os, const Body &b) {
    os << "Body(id=" << b.id_
       << ", name=\"" << b.name_ << "\""
       << ", density=" << b.density_.value
       << ", susc=" << b.magnetics_.susceptibility
       << ", active=" << (b.active_ ? "true" : "false")
       << ", visible=" << (b.drawing_.visible ? "true" : "false")
       << ")";
    return os;
}

} // namespace mod3d
