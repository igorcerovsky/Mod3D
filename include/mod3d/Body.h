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

    [[nodiscard]] bool is_visible() const noexcept { return visible_; }
    void set_visible(bool visible) noexcept { visible_ = visible; }

    [[nodiscard]] bool is_locked() const noexcept { return locked_; }
    void set_locked(bool locked) noexcept { locked_ = locked; }

    [[nodiscard]] bool is_filled() const noexcept { return filled_; }
    void set_filled(bool fill) noexcept { filled_ = fill; }

    // Physical Properties (Gravity / Density)
    [[nodiscard]] double density() const noexcept { return (active_ ? density_ : 0.0); }
    [[nodiscard]] double raw_density() const noexcept { return density_; }
    void set_density(double dens) noexcept { density_ = dens; }

    [[nodiscard]] const Point3D &density_gradient() const noexcept { return density_grad_; }
    void set_density_gradient(const Point3D &grad) noexcept { density_grad_ = grad; }

    [[nodiscard]] const Point3D &density_origin() const noexcept { return density_org_; }
    void set_density_origin(const Point3D &orig) noexcept { density_org_ = orig; }

    [[nodiscard]] double density_at_origin() const noexcept {
        return (density_ - density_grad_ * density_org_);
    }

    [[nodiscard]] double density_at(const Point3D &pt) const noexcept {
        if (!active_) return 0.0;
        return density_ + density_grad_ * (pt - density_org_);
    }

    // Physical Properties (Magnetics)
    [[nodiscard]] double susceptibility() const noexcept { return susceptibility_; }
    void set_susceptibility(double susc) noexcept { susceptibility_ = susc; }

    [[nodiscard]] const Point3D &magnetization_vector() const noexcept { return mag_vector_; }
    void set_magnetization_vector(const Point3D &mv) noexcept { mag_vector_ = mv; }

    [[nodiscard]] const Point3D &remanent_magnetization() const noexcept { return mag_rem_; }
    void set_remanent_magnetization(const Point3D &rem) noexcept { mag_rem_ = rem; }

    void compute_magnetization_vector(const Point3D &vIndFld);

    // Rendering Parameters
    [[nodiscard]] BodyColor color() const noexcept { return color_; }
    void set_color(const BodyColor &color) noexcept {
        color_ = color;
        if (fill_color_ == BodyColor()) fill_color_ = color;
        if (map_color_ == BodyColor()) map_color_ = color;
    }

    [[nodiscard]] BodyColor fill_color() const noexcept { return fill_color_; }
    void set_fill_color(const BodyColor &col) noexcept { fill_color_ = col; }

    [[nodiscard]] BodyColor map_color() const noexcept { return map_color_; }
    void set_map_color(const BodyColor &col) noexcept { map_color_ = col; }

    [[nodiscard]] float transparency() const noexcept { return transparency_; }
    void set_transparency(float alpha) noexcept { transparency_ = alpha; }

    [[nodiscard]] bool is_transparent() const noexcept { return transparent_; }
    void set_transparent(bool trans) noexcept { transparent_ = trans; }

    // Comparisons
    [[nodiscard]] bool operator==(const Body &other) const noexcept;
    [[nodiscard]] bool operator!=(const Body &other) const noexcept { return !(*this == other); }

    friend std::ostream &operator<<(std::ostream &os, const Body &b);


private:
    int id_{0};
    int index_{0};

    std::string name_{"Name the body..."};
    std::string description_{"Body description..."};
    bool visible_{true};
    bool locked_{false};
    bool filled_{true};
    bool active_{true};

    // Physical parameters
    double density_{2700.0};          // kg/m3
    Point3D density_grad_{0, 0, 0};   // linear density gradient
    Point3D density_org_{0, 0, 0};    // density reference origin
    double susceptibility_{0.01};     // SI susceptibility
    Point3D mag_vector_{0, 0, 0};     // total magnetization vector
    Point3D mag_rem_{0, 0, 0};        // remanent magnetization

    // Rendering parameters
    BodyColor color_{0, 0, 180, 255};
    BodyColor fill_color_{0, 0, 180, 255};
    BodyColor map_color_{0, 0, 180, 255};
    float transparency_{0.5f};
    bool transparent_{true};
};

static_assert(!std::is_polymorphic_v<Body>, "Body must not be polymorphic");

using BodyPtr = std::shared_ptr<Body>;
using BodyPtrArray = std::vector<BodyPtr>;

} // namespace mod3d
