#pragma once

#include <algorithm>
#include <future>
#include <span>
#include <thread>
#include <vector>

#include "facet.hpp"

namespace pfld {

namespace detail {

struct ThreadPartition {
	size_t num_threads{1};
};

[[nodiscard]] inline ThreadPartition calculate_partition(size_t length, size_t min_per_thread = 32) noexcept {
	if (length == 0) return {0};
	const size_t max_threads = (length + min_per_thread - 1) / min_per_thread;
	const size_t hardware_threads = std::max<size_t>(1, std::thread::hardware_concurrency());
	const size_t num_threads = std::min(hardware_threads, max_threads);
	return {num_threads};
}

// In-place parallel initialization of facets
inline void parallel_init_facets(pfld::facet_vec& facets) {
	if (facets.empty()) return;

	// Check if already initialized to avoid spawning unnecessary threads
	bool all_init = true;
	for (const auto& f : facets) {
		if (!f.is_initialized()) {
			all_init = false;
			break;
		}
	}
	if (all_init) return;

	const size_t length = facets.size();
	const auto [num_threads] = calculate_partition(length, 64);
	if (num_threads <= 1) {
		for (auto& f : facets) {
			f.Init();
		}
		return;
	}

	std::vector<std::jthread> threads;
	threads.reserve(num_threads);

	for (size_t i = 0; i < num_threads; ++i) {
		const size_t start_idx = i * length / num_threads;
		const size_t end_idx = (i + 1) * length / num_threads;
		threads.emplace_back([&facets, start_idx, end_idx]() {
			for (size_t j = start_idx; j < end_idx; ++j) {
				facets[j].Init();
			}
		});
	}
}

} // namespace detail

/**
 * @brief Serial 3D vector gravity field computation for all field points.
 * @param facets Facet list representing the 3D polyhedral bodies.
 * @param field_points Observation points.
 * @param out_field Output vector of 3D gravity vectors (accumulates or writes).
 */
inline void Field_G(pfld::facet_vec& facets, pfld::ptvec& field_points, pfld::ptvec& out_field)
{
	detail::parallel_init_facets(facets);

	if (out_field.size() < field_points.size()) {
		out_field.resize(field_points.size());
	}

	for (size_t i = 0; i < field_points.size(); ++i) {
		const auto& pt = field_points[i];
		auto& out = out_field[i];
		for (const auto& fct : facets) {
			fct.Fld_G(pt, out);
		}
	}
}

/**
 * @brief Parallel chunked Gz computation writing directly into pre-allocated output vector.
 * @param facets Facet list representing the 3D polyhedral bodies.
 * @param fldPoints Observation points.
 * @param outFld Pre-allocated output vector matching fldPoints.size().
 */
inline void Field_Gz(pfld::facet_vec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld)
{
	detail::parallel_init_facets(facets);

	const size_t length = fldPoints.size();
	if (length == 0) return;
	if (outFld.size() < length) {
		outFld.resize(length, 0.0);
	}

	const auto [num_threads] = detail::calculate_partition(length);
	if (num_threads <= 1) {
		for (size_t i = 0; i < length; ++i) {
			const auto& pt = fldPoints[i];
			double sum = 0.0;
			for (const auto& fct : facets) {
				sum += fct.field_gz(pt);
			}
			outFld[i] = sum;
		}
		return;
	}

	std::vector<std::jthread> threads;
	threads.reserve(num_threads);

	for (size_t i = 0; i < num_threads; ++i) {
		const size_t start_idx = i * length / num_threads;
		const size_t end_idx = (i + 1) * length / num_threads;
		threads.emplace_back([&facets, &fldPoints, &outFld, start_idx, end_idx]() {
			for (size_t j = start_idx; j < end_idx; ++j) {
				const auto& pt = fldPoints[j];
				double sum = 0.0;
				for (const auto& fct : facets) {
					sum += fct.field_gz(pt);
				}
				outFld[j] = sum;
			}
		});
	}
}

/**
 * @brief Parallel future/packaged_task-based Gz computation (appends results to out_field).
 * @param facets Facet list representing the 3D polyhedral bodies.
 * @param fldPoints Observation points.
 * @param outFld Output vector where chunk results are appended.
 */
inline void Field_Gz_(pfld::facet_vec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld)
{
	detail::parallel_init_facets(facets);

	const size_t length = fldPoints.size();
	if (length == 0) return;

	const auto [num_threads] = detail::calculate_partition(length);
	if (num_threads <= 1) {
		outFld.reserve(outFld.size() + length);
		for (const auto& pt : fldPoints) {
			double sum = 0.0;
			for (const auto& fct : facets) {
				sum += fct.field_gz(pt);
			}
			outFld.push_back(sum);
		}
		return;
	}

	std::vector<std::future<valvec>> futures(num_threads);
	std::vector<std::jthread> threads;
	threads.reserve(num_threads);

	for (size_t i = 0; i < num_threads; ++i) {
		const size_t start_idx = i * length / num_threads;
		const size_t end_idx = (i + 1) * length / num_threads;
		const size_t count = end_idx - start_idx;

		std::packaged_task<valvec()> task([&facets, &fldPoints, start_idx, count]() {
			valvec block_fld(count);
			for (size_t j = 0; j < count; ++j) {
				const auto& pt = fldPoints[start_idx + j];
				double sum = 0.0;
				for (const auto& fct : facets) {
					sum += fct.field_gz(pt);
				}
				block_fld[j] = sum;
			}
			return block_fld;
		});

		futures[i] = task.get_future();
		threads.emplace_back(std::move(task));
	}

	outFld.reserve(outFld.size() + length);
	for (size_t i = 0; i < num_threads; ++i) {
		valvec thread_result = futures[i].get();
		outFld.insert(outFld.end(), thread_result.begin(), thread_result.end());
	}
}

/**
 * @brief Naive/serial single-threaded Gz computation.
 * @param facets Facet list representing the 3D polyhedral bodies.
 * @param fldPoints Observation points.
 * @param outFld Output vector matching fldPoints.size().
 */
inline void Field_Gz__(pfld::facet_vec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld)
{
	for (auto& fct : facets) {
		fct.Init();
	}

	if (outFld.size() < fldPoints.size()) {
		outFld.resize(fldPoints.size(), 0.0);
	}

	for (size_t i = 0; i < fldPoints.size(); ++i) {
		const auto& pt = fldPoints[i];
		double sum = 0.0;
		for (const auto& fct : facets) {
			sum += fct.field_gz(pt);
		}
		outFld[i] = sum;
	}
}

// --- Modern C++20 Convenience Overloads ---

/**
 * @brief Computes vertical gravity field Gz in parallel and returns the resulting vector.
 */
[[nodiscard]] inline valvec Field_Gz(pfld::facet_vec& facets, const pfld::ptvec& field_points)
{
	valvec result(field_points.size(), 0.0);
	ptvec pts_copy = field_points;
	Field_Gz(facets, pts_copy, result);
	return result;
}

/**
 * @brief Computes 3D gravity vector field in parallel and returns the resulting vector.
 */
[[nodiscard]] inline ptvec Field_G(pfld::facet_vec& facets, const pfld::ptvec& field_points)
{
	ptvec result(field_points.size(), point{0, 0, 0});
	ptvec pts_copy = field_points;
	Field_G(facets, pts_copy, result);
	return result;
}

/**
 * @brief High-performance parallel computation using non-owning spans over pre-initialized facets.
 */
inline void Field_Gz(std::span<const facet> facets, std::span<const point> field_points, std::span<double> out_field)
{
	const size_t length = std::min(field_points.size(), out_field.size());
	if (length == 0) return;

	const auto [num_threads] = detail::calculate_partition(length);
	if (num_threads <= 1) {
		for (size_t i = 0; i < length; ++i) {
			const auto& pt = field_points[i];
			double sum = 0.0;
			for (const auto& fct : facets) {
				sum += fct.field_gz(pt);
			}
			out_field[i] = sum;
		}
		return;
	}

	std::vector<std::jthread> threads;
	threads.reserve(num_threads);

	for (size_t i = 0; i < num_threads; ++i) {
		const size_t start_idx = i * length / num_threads;
		const size_t end_idx = (i + 1) * length / num_threads;
		threads.emplace_back([facets, field_points, out_field, start_idx, end_idx]() {
			for (size_t j = start_idx; j < end_idx; ++j) {
				const auto& pt = field_points[j];
				double sum = 0.0;
				for (const auto& fct : facets) {
					sum += fct.field_gz(pt);
				}
				out_field[j] = sum;
			}
		});
	}
}

} // namespace pfld
