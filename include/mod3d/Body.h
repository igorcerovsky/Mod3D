#pragma once

#include "mod3d/Point3D.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <cstdint>
#include <iosfwd>
#include <utility>
#include <type_traits>

namespace mod3d {

struct BodyColor {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{180};
    uint8_t a{255};

    constexpr BodyColor() noexcept = default;
    constexpr BodyColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) noexcept
        : r(red), g(green), b(blue), a(alpha) {}

    [[nodiscard]] constexpr bool operator==(const BodyColor &other) const noexcept = default;

    friend std::ostream &operator<<(std::ostream &os, const BodyColor &col);
};

// ============================================================================
// Logical Parameter Structures
// ============================================================================

/**
 * @brief 3D Gravity / Density model parameters.
 */
struct BodyDensity {
    double value{2700.0};                 // Reference density (kg/m3)
    Point3D gradient{0, 0, 0};            // 3D linear density gradient (kg/m3 per meter)
    Point3D origin{0, 0, 0};              // Gradient reference origin (x0, y0, z0)

    [[nodiscard]] double at(const Point3D &pt, bool active = true) const noexcept {
        if (!active) return 0.0;
        return value + gradient * (pt - origin);
    }

    [[nodiscard]] double at_origin() const noexcept {
        return value - gradient * origin;
    }

    [[nodiscard]] constexpr bool operator==(const BodyDensity &other) const noexcept = default;
};

/**
 * @brief 3D Magnetic property parameters.
 */
struct BodyMagnetics {
    double susceptibility{0.01};          // SI susceptibility (dimensionless)
    Point3D remanence{0, 0, 0};           // Remanent magnetization vector
    Point3D total_vector{0, 0, 0};        // Total magnetization vector

    void compute_vector(const Point3D &vIndFld);

    [[nodiscard]] constexpr bool operator==(const BodyMagnetics &other) const noexcept = default;
};

/**
 * @brief 2D & 3D Drawing, styling, and visualization parameters.
 */
struct BodyDrawing {
    bool visible{true};                   // Whether body is rendered / visible
    bool filled{true};                    // Whether body profile interior polygon is filled
    BodyColor color{0, 0, 180, 255};      // Boundary / outline stroke color
    BodyColor fill_color{0, 0, 180, 255}; // Profile polygon interior fill color
    BodyColor map_color{0, 0, 180, 255};  // Top-down 2D map outline/fill color
    float transparency{0.5f};             // Alpha opacity factor [0.0 - 1.0]
    bool transparent{true};               // Enable transparency blending

    [[nodiscard]] constexpr bool operator==(const BodyDrawing &other) const noexcept = default;
};

/**
 * @brief Represents a 3D geological body with physical properties.
 * Decoupled from legacy MFC CBody while preserving exact physical equations.
 * Non-polymorphic, Rule-of-5 compliant value object with noexcept getters.
 */
class Body {
public:
    // ========================================================================
    // Lifecycle & Rule of 5
    // ========================================================================
    Body();
    explicit Body(int id, std::string name = "Body", double density = 2670.0);
    ~Body() = default;

    Body(const Body &) = default;
    Body &operator=(const Body &) = default;
    Body(Body &&) noexcept = default;
    Body &operator=(Body &&) noexcept = default;

    void swap(Body &other) noexcept;
    friend void swap(Body &a, Body &b) noexcept { a.swap(b); }

    // ========================================================================
    // Modern C++20 API (STL snake_case convention, noexcept, [[nodiscard]])
    // ========================================================================

    // Identifiers & Metadata
    [[nodiscard]] int id() const noexcept { return id_; }
    void set_id(int id) noexcept { id_ = id; }

    [[nodiscard]] int index() const noexcept { return index_; }
    void set_index(int idx) noexcept { index_ = idx; }

    [[nodiscard]] const std::string &name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const std::string &description() const noexcept { return description_; }
    void set_description(std::string desc) { description_ = std::move(desc); }

    // State Flags
    [[nodiscard]] bool is_active() const noexcept { return active_; }
    void set_active(bool active) noexcept { active_ = active; }

    [[nodiscard]] bool is_locked() const noexcept { return locked_; }
    void set_locked(bool locked) noexcept { locked_ = locked; }

    // Logical Component Struct Accessors
    [[nodiscard]] const BodyDensity &density_props() const noexcept { return density_; }
    [[nodiscard]] BodyDensity &density_props() noexcept { return density_; }

