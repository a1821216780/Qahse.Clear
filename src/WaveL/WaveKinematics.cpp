#include "WaveL/WaveKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "../IO/vtu11-cpp17.hpp"
#include "../Params.h"
#include "WaveL/IO/LocaleString_WaveL.hpp"

namespace
{
	constexpr double kDeepWaterThreshold = 20.0;
	constexpr int kVtkVertex = 1;

	double CacheX(const wavel_detail::WaveFieldCache &cache, int ix)
	{
		return (cache.nx == 1) ? 0.0 : (static_cast<double>(ix) - 0.5 * static_cast<double>(cache.nx - 1)) * cache.dx;
	}

	double CacheY(const wavel_detail::WaveFieldCache &cache, int iy)
	{
		return (cache.ny == 1) ? 0.0 : (static_cast<double>(iy) - 0.5 * static_cast<double>(cache.ny - 1)) * cache.dy;
	}

	double CacheZ(const wavel_detail::WaveFieldCache &cache, int iz)
	{
		return cache.zBottom + static_cast<double>(iz) * cache.dz;
	}

	double WrapTime(double time, double duration)
	{
		if (duration <= 0.0)
			return time;
		double wrapped = std::fmod(time, duration);
		if (wrapped < 0.0)
			wrapped += duration;
		return wrapped;
	}

	double MacCamyFuchsFactor(double waveNumber, double depth, double diameter)
	{
		if (diameter <= 0.0 || waveNumber <= 0.0)
			return 1.0;

		const double kd = waveNumber * depth;
		const double x = 0.5 * diameter * waveNumber;
		const double factor = 1.05 * std::tanh(kd) /
			std::pow(std::pow(std::fabs(x - 0.2), 2.2) + 1.0, 0.85);
		return std::min(1.0, factor);
	}

	double MacCamyFuchsPhaseShift(double ratio)
	{
		static const double ratios[] = {
			0.314159,0.320571,0.327249,0.334212,0.341477,0.349066,0.356999,0.365301,0.373999,0.383121,0.392699,0.402768,
			0.413367,0.42454,0.436332,0.448799,0.461999,0.475999,0.490874,0.506708,0.523599,0.541654,0.560999,0.581776,
			0.604152,0.628319,0.654498,0.682955,0.713998,0.747998,0.785398,0.826735,0.872665,0.923998,0.981748,1.047198,
			1.121997,1.208305,1.308997,1.427997,1.570796,1.745329,1.963495,2.243995,2.617994,3.141593,3.205707,3.272492,
			3.34212,3.414775,3.490659,3.569992,3.653015,3.739991,3.831211,3.926991,4.027683,4.133675,4.245395,4.363323,
			4.48799,4.619989,4.759989,4.908739,5.067085,5.235988,5.416539,5.609987,5.817764,6.041524,6.283185,6.544985,
			6.829549,7.139983,7.479983,7.853982,8.267349,8.726646,9.239978,9.817477,10.47198,11.21997,12.08305,13.08997,
			14.27997,15.70796,17.45329,19.63495,22.43995,26.17994,31.41593,39.26991,52.35988,78.53982,157.0796
		};
		static const double phases[] = {
			-443.14,-431.77,-420.39,-409.03,-397.66,-386.31,-374.96,-363.61,-352.27,-340.94,-329.62,-318.3,
			-306.99,-295.7,-284.21,-273.13,-261.87,-250.62,-239.38,-228.16,-216.97,-205.78,-194.62,-183.48,-173.03,
			-161.95,-150.91,-139.91,-128.95,-118.05,-107.20,-96.43,-85.74,-75.17,-64.67,-54.34,-44.10,-34.26,-24.62,
			-15.33,-6.53,1.61,8.86,14.83,18.97,20.54,20.52,20.47,20.39,20.26,20.10,19.91,19.68,19.41,19.11,18.77,
			18.39,17.98,17.54,17.07,16.56,16.03,15.47,14.48,14.27,13.64,13.00,12.34,11.67,11.00,10.32,9.64,8.96,8.29,
			7.63,6.98,6.35,5.74,5.15,4.59,4.05,3.54,3.06,2.61,2.20,1.82,1.47,1.16,0.89,0.65,0.45,0.29,0.16,0.07,0.02
		};

		const std::size_t count = sizeof(ratios) / sizeof(ratios[0]);
		if (ratio <= ratios[0])
			return phases[0] * M_PI / 180.0;
		if (ratio >= ratios[count - 1])
			return phases[count - 1] * M_PI / 180.0;

		for (std::size_t i = 0; i + 1 < count; ++i)
		{
			if (ratio >= ratios[i] && ratio <= ratios[i + 1])
			{
				const double w = (ratio - ratios[i]) / (ratios[i + 1] - ratios[i]);
				return (phases[i] + (phases[i + 1] - phases[i]) * w) * M_PI / 180.0;
			}
		}
		return 0.0;
	}

