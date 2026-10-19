#pragma once

#include "mod3d/Point3D.h"
#include "mod3d/Units.h"
#include <iosfwd>
#include <type_traits>

namespace mod3d {

class Body;

/**
 * @brief Stratigraphic contact point in a vertical geological column.
 * 
 * Each vertical column in the Mod3D grid contains an ordered sequence of
 * ColumnPoints from surface relief down to the base depth (hell), where pairs
 * of points define the upper and lower boundaries of geological bodies.
 * 
 * Trivially copyable, standard layout, and constexpr-ready.
 */
class ColumnPoint {
public:
    // ========================================================================
    // Lifecycle & Rule of 5 (constexpr)
    // ========================================================================
    constexpr ColumnPoint() noexcept = default;

    // Unit-aware constructors
    template <units::UnitType Uz>
        requires units::SameDimension<Uz, units::tag::Meters>
    constexpr explicit ColumnPoint(const units::Quantity<Uz>& zVal) noexcept
        : pt_(units::Meters(0.0), units::Meters(0.0), zVal), body_(nullptr), body_id_(-1), modified_(true) {}

    template <units::UnitType Uz>
        requires units::SameDimension<Uz, units::tag::Meters>
    constexpr ColumnPoint(int bodyIdVal, const units::Quantity<Uz>& zVal, Body *bodyPtr = nullptr) noexcept
        : pt_(units::Meters(0.0), units::Meters(0.0), zVal), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}

    template <units::UnitType Ux, units::UnitType Uy, units::UnitType Uz>
        requires (units::SameDimension<Ux, units::tag::Meters> &&
                  units::SameDimension<Uy, units::tag::Meters> &&
                  units::SameDimension<Uz, units::tag::Meters>)
    constexpr ColumnPoint(const units::Quantity<Ux>& xVal,
                          const units::Quantity<Uy>& yVal,
                          const units::Quantity<Uz>& zVal,
                          int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : pt_(xVal, yVal, zVal), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}

    constexpr explicit ColumnPoint(const Position3D &posVal, int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : pt_(posVal), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}

