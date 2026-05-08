#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../WaveKinematics.hpp"
#include "../WaveL_Type.hpp"
#include "../../IO/LocaleString.hpp"
#include "../../IO/Serializer.hpp"
#include "../../IO/Yaml.hpp"
#include "../../IO/ZFile.hpp"
#include "../../IO/ZPath.hpp"
#include "../../IO/ZString.hpp"
#include "../../Params.h"
#include "LocaleString_WaveL.hpp"

struct WaveComponentFileData
{
	double significantHeight = 0.0;
	double peakPeriod = 0.0;
	double peakFrequency = 0.0;
	double zeroMoment = 0.0;
	double waterDepth = 0.0;
	std::vector<WaveTrain> waveTrains;
};

struct WaveCacheFileData
{
	wavel_detail::WaveFieldCache cache;
	WaveSpectrumResult result;
	double waterDepth = 0.0;
};

namespace wavel_io_detail
{
	inline constexpr const char *kYamlRoot = "Qahse.WaveL";
	inline constexpr double kPi = 3.14159265358979323846;

	inline std::string WaveLYamlKey(const std::string &key)
	{
		return std::string(kYamlRoot) + "." + key;
	}

	inline bool IsYamlPath(const std::string &path)
	{
		const std::string ext = ZString::ToUpper(std::filesystem::path(path).extension().string());
		return ext == ".YAML" || ext == ".YML";
	}

	inline std::string KeyForFormat(Serializer::Format format, const std::string &key)
	{
		return format == Serializer::Format::Yaml ? WaveLYamlKey(key) : key;
	}

	template <typename T>
	struct FieldRef
	{
		const char *key;
		T &value;
	};

	template <typename T>
	FieldRef<T> Field(const char *key, T &value)
	{
		return {key, value};
	}

	inline void ResolveIfSet(const std::string &baseFilePath, std::string &path)
	{
		if (!path.empty())
			path = ZPath::ResolvePath(baseFilePath, path);
	}

	inline void CreateParentIfNeeded(const std::string &path)
	{
		if (path.empty())
			return;
		const auto parent = std::filesystem::path(path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent);
	}

	inline void ApplyDerivedFields(WaveLInput &input)
	{
	}

	inline void ResolvePaths(WaveLInput &input, const std::string &baseFilePath)
	{
		ResolveIfSet(baseFilePath, input.importedSpectrumPath);
		ResolveIfSet(baseFilePath, input.importedTimeSeriesPath);
		ResolveIfSet(baseFilePath, input.importedComponentsPath);
		ResolveIfSet(baseFilePath, input.importedCachePath);
		ResolveIfSet(baseFilePath, input.cachePath);
		ResolveIfSet(baseFilePath, input.metadataPath);
		ResolveIfSet(baseFilePath, input.componentsPath);
		ResolveIfSet(baseFilePath, input.timeSeriesPath);
		ResolveIfSet(baseFilePath, input.kinematicsPath);
		ResolveIfSet(baseFilePath, input.summaryPath);

		CreateParentIfNeeded(input.cachePath);
		CreateParentIfNeeded(input.metadataPath);
		CreateParentIfNeeded(input.componentsPath);
		CreateParentIfNeeded(input.timeSeriesPath);
		CreateParentIfNeeded(input.summaryPath);
		if (!input.kinematicsPath.empty())
			std::filesystem::create_directories(input.kinematicsPath);
	}

	inline std::vector<std::string> ReadDataLines(const std::string &path)
	{
		const auto lines = ZFile::ReadAllLines(path);
		std::vector<std::string> filtered;
		filtered.reserve(lines.size());
		bool inBlock = false;
		for (const std::string &raw : lines)
		{
			std::string line = ZString::Trim(raw);
			if (line.empty())
				continue;
			const std::string upper = ZString::ToUpper(std::string(line));
			if (upper == "!BEGIN")
			{
				inBlock = true;
				continue;
			}
			if (!inBlock)
				continue;
			if (line[0] == '#' || line.rfind("//", 0) == 0)
				continue;
			filtered.push_back(line);
		}
		return filtered;
	}

	inline std::vector<double> ParseDoubles(const std::string &line)
	{
		std::vector<double> values;
		std::istringstream stream(line);
		double value = 0.0;
		while (stream >> value)
			values.push_back(value);
		return values;
	}

	class WaveLInputSerializer : public Serializer
	{
	public:
		WaveLInput data;