	double ApplyStretching(double z, double eta, double depth, WaveStretching stretching)
	{
		switch (stretching)
		{
		case WaveStretching::NONE:
			return z;
		case WaveStretching::VERTICAL:
			return (z > 0.0) ? 0.0 : z;
		case WaveStretching::EXTRAPOLATION:
			return z;
		case WaveStretching::WHEELER:
			return depth * (z - eta) / (eta + depth);
		default:
			return z;
		}
	}

	struct PointMesh
	{
		std::vector<double> points_;
		std::vector<vtu11::VtkIndexType> connectivity_;
		std::vector<vtu11::VtkIndexType> offsets_;
		std::vector<vtu11::VtkCellType> types_;

		const std::vector<double> &points() { return points_; }
		const std::vector<vtu11::VtkIndexType> &connectivity() { return connectivity_; }
		const std::vector<vtu11::VtkIndexType> &offsets() { return offsets_; }
		const std::vector<vtu11::VtkCellType> &types() { return types_; }
		std::size_t numberOfPoints() { return points_.size() / 3; }
		std::size_t numberOfCells() { return types_.size(); }
	};
}

void wavel_detail::WaveFieldCache::Resize(int xCount, int yCount, int zCount, int tCount)
{
	nx = xCount;
	ny = yCount;
	nz = zCount;
	nt = tCount;
	const std::size_t total = static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny) * static_cast<std::size_t>(nz) * static_cast<std::size_t>(nt);
	eta.assign(total, 0.0);
	u.assign(total, 0.0);
	v.assign(total, 0.0);
	w.assign(total, 0.0);
	ax.assign(total, 0.0);
	ay.assign(total, 0.0);
	az.assign(total, 0.0);
	dynP.assign(total, 0.0);
}

std::size_t wavel_detail::WaveFieldCache::Index(int ix, int iy, int iz, int it) const
{
	return static_cast<std::size_t>(((it * nz + iz) * ny + iy) * nx + ix);
}

WaveKinematics wavel_detail::WaveKinematicsEngine::Evaluate(const std::vector<WaveTrain> &waveTrains,
	const WaveLInput &input,
	double x,
	double y,
	double z,
	double time)
{
	WaveKinematics result;
	if (waveTrains.empty())
		return result;

	const double localTime = WrapTime(time + input.timeOffset, input.simDuration);
	result.eta = 0.0;
	for (const auto &train : waveTrains)
	{
		const double X = x * train.cosDir + y * train.sinDir;
		result.eta += train.amplitude * std::sin(train.wavenumber * X - train.omega * localTime + train.phase);
	}

	const double eta = result.eta;
	double evalZ = z;
	if (input.stretching == WaveStretching::NONE)
	{
		if (z > 0.0)
			return result;
	}
	else if (z > eta)
	{
		return result;
	}

	evalZ = ApplyStretching(z, eta, input.waterDepth, input.stretching);
	if (evalZ + input.waterDepth < 0.0)
		return result;

	for (const auto &train : waveTrains)
	{
		const double X = x * train.cosDir + y * train.sinDir;
		const double kd = train.wavenumber * input.waterDepth;
		const bool deepWater = kd > kDeepWaterThreshold || input.waterDepth > 100.0;
		double depthVarXY = 0.0;
		double depthVarZ = 0.0;
		if (deepWater)
		{
			depthVarXY = std::exp(train.wavenumber * evalZ);
			depthVarZ = depthVarXY;
			if (input.stretching == WaveStretching::EXTRAPOLATION && z > 0.0)
			{
				depthVarXY = 1.0 + train.wavenumber * z;
				depthVarZ = depthVarXY;
			}
		}
		else
		{
			const double sinhDen = std::sinh(kd);
			depthVarXY = std::cosh(train.wavenumber * (evalZ + input.waterDepth)) / sinhDen;
			depthVarZ = std::sinh(train.wavenumber * (evalZ + input.waterDepth)) / sinhDen;
			if (input.stretching == WaveStretching::EXTRAPOLATION && z > 0.0)
			{
				const double z0xy = std::cosh(kd) / sinhDen;
				const double z0z = 1.0;
				const double dxy = train.wavenumber;
				const double dz = train.wavenumber * z0xy;
				depthVarXY = z0xy + z * dxy;
				depthVarZ = z0z + z * dz;
			}
		}

		double phase = train.wavenumber * X - train.omega * localTime + train.phase;
		double cosPhase = std::cos(phase);
		double sinPhase = std::sin(phase);
		double accFactor = 1.0;
		if (input.mcfDiameter > 0.0)
		{
			accFactor = MacCamyFuchsFactor(train.wavenumber, input.waterDepth, input.mcfDiameter);
			const double mcfPhaseShift = MacCamyFuchsPhaseShift(1.0 / (0.5 * input.mcfDiameter * train.wavenumber));
			phase += mcfPhaseShift;
			cosPhase = std::cos(phase);
			sinPhase = std::sin(phase);
		}

		result.vel[0] += train.A_omega * train.cosDir * depthVarXY * sinPhase;
		result.vel[1] += train.A_omega * train.sinDir * depthVarXY * sinPhase;
		result.vel[2] += -train.A_omega * depthVarZ * cosPhase;

		result.acc[0] += -train.A_omega2 * train.cosDir * depthVarXY * cosPhase * accFactor;
		result.acc[1] += -train.A_omega2 * train.sinDir * depthVarXY * cosPhase * accFactor;
		result.acc[2] += -train.A_omega2 * depthVarZ * sinPhase * accFactor;

		const double pressureFactor = deepWater
			? std::exp(train.wavenumber * evalZ)
			: std::cosh(train.wavenumber * (evalZ + input.waterDepth)) / std::cosh(kd);
		result.dynP += DENSITYWATER * GRAVITY * train.amplitude * pressureFactor * sinPhase;
	}

	return result;
}

