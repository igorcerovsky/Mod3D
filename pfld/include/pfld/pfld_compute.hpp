#ifndef PFLD_COMPUTE_H_
#define PFLD_COMPUTE_H_

#include "facet.hpp"

namespace pfld {

void Field_G(pfld::facet_vec& facets, pfld::ptvec& field_points, pfld::ptvec& out_field);
void Field_Gz(pfld::facet_vec& facets, pfld::ptvec& field_points, pfld::valvec& out_field);
void Field_Gz_(pfld::facet_vec& facets, pfld::ptvec& field_points, pfld::valvec& out_field);
void Field_Gz__(pfld::facet_vec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld);

}; // namespace pfld

#endif // PFLD_COMPUTE_H_
