#include "pfld_test_io.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <vector>

namespace pfld {

namespace fs = std::filesystem;

static fs::path resolve_path(const fs::path& path) {
	if (fs::exists(path)) return path;
	const fs::path filename = path.filename();
	const std::vector<fs::path> candidates = {
		filename,
		fs::path("test_data") / filename,
		fs::path("../test_data") / filename,
		fs::path("tests/test_data") / filename,
		fs::path("../tests/test_data") / filename,
		fs::path("../../tests/test_data") / filename,
		fs::path("pfld/tests/test_data") / filename,
		fs::path("../pfld/tests/test_data") / filename,
#ifdef PFLD_TEST_DATA_DIR
		fs::path(PFLD_TEST_DATA_DIR) / filename,
#endif
#ifdef PFLD_APP_DATA_DIR
		fs::path(PFLD_APP_DATA_DIR) / filename,
#endif
	};
	for (const auto& candidate : candidates) {
		if (fs::exists(candidate)) return candidate;
	}
	return path;
}

void GetFieldPoints(pfld::ptvec& points, const fs::path& sfile, const int n, bool bGenerate /*= false*/)
{
	if (bGenerate)
	{
		std::cout << "creating random points..." << "\n";
		std::srand(100);
		for (int i = 0; i < n; ++i)
		{
			points.emplace_back(point(rand() % 4000 - 2000, rand() % 40000 - 2000, rand() % 200));
		}
		std::ofstream file(sfile, std::ios::out);
		if (file.is_open())
		{
			for (auto it = points.begin(); it != points.end(); ++it)
			{
				file << it->x << " " << it->y << " " << it->z << "\n";
			}
			file.close();
		}
	}
	else
	{
		std::cout << "loading points..." << "\n";
		std::ifstream file(resolve_path(sfile), std::ios_base::in);
		if (file.is_open())
		{
			std::string stmp;
			int i = 0;
			while (std::getline(file, stmp))
			{
				std::istringstream buffer(stmp);
				std::vector<double> vln{ std::istream_iterator<double>(buffer),
					std::istream_iterator<double>() };
				points.emplace_back(pfld::point(vln[0], vln[1], vln[2]));
				i++;
				if (i >= n)
					break;
			}
			file.close();
		}
		std::cout << "loaded " << points.size() << " points." << "\n";
	}
}

void GetFacets(pfld::facet_vec& facets, const fs::path& sfile, const int n, bool bGenerate /*= false*/)
{
	if (bGenerate)
	{
		std::cout << "creating random facets..." << "\n";
		std::srand(1);
		for (int i = 0; i < n; ++i)
		{
			ptvec v{ point(rand() % 1001, rand() % 1001, -(rand() % 1001)),
				point(rand() % 1001, rand() % 1001, -rand() % 1001),
				point(rand() % 1001, rand() % 1001, -rand() % 1001) };
			facets.emplace_back(Facet(v));
		}

		std::ofstream file(sfile, std::ios::out);
		if (file.is_open())
		{
			int i = 0;
			for (auto it = facets.begin(); it != facets.end(); ++it)
			{
				ptvec& fpts = it->Data();
				file << "Facet " << i << " " << fpts.size() << "\n";
				for (auto itp = fpts.begin(); itp != fpts.end(); ++itp)
					file << itp->x << " " << itp->y << " " << itp->z << " ";
				file << "\n";
				i++;
			}
			file.close();
		}
	}
	else
	{
		std::cout << "loading facets..." << "\n";
		std::ifstream file(resolve_path(sfile), std::ios_base::in);
		if (file.is_open())
		{
			std::string stmp;
			ptvec v(3);
			int loadedFacets = 0;
			while (std::getline(file, stmp))
			{
				std::istringstream buffer(stmp);
				if (stmp.find("Facet") != std::string::npos)
				{
					// facet data
				}
				else
				{
					std::vector<double> vln{ std::istream_iterator<double>(buffer),
						std::istream_iterator<double>() };
					for (unsigned i = 0; i < vln.size() / 3; ++i)	{
						v[i] = point(vln[i * 3 + 0], vln[i * 3 + 1], vln[i * 3 + 2]);
					}
					facets.emplace_back(Facet(v));
					loadedFacets++;
					if (loadedFacets >= n)
						break;
				}
			}
			file.close();
		}
		std::cout << "loaded " << facets.size() << " facets." << "\n";
	}
}

void SaveResults(const pfld::valvec& res, const fs::path& sfile)
{
	std::cout << "saving results: " << sfile.string() << "\n";
	std::ofstream file(sfile, std::ios::out);
	if (file.is_open())
	{
		for (auto it = res.begin(); it != res.end(); ++it)
		{
			file << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
				<< *it << "\n";
		}
		file.close();
	}
}

void LoadResults(pfld::valvec& res, const fs::path& sfile, const int n)
{
	std::ifstream file(resolve_path(sfile), std::ios::in);
	if (file.is_open())
	{
		std::string stmp;
		int i = 0;
		while (std::getline(file, stmp))
		{
			std::istringstream buffer(stmp);
			std::vector<double> vln{ std::istream_iterator<double>(buffer),
					std::istream_iterator<double>() };
			if (!vln.empty()) {
				res.emplace_back(vln[0]);
				i++;
				if (i >= n)
					break;
			}
		}
		file.close();
	}
}

} // namespace pfld