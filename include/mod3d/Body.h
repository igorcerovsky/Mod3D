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
    // Constructors and Rule of 5
    Body();
    explicit Body(int id, std::string name = "Body", double density = 2670.0);
    ~Body() = default;

    Body(const Body &) = default;
    Body &operator=(const Body &) = default;
    Body(Body &&) noexcept = default;
    Body &operator=(Body &&) noexcept = default;

    void swap(Body &other) noexcept;
    friend void swap(Body &a, Body &b) noexcept { a.swap(b); }

    // Identifiers & Metadata
    [[nodiscard]] int id() const noexcept { return m_nID; }
    [[nodiscard]] int GetID() const noexcept { return m_nID; }
    void set_id(int id) noexcept { m_nID = id; }
    void SetID(int id) noexcept { m_nID = id; }

    [[nodiscard]] int index() const noexcept { return m_nIndex; }
    [[nodiscard]] int GetIndex() const noexcept { return m_nIndex; }
    void set_index(int idx) noexcept { m_nIndex = idx; }
    void SetIndex(int idx) noexcept { m_nIndex = idx; }

    [[nodiscard]] const std::string &name() const noexcept { return m_strName; }
    [[nodiscard]] const std::string &GetName() const noexcept { return m_strName; }
    void set_name(std::string name) { m_strName = std::move(name); }
    void SetName(std::string name) { m_strName = std::move(name); }

    [[nodiscard]] const std::string &description() const noexcept { return m_strDescription; }
    [[nodiscard]] const std::string &GetDescription() const noexcept { return m_strDescription; }
    void set_description(std::string desc) { m_strDescription = std::move(desc); }
    void SetDescription(std::string desc) { m_strDescription = std::move(desc); }

    // State Flags
    [[nodiscard]] bool is_active() const noexcept { return m_bActive; }
    [[nodiscard]] bool IsActive() const noexcept { return m_bActive; }
    void set_active(bool active) noexcept { m_bActive = active; }
    void SetActive(bool active) noexcept { m_bActive = active; }

    [[nodiscard]] bool is_visible() const noexcept { return m_bShow; }
    [[nodiscard]] bool IsVisible() const noexcept { return m_bShow; }
    void set_visible(bool visible) noexcept { m_bShow = visible; }
    void SetVisible(bool visible) noexcept { m_bShow = visible; }

    [[nodiscard]] bool is_locked() const noexcept { return m_bLocked; }
    [[nodiscard]] bool IsLocked() const noexcept { return m_bLocked; }
    void set_locked(bool locked) noexcept { m_bLocked = locked; }
    void SetLocked(bool locked) noexcept { m_bLocked = locked; }

    [[nodiscard]] bool is_filled() const noexcept { return m_bFill; }
    [[nodiscard]] bool IsFilled() const noexcept { return m_bFill; }
    void set_filled(bool fill) noexcept { m_bFill = fill; }
    void SetFilled(bool fill) noexcept { m_bFill = fill; }

    // Physical Properties (Gravity / Density)
    [[nodiscard]] double density() const noexcept { return (m_bActive ? m_dDensity : 0.0); }
    [[nodiscard]] double GetDensity() const noexcept { return density(); }
    [[nodiscard]] double raw_density() const noexcept { return m_dDensity; }
    [[nodiscard]] double GetRawDensity() const noexcept { return m_dDensity; }
    void set_density(double dens) noexcept { m_dDensity = dens; }
    void SetDensity(double dens) noexcept { m_dDensity = dens; }

    [[nodiscard]] const Point3D &density_gradient() const noexcept { return m_vDensGrad; }
    [[nodiscard]] Point3D GetDensityGradient() const noexcept { return m_vDensGrad; }
    void set_density_gradient(const Point3D &grad) noexcept { m_vDensGrad = grad; }
    void SetDensityGradient(const Point3D &grad) noexcept { m_vDensGrad = grad; }

    [[nodiscard]] const Point3D &density_origin() const noexcept { return m_vDensOrg; }
    [[nodiscard]] Point3D GetDensityOrigo() const noexcept { return m_vDensOrg; }
    void set_density_origin(const Point3D &orig) noexcept { m_vDensOrg = orig; }
    void SetDensityOrigo(const Point3D &orig) noexcept { m_vDensOrg = orig; }

    [[nodiscard]] double density_at_origin() const noexcept {
        return (m_dDensity - m_vDensGrad * m_vDensOrg);
    }
    [[nodiscard]] double GetDensityAtOrigin() const noexcept {
        return density_at_origin();
    }

    [[nodiscard]] double density_at(const Point3D &pt) const noexcept {
        if (!m_bActive) return 0.0;
        return m_dDensity + m_vDensGrad * (pt - m_vDensOrg);
    }

    // Physical Properties (Magnetics)
    [[nodiscard]] double susceptibility() const noexcept { return m_dSusc; }
    [[nodiscard]] double GetSusceptibility() const noexcept { return m_dSusc; }
    void set_susceptibility(double susc) noexcept { m_dSusc = susc; }
    void SetSusceptibility(double susc) noexcept { m_dSusc = susc; }

    [[nodiscard]] const Point3D &magnetization_vector() const noexcept { return m_vMagVector; }
    [[nodiscard]] Point3D GetMagnetizationVector() const noexcept { return m_vMagVector; }
    void set_magnetization_vector(const Point3D &mv) noexcept { m_vMagVector = mv; }
    void SetMagnetizationVector(const Point3D &mv) noexcept { m_vMagVector = mv; }

    [[nodiscard]] const Point3D &remanent_magnetization() const noexcept { return m_vMagRem; }
    [[nodiscard]] Point3D GetRemanentMagnetization() const noexcept { return m_vMagRem; }
    void set_remanent_magnetization(const Point3D &rem) noexcept { m_vMagRem = rem; }
    void SetRemanentMagnetization(const Point3D &rem) noexcept { m_vMagRem = rem; }

    void compute_magnetization_vector(const Point3D &vIndFld);
    void ComputeMagnetizationVector(const Point3D &vIndFld) { compute_magnetization_vector(vIndFld); }

    // Rendering Parameters
    [[nodiscard]] BodyColor color() const noexcept { return m_color; }
    [[nodiscard]] BodyColor GetColor() const noexcept { return m_color; }
    void set_color(const BodyColor &color) noexcept { m_color = color; }
    void SetColor(const BodyColor &color) noexcept { m_color = color; }

    [[nodiscard]] float transparency() const noexcept { return m_fAlpha; }
    [[nodiscard]] float GetTransparency() const noexcept { return m_fAlpha; }
    void set_transparency(float alpha) noexcept { m_fAlpha = alpha; }
    void SetTransparency(float alpha) noexcept { m_fAlpha = alpha; }

    [[nodiscard]] bool is_transparent() const noexcept { return m_bTransparent; }
    [[nodiscard]] bool IsTransparent() const noexcept { return m_bTransparent; }
    void set_transparent(bool trans) noexcept { m_bTransparent = trans; }
    void SetTransparent(bool trans) noexcept { m_bTransparent = trans; }

    // Comparisons
    [[nodiscard]] bool operator==(const Body &other) const noexcept;
    [[nodiscard]] bool operator!=(const Body &other) const noexcept { return !(*this == other); }

    friend std::ostream &operator<<(std::ostream &os, const Body &b);

private:
    int m_nID{0};
    int m_nIndex{0};

    std::string m_strName{"Name the body..."};
    std::string m_strDescription{"Body description..."};
    bool m_bShow{true};
    bool m_bLocked{false};
    bool m_bFill{true};
    bool m_bActive{true};

    // Physical parameters
    double m_dDensity{2700.0};       // kg/m3
    Point3D m_vDensGrad{0, 0, 0};    // linear density gradient
    Point3D m_vDensOrg{0, 0, 0};     // density reference origin
    double m_dSusc{0.01};            // SI susceptibility
    Point3D m_vMagVector{0, 0, 0};   // total magnetization vector
    Point3D m_vMagRem{0, 0, 0};      // remanent magnetization

    // Rendering parameters
    BodyColor m_color{0, 0, 180, 255};
    float m_fAlpha{0.5f};
    bool m_bTransparent{true};
};

static_assert(!std::is_polymorphic_v<Body>, "Body must not be polymorphic");

using BodyPtr = std::shared_ptr<Body>;
using BodyPtrArray = std::vector<BodyPtr>;

} // namespace mod3d
