#include "WaveL/WaveSpectrum.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"
#include "WaveL/WaveKinematics.hpp"

namespace
{
using wavel_math::kPi;
using wavel_math::kTiny;

struct SpectrumPoint
{
	double f = 0.0;
	double s = 0.0;
};

struct OchiParameters
{
	double hs1 = 0.0;
	double hs2 = 0.0;
	double f1 = 0.0;
	double f2 = 0.0;
	double lambda1 = 0.0;
	double lambda2 = 0.0;
};

std::vector<std::vector<double>> ReadRows(const std::string &path, std::size_t minCols)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open WaveL data file: " + path);

	std::vector<std::vector<double>> rows;
	for (const auto &line : ZFile::ReadAllLines(path))
	{
		std::vector<double> row;
		if (!Serializer::TryParseNumberRow(line, row) || row.size() < minCols)
			continue;
		rows.push_back(std::move(row));
	}
	return rows;
}

double JonswapSpectrum(double f, const WaveLInput &input, bool forceIssc)
{
	if (f <= kTiny || input.hs <= kTiny || input.tp <= kTiny)
		return 0.0;

	const double fp = 1.0 / input.tp;
	double gamma = forceIssc ? 1.0 : input.gamma;
	if (!forceIssc && input.autoGamma)
	{
		const double ratio = input.tp / std::sqrt(input.hs);
		if (ratio <= 3.6)
			gamma = 5.0;
		else if (ratio <= 5.0)
			gamma = std::exp(5.75 - 1.15 * ratio);
		else
			gamma = 1.0;
	}

	double sigma = f <= fp ? input.sigma1 : input.sigma2;
	if (forceIssc || input.autoSigma)
		sigma = f <= fp ? 0.07 : 0.09;

	const double fRatio = f / fp;
	const double peak = std::exp(-0.5 * std::pow((fRatio - 1.0) / sigma, 2.0));
	return 0.3125 * input.hs * input.hs * input.tp *
	       std::pow(fRatio, -5.0) *
	       std::exp(-1.25 * std::pow(fRatio, -4.0)) *
	       (1.0 - 0.287 * std::log(gamma)) *
	       std::pow(gamma, peak);
}

double TorsethaugenSpectrum(double f, const WaveLInput &input)
{
	if (f <= kTiny || input.hs <= kTiny || input.tp <= kTiny)
		return 0.0;

	const double fp = 1.0 / input.tp;

	const double aF = 6.6;
	const double aE = 2.0;
	const double aU = 25.0;
	const double a10 = 0.7;
	const double a1 = 0.5;
	const double kG = 35.0;
	const double b1 = 2.0;
	const double a20 = 0.6;
	const double a2 = 0.3;
	const double a3 = 6.0;

	const double tpf = aF * std::pow(input.hs, 1.0 / 3.0);
	const double tl = aE * std::sqrt(input.hs);
	const double tu = aU;
	double el = (tpf - input.tp) / std::max(tpf - tl, kTiny);
	double eu = (input.tp - tpf) / std::max(tu - tpf, kTiny);
	if (input.tp < tl)
		el = 1.0;
	if (input.tp > tu)
		eu = 1.0;

	double h1 = 0.0;
	double tp1 = input.tp;
	double gamma1 = 1.0;
	double h2 = 0.0;
	double tp2 = input.tp;
	if (input.tp <= tpf)
	{
		const double r = (1.0 - a10) * std::exp(-std::pow(el / a1, 2.0)) + a10;
		h1 = r * input.hs;
		tp1 = input.tp;
		const double steepness = 2.0 * kPi / input.gravity * h1 / (tp1 * tp1);
		gamma1 = kG * std::pow(std::max(steepness, kTiny), 6.0 / 7.0);
		h2 = std::sqrt(std::max(1.0 - r * r, 0.0)) * input.hs;
		tp2 = tpf + b1;
	}
	else
	{
		const double r = (1.0 - a20) * std::exp(-std::pow(eu / a2, 2.0)) + a20;
		h1 = r * input.hs;
		tp1 = input.tp;
		const double steepness = 2.0 * kPi / input.gravity * input.hs / (tpf * tpf);
		gamma1 = kG * std::pow(std::max(steepness, kTiny), 6.0 / 7.0) * (1.0 + a3 * eu);
		h2 = std::sqrt(std::max(1.0 - r * r, 0.0)) * input.hs;
		tp2 = aF * std::pow(std::max(h2, kTiny), 1.0 / 3.0);
	}

	if (!input.autoGamma)
		gamma1 = input.gamma;

	double sigma = f <= fp ? input.sigma1 : input.sigma2;
	if (input.autoSigma)
		sigma = f <= fp ? 0.07 : 0.09;
	gamma1 = std::max(gamma1, 1.0 + kTiny);
	sigma = std::max(sigma, kTiny);

	const double g0 = 3.26;
	const double ay = (1.0 + 1.1 * std::pow(std::log(gamma1), 1.19)) / gamma1;
	const double f1n = std::max(f * tp1, kTiny);
	const double f2n = std::max(f * tp2, kTiny);
	const double s1 = g0 * ay * std::pow(f1n, -4.0) *
	                  std::exp(-std::pow(f1n, -4.0)) *
	                  std::pow(gamma1, std::exp(-std::pow(f1n - 1.0, 2.0) / (2.0 * sigma * sigma)));
	const double s2 = g0 * std::pow(f2n, -4.0) * std::exp(-std::pow(f2n, -4.0));
	return input.torsethaugenDoublePeak ? (s1 + s2) : s1;
}

