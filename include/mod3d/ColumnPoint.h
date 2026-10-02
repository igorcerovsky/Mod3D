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
    // Constructors (constexpr and noexcept)
    constexpr ColumnPoint() noexcept = default;
    constexpr explicit ColumnPoint(double zVal) noexcept
        : m_pt(0.0, 0.0, zVal), m_z(zVal), m_body(nullptr), m_bodyId(-1), m_modified(true) {}
    constexpr ColumnPoint(int bodyIdVal, double zVal, Body *bodyPtr = nullptr) noexcept
        : m_pt(0.0, 0.0, zVal), m_z(zVal), m_body(bodyPtr), m_bodyId(bodyIdVal), m_modified(true) {}
    constexpr ColumnPoint(const Point3D &ptVal, int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : m_pt(ptVal), m_z(ptVal.z), m_body(bodyPtr), m_bodyId(bodyIdVal), m_modified(true) {}
    constexpr ColumnPoint(double xVal, double yVal, double zVal, int bodyIdVal = -1, Body *bodyPtr = nullptr) noexcept
        : m_pt(xVal, yVal, zVal), m_z(zVal), m_body(bodyPtr), m_bodyId(bodyIdVal), m_modified(true) {}

    // Special member functions (trivial copy, move, destruction)
    constexpr ColumnPoint(const ColumnPoint &) noexcept = default;
    constexpr ColumnPoint &operator=(const ColumnPoint &) noexcept = default;
    constexpr ColumnPoint(ColumnPoint &&) noexcept = default;
    constexpr ColumnPoint &operator=(ColumnPoint &&) noexcept = default;
    ~ColumnPoint() = default;

    // Comparisons (C++20 default member-wise equality)
    [[nodiscard]] constexpr bool operator==(const ColumnPoint &other) const noexcept = default;

    // Modification state
    [[nodiscard]] constexpr bool is_modified() const noexcept { return m_modified; }
    [[nodiscard]] constexpr bool isModified() const noexcept { return m_modified; }
    constexpr void set_modified(bool modified = true) noexcept { m_modified = modified; }
    constexpr void setModified(bool modified = true) noexcept { m_modified = modified; }

    // Body association
    [[nodiscard]] constexpr int body_id() const noexcept { return m_bodyId; }
    [[nodiscard]] constexpr int bodyId() const noexcept { return m_bodyId; }
    constexpr void set_body_id(int id) noexcept { m_bodyId = id; }
    constexpr void setBodyId(int id) noexcept { m_bodyId = id; }

    [[nodiscard]] constexpr const Body *body() const noexcept { return m_body; }
    [[nodiscard]] constexpr Body *body() noexcept { return m_body; }
    constexpr void set_body(Body *b) noexcept { m_body = b; }
    constexpr void setBody(Body *b) noexcept { m_body = b; }
    [[nodiscard]] constexpr bool has_body() const noexcept { return m_body != nullptr || m_bodyId >= 0; }

    // Coordinate accessors (STL and legacy naming)
    [[nodiscard]] constexpr double z() const noexcept { return m_z; }
    [[nodiscard]] constexpr double x() const noexcept { return m_pt.x; }
    [[nodiscard]] constexpr double y() const noexcept { return m_pt.y; }

    constexpr void set_z(double zVal) noexcept {
        m_z = zVal;
        m_pt.z = zVal;
        m_modified = true;
    }
    constexpr void setZ(double zVal) noexcept { set_z(zVal); }

    constexpr void set_x(double xVal) noexcept {
        m_pt.x = xVal;
        m_modified = true;
    }
    constexpr void set_y(double yVal) noexcept {
        m_pt.y = yVal;
        m_modified = true;
    }

    constexpr void set_coords(double xVal, double yVal, double zVal) noexcept {
        m_pt.x = xVal;
        m_pt.y = yVal;
        m_pt.z = zVal;
        m_z = zVal;
        m_modified = true;
    }

    [[nodiscard]] constexpr const Point3D &point() const noexcept { return m_pt; }
    [[nodiscard]] constexpr Point3D &point() noexcept { return m_pt; }
    constexpr void set_point(const Point3D &ptVal) noexcept {
        m_pt = ptVal;
        m_z = ptVal.z;
        m_modified = true;
    }
    constexpr void setPoint(const Point3D &ptVal) noexcept { set_point(ptVal); }

    // Relative vertical position queries (higher elevation Z is above)
    [[nodiscard]] constexpr bool is_above(const ColumnPoint &other) const noexcept { return m_z > other.m_z; }
    [[nodiscard]] constexpr bool is_below(const ColumnPoint &other) const noexcept { return m_z < other.m_z; }

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

    friend std::ostream &operator<<(std::ostream &os, const ColumnPoint &cp);

private:
    Point3D m_pt{0.0, 0.0, 0.0};
    double m_z{0.0};
    Body *m_body{nullptr};
    int m_bodyId{-1};
    bool m_modified{true};
};

static_assert(std::is_standard_layout_v<ColumnPoint>, "ColumnPoint must be standard layout");
static_assert(std::is_trivially_copyable_v<ColumnPoint>, "ColumnPoint must be trivially copyable");

} // namespace mod3d
