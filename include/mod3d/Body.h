#pragma once

#include "mod3d/Point3D.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace mod3d {

struct BodyColor {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{180};
    uint8_t a{255};
};

/**
 * @brief Represents a 3D geological body with physical properties.
 * Decoupled from MFC CBody while preserving exact physical equations.
 */
class Body {
public:
    Body();
    explicit Body(int id, std::string name = "Body", double density = 2670.0);
    virtual ~Body() = default;

    int GetID() const { return m_nID; }
    void SetID(int id) { m_nID = id; }

    int GetIndex() const { return m_nIndex; }
    void SetIndex(int index) { m_nIndex = index; }

    std::string GetName() const { return m_strName; }
    void SetName(const std::string& name) { m_strName = name; }

    std::string GetDescription() const { return m_strDescription; }
    void SetDescription(const std::string& desc) { m_strDescription = desc; }

    bool IsActive() const { return m_bActive; }
    void SetActive(bool active) { m_bActive = active; }

    bool IsVisible() const { return m_bShow; }
    void SetVisible(bool visible) { m_bShow = visible; }

    bool IsLocked() const { return m_bLocked; }
    void SetLocked(bool locked) { m_bLocked = locked; }

    bool IsFilled() const { return m_bFill; }
    void SetFilled(bool fill) { m_bFill = fill; }

    // Physical Properties
    double GetDensity() const { return (m_bActive ? m_dDensity : 0.0); }
    double GetRawDensity() const { return m_dDensity; }
    void SetDensity(double dens) { m_dDensity = dens; }

    double GetSusceptibility() const { return m_dSusc; }
    void SetSusceptibility(double susc) { m_dSusc = susc; }

    Point3D GetDensityGradient() const { return m_vDensGrad; }
    void SetDensityGradient(const Point3D& grad) { m_vDensGrad = grad; }

    Point3D GetDensityOrigo() const { return m_vDensOrg; }
    void SetDensityOrigo(const Point3D& origo) { m_vDensOrg = origo; }

    double GetDensityAtOrigin() const {
        return (m_dDensity - m_vDensGrad * m_vDensOrg);
    }

    Point3D GetMagnetizationVector() const { return m_vMagVector; }
    void SetMagnetizationVector(const Point3D& mv) { m_vMagVector = mv; }

    Point3D GetRemanentMagnetization() const { return m_vMagRem; }
    void SetRemanentMagnetization(const Point3D& rem) { m_vMagRem = rem; }

    void ComputeMagnetizationVector(const Point3D& vIndFld);

    // Color & Transparency
    BodyColor GetColor() const { return m_color; }
    void SetColor(const BodyColor& color) { m_color = color; }
    float GetTransparency() const { return m_fAlpha; }
    void SetTransparency(float alpha) { m_fAlpha = alpha; }
    bool IsTransparent() const { return m_bTransparent; }
    void SetTransparent(bool transparent) { m_bTransparent = transparent; }

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

using BodyPtr = std::shared_ptr<Body>;
using BodyPtrArray = std::vector<BodyPtr>;

} // namespace mod3d
