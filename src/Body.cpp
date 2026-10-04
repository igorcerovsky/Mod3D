#include "mod3d/Body.h"
#include "mod3d/PotField.h"
#include <ostream>
#include <utility>

namespace mod3d {

Body::Body() = default;

Body::Body(int id, std::string name, double density)
    : id_(id), index_(0), name_(std::move(name)), density_(density)
{
}

void Body::swap(Body &other) noexcept {
    using std::swap;
    swap(id_, other.id_);
    swap(index_, other.index_);
    swap(name_, other.name_);
    swap(description_, other.description_);
    swap(visible_, other.visible_);
    swap(locked_, other.locked_);
    swap(filled_, other.filled_);
    swap(active_, other.active_);
    swap(density_, other.density_);
    swap(density_grad_, other.density_grad_);
    swap(density_org_, other.density_org_);
    swap(susceptibility_, other.susceptibility_);
    swap(mag_vector_, other.mag_vector_);
    swap(mag_rem_, other.mag_rem_);
    swap(color_, other.color_);
    swap(transparency_, other.transparency_);
    swap(transparent_, other.transparent_);
}

void Body::compute_magnetization_vector(const Point3D &vIndFld) {
    mag_vector_ = MagnetizationVector(susceptibility_, vIndFld.x, vIndFld.y, vIndFld.z,
                                      mag_rem_.x, mag_rem_.y, mag_rem_.z);
}

bool Body::operator==(const Body &other) const noexcept {
    return id_ == other.id_ &&
           index_ == other.index_ &&
           name_ == other.name_ &&
           description_ == other.description_ &&
           visible_ == other.visible_ &&
           locked_ == other.locked_ &&
           filled_ == other.filled_ &&
           active_ == other.active_ &&
           density_ == other.density_ &&
           density_grad_ == other.density_grad_ &&
           density_org_ == other.density_org_ &&
           susceptibility_ == other.susceptibility_ &&
           mag_vector_ == other.mag_vector_ &&
           mag_rem_ == other.mag_rem_ &&
           color_ == other.color_ &&
           transparency_ == other.transparency_ &&
           transparent_ == other.transparent_;
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
       << ", density=" << b.density_
       << ", susc=" << b.susceptibility_
       << ", active=" << (b.active_ ? "true" : "false")
       << ")";
    return os;
}

} // namespace mod3d
