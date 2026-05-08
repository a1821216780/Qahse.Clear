#pragma once

#include <string>
#include <vector>

#include "WaveL/WaveL_Type.hpp"

namespace wavel_detail
{
	struct WaveFieldCache
	{
		int nx = 0;
		int ny = 0;
		int nz = 0;
		int nt = 0;
		double dx = 0.0;
		double dy = 0.0;
		double dz = 0.0;
		double dt = 0.0;
		double zBottom = 0.0;
		std::vector<double> eta;
		std::vector<double> u;
		std::vector<double> v;
		std::vector<double> w;
		std::vector<double> ax;
		std::vector<double> ay;
		std::vector<double> az;
		std::vector<double> dynP;

		void Resize(int xCount, int yCount, int zCount, int tCount);
		std::size_t Index(int ix, int iy, int iz, int it) const;
	};

	class WaveKinematicsEngine
	{
	public:
		static WaveKinematics Evaluate(const std::vector<WaveTrain> &waveTrains,
			const WaveLInput &input,
			double x,
			double y,
			double z,
			double time);

		static WaveKinematics EvaluateFromCache(const WaveFieldCache &cache,
			double x,
			double y,
			double z,
			double time);

		static void BuildCache(const std::vector<WaveTrain> &waveTrains,
			const WaveLInput &input,
			WaveFieldCache &cache);

		static void WriteKinematicsSnapshots(const std::vector<WaveTrain> &waveTrains,
			const WaveLInput &input,
			const std::string &directory);
	};
}