OchiParameters EffectiveOchiParameters(const WaveLInput &input)
{
	if (input.autoOchi)
	{
		return {
			0.84 * input.hs,
			0.54 * input.hs,
			0.7 * std::exp(-0.046 * input.hs) / (2.0 * kPi),
			1.15 * std::exp(-0.039 * input.hs) / (2.0 * kPi),
			3.0,
			1.54 * std::exp(-0.062 * input.hs)};
	}

	return {
		input.ochiHs1,
		input.ochiHs2,
		input.ochiF1,
		input.ochiF2,
		input.ochiLambda1,
		input.ochiLambda2};
}

double OchiHubbleSpectrum(double f, const WaveLInput &input)
{
	if (f <= kTiny)
		return 0.0;
	const auto p = EffectiveOchiParameters(input);
	if (p.hs1 <= kTiny || p.hs2 <= kTiny || p.f1 <= kTiny || p.f2 <= kTiny ||
	    p.lambda1 <= kTiny || p.lambda2 <= kTiny)
		throw std::runtime_error("WaveL Ochi-Hubble requires positive Hs1/Hs2/F1/F2/Lambda1/Lambda2");

	const double w1 = p.f1 * 2.0 * kPi;
	const double w2 = p.f2 * 2.0 * kPi;
	const double w = f * 2.0 * kPi;
	const double c1 = (4.0 * p.lambda1 + 1.0) / 4.0;
	const double c2 = (4.0 * p.lambda2 + 1.0) / 4.0;
	const double s1 = 0.25 * std::pow(c1 * std::pow(w1, 4.0), p.lambda1) /
	                  std::tgamma(p.lambda1) * p.hs1 * p.hs1 /
	                  std::pow(w, 4.0 * p.lambda1 + 1.0) *
	                  std::exp(-c1 * std::pow(w1 / w, 4.0));
	const double s2 = 0.25 * std::pow(c2 * std::pow(w2, 4.0), p.lambda2) /
	                  std::tgamma(p.lambda2) * p.hs2 * p.hs2 /
	                  std::pow(w, 4.0 * p.lambda2 + 1.0) *
	                  std::exp(-c2 * std::pow(w2 / w, 4.0));
	return s1 + s2;
}

double TargetSpectrumHs(const WaveLInput &input)
{
	if (input.waveType == WaveType::OCHI_HUBBLE)
	{
		const auto p = EffectiveOchiParameters(input);
		return std::sqrt(p.hs1 * p.hs1 + p.hs2 * p.hs2);
	}
	return input.hs;
}

