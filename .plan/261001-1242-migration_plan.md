# Mod3D Modernization & Migration Plan

## 1. Post-Mortem: Analysis of the Failed Initial Attempt

The initial modernization attempt failed because it violated the fundamental principles of legacy scientific software refactoring:

| Area | What Went Wrong in Initial Attempt | Root Cause |
| :--- | :--- | :--- |
| **Physics & Math** | Dropped `FldVlado` and `FldVladoGrd`; stripped `PotField.cpp` from 608 to 261 lines and `Facet3Pt.cpp` from 967 to 516 lines. Computed fields were completely wrong. | Prematurely "rewrote" equations from scratch rather than lifting existing, verified code verbatim. |
| **Model & Facets** | Truncated `Model.cpp` from 1,988 to 713 lines; deleted `FctGener` 4-column prism triangulation, side facets, boundary tracking, and pinchouts. | Substituted an oversimplified layer abstraction for the authentic columnar stratigraphy grid. |
| **Testing Illusion** | Created unit tests that tested the *flawed rewritten math against itself*, yielding 28 "passing" tests while producing incorrect physical values. | Did not use ground-truth datasets (`pfld_UnitTest/test_data/`, `legacy/sample_data/`). |
| **User Experience** | Replaced the authentic multi-window workflow with an arbitrary single-window layout; missed the New Model Wizard, observation definition, active field selection, and row/column profile navigation. | Built UI before understanding the geophysicist's operational workflow. |

---

## 2. Core Philosophy for the Clean Migration

1. **Zero Guesswork / Zero Math Alteration**:
   - The code in `legacy/old_core/` is the ground truth.
   - Algorithms must be ported **1:1 line-by-line**, replacing *only* MFC/Win32 containers (`CArray` $\rightarrow$ `std::vector`, `CString` $\rightarrow$ `std::string`, `POSITION` $\rightarrow$ index/iterator, `CObject` removed).
   - Mathematical equations, coordinate transformations, sign conventions, and numerical constants must remain identical.
2. **Ground-Truth Test Harness First (`pfld_UnitTest`)**:
   - Before building any UI or higher-level logic, the physics engine must pass all tests in `pfld_UnitTest` against `pfld_test_results.txt` with double-precision accuracy ($10^{-14}$ to $10^{-16}$).
3. **Strict Validation Gates**:
   - Do not advance to the next phase until the current phase passes its verification gate.
4. **Authentic Workflow Fidelity**:
   - The Qt 6 desktop application will faithfully recreate the real Mod3D workflow: Observation Definition $\rightarrow$ Component Selection $\rightarrow$ Column Discretization $\rightarrow$ Row/Col Profile Editing $\rightarrow$ Real-Time / Threaded Forward Modeling $\rightarrow$ 3D OpenGL & 2D Map Visualization.

---

## 3. Migration Roadmap & Execution Phases