		WaveLInputSerializer()
		{
			SetValueFirst(true);
		}

		explicit WaveLInputSerializer(const WaveLInput &input)
			: data(input)
		{
			SetValueFirst(true);
		}

	protected:
		void SerializeFields() override
		{
			Fields(
				Field("Mode", data.mode),
				Field("FreqSpectrum", data.freqSpectrum),
				Field("DirSpectrum", data.dirSpectrum),
				Field("Discretization", data.discretization),
				Field("Stretching", data.stretching),
				Field("Hs", data.Hs),
				Field("Tp", data.Tp),
				Field("TimeOffset", data.timeOffset),
				Field("DirMean", data.dirMean),
				Field("DirMax", data.dirMax),
				Field("DirSpreadExp", data.dirSpreadExp),
				Field("FCutIn", data.fCutIn),
				Field("FCutOut", data.fCutOut),
				Field("DfMax", data.dfMax),
				Field("NumFreqBins", data.numFreqBins),
				Field("NumDirBins", data.numDirBins),
				Field("RandomSeed", data.randomSeed),
				Field("Gamma", data.gamma),
				Field("Sigma1", data.sigma1),
				Field("Sigma2", data.sigma2),
				Field("AutoGamma", data.autoGamma),
				Field("AutoSigma", data.autoSigma),
				Field("AutoFreqRange", data.autoFreqRange),
				Field("Hs1", data.Hs1),
				Field("Hs2", data.Hs2),
				Field("F1", data.f1),
				Field("F2", data.f2),
				Field("Lambda1", data.lambda1),
				Field("Lambda2", data.lambda2),
				Field("AutoOchi", data.autoOchi),
				Field("DoublePeak", data.doublePeak),
				Field("RegularPhase", data.regularPhase),
				Field("TimeStep", data.timeStep),
				Field("SimDuration", data.simDuration),
				Field("WaterDepth", data.waterDepth),
				Field("MCFDiameter", data.mcfDiameter),
				Field("OutputComponents", data.outputComponents),
				Field("OutputTimeSeries", data.outputTimeSeries),
				Field("OutputKinematicsGrid", data.outputKinematicsGrid),
				Field("CachePath", data.cachePath),
				Field("MetadataPath", data.metadataPath),
				Field("ComponentsPath", data.componentsPath),
				Field("TimeSeriesPath", data.timeSeriesPath),
				Field("KinematicsPath", data.kinematicsPath),
				Field("SummaryPath", data.summaryPath),
				Field("GridNX", data.gridNX),
				Field("GridNY", data.gridNY),
				Field("GridNZ", data.gridNZ),
				Field("GridDX", data.gridDX),
				Field("GridDY", data.gridDY),
				Field("GridDZ", data.gridDZ),
				Field("ImportedSpectrumPath", data.importedSpectrumPath),
				Field("ImportedTimeSeriesPath", data.importedTimeSeriesPath),
				Field("ImportedComponentsPath", data.importedComponentsPath),
				Field("ImportedCachePath", data.importedCachePath));
		}

	private:
		template <typename T>
		void One(const FieldRef<T> &field)
		{
			ReadOrWrite(KeyForFormat(GetFormat(), field.key), field.value);
		}

		template <typename... Fs>
		void Fields(const Fs &...fields)
		{
			(One(fields), ...);
		}
	};
}

inline WaveLInput ReadWaveLInput(const std::string &path)
{
	wavel_io_detail::WaveLInputSerializer serializer;
	if (wavel_io_detail::IsYamlPath(path))
		serializer.ReadYamlFile(path);
	else
		serializer.ReadTextFile(path);
	wavel_io_detail::ApplyDerivedFields(serializer.data);
	wavel_io_detail::ResolvePaths(serializer.data, path);
	return serializer.data;
}

inline void WriteWaveLInput(const std::string &path, const WaveLInput &input)
{
	wavel_io_detail::WaveLInputSerializer serializer(input);
	if (wavel_io_detail::IsYamlPath(path))
		serializer.WriteYamlFile(path);
	else
		serializer.WriteTextFile(path);
}

inline WaveUserSpectrumData ReadWaveUserSpectrumData(const std::string &path)
{
	WaveUserSpectrumData data;
	for (const auto &line : wavel_io_detail::ReadDataLines(path))
	{
		const auto values = wavel_io_detail::ParseDoubles(line);
		if (values.size() < 2)
			continue;
		data.frequencies.push_back(values[0]);
		data.spectralDensity.push_back(values[1]);
	}
	if (data.frequencies.size() < 2 || data.frequencies.size() != data.spectralDensity.size())
		throw std::runtime_error(std::string(L_WAVEL_InvalidSpectrumFile) + ": " + path);
	return data;
}