WaveKinematics wavel_detail::WaveKinematicsEngine::EvaluateFromCache(const WaveFieldCache &cache,
	double x,
	double y,
	double z,
	double time)
{
	WaveKinematics result;
	if (cache.nx <= 0 || cache.ny <= 0 || cache.nz <= 0 || cache.nt <= 0)
		return result;

	const double duration = cache.dt * static_cast<double>(std::max(1, cache.nt - 1));
	const double localTime = WrapTime(time, duration);
	const auto indexWeight = [](double value, double origin0, double spacing, int count) {
		if (count <= 1 || spacing <= 0.0)
			return std::tuple<int, int, double>(0, 0, 0.0);
		double coord = (value - origin0) / spacing;
		coord = std::max(0.0, std::min(coord, static_cast<double>(count - 1)));
		const int i0 = static_cast<int>(std::floor(coord));
		const int i1 = std::min(count - 1, i0 + 1);
		return std::tuple<int, int, double>(i0, i1, coord - static_cast<double>(i0));
	};

	const auto [ix0, ix1, wx] = indexWeight(x, CacheX(cache, 0), cache.dx, cache.nx);
	const auto [iy0, iy1, wy] = indexWeight(y, CacheY(cache, 0), cache.dy, cache.ny);
	const auto [iz0, iz1, wz] = indexWeight(z, CacheZ(cache, 0), cache.dz, cache.nz);
	const auto [it0, it1, wt] = indexWeight(localTime, 0.0, cache.dt, cache.nt);

	auto sample = [&](const std::vector<double> &values) {
		const auto lerp = [](double a, double b, double w) { return a + (b - a) * w; };
		auto cell = [&](int it) {
			const double c000 = values[cache.Index(ix0, iy0, iz0, it)];
			const double c100 = values[cache.Index(ix1, iy0, iz0, it)];
			const double c010 = values[cache.Index(ix0, iy1, iz0, it)];
			const double c110 = values[cache.Index(ix1, iy1, iz0, it)];
			const double c001 = values[cache.Index(ix0, iy0, iz1, it)];
			const double c101 = values[cache.Index(ix1, iy0, iz1, it)];
			const double c011 = values[cache.Index(ix0, iy1, iz1, it)];
			const double c111 = values[cache.Index(ix1, iy1, iz1, it)];
			const double c00 = lerp(c000, c100, wx);
			const double c10 = lerp(c010, c110, wx);
			const double c01 = lerp(c001, c101, wx);
			const double c11 = lerp(c011, c111, wx);
			const double c0 = lerp(c00, c10, wy);
			const double c1 = lerp(c01, c11, wy);
			return lerp(c0, c1, wz);
		};
		const double v0 = cell(it0);
		const double v1 = cell(it1);
		return lerp(v0, v1, wt);
	};

	result.eta = sample(cache.eta);
	result.vel[0] = sample(cache.u);
	result.vel[1] = sample(cache.v);
	result.vel[2] = sample(cache.w);
	result.acc[0] = sample(cache.ax);
	result.acc[1] = sample(cache.ay);
	result.acc[2] = sample(cache.az);
	result.dynP = sample(cache.dynP);
	return result;
}

