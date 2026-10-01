#pragma once

#include <filesystem>
#include "pfld/facet.hpp"

namespace pfld {

void GetFacets(pfld::facet_vec& facets, const std::filesystem::path& sfile, int n, bool bGenerate = false);
void GetFieldPoints(pfld::ptvec& points, const std::filesystem::path& sfile, int n, bool bGenerate = false);
void SaveResults(const pfld::valvec& res, const std::filesystem::path& sfile);
void LoadResults(pfld::valvec& res, const std::filesystem::path& sfile, int n);

} // namespace pfld