    [[nodiscard]] const BodyMagnetics &magnetics() const noexcept { return magnetics_; }
    [[nodiscard]] BodyMagnetics &magnetics() noexcept { return magnetics_; }

    [[nodiscard]] const BodyDrawing &drawing() const noexcept { return drawing_; }
    [[nodiscard]] BodyDrawing &drawing() noexcept { return drawing_; }

    // Drawing / Visualization Forwarders
    [[nodiscard]] bool is_visible() const noexcept { return drawing_.visible; }
    void set_visible(bool visible) noexcept { drawing_.visible = visible; }

    [[nodiscard]] bool is_filled() const noexcept { return drawing_.filled; }
    void set_filled(bool fill) noexcept { drawing_.filled = fill; }

    [[nodiscard]] BodyColor color() const noexcept { return drawing_.color; }
    void set_color(const BodyColor &color) noexcept {
        drawing_.color = color;
        if (drawing_.fill_color == BodyColor()) drawing_.fill_color = color;
        if (drawing_.map_color == BodyColor()) drawing_.map_color = color;
    }

    [[nodiscard]] BodyColor fill_color() const noexcept { return drawing_.fill_color; }
    void set_fill_color(const BodyColor &col) noexcept { drawing_.fill_color = col; }

    [[nodiscard]] BodyColor map_color() const noexcept { return drawing_.map_color; }
    void set_map_color(const BodyColor &col) noexcept { drawing_.map_color = col; }

    [[nodiscard]] float transparency() const noexcept { return drawing_.transparency; }
    void set_transparency(float alpha) noexcept { drawing_.transparency = alpha; }

    [[nodiscard]] bool is_transparent() const noexcept { return drawing_.transparent; }
    void set_transparent(bool trans) noexcept { drawing_.transparent = trans; }

    // Physical Properties Forwarders (Gravity / Density)
    [[nodiscard]] double density() const noexcept { return (active_ ? density_.value : 0.0); }
    [[nodiscard]] double raw_density() const noexcept { return density_.value; }
    void set_density(double dens) noexcept { density_.value = dens; }

    [[nodiscard]] const Point3D &density_gradient() const noexcept { return density_.gradient; }
    void set_density_gradient(const Point3D &grad) noexcept { density_.gradient = grad; }

    [[nodiscard]] const Point3D &density_origin() const noexcept { return density_.origin; }
    void set_density_origin(const Point3D &orig) noexcept { density_.origin = orig; }

    [[nodiscard]] double density_at_origin() const noexcept { return density_.at_origin(); }
    [[nodiscard]] double density_at(const Point3D &pt) const noexcept { return density_.at(pt, active_); }

    // Physical Properties Forwarders (Magnetics)
    [[nodiscard]] double susceptibility() const noexcept { return magnetics_.susceptibility; }
    void set_susceptibility(double susc) noexcept { magnetics_.susceptibility = susc; }

    [[nodiscard]] const Point3D &magnetization_vector() const noexcept { return magnetics_.total_vector; }
    void set_magnetization_vector(const Point3D &mv) noexcept { magnetics_.total_vector = mv; }

    [[nodiscard]] const Point3D &remanent_magnetization() const noexcept { return magnetics_.remanence; }
    void set_remanent_magnetization(const Point3D &rem) noexcept { magnetics_.remanence = rem; }

    void compute_magnetization_vector(const Point3D &vIndFld);

    // Comparisons
    [[nodiscard]] bool operator==(const Body &other) const noexcept;
    [[nodiscard]] bool operator!=(const Body &other) const noexcept { return !(*this == other); }

    friend std::ostream &operator<<(std::ostream &os, const Body &b);

private:
    int id_{0};
    int index_{0};

    std::string name_{"Name the body..."};
    std::string description_{"Body description..."};
    bool active_{true};
    bool locked_{false};

    BodyDensity density_;
    BodyMagnetics magnetics_;
    BodyDrawing drawing_;
};

static_assert(!std::is_polymorphic_v<BodyDensity>, "BodyDensity must not be polymorphic");
static_assert(!std::is_polymorphic_v<BodyMagnetics>, "BodyMagnetics must not be polymorphic");
static_assert(!std::is_polymorphic_v<BodyDrawing>, "BodyDrawing must not be polymorphic");
static_assert(!std::is_polymorphic_v<Body>, "Body must not be polymorphic");

using BodyPtr = std::shared_ptr<Body>;
using BodyPtrArray = std::vector<BodyPtr>;

} // namespace mod3d