std::vector<SpectrumPoint> ReadUserSpectrum(const std::string &path)
{
	const auto rows = ReadRows(path, 2);
	std::vector<SpectrumPoint> data;
	data.reserve(rows.size());
	for (const auto &row : rows)
	{
		if (row[0] >= 0.0 && row[1] >= 0.0)
			data.push_back({row[0], row[1]});
	}
	std::sort(data.begin(), data.end(), [](const auto &a, const auto &b) { return a.f < b.f; });
	if (data.size() < 2)
		throw std::runtime_error("WaveL user spectrum requires at least two numeric rows");
	return data;
}

double InterpSpectrum(const std::vector<SpectrumPoint> &data, double f)
{
	if (data.empty() || f < data.front().f || f > data.back().f)
		return 0.0;
	const auto upper = std::upper_bound(data.begin(), data.end(), f, [](double value, const SpectrumPoint &point) {
		return value < point.f;
	});
	if (upper == data.begin())
		return upper->s;
	if (upper == data.end())
		return data.back().s;
	const auto left = std::prev(upper);
	const double span = std::max(upper->f - left->f, kTiny);
	const double alpha = (f - left->f) / span;
	return left->s * (1.0 - alpha) + upper->s * alpha;
}

double SpectrumAt(double f, const WaveLInput &input, const std::vector<SpectrumPoint> &userSpectrum)
{
	switch (input.waveType)
	{
	case WaveType::JONSWAP:
		return JonswapSpectrum(f, input, false);
	case WaveType::ISSC:
		return JonswapSpectrum(f, input, true);
	case WaveType::TORSETHAUGEN:
		return TorsethaugenSpectrum(f, input);
	case WaveType::OCHI_HUBBLE:
		return OchiHubbleSpectrum(f, input);
	case WaveType::USER_SPECTRUM:
		return InterpSpectrum(userSpectrum, f);
	default:
		return 0.0;
	}
}

std::pair<double, double> FrequencyRange(const WaveLInput &input, const std::vector<SpectrumPoint> &userSpectrum)
{
	if (!input.autoFreqRange && input.freqEnd > input.freqStart && input.freqStart > 0.0)
		return {input.freqStart, input.freqEnd};
	if (input.waveType == WaveType::USER_SPECTRUM && !userSpectrum.empty())
		return {userSpectrum.front().f, userSpectrum.back().f};
	if (input.waveType == WaveType::OCHI_HUBBLE)
	{
		const auto p = EffectiveOchiParameters(input);
		if (p.f1 <= kTiny || p.f2 <= kTiny)
			throw std::runtime_error("WaveL Ochi-Hubble frequency parameters are invalid");
		return {0.5 * std::min(p.f1, p.f2), 10.0 * std::max(p.f1, p.f2)};
	}
	if (input.tp <= kTiny)
		throw std::runtime_error("WaveL spectral generation requires positive Tp");
	return {0.5 / input.tp, 10.0 / input.tp};
}

std::vector<double> IntegrateSpectrum(const WaveLInput &input,
                                      const std::vector<SpectrumPoint> &userSpectrum,
                                      double fStart,
                                      double fEnd,
                                      double &area)
{
	const int disc = 20000;
	const double df = (fEnd - fStart) / static_cast<double>(disc);
	std::vector<double> cumulative(static_cast<std::size_t>(disc + 1), 0.0);
	area = 0.0;
	for (int i = 0; i <= disc; ++i)
	{
		const double f = fStart + df * static_cast<double>(i);
		area += SpectrumAt(f, input, userSpectrum) * df;
		cumulative[static_cast<std::size_t>(i)] = area;
	}
	return cumulative;
}

double FrequencyAtEnergy(const std::vector<double> &cumulative, double fStart, double fEnd, double energy)
{
	if (cumulative.empty())
		return fStart;
	if (energy <= 0.0)
		return fStart;
	if (energy >= cumulative.back())
		return fEnd;
	const auto upper = std::upper_bound(cumulative.begin(), cumulative.end(), energy);
	const std::size_t i1 = static_cast<std::size_t>(std::distance(cumulative.begin(), upper));
	const std::size_t i0 = i1 == 0 ? 0 : i1 - 1;
	const double alpha = (*upper - cumulative[i0]) > kTiny ? (energy - cumulative[i0]) / (*upper - cumulative[i0]) : 0.0;
	const double df = (fEnd - fStart) / static_cast<double>(cumulative.size() - 1);
	return fStart + (static_cast<double>(i0) + alpha) * df;
}