void wavel_detail::WaveKinematicsEngine::BuildCache(const std::vector<WaveTrain> &waveTrains,
	const WaveLInput &input,
	WaveFieldCache &cache)
{
	if (input.gridNX <= 0 || input.gridNY <= 0 || input.gridNZ <= 0)
		throw std::runtime_error(L_WAVEL_GridInvalid);

	const int nt = std::max(1, static_cast<int>(std::llround(input.simDuration / input.timeStep)) + 1);
	cache.dx = input.gridDX > 0.0 ? input.gridDX : 1.0;
	cache.dy = input.gridDY > 0.0 ? input.gridDY : 1.0;
	cache.dz = input.gridDZ > 0.0 ? input.gridDZ : ((input.gridNZ > 1) ? input.waterDepth / static_cast<double>(input.gridNZ - 1) : 1.0);
	cache.dt = input.timeStep;
	cache.zBottom = -input.waterDepth;
	cache.Resize(input.gridNX, input.gridNY, input.gridNZ, nt);

	for (int it = 0; it < nt; ++it)
	{
		const double time = static_cast<double>(it) * input.timeStep;
		for (int iz = 0; iz < input.gridNZ; ++iz)
		{
			const double z = cache.zBottom + static_cast<double>(iz) * cache.dz;
			for (int iy = 0; iy < input.gridNY; ++iy)
			{
				const double y = (input.gridNY == 1) ? 0.0 : (static_cast<double>(iy) - 0.5 * static_cast<double>(input.gridNY - 1)) * cache.dy;
				for (int ix = 0; ix < input.gridNX; ++ix)
				{
					const double x = CacheX(cache, ix);
					const auto kin = Evaluate(waveTrains, input, x, y, z, time);
					const std::size_t idx = cache.Index(ix, iy, iz, it);
					cache.eta[idx] = kin.eta;
					cache.u[idx] = kin.vel[0];
					cache.v[idx] = kin.vel[1];
					cache.w[idx] = kin.vel[2];
					cache.ax[idx] = kin.acc[0];
					cache.ay[idx] = kin.acc[1];
					cache.az[idx] = kin.acc[2];
					cache.dynP[idx] = kin.dynP;
				}
			}
		}
	}
}

void wavel_detail::WaveKinematicsEngine::WriteKinematicsSnapshots(const std::vector<WaveTrain> &waveTrains,
	const WaveLInput &input,
	const std::string &directory)
{
	if (!input.outputKinematicsGrid)
		return;

	std::filesystem::create_directories(directory);
	WaveFieldCache cache;
	BuildCache(waveTrains, input, cache);

	PointMesh mesh;
	mesh.points_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz * 3);
	mesh.connectivity_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz);
	mesh.offsets_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz);
	mesh.types_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz);

	vtu11::VtkIndexType offset = 0;
	for (int iz = 0; iz < cache.nz; ++iz)
	{
		const double z = cache.zBottom + static_cast<double>(iz) * cache.dz;
		for (int iy = 0; iy < cache.ny; ++iy)
		{
			const double y = (cache.ny == 1) ? 0.0 : (static_cast<double>(iy) - 0.5 * static_cast<double>(cache.ny - 1)) * cache.dy;
				for (int ix = 0; ix < cache.nx; ++ix)
				{
					const double x = CacheX(cache, ix);
					mesh.points_.push_back(x);
				mesh.points_.push_back(y);
				mesh.points_.push_back(z);
				mesh.connectivity_.push_back(offset);
				++offset;
				mesh.offsets_.push_back(offset);
				mesh.types_.push_back(kVtkVertex);
			}
		}
	}

	const std::size_t pointCount = static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz;
	for (int it = 0; it < cache.nt; ++it)
	{
		std::vector<double> velocity;
		std::vector<double> acceleration;
		std::vector<double> pressure;
		velocity.reserve(pointCount * 3);
		acceleration.reserve(pointCount * 3);
		pressure.reserve(pointCount);

		for (int iz = 0; iz < cache.nz; ++iz)
		{
			for (int iy = 0; iy < cache.ny; ++iy)
			{
				for (int ix = 0; ix < cache.nx; ++ix)
				{
					const std::size_t idx = cache.Index(ix, iy, iz, it);
					velocity.push_back(cache.u[idx]);
					velocity.push_back(cache.v[idx]);
					velocity.push_back(cache.w[idx]);
					acceleration.push_back(cache.ax[idx]);
					acceleration.push_back(cache.ay[idx]);
					acceleration.push_back(cache.az[idx]);
					pressure.push_back(cache.dynP[idx]);
				}
			}
		}

		const std::vector<vtu11::DataSetInfo> infos = {
			{"velocity", vtu11::DataSetType::PointData, 3},
			{"acceleration", vtu11::DataSetType::PointData, 3},
			{"dynamic_pressure", vtu11::DataSetType::PointData, 1}
		};
		const std::vector<vtu11::DataSetData> data = {velocity, acceleration, pressure};
		const std::filesystem::path filePath = std::filesystem::path(directory) / ("wave_" + std::to_string(it) + ".vtu");
		vtu11::writeVtu(filePath.string(), mesh, infos, data, "Ascii");
	}
}
