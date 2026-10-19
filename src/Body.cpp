#include "mod3d/Body.h"
#include "mod3d/Magnetics.h"
#include <ostream>
#include <utility>

namespace mod3d {

Body::Body() = default;

Body::Body(int id, std::string name, double density)
    : id_(id), name_(std::move(name))
{
    density_.value = units::KgPerM3(density);
    if (name_.empty()) {
        name_ = "Body " + std::to_string(id);
    }
}

void BodyMagnetics::compute_vector(double inc_deg, double dec_deg, double intensity_nt) {
    Point3D mv = mod3d::magnetization_vector(susceptibility, inc_deg, dec_deg, intensity_nt,
                                            remanence.mx.value(), remanence.my.value(), remanence.mz.value());
    total_vector = MagneticVector(mv);
}

void BodyMagnetics::compute_vector(const Point3D &vIndFld) {
    compute_vector(vIndFld.x, vIndFld.y, vIndFld.z);
}

void BodyMagnetics::compute_vector(const MagneticVector &vIndFld) {
    Point3D mv = mod3d::magnetization_vector(susceptibility, vIndFld.mx.value(), vIndFld.my.value(), vIndFld.mz.value(),
                                            remanence.mx.value(), remanence.my.value(), remanence.mz.value());
    total_vector = MagneticVector(mv);
}

void Body::swap(Body &other) noexcept {
    using std::swap;
    swap(id_, other.id_);
    swap(name_, other.name_);
    swap(description_, other.description_);
    swap(active_, other.active_);
    swap(locked_, other.locked_);
    swap(density_, other.density_);
    swap(magnetics_, other.magnetics_);
    swap(drawing_, other.drawing_);
}

void Body::compute_magnetization_vector(double inc_deg, double dec_deg, double intensity_nt) {
    magnetics_.compute_vector(inc_deg, dec_deg, intensity_nt);
}

void Body::compute_magnetization_vector(const Point3D &vIndFld) {
    magnetics_.compute_vector(vIndFld);
}

void Body::compute_magnetization_vector(const MagneticVector &vIndFld) {
    magnetics_.compute_vector(vIndFld);
}

bool Body::operator==(const Body &other) const noexcept {
    return id_ == other.id_ &&
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