std::vector<double> DirectionSteps(const WaveLInput &input)
{
	if (input.waveDirNum <= 1 || std::abs(input.waveDirMax) <= kTiny)
		return {wavel_math::DegToRad(input.waveDirMean)};

	const int n = input.waveDirNum;
	std::vector<double> dirs;
	dirs.reserve(static_cast<std::size_t>(n));
	const double mean = wavel_math::DegToRad(input.waveDirMean);
	const double half = wavel_math::DegToRad(std::abs(input.waveDirMax));
	const double spread = std::max(input.waveDirSpread, 0.0);
	const int disc = 10000;
	const double dDir = 2.0 * half / static_cast<double>(disc);
	std::vector<double> cumulative(static_cast<std::size_t>(disc + 1), 0.0);
	double area = 0.0;
	for (int i = 0; i <= disc; ++i)
	{
		const double dir = mean - half + dDir * static_cast<double>(i);
		const double c = half > kTiny
			? std::sqrt(kPi) * std::tgamma(spread + 1.0) / (2.0 * half * std::tgamma(spread + 0.5))
			: 1.0;
		const double density = c * std::pow(std::abs(std::cos(kPi * (dir - mean) / (2.0 * half))), 2.0 * spread);
		area += density * dDir;
		cumulative[static_cast<std::size_t>(i)] = area;
	}

	for (int i = 0; i < n; ++i)
	{
		const double target = area * (static_cast<double>(i) + 0.5) / static_cast<double>(n);
		const auto upper = std::upper_bound(cumulative.begin(), cumulative.end(), target);
		const std::size_t idx = upper == cumulative.end()
			? cumulative.size() - 1
			: static_cast<std::size_t>(std::distance(cumulative.begin(), upper));
		dirs.push_back(mean - half + dDir * static_cast<double>(idx));
	}
	return dirs;
}

void AssignDirections(std::vector<WaveComponent> &components, const WaveLInput &input)
{
	auto dirs = DirectionSteps(input);
	std::mt19937 rng(static_cast<std::mt19937::result_type>(std::max(input.seed, 1)));
	std::shuffle(dirs.begin(), dirs.end(), rng);
	for (std::size_t i = 0; i < components.size(); ++i)
		components[i].direction = dirs[i % dirs.size()];
}

void CompleteComponents(std::vector<WaveComponent> &components, const WaveLInput &input)
{
	for (auto &component : components)
	{
		if (component.omega <= kTiny && component.frequency > kTiny)
			component.omega = 2.0 * kPi * component.frequency;
		if (component.frequency <= kTiny && component.omega > kTiny)
			component.frequency = component.omega / (2.0 * kPi);
		if (component.wavenumber <= kTiny)
			component.wavenumber = wavel_math::SolveDispersion(component.omega, input.gravity, input.waterDepth);
	}
	std::sort(components.begin(), components.end(), [](const auto &a, const auto &b) {
		return a.omega < b.omega;
	});
}

std::vector<WaveComponent> BuildRegular(const WaveLInput &input)
{
	if (input.hs <= kTiny || input.tp <= kTiny)
		throw std::runtime_error("WaveL regular wave requires positive Hs and Tp");
	WaveComponent component;
	component.frequency = 1.0 / input.tp;
	component.omega = 2.0 * kPi * component.frequency;
	component.amplitude = 0.5 * input.hs;
	component.phase = 0.0;
	component.direction = wavel_math::DegToRad(input.waveDirMean);
	component.wavenumber = wavel_math::SolveDispersion(component.omega, input.gravity, input.waterDepth);
	return {component};
}

