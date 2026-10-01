# pfld (Potential Field Computation Engine)

Modern C++20 header-only library, application, and test suite for gravitational potential field and gravity gradient computation across 3D polygonal facets.

## Structure

```
pfld/
├── CMakeLists.txt            # Root CMake build definition (defines pfld INTERFACE library)
├── README.md                 # Project documentation
├── include/                  # Pure math/physics header-only library (zero I/O dependencies)
│   └── pfld/
│       ├── point.hpp         # 3D vector / point primitive template
│       ├── facet.hpp         # Unified templated polygonal facet geometry & field operators
│       ├── pfld_compute.hpp  # Parallel / serial field computation functions
│       └── pfld.hpp          # Umbrella library header
├── app/                      # Standalone console application
│   └── pfld_app.cpp          # Benchmark and execution runner
└── tests/                    # Unit test suite & test dataset I/O
    ├── CMakeLists.txt        # GoogleTest discovery and test configuration
    ├── pfld_test_io.h        # Test dataset loading and result persistence header
    ├── pfld_test_io.cpp      # File parsing and dataset loaders implementation
    ├── unittest_pfld.cpp     # Point, Facet, Templated float/double, and Parallel test suite
    └── test_data/            # Reference datasets
        ├── pfld_facets.txt       # 1,000 facet dataset
        ├── pfld_points.txt       # 10,000 observation point dataset
        ├── pfld_results.txt      # 1,000-facet benchmark reference
        └── pfld_test_results.txt # 100-facet unit test reference
```

## Build & Test

### Standalone Build

```bash
# Configure
cmake -B build -S .

# Build library, app, and tests
cmake --build build

# Run unit tests
ctest --test-dir build --output-on-failure

# Run benchmark application
./build/pfld_app
```

### Integration with Mod3D

`pfld` can also be consumed directly as a subdirectory in CMake:
```cmake
add_subdirectory(pfld)
target_link_libraries(my_target PRIVATE pfld)
```
