#pragma once

#include "mod3d/Point3D.h"
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
    constexpr explicit ColumnPoint(double zVal) noexcept
        : pt_(0.0, 0.0, zVal), z_(zVal), body_(nullptr), body_id_(-1), modified_(true) {}
    constexpr ColumnPoint(int bodyIdVal, double zVal, Body *bodyPtr = nullptr) noexcept
        : pt_(0.0, 0.0, zVal), z_(zVal), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}
    constexpr ColumnPoint(const Point3D &ptVal, int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : pt_(ptVal), z_(ptVal.z), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}
    constexpr ColumnPoint(double xVal, double yVal, double zVal, int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : pt_(xVal, yVal, zVal), z_(zVal), body_(bodyPtr), body_id_(bodyIdVal), modified_(true) {}

    constexpr ColumnPoint(const ColumnPoint &) noexcept = default;
    constexpr ColumnPoint &operator=(const ColumnPoint &) noexcept = default;
    constexpr ColumnPoint(ColumnPoint &&) noexcept = default;
    constexpr ColumnPoint &operator=(ColumnPoint &&) noexcept = default;
    ~ColumnPoint() = default;

    // ========================================================================
    // Modern C++20 API (STL snake_case convention, constexpr, noexcept, [[nodiscard]])
    // ========================================================================

    // Coordinate accessors
    [[nodiscard]] constexpr double z() const noexcept { return z_; }
    [[nodiscard]] constexpr double x() const noexcept { return pt_.x; }
    [[nodiscard]] constexpr double y() const noexcept { return pt_.y; }

    constexpr void set_z(double zVal) noexcept {
        z_ = zVal;
        pt_.z = zVal;
        modified_ = true;
    }

    constexpr void set_x(double xVal) noexcept {
        pt_.x = xVal;
        modified_ = true;
    }
    constexpr void set_y(double yVal) noexcept {
        pt_.y = yVal;
        modified_ = true;
    }

    constexpr void set_coords(double xVal, double yVal, double zVal) noexcept {
        pt_.x = xVal;
        pt_.y = yVal;
        pt_.z = zVal;
        z_ = zVal;
        modified_ = true;
    }

    [[nodiscard]] constexpr const Point3D &point() const noexcept { return pt_; }
    [[nodiscard]] constexpr Point3D &point() noexcept { return pt_; }
    constexpr void set_point(const Point3D &ptVal) noexcept {
        pt_ = ptVal;
        z_ = ptVal.z;
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
    [[nodiscard]] constexpr bool is_above(const ColumnPoint &other) const noexcept { return z_ > other.z_; }
    [[nodiscard]] constexpr bool is_below(const ColumnPoint &other) const noexcept { return z_ < other.z_; }

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

    // ========================================================================
    // Legacy API (CamelCase compatibility wrappers for MFC Mod3D codebase)
    // ========================================================================
    [[nodiscard]] constexpr bool isModified() const noexcept { return is_modified(); }
    constexpr void setModified(bool modified = true) noexcept { set_modified(modified); }

    [[nodiscard]] constexpr int bodyId() const noexcept { return body_id(); }
    constexpr void setBodyId(int id) noexcept { set_body_id(id); }

    constexpr void setBody(Body *b) noexcept { set_body(b); }
    constexpr void setZ(double zVal) noexcept { set_z(zVal); }
    constexpr void setPoint(const Point3D &ptVal) noexcept { set_point(ptVal); }

private:
    Point3D pt_{0.0, 0.0, 0.0};
    double z_{0.0};
    Body *body_{nullptr};
    int body_id_{-1};
    bool modified_{true};
};

static_assert(std::is_standard_layout_v<ColumnPoint>, "ColumnPoint must be standard layout");
static_assert(std::is_trivially_copyable_v<ColumnPoint>, "ColumnPoint must be trivially copyable");

} // namespace mod3d