std::vector<WaveComponent> BuildSpectral(const WaveLInput &input)
{
	std::vector<SpectrumPoint> userSpectrum;
	if (input.waveType == WaveType::USER_SPECTRUM)
		userSpectrum = ReadUserSpectrum(input.spectrumFilePath);

	const auto [fStart, fEnd] = FrequencyRange(input, userSpectrum);
	if (fEnd <= fStart)
		throw std::runtime_error("WaveL spectral frequency range is invalid");

	double area = 0.0;
	const auto cumulative = IntegrateSpectrum(input, userSpectrum, fStart, fEnd, area);
	if (area <= kTiny)
		throw std::runtime_error("WaveL spectral area is zero");

	const int n = std::max(input.waveFreqNum, 1);
	std::vector<WaveComponent> components;
	components.reserve(static_cast<std::size_t>(n));
	std::mt19937 rng(static_cast<std::mt19937::result_type>(std::max(input.seed, 1)));
	std::uniform_real_distribution<double> phaseDist(0.0, 2.0 * kPi);

	const double targetHs = TargetSpectrumHs(input);
	const double targetArea = targetHs > kTiny ? targetHs * targetHs / 16.0 : area;
	const double norm = targetArea / area;
	if (input.freqDisc == WaveFrequencyDiscretization::EQUAL_FREQUENCY)
	{
		const double df = (fEnd - fStart) / static_cast<double>(n);
		for (int i = 0; i < n; ++i)
		{
			const double left = fStart + df * static_cast<double>(i);
			const double right = left + df;
			const double mid = 0.5 * (left + right);
			const double en = SpectrumAt(mid, input, userSpectrum) * df;
			components.push_back({mid, 2.0 * kPi * mid, std::sqrt(std::max(2.0 * en * norm, 0.0)), phaseDist(rng), 0.0, 0.0});
		}
	}
	else
	{
		const double dEnergy = area / static_cast<double>(n);
		for (int i = 0; i < n; ++i)
		{
			const double e0 = dEnergy * static_cast<double>(i);
			const double e1 = dEnergy * static_cast<double>(i + 1);
			const double f0 = FrequencyAtEnergy(cumulative, fStart, fEnd, e0);
			const double f1 = FrequencyAtEnergy(cumulative, fStart, fEnd, e1);
			const int subSteps = input.dfMax > kTiny && f1 > f0
				? std::max(1, static_cast<int>(std::ceil((f1 - f0) / input.dfMax)))
				: 1;
			for (int sub = 0; sub < subSteps; ++sub)
			{
				const double se0 = e0 + dEnergy * static_cast<double>(sub) / static_cast<double>(subSteps);
				const double se1 = e0 + dEnergy * static_cast<double>(sub + 1) / static_cast<double>(subSteps);
				const double f = FrequencyAtEnergy(cumulative, fStart, fEnd, 0.5 * (se0 + se1));
				const double en = dEnergy / static_cast<double>(subSteps);
				components.push_back({f, 2.0 * kPi * f, std::sqrt(std::max(2.0 * en * norm, 0.0)), phaseDist(rng), 0.0, 0.0});
			}
		}
	}

	AssignDirections(components, input);
	CompleteComponents(components, input);
	return components;
}

double InterpolateSeries(const std::vector<std::vector<double>> &rows, double t)
{
	if (rows.empty())
		return 0.0;
	if (t <= rows.front()[0])
		return rows.front()[1];
	if (t >= rows.back()[0])
		return rows.back()[1];
	for (std::size_t i = 0; i + 1 < rows.size(); ++i)
	{
		if (t >= rows[i][0] && t <= rows[i + 1][0])
		{
			const double span = std::max(rows[i + 1][0] - rows[i][0], kTiny);
			const double alpha = (t - rows[i][0]) / span;
			return rows[i][1] * (1.0 - alpha) + rows[i + 1][1] * alpha;
		}
	}
	return rows.back()[1];
}

