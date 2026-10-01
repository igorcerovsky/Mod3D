#ifndef _PFLD_TEST_IO_H__
#define _PFLD_TEST_IO_H__

#include "facet.hpp"
#include <string>
#include <filesystem>

namespace pfld {

void GetFacets(pfld::facet_vec& facets, const std::filesystem::path& sfile, const int n, bool bGenerate = false);
void GetFieldPoints(pfld::ptvec& points, const std::filesystem::path& sfile, const int n, bool bGenerate = false);
void SaveResults(const pfld::valvec& res, const std::filesystem::path& sfile);
void LoadResults(pfld::valvec& res, const std::filesystem::path& sfile, const int n);

} // namespace pfld

#endif //_PFLD_TEST_IO_H__