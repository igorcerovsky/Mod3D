#include "pfld_compute.hpp"
#include <thread>
#include <future>
#include <vector>
#include <algorithm>
#include <functional>

namespace pfld {

namespace {

// Helper to determine optimal thread count and chunk size for a range
struct ThreadPartition {
	unsigned long num_threads{1};
	unsigned long block_size{0};
};

ThreadPartition calculate_partition(unsigned long length, unsigned long min_per_thread = 25) {
	if (length == 0) return {0, 0};
	const unsigned long max_threads = (length + min_per_thread - 1) / min_per_thread;
	const unsigned long hardware_threads = std::thread::hardware_concurrency();
	const unsigned long num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2ul, max_threads);
	const unsigned long block_size = length / num_threads;
	return {num_threads, block_size};
}

// In-place parallel initialization of facets
void parallel_init_facets(pfld::facet_vec& facets) {
	const unsigned long length = static_cast<unsigned long>(facets.size());
	const auto [num_threads, block_size] = calculate_partition(length);
	if (num_threads == 0) return;

	std::vector<std::jthread> threads(num_threads);
	auto block_start = facets.begin();
	for (unsigned long i = 0; i < num_threads; ++i) {
		auto block_end = (i == num_threads - 1) ? facets.end() : block_start + block_size;
		threads[i] = std::jthread([block_start, block_end]() {
			for (auto it = block_start; it != block_end; ++it) {
				it->Init();
			}
		});
		block_start = block_end;
	}
}

} // anonymous namespace

// Serial 3D vector gravity field computation
void Field_G(pfld::facet_vec& facets, pfld::ptvec& field_points, pfld::ptvec& out_field)
{
	if (field_points.size() != out_field.size())
		return;
	auto itf = out_field.begin();
	for (auto itp = field_points.begin(); itp != field_points.end(); ++itp, ++itf)
	{
		for (auto it = facets.begin(); it != facets.end(); ++it)
			it->operator()(*itp, *itf);
	}
}

// Parallel chunked Gz computation (pre-allocated output vector)
void Field_Gz(pfld::facet_vec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld)
{
	parallel_init_facets(facets);

	const unsigned long length = static_cast<unsigned long>(fldPoints.size());
	const auto [num_threads, block_size] = calculate_partition(length);
	if (num_threads == 0) return;

	std::vector<std::jthread> threads(num_threads);
	auto pt_start = fldPoints.begin();
	auto fld_start = outFld.begin();
	for (unsigned long i = 0; i < num_threads; ++i) {
		auto pt_end = (i == num_threads - 1) ? fldPoints.end() : pt_start + block_size;
		const auto count = std::distance(pt_start, pt_end);
		threads[i] = std::jthread([pt_start, pt_end, fld_start, &facets]() {
			auto itFld = fld_start;
			for (auto it = pt_start; it != pt_end; ++it, ++itFld) {
				double_pfld f(0.0);
				for (auto& fct : facets) {
					fct.Fld_Gz(*it, f);
				}
				*itFld = f;
			}
		});
		pt_start = pt_end;
		fld_start += count;
	}
}

// Parallel future/task-based Gz computation (accumulates/appends to outFld)
void Field_Gz_(pfld::facet_vec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld)
{
	parallel_init_facets(facets);

	const unsigned long length = static_cast<unsigned long>(fldPoints.size());
	const auto [num_threads, block_size] = calculate_partition(length);
	if (num_threads == 0) return;

	std::vector<std::future<valvec>> futures(num_threads);
	std::vector<std::jthread> threads(num_threads);

	auto pt_start = fldPoints.begin();
	for (unsigned long i = 0; i < num_threads; ++i) {
		auto pt_end = (i == num_threads - 1) ? fldPoints.end() : pt_start + block_size;
		std::packaged_task<valvec()> task([pt_start, pt_end, &facets]() {
			valvec block_fld(std::distance(pt_start, pt_end), 0.0);
			auto itFld = block_fld.begin();
			for (auto it = pt_start; it != pt_end; ++it, ++itFld) {
				double_pfld f(0.0);
				for (auto& fct : facets) {
					fct.Fld_Gz(*it, f);
				}
				*itFld = f;
			}
			return block_fld;
		});
		futures[i] = task.get_future();
		threads[i] = std::jthread(std::move(task));
		pt_start = pt_end;
	}

	for (unsigned long i = 0; i < num_threads; ++i) {
		valvec thread_result = futures[i].get();
		outFld.insert(outFld.end(), thread_result.begin(), thread_result.end());
	}
}

// Serial / naive Gz computation
void Field_Gz__(pfld::facet_vec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld)
{
	for (auto& fct : facets)
		fct.Init();

	auto itFld = outFld.begin();
	for (auto& pt : fldPoints)
	{
		double_pfld f(0.0);
		for (auto& fct : facets)
		{
			fct.Fld_Gz(pt, f);
		}
		*itFld = f;
		++itFld;
	}
}

} // namespace pfld