```
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 0: Build & Test Infrastructure                                   │
│ - Minimal CMake with C++20 and GoogleTest                              │
│ - Harness pfld_UnitTest test suites & test_data/ into CTest            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 1: Mathematical & Potential Field Engine                         │
│ - Point3D / Vector3D vector algebra                                    │
│ - PotField.cpp: Pohanka gravity & Guptasarma-Singh magnetics           │
│ - Facet3Pt.cpp: FldVlado (constant & linear), FldVladoGrd (tensor),    │
│                 FldGS (magnetics), opposite facets, signed updates     │
│ [GATE 1]: Pass all pfld_UnitTest cases against pfld_test_results.txt   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 2: Geological Model & Stratigraphic Facet Engine                 │
│ - ClmnPt: Columnar stratigraphic contact points                        │
│ - Body: Physical properties (density, susceptibility, remanence)       │
│ - Grid: Surfer 6 ASCII/Binary & Surfer 7 import/export                 │
│ - Model: 1:1 lift of 1,988-line Model.cpp (FctGener 4-column cell      │
│          triangulation, InitFacet, InitSideFacets, pinchout handling)  │
│ [GATE 2]: Facet generation matches legacy/sample_data/FacetList.fct    │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 3: Observation Space, Forward Modeling & Inversion               │
│ - Observation header, relief grid, observation elevations              │
│ - Background thread & real-time delta field updates (ComputeField)     │
│ - Difference grids (Model - Obs) & background mean removal             │
│ - 1D Golden Section / Brent inversion for density & contact depth      │
│ [GATE 3]: Verify inversion logs match FitLogDens.dat / FitLogVrtx.dat  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 4: Authentic Mod3D User Experience (Qt 6)                        │
│ - New Model Wizard: CDlgDefineObs -> MakeObservations -> CDlgModGrids  │
│ - 2D Profile Editor (ViewProf): Row/Col cross-sections, vertex drag,   │
│   adjacent slice ghosting, pinchout management, live field curves      │
│ - 2D Map View (ViewMap): Contours, observation overlay, slice tracker  │
│ - 3D OpenGL View (ViewGL3D): 3D polyhedral models, slice plane, opacity│
│ - Geological Body Inspector: 8-column physical property table          │
│ [GATE 4]: Full interactive workflow verified against legacy operations │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 5: Polish, Project I/O & Packaging                               │
│ - Legacy .m3d binary file loading & modern JSON/YAML project format    │
│ - Standalone macOS application bundle (.app) and DMG packaging         │
│ - Cross-platform validation (macOS, Linux, Windows)                    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Phase Breakdown & Deliverables

### Phase 0: Build & Test Infrastructure
- **Objective**: Establish a clean modern CMake build system on macOS AppleClang without touching legacy files.
- **Tasks**:
  1. Create `CMakeLists.txt` targeting C++20 with strict warnings (`-Wall -Wextra`).
  2. Integrate GoogleTest via `FetchContent`.
  3. Set up the testing directory `tests/` with access to `pfld_UnitTest/test_data/` (`pfld_test_facets.txt`, `pfld_test_points.txt`, `pfld_test_results.txt`).

### Phase 1: Mathematical & Potential Field Engine
- **Objective**: Restore the exact analytical physics engine 1:1 from `legacy/old_core/`.
- **Files**: `include/mod3d/Vector3D.h`, `include/mod3d/PotField.h`, `include/mod3d/Facet3Pt.h`, `src/Vector3D.cpp`, `src/PotField.cpp`, `src/Facet3Pt.cpp`.
- **Key Equations & Implementations to Preserve**:
  - `FldVlado(v_r, v_Grv)`: Constant density polygon gravity field.
  - `FldVlado(v_r, v_Grv, ro, ro0)`: Linear density gradient polygon gravity field with origin density $ro_0$ and gradient vector $\mathbf{ro}$.
  - `FldVladoGrd(...)`: Full gravity gradient tensor components ($g_{xx}, g_{yy}, g_{zz}, g_{xy}, g_{xz}, g_{yz}$).
  - `FldGS(v_r, v_M, v_Mag, dSign)`: Singh-Guptasarma magnetic field calculation.
  - `Compute(...)`: Reference model subtraction ($G - G_{\text{ref}}$) and opposite facet handling (`pBodyOpos`).
- **Gate 1 Criteria**:
  - `Test_Point3D_Unit` passes.
  - `Test_Body` passes with result `(5.1341030021201644e-09, 5.1341030021201644e-09, 1.2401175118216113e-08)`.
  - `Test_Facet` passes with `resval = 4.8207079871718046e-08`.
  - `Test_Facet_Lin` passes with `result(-3.2142476436014269e-05, -3.2142476436014269e-05, 5.6270119911809142e-05)`.
  - `Test_Facet_Parallell` matches `pfld_test_results.txt` across all 10,000 points.

### Phase 2: Geological Model & Stratigraphic Facet Engine
- **Objective**: Restore the authentic 3D columnar stratigraphy model and polyhedral facet generator.
- **Files**: `include/mod3d/ColumnPoint.h`, `include/mod3d/Body.h`, `include/mod3d/Grid.h`, `include/mod3d/Model.h`, `src/ColumnPoint.cpp`, `src/Body.cpp`, `src/Grid.cpp`, `src/Model.cpp`.
- **Key Algorithms to Preserve**:
  - `FCTGENER`: 4-column prism cell data structure with 4 corner vertices $[0..3]$ across adjacent grid lines.
  - `AddFctGen`: Detects layer interfaces between 4 column points and determines body continuity or termination.
  - `InitFacet` / `InitFacetTBM`: Triangulates upper/lower interfaces with correct outward normals and sign tracking.
  - `InitSideFacets`, `InitSideFacets_2`, `InitSideFacets_3`: Triangulates lateral boundaries when layers pinch out or step.
  - `MoveVertex`: Contact depth adjustments (normal, splitting, and constrained).
- **Gate 2 Criteria**:
  - Automated test loading or generating a multi-layer model produces polyhedra whose surface facets match `legacy/sample_data/FacetList.fct`.

### Phase 3: Observation Space, Forward Modeling & Inversion [COMPLETED & VERIFIED]
- **Objective**: Connect the model facets to the observation grids for full forward modeling and automated inversion.
- **Files**: `include/mod3d/Observation.h`, `include/mod3d/Inversion.h`, `src/Observation.cpp`, `src/Inversion.cpp`.
- **Key Logic Preserved**:
  - `MakeObservations`: Relief grid, flight altitude, and observation points for gravity, magnetics, and tensor.
  - Real-time delta updates: Only recomputing facets that changed with sign $+1$ and subtracting old facets with sign $-1$.
  - 1D Inversion: Golden section search & parabolic interpolation (Brent's method) for optimal density contrast and contact depth.
- **Gate 3 Criteria [PASSED]**:
  - Density recovery test converges to values matching `legacy/sample_data/FitLogDens.dat` (optimal $\rho = 2850.0 \pm 0.1$ kg/m$^3$, RMS drops to $< 10^{-8}$).
  - Vertex depth recovery matches `legacy/sample_data/FitLogVrtx.dat` (optimal $z = -707.58 \pm 0.1$ m, RMS drops to $< 10^{-8}$).
  - All 48 unit tests passing across entire physics, model, grid, tensor, and inversion test suites.


### Phase 4: Authentic Mod3D User Experience (Qt 6)
- **Objective**: Provide the exact geophysical modeling workflow in a modern Qt 6 interface.
- **Components**:
  - **New Model Wizard**:
    - Step 1: `CDlgDefineObs` — bounding box ($X_{\min}, X_{\max}, Y_{\min}, Y_{\max}, Z_{\min}, Z_{\max}$), grid rows/cols or cell size.
    - Step 2: `CDlgModGrids` — checkboxes to select which field components to compute ($G_z, G_x, G_y, \Delta T, T_{xx} \dots$).
    - Step 3: Initial relief and stratigraphic layers.
  - **2D Profile View (`ProfileView`)**:
    - Cross-section display along rows (West-East) and columns (South-North).
    - Draggable contact vertices with constraint indicators.
    - Adjacent profile ghosting (showing previous/next slice for stratigraphic continuity).
    - Top panel: Real-time calculated field curves ($G_z, \Delta T$) vs observed field curves.
  - **2D Map View (`MapView`)**:
    - Isoline contour display of active modeled field, observed field, or difference field.
    - Profile indicator line showing active cross-section location.
  - **3D OpenGL View (`GL3DView`)**:
    - OpenGL 3.2+ Core Profile rendering of the polyhedral bodies with customizable body opacity and lighting.
    - 3D profile slice indicator plane.
  - **Geological Body Inspector**:
    - Table of bodies with Name, Density ($\text{kg/m}^3$), Susceptibility, Remanence, Color, and Active/Locked status.

### Phase 5: Packaging & Cross-Platform CI
- **Objective**: Production delivery.
- **Deliverables**:
  - Standalone macOS `.app` bundle with application icon.
  - DMG installer.
  - Backward compatibility: parsing legacy binary `.m3d` files directly.
