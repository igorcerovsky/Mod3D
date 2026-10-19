# units - Custom C++20 Header-Only Units Library

A lightweight, zero-overhead, compile-time dimensional analysis and physical units library written in C++20 for science, engineering, and geophysics.

## Features

- **Header-Only & Standard C++20**: Zero external dependencies, pure standard C++ (`<concepts>`, `<numbers>`, `<cmath>`).
- **Zero Runtime Overhead**: `sizeof(Quantity<Unit, double>) == sizeof(double)`. Standard layout, trivially copyable, non-polymorphic.
- **Strong Compile-Time Safety**: Prevents unit mismatch errors at compile-time (e.g. cannot add meters to degrees or seconds).
- **Physical Algebra**: Supports cross-dimensional operations:
  - $\text{Acceleration} / \text{Length} = \text{Gradient}$ (e.g. $10\ \text{mGal} / 1\ \text{km} = 100\ \text{Eötvös}$)
  - $\text{Gradient} \times \text{Length} = \text{Acceleration}$
  - $\text{Length} \times \text{Length} = \text{Area}$
  - $\text{Area} \times \text{Length} = \text{Volume}$
  - $\text{Volume} \times \text{Density} = \text{Mass}$
  - $\text{Length} / \text{Time} = \text{Velocity}$
  - Dimensionless ratio division: $\text{Length} / \text{Length} = \text{double}$
- **Geophysical Unit System**:
  - **Length**: Meters (`_m`), Kilometers (`_km`), Centimeters (`_cm`), Millimeters (`_mm`), Feet (`_ft`)
  - **Angle**: Radians (`_rad`), Degrees (`_deg`), ArcMinutes (`_arcmin`), ArcSeconds (`_arcsec`)
  - **Density**: $\text{kg/m}^3$ (`_kg_m3`), $\text{g/cm}^3$ (`_g_cm3`)
  - **Gravity Acceleration**: $\text{m/s}^2$ (`_ms2`), $\text{mGal}$ (`_mgal`), Gravity Units / $\mu\text{m/s}^2$ (`_gu`), $\mu\text{Gal}$ (`_uGal`), $\text{Gal}$ (`_gal`)
  - **Gravity Gradient Tensor**: $\text{s}^{-2}$ (`_s2`), $\text{Eötvös}$ (`_E`)
  - **Magnetic Field**: $\text{Tesla}$ (`_T`), $\text{milliTesla}$ (`_mT`), $\mu\text{Tesla}$ (`_uT`), $\text{nanoTesla}$ (`_nT`, `_gamma`)
  - **Time & Mass**: $\text{Seconds}$ (`_s`), $\text{Milliseconds}$ (`_ms`), $\text{Kilograms}$ (`_kg`), $\text{Tonnes}$ (`_t`)
- **Trigonometry & Comparisons**:
  - `sin(angle)`, `cos(angle)`, `tan(angle)` taking any `Quantity<Angle>`
  - `asin_rad`, `asin_deg`, `acos_deg`, `atan2_deg`
  - C++20 `<=>` spaceship operator and `approx_equal(a, b)`
  - Standard output streaming `operator<<` (e.g. `"12.5 mGal"`, `"2670 kg/m³"`)

## Quick Start

```cpp
#include "units/units.hpp"
#include <iostream>

using namespace units;
using namespace units::literals;

int main() {
    Meters depth = 1.5_km;
    std::cout << "Depth: " << depth << "\n"; // "1500 m"

    MGal anomaly = 12.5_mgal + 25.0_gu; // 12.5 + 2.5 = 15.0 mGal
    std::cout << "Gravity anomaly: " << anomaly << "\n"; // "15 mGal"

    Eotvos gradient = 10.0_mgal / 1.0_km; // 100 E
    std::cout << "Vertical gradient: " << gradient << "\n"; // "100 E"

    return 0;
}
```

## Integration with CMake

```cmake
add_subdirectory(units)
target_link_libraries(my_project PRIVATE units)
```