    // Convenience constructors accepting raw numeric values (interpreted in meters)
    constexpr explicit ColumnPoint(double zVal) noexcept
        : pt_(0.0, 0.0, zVal), body_(nullptr), body_id_(-1), modified_(true) {}
    constexpr ColumnPoint(int bodyIdVal, double zVal, Body *bodyPtr = nullptr) noexcept
        : pt_(0.0, 0.0, zVal), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}
    constexpr ColumnPoint(const Point3D &ptVal, int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : pt_(Position3D(ptVal)), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}
    constexpr ColumnPoint(double xVal, double yVal, double zVal, int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : pt_(xVal, yVal, zVal), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}

    constexpr ColumnPoint(const ColumnPoint &) noexcept = default;
    constexpr ColumnPoint &operator=(const ColumnPoint &) noexcept = default;
    constexpr ColumnPoint(ColumnPoint &&) noexcept = default;
    constexpr ColumnPoint &operator=(ColumnPoint &&) noexcept = default;
    ~ColumnPoint() = default;

    // ========================================================================
    // Modern C++20 API (STL snake_case convention, constexpr, noexcept, [[nodiscard]])
    // ========================================================================

    // Coordinate accessors (physical quantities in meters)
    [[nodiscard]] constexpr units::Meters z() const noexcept { return pt_.z; }
    [[nodiscard]] constexpr units::Meters x() const noexcept { return pt_.x; }
    [[nodiscard]] constexpr units::Meters y() const noexcept { return pt_.y; }

    [[nodiscard]] constexpr double z_val() const noexcept { return pt_.z.value(); }
    [[nodiscard]] constexpr double x_val() const noexcept { return pt_.x.value(); }
    [[nodiscard]] constexpr double y_val() const noexcept { return pt_.y.value(); }

    template <units::UnitType U>
        requires units::SameDimension<U, units::tag::Meters>
    constexpr void set_z(const units::Quantity<U>& zVal) noexcept {
        pt_.z = zVal.template as<units::tag::Meters>();
        modified_ = true;
    }
    constexpr void set_z(double zVal) noexcept {
        pt_.z = units::Meters(zVal);
        modified_ = true;
    }

    template <units::UnitType U>
        requires units::SameDimension<U, units::tag::Meters>
    constexpr void set_x(const units::Quantity<U>& xVal) noexcept {
        pt_.x = xVal.template as<units::tag::Meters>();
        modified_ = true;
    }
    constexpr void set_x(double xVal) noexcept {
        pt_.x = units::Meters(xVal);
        modified_ = true;
    }

    template <units::UnitType U>
        requires units::SameDimension<U, units::tag::Meters>
    constexpr void set_y(const units::Quantity<U>& yVal) noexcept {
        pt_.y = yVal.template as<units::tag::Meters>();
        modified_ = true;
    }
    constexpr void set_y(double yVal) noexcept {
        pt_.y = units::Meters(yVal);
        modified_ = true;
    }

    template <units::UnitType Ux, units::UnitType Uy, units::UnitType Uz>
        requires (units::SameDimension<Ux, units::tag::Meters> &&
                  units::SameDimension<Uy, units::tag::Meters> &&
                  units::SameDimension<Uz, units::tag::Meters>)
    constexpr void set_coords(const units::Quantity<Ux>& xVal,
                              const units::Quantity<Uy>& yVal,
                              const units::Quantity<Uz>& zVal) noexcept {
        pt_.x = xVal.template as<units::tag::Meters>();
        pt_.y = yVal.template as<units::tag::Meters>();
        pt_.z = zVal.template as<units::tag::Meters>();
        modified_ = true;
    }
    constexpr void set_coords(double xVal, double yVal, double zVal) noexcept {
        pt_.x = units::Meters(xVal);
        pt_.y = units::Meters(yVal);
        pt_.z = units::Meters(zVal);
        modified_ = true;
    }

    [[nodiscard]] constexpr const Position3D &position() const noexcept { return pt_; }
    [[nodiscard]] constexpr Position3D &position() noexcept { return pt_; }
    constexpr void set_position(const Position3D &posVal) noexcept {
        pt_ = posVal;
        modified_ = true;
    }

    [[nodiscard]] constexpr Point3D point() const noexcept { return pt_.to_point3d(); }
    constexpr void set_point(const Point3D &ptVal) noexcept {
        pt_ = Position3D(ptVal);
        modified_ = true;
    }

    // Body association
    [[nodiscard]] constexpr int body_id() const noexcept { return body_id_; }
    constexpr void set_body_id(int id) noexcept { body_id_ = id; }

    [[nodiscard]] constexpr const Body *body() const noexcept { return body_; }
    [[nodiscard]] constexpr Body *body() noexcept { return body_; }
    constexpr void set_body(Body *b) noexcept { body_ = b; }
    [[nodiscard]] constexpr bool has_body() const noexcept { return body_ != nullptr || body_id_ >= 0; }

    // Modification state
    [[nodiscard]] constexpr bool is_modified() const noexcept { return modified_; }
    constexpr void set_modified(bool modified = true) noexcept { modified_ = modified; }

    // Relative vertical position queries (higher elevation Z is above)
    [[nodiscard]] constexpr bool is_above(const ColumnPoint &other) const noexcept { return pt_.z > other.pt_.z; }
    [[nodiscard]] constexpr bool is_below(const ColumnPoint &other) const noexcept { return pt_.z < other.pt_.z; }

    // Sorting predicates for vertical stratigraphic columns
    struct DepthLess {
        constexpr bool operator()(const ColumnPoint &a, const ColumnPoint &b) const noexcept {
            return a.z() < b.z();
        }
    };
    struct DepthGreater {
        constexpr bool operator()(const ColumnPoint &a, const ColumnPoint &b) const noexcept {
            return a.z() > b.z();
        }
    };

    // Comparisons
    [[nodiscard]] constexpr bool operator==(const ColumnPoint &other) const noexcept = default;

    friend std::ostream &operator<<(std::ostream &os, const ColumnPoint &cp);


private:
    Position3D pt_{};
    Body *body_{nullptr};
    int body_id_{-1};
    bool modified_{true};
};

static_assert(std::is_standard_layout_v<ColumnPoint>, "ColumnPoint must be standard layout");
static_assert(std::is_trivially_copyable_v<ColumnPoint>, "ColumnPoint must be trivially copyable");

} // namespace mod3d