inline WaveTimeSeriesData ReadWaveTimeSeriesData(const std::string &path)
{
	WaveTimeSeriesData data;
	for (const auto &line : wavel_io_detail::ReadDataLines(path))
	{
		const auto values = wavel_io_detail::ParseDoubles(line);
		if (values.size() < 2)
			continue;
		data.times.push_back(values[0]);
		data.elevations.push_back(values[1]);
	}
	if (data.times.size() < 2 || data.times.size() != data.elevations.size())
		throw std::runtime_error(std::string(L_WAVEL_InvalidSeriesFile) + ": " + path);
	return data;
}

inline void WriteWaveComponentFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result)
{
	std::vector<std::string> lines;
	lines.reserve(result.waveTrains.size() + 16);
	lines.push_back("# Qahse.WaveL wave components");
	lines.push_back(std::to_string(result.significantHeight) + "\tHs");
	lines.push_back(std::to_string(result.peakPeriod) + "\tTp");
	lines.push_back(std::to_string(result.peakFrequency) + "\tFp");
	lines.push_back(std::to_string(result.zeroMoment) + "\tm0");
	lines.push_back(std::to_string(input.waterDepth) + "\tWaterDepth");
	lines.push_back(std::to_string(result.numComponents) + "\tNumComponents");
	lines.push_back("!Begin");
	lines.push_back("# frequency_Hz amplitude_m phase_rad direction_rad wavenumber_1pm");
	for (const auto &train : result.waveTrains)
	{
		const double frequency = train.omega / (2.0 * wavel_io_detail::kPi);
		lines.push_back(
			std::to_string(frequency) + "\t" +
			std::to_string(train.amplitude) + "\t" +
			std::to_string(train.phase) + "\t" +
			std::to_string(train.direction) + "\t" +
			std::to_string(train.wavenumber));
	}
	ZFile::WriteAllLines(path, lines);
}

inline WaveComponentFileData ReadWaveComponentFile(const std::string &path)
{
	WaveComponentFileData data;
	const auto lines = ZFile::ReadAllLines(path);
	bool inBlock = false;
	for (const auto &raw : lines)
	{
		std::string line = ZString::Trim(raw);
		if (line.empty())
			continue;
		const std::string upper = ZString::ToUpper(std::string(line));
		if (upper == "!BEGIN")
		{
			inBlock = true;
			continue;
		}
		if (!inBlock)
		{
			if (line[0] == '#')
				continue;
			const auto parts = ZString::Split(line, '\t', true);
			if (parts.size() < 2)
				continue;
			const std::string key = ZString::ToUpper(parts[1]);
			const double value = ZString::StringTo<double>(parts[0]);
			if (key == "HS")
				data.significantHeight = value;
			else if (key == "TP")
				data.peakPeriod = value;
			else if (key == "FP")
				data.peakFrequency = value;
			else if (key == "M0")
				data.zeroMoment = value;
			else if (key == "WATERDEPTH")
				data.waterDepth = value;
			continue;
		}
		if (line[0] == '#')
			continue;
		const auto values = wavel_io_detail::ParseDoubles(line);
		if (values.size() < 5)
			continue;
		WaveTrain train;
		const double frequency = values[0];
		train.amplitude = values[1];
		train.phase = values[2];
		train.direction = values[3];
		train.wavenumber = values[4];
		train.omega = 2.0 * wavel_io_detail::kPi * frequency;
		train.cosDir = std::cos(train.direction);
		train.sinDir = std::sin(train.direction);
		train.A_omega = train.amplitude * train.omega;
		train.A_omega2 = train.A_omega * train.omega;
		data.waveTrains.push_back(train);
	}
	if (data.waveTrains.empty())
		throw std::runtime_error(std::string(L_WAVEL_InvalidComponentFile) + ": " + path);
	return data;
}

