#ifndef PFLD_HPP_
#define PFLD_HPP_

#if defined(_WIN32) || defined(__WIN32__)
#if defined(PFLD_EXPORTS)
#define  PFLD_API __declspec(dllexport)
#else
#define  PFLD_API __declspec(dllimport)
#endif
#elif defined(linux) || defined(__linux)
#define PFLD_API
#else
#define PFLD_API
#endif

PFLD_API void Field_G(double* facets, const int num_facets,
	double* field_points, double* out_field, const int num_points);

#endif // PFLD_HPP_