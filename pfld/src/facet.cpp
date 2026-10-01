#include "pfld/facet.hpp"

namespace pfld {

// Explicit template instantiations for supported floating point types
template class Facet<float>;
template class Facet<double>;

} // namespace pfld