inline void WriteWaveTimeSeriesFile(const std::string &path,
	double timeStep,
	const std::vector<double> &times,
	const std::vector<double> &elevations)
{
	if (times.size() != elevations.size())
		throw std::runtime_error(L_WAVEL_InvalidSeriesFile);
	std::vector<std::string> lines;
	lines.reserve(times.size() + 8);
	lines.push_back("# Qahse.WaveL free-surface time series");
	lines.push_back(std::to_string(timeStep) + "\tTimeStep");
	lines.push_back(std::to_string(times.empty() ? 0.0 : times.back()) + "\tSimDuration");
	lines.push_back(std::to_string(times.size()) + "\tNumSamples");
	lines.push_back("!Begin");
	lines.push_back("# time_s eta_m");
	for (std::size_t i = 0; i < times.size(); ++i)
		lines.push_back(std::to_string(times[i]) + "\t" + std::to_string(elevations[i]));
	ZFile::WriteAllLines(path, lines);
}

inline void WriteWaveCacheFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result,
	const wavel_detail::WaveFieldCache &cache)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open())
		throw std::runtime_error(std::string(L_WAVEL_CannotOpenOutput) + ": " + path);

	const char magic[8] = {'Q', 'W', 'F', 'C', '0', '0', '0', '1'};
	stream.write(magic, sizeof(magic));

	auto writePod = [&](const auto &value) {
		stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
	};
	auto writeVector = [&](const std::vector<double> &values) {
		if (!values.empty())
			stream.write(reinterpret_cast<const char *>(values.data()), static_cast<std::streamsize>(sizeof(double) * values.size()));
	};

	const std::uint32_t version = 1;
	const std::uint32_t flags = 0u;
	const std::int32_t nx = cache.nx;
	const std::int32_t ny = cache.ny;
	const std::int32_t nz = cache.nz;
	const std::int32_t nt = cache.nt;
	const std::int32_t numComponents = result.numComponents;

	writePod(version);
	writePod(flags);
	writePod(nx);
	writePod(ny);
	writePod(nz);
	writePod(nt);
	writePod(cache.dx);
	writePod(cache.dy);
	writePod(cache.dz);
	writePod(cache.dt);
	writePod(cache.zBottom);
	writePod(input.waterDepth);
	writePod(result.significantHeight);
	writePod(result.peakPeriod);
	writePod(result.peakFrequency);
	writePod(result.zeroMoment);
	writePod(result.fMin);
	writePod(result.fMax);
	writePod(result.spectralArea);
	writePod(std::int32_t{1});
	writePod(numComponents);
	writeVector(cache.eta);
	writeVector(cache.u);
	writeVector(cache.v);
	writeVector(cache.w);
	writeVector(cache.ax);
	writeVector(cache.ay);
	writeVector(cache.az);
	writeVector(cache.dynP);

	if (!stream.good())
		throw std::runtime_error("Failed to write WaveL cache file: " + path);
}

inline WaveCacheFileData ReadWaveCacheFile(const std::string &path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open())
		throw std::runtime_error(std::string(L_WAVEL_CannotOpenOutput) + ": " + path);

	char magic[8] = {};
	stream.read(magic, sizeof(magic));
	if (std::string(magic, sizeof(magic)) != std::string("QWFC0001", 8))
		throw std::runtime_error("Invalid WaveL cache file header: " + path);

	auto readPod = [&](auto &value) {
		stream.read(reinterpret_cast<char *>(&value), sizeof(value));
	};
	auto readVector = [&](std::vector<double> &values) {
		if (!values.empty())
			stream.read(reinterpret_cast<char *>(values.data()), static_cast<std::streamsize>(sizeof(double) * values.size()));
	};

	WaveCacheFileData data;
	std::uint32_t version = 0;
	std::uint32_t flags = 0;
	std::int32_t reservedWaveOrder = 0;
	readPod(version);
	readPod(flags);
	readPod(data.cache.nx);
	readPod(data.cache.ny);
	readPod(data.cache.nz);
	readPod(data.cache.nt);
	readPod(data.cache.dx);
	readPod(data.cache.dy);
	readPod(data.cache.dz);
	readPod(data.cache.dt);
	readPod(data.cache.zBottom);
	readPod(data.waterDepth);
	readPod(data.result.significantHeight);
	readPod(data.result.peakPeriod);
	readPod(data.result.peakFrequency);
	readPod(data.result.zeroMoment);
	readPod(data.result.fMin);
	readPod(data.result.fMax);
	readPod(data.result.spectralArea);
	readPod(reservedWaveOrder);
	readPod(data.result.numComponents);
	if (data.cache.nx <= 0 || data.cache.ny <= 0 || data.cache.nz <= 0 || data.cache.nt <= 0)
		throw std::runtime_error("Invalid WaveL cache dimensions: " + path);
	data.cache.Resize(data.cache.nx, data.cache.ny, data.cache.nz, data.cache.nt);
	readVector(data.cache.eta);
	readVector(data.cache.u);
	readVector(data.cache.v);
	readVector(data.cache.w);
	readVector(data.cache.ax);
	readVector(data.cache.ay);
	readVector(data.cache.az);
	readVector(data.cache.dynP);

	if (!stream.good())
		throw std::runtime_error("Failed to read WaveL cache file: " + path);
	return data;
}