std::vector<WaveComponent> BuildFromTimeSeries(const WaveLInput &input)
{
	auto rows = ReadRows(input.timeSeriesFilePath, 2);
	std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a[0] < b[0]; });
	const double dt = input.dftSample > kTiny ? 1.0 / input.dftSample : rows[1][0] - rows[0][0];
	const double t0 = rows.front()[0];
	const double t1 = rows.back()[0];
	const int n = static_cast<int>(std::floor((t1 - t0) / dt));
	if (n < 4)
		throw std::runtime_error("WaveL time series is too short for DFT");

	std::vector<double> eta(static_cast<std::size_t>(n));
	for (int i = 0; i < n; ++i)
		eta[static_cast<std::size_t>(i)] = InterpolateSeries(rows, t0 + dt * static_cast<double>(i));

	const double cutIn = input.dftCutIn > 0.0 ? input.dftCutIn : 1.0 / (dt * n);
	const double cutOut = input.dftCutOut > 0.0 ? input.dftCutOut : 0.5 / dt;
	std::vector<WaveComponent> components;
	for (int k = 1; k < n / 2; ++k)
	{
		const double f = static_cast<double>(k) / (dt * static_cast<double>(n));
		if (f < cutIn || f > cutOut)
			continue;
		double cosSum = 0.0;
		double sinSum = 0.0;
		for (int i = 0; i < n; ++i)
		{
			const double angle = 2.0 * kPi * static_cast<double>(k) * static_cast<double>(i) / static_cast<double>(n);
			cosSum += eta[static_cast<std::size_t>(i)] * std::cos(angle);
			sinSum += eta[static_cast<std::size_t>(i)] * std::sin(angle);
		}
		const double a = 2.0 * cosSum / static_cast<double>(n);
		const double b = 2.0 * sinSum / static_cast<double>(n);
		const double amplitude = std::sqrt(a * a + b * b);
		if (amplitude < input.dftThreshold)
			continue;
		const double phase = std::atan2(a, -b);
		components.push_back({f, 2.0 * kPi * f, amplitude, phase, wavel_math::DegToRad(input.waveDirMean), 0.0});
	}
	CompleteComponents(components, input);
	return components;
}
} // namespace

namespace wavel_spectrum
{
std::vector<WaveComponent> ReadComponentFile(const std::string &path, const WaveLInput &input)
{
	const auto rows = ReadRows(path, 4);
	std::vector<WaveComponent> components;
	components.reserve(rows.size());
	for (const auto &row : rows)
	{
		WaveComponent c;
		c.frequency = row[0];
		c.omega = 2.0 * kPi * c.frequency;
		c.amplitude = row[1];
		c.phase = wavel_math::DegToRad(row[2]);
		c.direction = wavel_math::DegToRad(row[3]);
		if (row.size() >= 5)
			c.wavenumber = row[4];
		components.push_back(c);
	}
	if (components.empty())
		throw std::runtime_error("WaveL component file contains no numeric components");
	CompleteComponents(components, input);
	return components;
}

std::vector<WaveComponent> BuildComponents(const WaveLInput &input)
{
	switch (input.waveType)
	{
	case WaveType::NONE:
		return {};
	case WaveType::REGULAR:
		return BuildRegular(input);
	case WaveType::JONSWAP:
	case WaveType::ISSC:
	case WaveType::TORSETHAUGEN:
	case WaveType::OCHI_HUBBLE:
	case WaveType::USER_SPECTRUM:
		return BuildSpectral(input);
	case WaveType::COMPONENT_FILE:
		return ReadComponentFile(input.componentFilePath, input);
	case WaveType::ELEVATION_TIME_SERIES:
		return BuildFromTimeSeries(input);
	default:
		throw std::runtime_error("Unsupported WaveL WaveType");
	}
}

void WriteComponentFile(const std::string &path, const std::vector<WaveComponent> &components)
{
	const auto parent = std::filesystem::path(path).parent_path();
	if (!parent.empty())
		std::filesystem::create_directories(parent);
	std::ofstream out(path);
	if (!out)
		throw std::runtime_error("Cannot write WaveL component file: " + path);
	out << "# frequency_Hz amplitude_m phase_deg direction_deg wavenumber_rad_per_m\n";
	out << std::setprecision(15);
	for (const auto &c : components)
	{
		out << c.frequency << ' '
		    << c.amplitude << ' '
		    << c.phase * 180.0 / kPi << ' '
		    << c.direction * 180.0 / kPi << ' '
		    << c.wavenumber << '\n';
	}
}
} // namespace wavel_spectrum