inline void WriteWaveSummaryFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result,
	const std::string &modeLabel)
{
	std::ostringstream stream;
	stream.setf(std::ios::fixed);
	stream.precision(6);
	stream << "Qahse WaveL Summary\n";
	stream << "Mode: " << modeLabel << "\n";
	stream << "Significant wave height Hs: " << result.significantHeight << " m\n";
	stream << "Peak period Tp: " << result.peakPeriod << " s\n";
	stream << "Peak frequency fp: " << result.peakFrequency << " Hz\n";
	stream << "Water depth: " << input.waterDepth << " m\n";
	stream << "Component count: " << result.numComponents << "\n";
	stream << "Frequency range: [" << result.fMin << ", " << result.fMax << "] Hz\n";
	stream << "Spectral area: " << result.spectralArea << " m^2\n";
	stream << "m0: " << result.zeroMoment << " m^2\n";
	stream << "Primary cache file: " << result.cacheFilePath << "\n";
	stream << "Primary metadata file: " << result.metadataFilePath << "\n";
	stream << "WaveL scope: first-order incident wave environment service\n";
	if (!result.componentsFilePath.empty())
		stream << "Aux components file: " << result.componentsFilePath << "\n";
	if (!result.timeSeriesFilePath.empty())
		stream << "Aux time series file: " << result.timeSeriesFilePath << "\n";
	if (!result.kinematicsDirectory.empty())
		stream << "Diagnostic VTU directory: " << result.kinematicsDirectory << "\n";
	if (!result.warnings.empty())
	{
		stream << "Warnings:\n";
		for (const auto &warning : result.warnings)
			stream << "  - " << warning << "\n";
	}
	ZFile::WriteAllText(path, stream.str());
}

inline void WriteWaveMetadataFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result,
	const std::string &modeLabel)
{
	std::ostringstream stream;
	stream.setf(std::ios::fixed);
	stream.precision(6);
	stream << "Qahse WaveL Wave Field Metadata\n";
	stream << "Mode: " << modeLabel << "\n";
	stream << "Primary interface: .wfc/.wfm\n";
	stream << "Significant wave height Hs: " << result.significantHeight << " m\n";
	stream << "Peak period Tp: " << result.peakPeriod << " s\n";
	stream << "Peak frequency fp: " << result.peakFrequency << " Hz\n";
	stream << "Water depth: " << input.waterDepth << " m\n";
	stream << "WaveL scope: first-order incident wave environment service\n";
	stream << "Component count: " << result.numComponents << "\n";
	stream << "Frequency range: [" << result.fMin << ", " << result.fMax << "] Hz\n";
	stream << "Spectral area: " << result.spectralArea << " m^2\n";
	stream << "m0: " << result.zeroMoment << " m^2\n";
	stream << "GridNX/GridNY/GridNZ: " << input.gridNX << ", " << input.gridNY << ", " << input.gridNZ << "\n";
	stream << "GridDX/GridDY/GridDZ: " << input.gridDX << ", " << input.gridDY << ", " << input.gridDZ << " m\n";
	stream << "TimeStep/Duration: " << input.timeStep << " s, " << input.simDuration << " s\n";
	stream << "Cache file: " << result.cacheFilePath << "\n";
	if (!result.componentsFilePath.empty())
		stream << "Aux components file: " << result.componentsFilePath << "\n";
	if (!result.timeSeriesFilePath.empty())
		stream << "Aux time series file: " << result.timeSeriesFilePath << "\n";
	if (!result.summaryFilePath.empty())
		stream << "Aux summary file: " << result.summaryFilePath << "\n";
	if (!result.kinematicsDirectory.empty())
		stream << "Diagnostic VTU directory: " << result.kinematicsDirectory << "\n";
	if (!result.warnings.empty())
	{
		stream << "Warnings:\n";
		for (const auto &warning : result.warnings)
			stream << "  - " << warning << "\n";
	}
	ZFile::WriteAllText(path, stream.str());
}
